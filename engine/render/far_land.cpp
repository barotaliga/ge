// The far land's shape (the height a kind gives a point), its mesh (the polar grid, its normals,
// moisture and horizons, baked in bands on the pool), and the pass's buffers, pipeline and draw.
//
// ge:summary The far land's height field, polar mesh, horizon bake, buffers, pipeline and pass
// ge:keywords render far-land far_height build_far_land ridged-noise polar-grid horizon-bake
// ge:keywords encode_far_land prepare_far_land set_far_land
// ge:skill render
// ge:test tests/render_far_land_test.cpp
#include "render/far_land.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/noise.hpp"
#include "core/parallel.hpp"
#include "render/far_land.wgsl.hpp"
#include "render/night.hpp"

namespace ge::render {

namespace {

namespace lm = core::libm;
using BU = wgpu::BufferUsage;
constexpr double kPi = 3.14159265358979323846;
constexpr double kHalfPi = kPi / 2;

double clamp01(double t) { return std::clamp(t, 0.0, 1.0); }
double smooth01(double t) { return core::smooth(clamp01(t)); }

// Value noise summed over `octaves`, each turned and offset so the lattice's axes never line up,
// 0..1.
double fbm(double x, double z, double span, double seed, int octaves) {
  double sum = 0, amp = 0.5, norm = 0, c = 1, s = 0;
  const double cr = 0.8253356149, sr = 0.5646424734;  // a turn of 0.6 rad an octave
  for (int o = 0; o < octaves; ++o) {
    const double xr = c * x - s * z + o * 137.0, zr = s * x + c * z - o * 71.0;
    sum += amp * core::value_noise(xr, zr, span, seed + o);
    norm += amp;
    amp *= 0.5;
    span *= 0.5;
    const double cn = c * cr - s * sr;
    s = s * cr + c * sr;
    c = cn;
  }
  return sum / norm;
}

// RIDGED NOISE, 0..1: each octave folded about its middle (1 - |2n - 1|, a crest where the noise
// crosses its mean), squared toward a sharp arête by `sharp`, and the finer octaves weighed by the
// coarser's crest, so the detail climbs the ridges and the valleys stay smooth.
double ridged(double x, double z, double span, double seed, double sharp) {
  double sum = 0, amp = 0.5, norm = 0, weight = 1, c = 1, s = 0;
  const double cr = 0.8253356149, sr = 0.5646424734;
  for (int o = 0; o < 7; ++o) {
    const double xr = c * x - s * z + o * 211.0, zr = s * x + c * z + o * 93.0;
    const double n = core::value_noise(xr, zr, span, seed + 17 * o);
    double r = 1 - std::abs(2 * n - 1);
    r = r + (r * r - r) * sharp;
    sum += amp * r * weight;
    norm += amp;
    weight = clamp01(r * (1.2 + sharp));
    weight = 1 + (weight - 1) * sharp;
    amp *= 0.5;
    span *= 0.5;
    const double cn = c * cr - s * sr;
    s = s * cr + c * sr;
    c = cn;
  }
  return sum / norm;
}

// The polar grid: ring j's radius and back.
struct Rings {
  double r0 = 1, ratio = 1, log_ratio = 1;
  int count = 0;
  double radius(int j) const { return r0 * lm::exp(j * log_ratio); }
};

Rings rings_for(const FarSite& site, int rings) {
  Rings g;
  g.count = std::max(rings, 2);
  g.r0 = std::max(0.3 * site.radius_m, 5.0);
  g.log_ratio = lm::log(kFarReach / g.r0) / (g.count - 1);
  g.ratio = lm::exp(g.log_ratio);
  return g;
}

// The land's height at a plan point (from the site's middle) off the baked grid, bilinear; under
// ring 0 the first ring's mean, past the last ring nothing (-1e30).
struct GridRead {
  const std::vector<FarVertex>* v;
  int bearings;
  Rings g;
  double top;
  double at(double x, double z) const {
    const double r = std::sqrt(x * x + z * z);
    if (r <= g.r0) return top;
    const double jf = lm::log(r / g.r0) / g.log_ratio;
    if (jf >= g.count - 1) return -1e30;
    double u = lm::atan2(x, z) / (2 * kPi) * bearings;
    if (u < 0) u += bearings;
    const int i0 = std::min(static_cast<int>(u), bearings - 1), j0 = static_cast<int>(jf);
    const int i1 = (i0 + 1) % bearings;
    const double fu = u - i0, fj = jf - j0;
    const auto y = [&](int i, int j) { return double((*v)[size_t(j) * bearings + i].pos[1]); };
    const double a = y(i0, j0) + (y(i1, j0) - y(i0, j0)) * fu;
    const double b = y(i0, j0 + 1) + (y(i1, j0 + 1) - y(i0, j0 + 1)) * fu;
    return a + (b - a) * fj;
  }
};

uint32_t pack_angle(double a) {
  return static_cast<uint32_t>(std::lround(std::clamp(a / kHalfPi, 0.0, 1.0) * 255));
}

// A vertex's horizon in each bearing: the highest elevation the land ahead stands at, from a step
// `step0` m growing by 1.35 a step to 30 km; and the share of the sky it leaves open.
void march_horizons(const GridRead& read, double step0, FarVertex& v) {
  double open = 0;
  uint32_t packed[2] = {0, 0};
  const double y0 = v.pos[1] + 2.0;
  for (int k = 0; k < kFarBearings; ++k) {
    const double hd = 2 * kPi * k / kFarBearings;
    const double sx = lm::sin(hd), sz = lm::cos(hd);
    double best = 0, s = step0;
    for (int step = 0; step < 64 && s < 30000; ++step, s *= 1.35) {
      const double y = read.at(v.pos[0] + sx * s, v.pos[2] + sz * s);
      if (y > y0) best = std::max(best, lm::atan2(y - y0, s));
    }
    open += 1 - lm::sin(best);
    packed[k / 4] |= pack_angle(best) << (8 * (k % 4));
  }
  v.n[3] = float(open / kFarBearings);
  v.horizon[0] = packed[0];
  v.horizon[1] = packed[1];
  v.pad[0] = v.pad[1] = 0;
}

}  // namespace

double far_height(const FarLandKind& k, const FarSite& site, double x, double z) {
  const double dx = x - site.x, dz = z - site.z;
  const double d = std::sqrt(dx * dx + dz * dz);
  const double top = site.floor_y - 1;
  const double seed = site.seed;
  // THE HILL the world stands on: level inside its circle, falling to the valley floor past it.
  const double t = smooth01((d - site.radius_m) / std::max(k.hill_run, 1.0));
  if (t <= 0) return top;
  const double roll = (fbm(dx, dz, 450, seed + 3, 4) - 0.5) * std::min(60.0, 0.3 * k.hill_drop);
  double y = -k.hill_drop * t + roll * t;
  // The valley floor's own swell: low hills between the lakes.
  const double swell = std::min(0.05 * k.peak, 140.0);
  y += (fbm(dx, dz, 0.9 * k.span, seed + 2, 4) - 0.45) * 2 * swell * t;
  // THE RANGES: rising from half the first range's distance, massifs (a broad body) crowned by
  // ridges, with passes between them.
  const double env = smooth01((d - 0.45 * k.near_m) / (0.9 * k.near_m)) * t;
  if (env > 0) {
    const double massif = 0.35 + 0.65 * smooth01(fbm(dx, dz, 5 * k.span, seed + 5, 3) * 1.6 - 0.3);
    const double body = smooth01(fbm(dx, dz, 1.6 * k.span, seed + 9, 4) * 1.7 - 0.35);
    const double ridge = ridged(dx, dz, k.span, seed + 7, k.ridged);
    double m = k.peak * massif * (0.2 * body + ridge * std::sqrt(ridge));
    if (k.towers > 0) {
      // Towers: flat-topped stacks with near-sheer walls where a coarse noise crosses a band.
      const double n = fbm(dx, dz, 0.45 * k.span, seed + 11, 3);
      const double stack = smooth01((n - 0.6) / 0.1);
      m += k.towers * 0.45 * k.peak * stack * (0.6 + 0.4 * fbm(dx, dz, 0.2 * k.span, seed + 13, 2));
    }
    if (k.cones > 0) {
      // Cones: in a grid of cells 3 spans wide, a share of them hold a volcano at a dealt place,
      // its flanks concave (steeper toward the top), a crater in its summit.
      const double cell = 3 * k.span;
      const double cx = std::floor(dx / cell), cz = std::floor(dz / cell);
      for (int oz = -1; oz <= 1; ++oz) {
        for (int ox = -1; ox <= 1; ++ox) {
          const double gx = cx + ox, gz = cz + oz;
          if (core::hash2(gx, gz, seed + 29) >= k.cones) continue;
          const double px = (gx + 0.2 + 0.6 * core::hash2(gx, gz, seed + 31)) * cell;
          const double pz = (gz + 0.2 + 0.6 * core::hash2(gx, gz, seed + 37)) * cell;
          const double radius = cell * (0.25 + 0.15 * core::hash2(gx, gz, seed + 41));
          const double tall = k.peak * (1.6 + 1.2 * core::hash2(gx, gz, seed + 43));
          const double q = std::sqrt((dx - px) * (dx - px) + (dz - pz) * (dz - pz)) / radius;
          if (q >= 1) continue;
          const double p = 1 - q;
          double cone = tall * p * p * (0.6 + 0.4 * p);
          if (q < 0.06) cone -= tall * 0.05 * smooth01((0.06 - q) / 0.03);  // the crater
          m = std::max(m, cone);
        }
      }
    }
    if (k.terraces > 0) {
      // A mesa's terraces: each step a flat tread and a short steep riser.
      const double q = m / k.terraces, f = q - std::floor(q);
      m = (std::floor(q) + smooth01((f - 0.7) / 0.3)) * k.terraces;
    }
    y += m * env;
  }
  // THE LAKES: dug into the low ground past the hill, below the water's level.
  if (k.lakes > 0) {
    const double l = fbm(dx, dz, 1.6 * k.span, seed + 19, 3);
    const double dig = smooth01((l - 0.5) / 0.15) * k.lakes;
    const double low = 1 - smooth01((y + k.hill_drop) / (0.25 * k.peak));
    y -= dig * low * t * (std::max(k.water, 0.0) + swell + 0.04 * k.peak);
  }
  return top + y;
}

double far_snow_line(const FarLandKind& kind, const FarMesh& mesh, double warm_c) {
  return mesh.valley_y + (kind.temp_c + warm_c) / kLapse;
}

double far_horizon(const FarVertex& v, int bearing) {
  const uint32_t w = v.horizon[bearing / 4];
  return ((w >> (8 * (bearing % 4))) & 0xffu) / 255.0 * kHalfPi;
}

FarMesh build_far_land(const FarLandKind& kind, const FarSite& site, int bearings, int rings) {
  FarMesh mesh;
  const int b = std::max(bearings, 8);
  const Rings g = rings_for(site, rings);
  mesh.bearings = b;
  mesh.rings = g.count;
  const double top = site.floor_y - 1;
  mesh.valley_y = top - kind.hill_drop;
  const size_t land = size_t(b) * g.count;
  mesh.vertices.resize(land);
  std::vector<double> cos_b(b), sin_b(b);
  for (int i = 0; i < b; ++i) {
    const double h = 2 * kPi * i / b;
    sin_b[i] = lm::sin(h);
    cos_b[i] = lm::cos(h);
  }
  // The heights and the moisture, ring by ring.
  const size_t workers = core::default_workers();
  core::parallel_bands(size_t(g.count), workers, [&](size_t lo, size_t hi) {
    for (size_t j = lo; j < hi; ++j) {
      const double r = g.radius(int(j));
      const double sink = far_sink(r);
      for (int i = 0; i < b; ++i) {
        const double x = r * sin_b[i], z = r * cos_b[i];
        FarVertex& v = mesh.vertices[j * b + i];
        v.pos[0] = float(x);
        v.pos[1] = float(far_height(kind, site, site.x + x, site.z + z) - sink);
        v.pos[2] = float(z);
        v.pos[3] = float(fbm(x, z, 0.8 * kind.span, site.seed + 23.0, 3));
      }
    }
  });
  // The normals off the grid's neighbours, and the horizons marched over it.
  const GridRead read{&mesh.vertices, b, g, top};
  core::parallel_bands(size_t(g.count), workers, [&](size_t lo, size_t hi) {
    for (size_t j = lo; j < hi; ++j) {
      const size_t jm = j == 0 ? 0 : j - 1, jp = std::min<size_t>(j + 1, size_t(g.count) - 1);
      const double r = g.radius(int(j));
      const double step0 = std::max(r * (g.ratio - 1), 12.0);
      for (int i = 0; i < b; ++i) {
        FarVertex& v = mesh.vertices[j * b + i];
        const FarVertex& e = mesh.vertices[j * b + (i + 1) % b];
        const FarVertex& w = mesh.vertices[j * b + (i + b - 1) % b];
        const FarVertex& o = mesh.vertices[jp * b + i];
        const FarVertex& n = mesh.vertices[jm * b + i];
        const double tr[3] = {o.pos[0] - n.pos[0], o.pos[1] - n.pos[1], o.pos[2] - n.pos[2]};
        const double tb[3] = {e.pos[0] - w.pos[0], e.pos[1] - w.pos[1], e.pos[2] - w.pos[2]};
        double nx = tr[1] * tb[2] - tr[2] * tb[1], ny = tr[2] * tb[0] - tr[0] * tb[2],
               nz = tr[0] * tb[1] - tr[1] * tb[0];
        const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (ny < 0) {
          nx = -nx;
          ny = -ny;
          nz = -nz;
        }
        const double inv = len > 0 ? 1 / len : 0;
        v.n[0] = float(nx * inv);
        v.n[1] = float(len > 0 ? ny * inv : 1);
        v.n[2] = float(nz * inv);
        march_horizons(read, step0, v);
      }
    }
  });
  mesh.low_y = 1e30;
  mesh.high_y = -1e30;
  for (const FarVertex& v : mesh.vertices) {
    mesh.low_y = std::min(mesh.low_y, double(v.pos[1]));
    mesh.high_y = std::max(mesh.high_y, double(v.pos[1]));
  }
  // The land's quads, ring after ring from the middle out: drawn near to far.
  mesh.indices.reserve(size_t(b) * (g.count - 1) * 6);
  for (int j = 0; j + 1 < g.count; ++j) {
    for (int i = 0; i < b; ++i) {
      const uint32_t a = uint32_t(j * b + i), c = uint32_t(j * b + (i + 1) % b);
      const uint32_t d = a + uint32_t(b), e = c + uint32_t(b);
      mesh.indices.insert(mesh.indices.end(), {a, d, c, c, d, e});
    }
  }
  mesh.land_indices = uint32_t(mesh.indices.size());
  // THE WATER: a sheet at its level from the hill's foot out, sunk on the curve, kept under the
  // world's floor.
  if (kind.water >= 0) {
    mesh.water_y = mesh.valley_y + std::min(kind.water, kind.hill_drop - 3);
    const int wb = 256, wr = 64;
    const double r0 = std::max(site.radius_m * 0.9, 5.0);
    const double lr = lm::log(kFarReach / r0) / (wr - 1);
    const uint32_t base = uint32_t(mesh.vertices.size());
    for (int j = 0; j < wr; ++j) {
      const double r = r0 * lm::exp(j * lr);
      for (int i = 0; i < wb; ++i) {
        const double h = 2 * kPi * i / wb;
        FarVertex v{};
        v.pos[0] = float(r * lm::sin(h));
        v.pos[1] = float(mesh.water_y - far_sink(r));
        v.pos[2] = float(r * lm::cos(h));
        v.pos[3] = -1;
        v.n[1] = 1;
        v.n[3] = 1;
        mesh.vertices.push_back(v);
      }
    }
    // Its horizons over the land, as the land's own: the key light (and its glade) is hidden from
    // the water behind a ridge.
    core::parallel_bands(size_t(wr), workers, [&](size_t lo, size_t hi) {
      for (size_t j = lo; j < hi; ++j) {
        const double step0 = std::max(r0 * lm::exp(double(j) * lr) * (lm::exp(lr) - 1), 12.0);
        for (int i = 0; i < wb; ++i) march_horizons(read, step0, mesh.vertices[base + j * wb + i]);
      }
    });
    for (int j = 0; j + 1 < wr; ++j) {
      for (int i = 0; i < wb; ++i) {
        const uint32_t a = base + uint32_t(j * wb + i), c = base + uint32_t(j * wb + (i + 1) % wb);
        const uint32_t d = a + uint32_t(wb), e = c + uint32_t(wb);
        mesh.indices.insert(mesh.indices.end(), {a, d, c, c, d, e});
      }
    }
  }
  return mesh;
}

// ── The pass ──

void create_far_land(gpu::Device& device, gpu::PipelineSet& pipelines, FarLand& land,
                     const wgpu::BindGroupLayout& frame) {
  land.params = make_buffer(device, "far_land.params", sizeof(FarLand::Params), BU::Uniform);
  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  land.layout = make_layout(device, "far_land", {Slot::uniform, Slot::storage_read}, vf);
  const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::far_land, "far_land");
  wgpu::ColorTargetState colours[2]{};
  colours[0].format = kHdrFormat;
  colours[1].format = kMotionFormat;
  wgpu::FragmentState fragment{};
  fragment.module = module;
  fragment.entryPoint = "fs_main";
  fragment.targetCount = 2;
  fragment.targets = colours;
  wgpu::DepthStencilState depth{};
  depth.format = kDepthFormat;
  depth.depthWriteEnabled = true;
  depth.depthCompare = wgpu::CompareFunction::Greater;  // reversed-z: nearer is greater
  wgpu::RenderPipelineDescriptor desc{};
  desc.label = "far_land.draw";
  desc.layout = make_pipeline_layout(device, "far_land", {frame, land.layout});
  desc.vertex.module = module;
  desc.vertex.entryPoint = "vs_main";
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.primitive.cullMode = wgpu::CullMode::None;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;
  land.p_draw = pipelines.add_render(device, desc);
}

void set_far_land(gpu::Device& device, FarLand& land, const FarLandKind& kind, const FarSite& site,
                  const FarMesh& mesh) {
  land.vertices = make_buffer(device, "far_land.vertices", mesh.vertices.size() * sizeof(FarVertex),
                              BU::Storage);
  device.queue.WriteBuffer(land.vertices, 0, mesh.vertices.data(),
                           mesh.vertices.size() * sizeof(FarVertex));
  land.indices =
      make_buffer(device, "far_land.indices", mesh.indices.size() * sizeof(uint32_t), BU::Index);
  device.queue.WriteBuffer(land.indices, 0, mesh.indices.data(),
                           mesh.indices.size() * sizeof(uint32_t));
  land.group = make_group(device, "far_land", land.layout, {land.params, land.vertices});
  land.index_count = uint32_t(mesh.indices.size());
  land.site[0] = site.x;
  land.site[1] = site.z;
  land.valley_y = mesh.valley_y;
  land.water_y = mesh.water_y;
  land.kind = &kind;
  land.on = true;
}

void clear_far_land(FarLand& land) {
  land.on = false;
  land.drawn = false;
}

void prepare_far_land(gpu::Device& device, FarLand& land, const FrameView& view, double warm_c,
                      bool drawn) {
  land.drawn = drawn && land.on && land.kind != nullptr;
  if (!land.drawn) return;
  const FarLandKind& k = *land.kind;
  FarLand::Params& p = land.staged;
  p.eye[0] = float(view.eye[0] - land.site[0]);
  p.eye[1] = float(view.eye[1]);
  p.eye[2] = float(view.eye[2] - land.site[1]);
  p.eye[3] = float(std::fmod(view.time_s, 3600.0));
  // Hot lava's light, about 5000 cd/m^2 through the breaks in its crust, in the frame's units.
  const double glow = 5000 / kSunLux * pre_exposure(view);
  p.site[0] = float(land.valley_y);
  p.site[1] = float(land.water_y);
  p.site[2] = k.liquid == FarLiquid::lava ? 1.0f : 0.0f;
  p.site[3] = float(glow);
  p.climate[0] = float(k.temp_c + warm_c);
  p.climate[1] = float(kLapse);
  p.climate[2] = k.tree_m > 0 ? float(land.valley_y + k.tree_m) : -1e9f;  // no forest
  p.climate[3] = float(k.strata_m);
  p.far[0] = float(k.clarity_km * 1000);
  p.far[1] = float(k.towers);
  p.far[2] = float(land.site[0]);
  p.far[3] = float(land.site[1]);
  const float* rows[9] = {k.rock, k.rock2, k.grass, k.dry,    k.forest,
                          k.snow, k.sand,  k.deep,  k.shallow};
  for (int r = 0; r < 9; ++r) {
    for (int c = 0; c < 3; ++c) p.colours[r][c] = rows[r][c];
    p.colours[r][3] = 0;
  }
  device.queue.WriteBuffer(land.params, 0, &p, sizeof p);
}

void encode_far_land(const FarLand& land, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                     wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                     const wgpu::BindGroup& frame, bool clear) {
  if (!land.drawn && !clear) {
    rec.compute(encoder, "far_land").End();
    return;
  }
  wgpu::RenderPassColorAttachment colours[2]{};
  colours[0].view = targets.hdr_view;
  colours[1].view = targets.motion_view;
  for (auto& c : colours) {
    c.loadOp = clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
    c.storeOp = wgpu::StoreOp::Store;
    c.clearValue = {0, 0, 0, 0};
  }
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = targets.depth_view;
  depth.depthLoadOp = clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
  depth.depthStoreOp = wgpu::StoreOp::Store;
  depth.depthClearValue = 0;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 2;
  desc.colorAttachments = colours;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "far_land");
  if (land.drawn) {
    pass.SetPipeline(pipelines.render(land.p_draw));
    pass.SetBindGroup(0, frame);
    pass.SetBindGroup(1, land.group);
    pass.SetIndexBuffer(land.indices, wgpu::IndexFormat::Uint32);
    pass.DrawIndexed(land.index_count);
    ++rec.current().draws;
  }
  pass.End();
}

}  // namespace ge::render
