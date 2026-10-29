// The instance families: the LOD pick's CPU twin, the shared pipelines, a family's buffers and
// groups, the instance writes, and the cull, depth, colour and probe passes.
//
// ge:summary Instance families: tables, streams, the two-phase cull and the indirect draws
// ge:keywords render instances family cull lod bucket indirect upload streams
// ge:skill render
// ge:test tests/render_instances_test.cpp
#include "render/instances.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/output.hpp"
#include "render/instances.wgsl.hpp"
#include "render/instances_cull.wgsl.hpp"
#include "render/shadows.hpp"

namespace ge::render {

namespace {

using BU = wgpu::BufferUsage;

constexpr uint32_t kStreams = 5;                  // instances_common.wgsl's streams
constexpr uint32_t kDrawSlots = 2 * kMaxBuckets;  // instances_cull.wgsl's Counters.draws
constexpr uint64_t kStats = 8;
constexpr uint64_t kCountersBytes = uint64_t{kDrawSlots} * 20 + kStats * 4;
constexpr uint32_t kBucketShift = 20;  // the base vertex's bucket bits

// instances_common.wgsl's Mesh and Lod.
struct MeshRow {
  float sphere[4];
  uint32_t info[4];
};
struct LodRow {
  float box_min[4];
  float box_size[4];
  uint32_t info[4];
  uint32_t motion[4];  // the first word of the last frame's positions, of its skin; 0: none
};

wgpu::RenderPassDepthStencilAttachment depth_load(const FrameTargets& targets, bool read_only) {
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = targets.depth_view;
  if (read_only) {
    depth.depthReadOnly = true;
  } else {
    depth.depthLoadOp = wgpu::LoadOp::Load;
    depth.depthStoreOp = wgpu::StoreOp::Store;
  }
  return depth;
}

// Bind a family's draw group and draw its buckets of one phase (or both) from `args`: one
// indirect draw each.
uint32_t draw_buckets(wgpu::RenderPassEncoder& pass, const InstanceFamily& f,
                      const wgpu::BindGroup& group, const wgpu::Buffer& args, int first_phase,
                      int last_phase) {
  pass.SetBindGroup(2, group);
  pass.SetIndexBuffer(f.indices, wgpu::IndexFormat::Uint32);
  uint32_t draws = 0;
  for (int phase = first_phase; phase <= last_phase; ++phase) {
    for (uint32_t l = 0; l < f.lod_count; ++l) {
      pass.DrawIndexedIndirect(args, uint64_t{phase * f.lod_count + l} * 20);
      ++draws;
    }
  }
  return draws;
}
uint32_t draw_buckets(wgpu::RenderPassEncoder& pass, const InstanceFamily& f, int first_phase,
                      int last_phase) {
  return draw_buckets(pass, f, f.draw_group, f.args, first_phase, last_phase);
}

}  // namespace

namespace {

struct V3 {
  double x, y, z;
};
V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 add_scaled(V3 a, V3 b, double t) { return {a.x + b.x * t, a.y + b.y * t, a.z + b.z * t}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// The least distance between segments p0 p1 and q0 q1 (either may be a point). instances.wgsl's
// `segment_gap` is its twin.
double segment_distance(V3 p0, V3 p1, V3 q0, V3 q1) {
  const V3 d1 = sub(p1, p0), d2 = sub(q1, q0), r = sub(p0, q0);
  const double a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
  double s = 0, t = 0;
  if (a <= 1e-12 && e <= 1e-12) {
    s = t = 0;
  } else if (a <= 1e-12) {
    t = clamp01(f / e);
  } else {
    const double c = dot(d1, r);
    if (e <= 1e-12) {
      s = clamp01(-c / a);
    } else {
      const double b = dot(d1, d2), den = a * e - b * b;
      s = den > 1e-12 ? clamp01((b * f - c * e) / den) : 0;
      t = (b * s + f) / e;
      if (t < 0) {
        t = 0;
        s = clamp01(-c / a);
      } else if (t > 1) {
        t = 1;
        s = clamp01((b - c) / a);
      }
    }
  }
  const V3 w = sub(add_scaled(p0, d1, s), add_scaled(q0, d2, t));
  return std::sqrt(dot(w, w));
}

}  // namespace

float see_through_keep(const SeeThrough& see, const double eye[3], const Instance& in) {
  if (!see.on || see.width_m <= 0) return 1;
  const V3 e{eye[0], eye[1], eye[2]};
  const V3 to = sub(V3{see.subject[0], see.subject[1], see.subject[2]}, e);
  const double len = std::sqrt(dot(to, to));
  const double reach = len > see.short_m ? 1 - see.short_m / len : 0;
  const V3 end = add_scaled(e, to, reach);
  // The axis: +y rotated by the instance's quaternion (engine/core/quat.hpp's rotate of (0, 1, 0)).
  const double x = in.rotation[0], y = in.rotation[1], z = in.rotation[2], w = in.rotation[3];
  const V3 up{2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x)};
  const V3 foot{in.position[0], in.position[1], in.position[2]};
  const V3 top = add_scaled(foot, up, in.scale[1]);
  const double crown = std::max(in.scale[0], in.scale[2]);
  const double gap = segment_distance(e, end, foot, top) - crown;
  const double u = clamp01(gap / see.width_m);
  const double ease = u * u * (3 - 2 * u);
  return static_cast<float>(see.floor + (1 - see.floor) * ease);
}

LodPick pick_lod(const float* min_px, int lods, float px) {
  LodPick p;
  if (px < kMinPixels || lods <= 0) return p;
  for (int j = 0; j + 1 < lods; ++j) {
    const float t = min_px[j];
    if (px >= t * (1 - kLodBand) && px <= t * (1 + kLodBand)) {
      p.lod = j;
      p.other = j + 1;
      p.share = std::clamp((px - t * (1 - kLodBand)) / (2 * kLodBand * t), 0.0f, 1.0f);
      return p;
    }
  }
  int k = 0;
  while (k + 1 < lods && px < min_px[k]) ++k;
  p.lod = k;
  return p;
}

void create_instance_pipelines(gpu::Device& device, gpu::PipelineSet& pipelines,
                               InstancePipelines& ip, const wgpu::BindGroupLayout& frame,
                               bool overdraw_probe, bool wire_probe) {
  ip.cull_layout =
      make_layout(device, "instances.cull",
                  {Slot::uniform, Slot::storage_read, Slot::storage_read, Slot::storage_read,
                   Slot::storage, Slot::storage, Slot::storage, Slot::texture_raw},
                  wgpu::ShaderStage::Compute);
  const auto vf = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  ip.draw_layout =
      make_layout(device, "instances.draw",
                  {Slot::uniform, Slot::storage_read, Slot::storage_read, Slot::storage_read,
                   Slot::storage_read, Slot::storage_read, Slot::storage_read},
                  vf);
  ip.light_layout =
      make_layout(device, "instances.light", {Slot::uniform, Slot::texture, Slot::texture}, vf);
  ip.empty_layout = make_layout(device, "instances.empty", {}, vf);
  ip.empty_group = make_group(device, "instances.empty", ip.empty_layout, {});
  ip.shadows = no_sun_shadows(device);

  const wgpu::ShaderModule cull =
      gpu::create_shader(device, wgsl::instances_cull, "instances_cull");
  wgpu::ComputePipelineDescriptor cd{};
  cd.layout = make_pipeline_layout(device, "instances.cull", {frame, ip.cull_layout});
  cd.compute.module = cull;
  cd.label = "instances.cull.early";
  cd.compute.entryPoint = "early";
  ip.p_early = pipelines.add_compute(device, cd);
  cd.label = "instances.cull.late";
  cd.compute.entryPoint = "late";
  ip.p_late = pipelines.add_compute(device, cd);
  cd.label = "instances.cull.shadow";
  cd.compute.entryPoint = "shadow";
  ip.p_shadow_cull = pipelines.add_compute(device, cd);

  // One vertex stage for every draw (the prepass, the colour pass and the probe), so the depths
  // they compute are the same bits.
  const wgpu::ShaderModule draw = gpu::create_shader(device, wgsl::instances, "instances");
  const wgpu::PipelineLayout layout =
      make_pipeline_layout(device, "instances.draw",
                           {frame, ip.light_layout, ip.draw_layout, sun_shadow_layout(device)});
  auto pipeline = [&](const char* label, const char* entry,
                      std::initializer_list<wgpu::TextureFormat> targets, bool write_depth,
                      wgpu::CompareFunction compare, bool add) {
    wgpu::BlendState blend{};
    blend.color = {wgpu::BlendOperation::Add, wgpu::BlendFactor::One, wgpu::BlendFactor::One};
    blend.alpha = blend.color;
    std::vector<wgpu::ColorTargetState> colours;
    for (wgpu::TextureFormat format : targets) {
      wgpu::ColorTargetState c{};
      c.format = format;
      if (add) c.blend = &blend;
      colours.push_back(c);
    }
    wgpu::FragmentState fragment{};
    fragment.module = draw;
    fragment.entryPoint = entry;
    fragment.targetCount = colours.size();
    fragment.targets = colours.data();
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = write_depth;
    depth.depthCompare = compare;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = label;
    desc.layout = layout;
    desc.vertex.module = draw;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    // render/mesh.hpp's winding: a front face is clockwise on the screen.
    desc.primitive.frontFace = wgpu::FrontFace::CW;
    desc.primitive.cullMode = wgpu::CullMode::Back;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    return pipelines.add_render(device, desc);
  };
  using CF = wgpu::CompareFunction;
  {
    // A shadow map's casters: the vertex stage alone, both faces (a map has no winding to
    // trust), depth toward the sun kept (1 toward it).
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = CF::Greater;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "instances.caster";
    desc.layout =
        make_pipeline_layout(device, "instances.caster", {frame, ip.empty_layout, ip.draw_layout});
    desc.vertex.module = draw;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.cullMode = wgpu::CullMode::None;
    desc.depthStencil = &depth;
    ip.p_caster = pipelines.add_render(device, desc);
  }
  ip.p_depth = pipeline("instances.depth", "fs_depth", {}, true, CF::Greater, false);
  ip.p_draw =
      pipeline("instances.draw", "fs_main", {kHdrFormat, kMotionFormat}, false, CF::Equal, false);
  if (overdraw_probe) {
    const auto r16 = wgpu::TextureFormat::R16Float;
    ip.p_count_equal = pipeline("instances.count.equal", "fs_count", {r16}, false, CF::Equal, true);
    ip.p_count_greater =
        pipeline("instances.count.greater", "fs_count", {r16}, true, CF::Greater, true);
  }
  ip.wire = wire_probe;
  if (wire_probe) {
    // Lines over the frame's depth: no faces to cull, tested against the depth the frame left.
    wgpu::ColorTargetState c{};
    c.format = kWireFormat;
    wgpu::FragmentState fragment{};
    fragment.module = draw;
    fragment.entryPoint = "fs_wire";
    fragment.targetCount = 1;
    fragment.targets = &c;
    wgpu::DepthStencilState depth{};
    depth.format = kDepthFormat;
    depth.depthWriteEnabled = false;
    depth.depthCompare = CF::GreaterEqual;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "instances.wire";
    desc.layout = layout;
    desc.vertex.module = draw;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::LineList;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    ip.p_wire = pipelines.add_render(device, desc);
  }
}

wgpu::BindGroup instance_light_group(gpu::Device& device, const InstancePipelines& ip,
                                     const Terrain& terrain) {
  return make_group(device, "instances.light", ip.light_layout,
                    {terrain.params, terrain.horizon.views[0], terrain.horizon.views[1]});
}

void bind_instance_shadows(InstancePipelines& ip, const wgpu::BindGroup& shadows) {
  ip.shadows = shadows;
}

void create_family(gpu::Device& device, const InstancePipelines& ip, InstanceFamily& f,
                   std::string_view label, std::span<const MeshSource> meshes,
                   std::span<const float> materials_rgb, uint32_t capacity,
                   uint32_t palette_joints) {
  std::vector<Surface> surfaces;
  for (size_t i = 0; i + 2 < materials_rgb.size(); i += 3) {
    surfaces.push_back(matte_surface(&materials_rgb[i]));
  }
  create_family(device, ip, f, label, meshes, std::span<const Surface>(surfaces), capacity,
                palette_joints);
}

void create_family(gpu::Device& device, const InstancePipelines& ip, InstanceFamily& f,
                   std::string_view label, std::span<const MeshSource> meshes,
                   std::span<const Surface> surfaces, uint32_t capacity, uint32_t palette_joints) {
  f.label = std::string(label);
  f.capacity = std::clamp(capacity, 1u, kMaxInstances);
  f.count = 0;
  std::vector<uint32_t> words, indices, wire;
  std::vector<uint32_t> wire_first, wire_count;  // each LOD's lines in `wire`
  std::vector<MeshRow> mesh_rows;
  std::vector<LodRow> lod_rows;
  f.lod_px.clear();
  f.mesh_lod.clear();
  f.lod_vertex.clear();
  f.lod_last.clear();
  std::vector<uint32_t> lod_vertices;                  // each LOD's vertex count
  std::vector<const std::vector<uint32_t>*> lod_skin;  // each LOD's skin words (palette LODs)
  for (const MeshSource& m : meshes) {
    MeshRow row{};
    row.info[0] = static_cast<uint32_t>(lod_rows.size());
    f.mesh_lod.push_back(row.info[0]);
    float sphere[4] = {0, 0, 0, 0};
    const size_t n = std::min<size_t>(m.lods.size(), kMaxLods);
    if (n < m.lods.size()) {
      core::output::error("render: family '" + f.label + "' has a mesh of " +
                          std::to_string(m.lods.size()) + " LODs; only the first " +
                          std::to_string(kMaxLods) +
                          " are drawn; next: merge LODs or raise kMaxLods.");
    }
    for (size_t l = 0; l < n && lod_rows.size() < kMaxBuckets; ++l) {
      const LodSource& src = m.lods[l];
      const PackedMesh p = src.reach
                               ? pack_mesh(src.mesh, src.reach->min.data(), src.reach->size.data())
                               : pack_mesh(src.mesh);
      LodRow lr{};
      for (int k = 0; k < 3; ++k) {
        lr.box_min[k] = p.box_min[k];
        lr.box_size[k] = p.box_size[k];
      }
      lr.box_min[3] = l + 1 < n ? src.min_px : 0.0f;
      lr.info[0] = static_cast<uint32_t>(indices.size());
      lr.info[1] = static_cast<uint32_t>(p.indices.size());
      lr.info[2] = static_cast<uint32_t>(words.size() / 3);
      f.lod_vertex.push_back(lr.info[2]);
      // Placed after every LOD's vertices, below: a skinned LOD's last positions, or a palette
      // LOD's skin.
      f.lod_last.push_back(src.reach && src.skin.empty() ? 1u : 0u);
      lod_skin.push_back(src.skin.empty() ? nullptr : &src.skin);
      lod_vertices.push_back(static_cast<uint32_t>(p.words.size() / 3));
      lr.info[3] = static_cast<uint32_t>(mesh_rows.size());
      words.insert(words.end(), p.words.begin(), p.words.end());
      indices.insert(indices.end(), p.indices.begin(), p.indices.end());
      if (ip.wire) {
        wire_first.push_back(static_cast<uint32_t>(wire.size()));
        for (size_t k = 0; k + 2 < p.indices.size(); k += 3) {
          const uint32_t a = p.indices[k], b = p.indices[k + 1], c = p.indices[k + 2];
          for (uint32_t v : {a, b, b, c, c, a}) wire.push_back(v);
        }
        wire_count.push_back(static_cast<uint32_t>(wire.size()) - wire_first.back());
      }
      lod_rows.push_back(lr);
      f.lod_px.push_back(lr.box_min[3]);
      // The mesh's sphere holds every LOD's.
      float s[4];
      if (src.reach) {
        // The sphere round the reach box: every pose's.
        float r2 = 0;
        const LodReach& b = *src.reach;
        for (size_t k = 0; k < 3; ++k) {
          s[k] = b.min[k] + b.size[k] / 2;
          r2 += b.size[k] * b.size[k] / 4;
        }
        s[3] = std::sqrt(r2) * 1.0001f;
      } else {
        bounding_sphere(src.mesh, s);
      }
      if (l == 0) {
        std::memcpy(sphere, s, sizeof s);
      } else {
        const float d = std::sqrt((s[0] - sphere[0]) * (s[0] - sphere[0]) +
                                  (s[1] - sphere[1]) * (s[1] - sphere[1]) +
                                  (s[2] - sphere[2]) * (s[2] - sphere[2]));
        sphere[3] = std::max(sphere[3], d + s[3]);
      }
    }
    std::memcpy(row.sphere, sphere, sizeof sphere);
    row.info[1] = static_cast<uint32_t>(lod_rows.size()) - row.info[0];
    mesh_rows.push_back(row);
  }
  if (lod_rows.size() >= kMaxBuckets) {
    core::output::error("render: family '" + f.label + "' reaches " + std::to_string(kMaxBuckets) +
                        " LODs in all; the rest are not drawn; next: split it into two families.");
  }
  f.lod_count = static_cast<uint32_t>(lod_rows.size());
  // A skinned LOD keeps its vertices' last frame's positions after every LOD's vertices, two words
  // a vertex (the packed position's words, the second's part bits cleared), the bind's to start:
  // the skinning pass writes them (render/skinning.hpp).
  for (size_t l = 0; l < lod_rows.size(); ++l) {
    if (!f.lod_last[l]) continue;
    const uint32_t first = lod_rows[l].info[2];
    f.lod_last[l] = static_cast<uint32_t>(words.size());
    lod_rows[l].motion[0] = f.lod_last[l];
    for (uint32_t v = 0; v < lod_vertices[l]; ++v) {
      const uint32_t w0 = words[size_t{first + v} * 3], w1 = words[size_t{first + v} * 3 + 1];
      words.insert(words.end(), {w0, w1 & 0xffffu});
    }
  }
  // A palette LOD's skin after them: three words a vertex (instances.wgsl's `skinned`).
  for (size_t l = 0; l < lod_rows.size(); ++l) {
    if (!lod_skin[l] || palette_joints == 0) continue;
    std::vector<uint32_t> skin = *lod_skin[l];
    skin.resize(size_t{lod_vertices[l]} * 3, 0);
    lod_rows[l].motion[1] = static_cast<uint32_t>(words.size());
    words.insert(words.end(), skin.begin(), skin.end());
  }
  if (words.empty()) words.assign(3, 0);
  if (indices.empty()) indices.assign(3, 0);
  if (mesh_rows.empty()) mesh_rows.push_back(MeshRow{});
  if (lod_rows.empty()) lod_rows.push_back(LodRow{});

  auto buffer = [&](const char* name, uint64_t size, wgpu::BufferUsage usage) {
    return make_buffer(device, f.label + "." + name, std::max<uint64_t>(size, 16), usage);
  };
  f.vertices = buffer("vertices", words.size() * 4, BU::Storage | BU::CopySrc);
  device.queue.WriteBuffer(f.vertices, 0, words.data(), words.size() * 4);
  f.indices = buffer("indices", indices.size() * 4, BU::Index);
  device.queue.WriteBuffer(f.indices, 0, indices.data(), indices.size() * 4);
  f.meshes = buffer("meshes", mesh_rows.size() * sizeof(MeshRow), BU::Storage);
  device.queue.WriteBuffer(f.meshes, 0, mesh_rows.data(), mesh_rows.size() * sizeof(MeshRow));
  f.lods = buffer("lods", lod_rows.size() * sizeof(LodRow), BU::Storage);
  device.queue.WriteBuffer(f.lods, 0, lod_rows.data(), lod_rows.size() * sizeof(LodRow));
  std::vector<float> mats;
  pack_surfaces(surfaces, mats);
  if (mats.empty()) {
    const Surface white;
    pack_surfaces(std::span<const Surface>(&white, 1), mats);
  }
  f.materials = buffer("materials", mats.size() * 4, BU::Storage);
  device.queue.WriteBuffer(f.materials, 0, mats.data(), mats.size() * 4);
  f.instances = buffer("instances", uint64_t{kStreams} * f.capacity * 16, BU::Storage);
  f.lists = buffer("lists", uint64_t{2} * std::max(f.lod_count, 1u) * f.capacity * 4, BU::Storage);
  f.args = buffer("args", kCountersBytes, BU::Storage | BU::Indirect | BU::CopySrc);
  f.state = buffer("state", uint64_t{f.capacity} * 4, BU::Storage);
  f.params = buffer("params", sizeof(InstanceFamily::Params), BU::Uniform);
  f.streams.assign(size_t{kStreams} * f.capacity * 4, 0.0f);
  f.moved_since_cut = false;
  f.palette_joints = palette_joints;
  f.palette_half = 0;
  f.palettes_settle = true;
  f.palettes =
      buffer("palettes", uint64_t{2} * f.capacity * palette_joints * 48, BU::Storage | BU::CopyDst);

  // The draws' arguments with no instances: bucket b of phase p draws its LOD's triangles from
  // base vertex b << 20, so the vertex stage knows the bucket.
  f.args_reset.assign(kCountersBytes / 4, 0);
  if (ip.wire) {
    // The lines' draws: bucket b's from the same base vertex as its triangles'; the instance
    // counts are copied in from `args` before each wire draw.
    if (wire.empty()) wire.assign(2, 0);
    f.wire_indices = buffer("wire.indices", wire.size() * 4, BU::Index);
    device.queue.WriteBuffer(f.wire_indices, 0, wire.data(), wire.size() * 4);
    std::vector<uint32_t> wire_args(kCountersBytes / 4, 0);
    for (uint32_t phase = 0; phase < 2; ++phase) {
      for (uint32_t l = 0; l < f.lod_count; ++l) {
        const uint32_t b = phase * f.lod_count + l;
        uint32_t* a = &wire_args[size_t{b} * 5];
        a[0] = wire_count[l];
        a[2] = wire_first[l];
        a[3] = b << kBucketShift;
      }
    }
    f.wire_args = buffer("wire.args", kCountersBytes, BU::Indirect);
    device.queue.WriteBuffer(f.wire_args, 0, wire_args.data(), wire_args.size() * 4);
  }
  for (uint32_t phase = 0; phase < 2; ++phase) {
    for (uint32_t l = 0; l < f.lod_count; ++l) {
      const uint32_t b = phase * f.lod_count + l;
      uint32_t* a = &f.args_reset[size_t{b} * 5];
      a[0] = lod_rows[l].info[1];
      a[1] = 0;
      a[2] = lod_rows[l].info[0];
      a[3] = b << kBucketShift;
      a[4] = 0;
    }
  }
  f.draw_group =
      make_group(device, f.label + ".draw", ip.draw_layout,
                 {f.params, f.vertices, f.lods, f.instances, f.lists, f.materials, f.palettes});
}

void create_family_casters(gpu::Device& device, const InstancePipelines& ip, InstanceFamily& f,
                           int maps) {
  f.casters.assign(static_cast<size_t>(std::max(maps, 0)), CasterSlot{});
  for (size_t k = 0; k < f.casters.size(); ++k) {
    CasterSlot& c = f.casters[k];
    const std::string name = f.label + ".caster" + std::to_string(k);
    c.params = make_buffer(device, name + ".params", sizeof(InstanceFamily::Params), BU::Uniform);
    c.lists = make_buffer(
        device, name + ".lists",
        std::max<uint64_t>(uint64_t{std::max(f.lod_count, 1u)} * f.capacity * 4, 16), BU::Storage);
    c.args = make_buffer(device, name + ".args", kCountersBytes,
                         BU::Storage | BU::Indirect | BU::CopySrc);
    c.draw_group =
        make_group(device, name + ".draw", ip.draw_layout,
                   {c.params, f.vertices, f.lods, f.instances, c.lists, f.materials, f.palettes});
  }
}

void bind_family_hiz(gpu::Device& device, const InstancePipelines& ip, InstanceFamily& f,
                     const Hiz& hiz) {
  f.cull_group =
      make_group(device, f.label + ".cull", ip.cull_layout,
                 {f.params, f.meshes, f.lods, f.instances, f.lists, f.args, f.state, hiz.view});
  // A caster's cull reads no pyramid, but its layout is the cull's: the pyramid fills the slot.
  for (size_t k = 0; k < f.casters.size(); ++k) {
    CasterSlot& c = f.casters[k];
    c.cull_group =
        make_group(device, f.label + ".caster" + std::to_string(k) + ".cull", ip.cull_layout,
                   {c.params, f.meshes, f.lods, f.instances, c.lists, c.args, f.state, hiz.view});
  }
}

void set_instances(gpu::Device& device, InstanceFamily& f, uint32_t first,
                   std::span<const Instance> instances) {
  const uint32_t n = static_cast<uint32_t>(
      std::min<size_t>(instances.size(), f.capacity > first ? f.capacity - first : 0));
  if (n == 0) return;
  const size_t cap = f.capacity;
  float* s = f.streams.data();
  for (uint32_t k = 0; k < n; ++k) {
    const Instance& in = instances[k];
    const size_t i = first + k;
    float* place = s + (0 * cap + i) * 4;
    float* rot = s + (1 * cap + i) * 4;
    float* scale = s + (2 * cap + i) * 4;
    float* last_place = s + (3 * cap + i) * 4;
    float* last_rot = s + (4 * cap + i) * 4;
    const bool fresh = !f.moved_since_cut || i >= f.count;
    for (int c = 0; c < 3; ++c) {
      const float p = static_cast<float>(in.position[c]);
      last_place[c] = fresh ? p : place[c];
      place[c] = p;
      scale[c] = in.scale[c];
    }
    for (int c = 0; c < 4; ++c) {
      last_rot[c] = fresh ? in.rotation[c] : rot[c];
      rot[c] = in.rotation[c];
    }
    place[3] = static_cast<float>(in.material);
    scale[3] = static_cast<float>(in.mesh);
    last_place[3] = 0;
  }
  for (uint32_t stream = 0; stream < kStreams; ++stream) {
    const size_t at = (stream * cap + first) * 4;
    device.queue.WriteBuffer(f.instances, at * 4, s + at, size_t{n} * 16);
  }
  f.count = std::max(f.count, first + n);
  f.moved_since_cut = true;
}

void set_instance_count(InstanceFamily& f, uint32_t count) {
  f.count = std::min(count, f.capacity);
}

void cut_family(InstanceFamily& f) {
  f.moved_since_cut = false;
  f.palettes_settle = true;
}

void palette_rows(const InstanceFamily& f, uint32_t out[4]) {
  const uint32_t half = f.capacity * f.palette_joints * 3;
  out[0] = f.palette_joints;
  out[1] = f.palette_half * half;
  out[2] = (f.palette_half ^ 1u) * half;
  out[3] = 0;
}

void prepare_family(gpu::Device& device, InstanceFamily& f, const FrameView& view,
                    uint32_t render_height, bool occlusion) {
  f.staged.info[0] = f.count;
  f.staged.info[1] = std::max(f.lod_count, 1u);
  f.staged.info[2] = f.capacity;
  f.staged.info[3] = occlusion ? 1u : 0u;
  f.staged.cull[0] =
      static_cast<float>(render_height * 0.5 / std::tan(std::max(view.fov_y, 1e-3) * 0.5));
  f.staged.cull[1] = kMinPixels;
  f.staged.cull[2] = kLodBand;
  f.staged.cull[3] = 0;
  palette_rows(f, f.staged.skin);
  const bool see = f.see.on && f.see.width_m > 0;
  for (int k = 0; k < 3; ++k) f.staged.see[k] = static_cast<float>(f.see.subject[k]);
  f.staged.see[3] = see ? f.see.width_m : 0;
  f.staged.see_more[0] = f.see.floor;
  f.staged.see_more[1] = f.see.short_m;
  f.staged.see_more[2] = 0;
  f.staged.see_more[3] = 0;
  device.queue.WriteBuffer(f.params, 0, &f.staged, sizeof f.staged);
  device.queue.WriteBuffer(f.args, 0, f.args_reset.data(), f.args_reset.size() * 4);
}

void set_see_through(InstanceFamily& f, const SeeThrough& see) { f.see = see; }

void prepare_family_caster(gpu::Device& device, InstanceFamily& f, int slot, float texels_per_m) {
  if (slot < 0 || static_cast<size_t>(slot) >= f.casters.size()) return;
  CasterSlot& c = f.casters[static_cast<size_t>(slot)];
  InstanceFamily::Params p{};
  p.info[0] = f.count;
  p.info[1] = std::max(f.lod_count, 1u);
  p.info[2] = f.capacity;
  p.info[3] = 0;
  p.cull[0] = texels_per_m;
  p.cull[1] = kMinPixels;
  p.cull[2] = 0;  // no band: a map draws one LOD
  p.cull[3] = 0;
  palette_rows(f, p.skin);
  device.queue.WriteBuffer(c.params, 0, &p, sizeof p);
  device.queue.WriteBuffer(c.args, 0, f.args_reset.data(), f.args_reset.size() * 4);
  c.culled = f.count > 0;
}

uint32_t cull_shadow(wgpu::ComputePassEncoder& pass, std::span<InstanceFamily* const> families,
                     int slot) {
  uint32_t dispatches = 0;
  for (InstanceFamily* f : families) {
    if (static_cast<size_t>(slot) >= f->casters.size()) continue;
    const CasterSlot& c = f->casters[static_cast<size_t>(slot)];
    if (!c.culled) continue;
    pass.SetBindGroup(1, c.cull_group);
    pass.DispatchWorkgroups((f->count + 63) / 64);
    ++dispatches;
  }
  return dispatches;
}

uint32_t draw_casters(wgpu::RenderPassEncoder& pass, std::span<InstanceFamily* const> families,
                      int slot) {
  uint32_t draws = 0;
  for (InstanceFamily* f : families) {
    if (static_cast<size_t>(slot) >= f->casters.size()) continue;
    const CasterSlot& c = f->casters[static_cast<size_t>(slot)];
    if (!c.culled) continue;
    draws += draw_buckets(pass, *f, c.draw_group, c.args, 0, 0);
  }
  return draws;
}

void encode_instances_cull(std::span<InstanceFamily* const> families, const InstancePipelines& ip,
                           const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                           wgpu::CommandEncoder& encoder, const wgpu::BindGroup& frame, bool late) {
  wgpu::ComputePassEncoder pass =
      rec.compute(encoder, late ? "instances.cull.late" : "instances.cull");
  PassStat& stat = rec.current();
  pass.SetPipeline(pipelines.compute(late ? ip.p_late : ip.p_early));
  pass.SetBindGroup(0, frame);
  for (InstanceFamily* f : families) {
    if (f->count == 0) continue;
    pass.SetBindGroup(1, f->cull_group);
    pass.DispatchWorkgroups((f->count + 63) / 64);
    ++stat.dispatches;
  }
  pass.End();
}

void encode_instances_depth(std::span<InstanceFamily* const> families, const InstancePipelines& ip,
                            const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                            wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                            const wgpu::BindGroup& frame, const wgpu::BindGroup& light, bool late) {
  const wgpu::RenderPassDepthStencilAttachment depth = depth_load(targets, false);
  wgpu::RenderPassDescriptor desc{};
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass =
      rec.render(encoder, desc, late ? "instances.depth.late" : "instances.depth");
  pass.SetPipeline(pipelines.render(ip.p_depth));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, light);
  pass.SetBindGroup(3, ip.shadows);
  for (InstanceFamily* f : families) {
    if (f->count == 0) continue;
    rec.current().draws += draw_buckets(pass, *f, late ? 1 : 0, late ? 1 : 0);
  }
  pass.End();
}

void encode_instances(std::span<InstanceFamily* const> families, const InstancePipelines& ip,
                      const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                      wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                      const wgpu::BindGroup& frame, const wgpu::BindGroup& light) {
  wgpu::RenderPassColorAttachment colours[2]{};
  colours[0].view = targets.hdr_view;
  colours[1].view = targets.motion_view;
  for (auto& c : colours) {
    c.loadOp = wgpu::LoadOp::Load;
    c.storeOp = wgpu::StoreOp::Store;
  }
  const wgpu::RenderPassDepthStencilAttachment depth = depth_load(targets, true);
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 2;
  desc.colorAttachments = colours;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "instances");
  pass.SetPipeline(pipelines.render(ip.p_draw));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, light);
  pass.SetBindGroup(3, ip.shadows);
  for (InstanceFamily* f : families) {
    if (f->count == 0) continue;
    rec.current().draws += draw_buckets(pass, *f, 0, 1);
  }
  pass.End();
}

void encode_instances_overdraw(std::span<InstanceFamily* const> families,
                               const InstancePipelines& ip, const gpu::PipelineSet& pipelines,
                               wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                               const wgpu::BindGroup& frame, const wgpu::BindGroup& light,
                               const wgpu::TextureView& count, bool after_prepass) {
  wgpu::RenderPassColorAttachment colour{};
  colour.view = count;
  colour.loadOp = wgpu::LoadOp::Load;
  colour.storeOp = wgpu::StoreOp::Store;
  const wgpu::RenderPassDepthStencilAttachment depth = depth_load(targets, after_prepass);
  wgpu::RenderPassDescriptor desc{};
  desc.label = "instances.overdraw";
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&desc);
  pass.SetPipeline(pipelines.render(after_prepass ? ip.p_count_equal : ip.p_count_greater));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, light);
  pass.SetBindGroup(3, ip.shadows);
  for (InstanceFamily* f : families) {
    if (f->count != 0) draw_buckets(pass, *f, 0, 1);
  }
  pass.End();
}

void encode_instances_wire(std::span<InstanceFamily* const> families, const InstancePipelines& ip,
                           const gpu::PipelineSet& pipelines, wgpu::CommandEncoder& encoder,
                           const FrameTargets& targets, const wgpu::BindGroup& frame,
                           const wgpu::BindGroup& light, const wgpu::TextureView& colour) {
  if (!ip.wire) return;
  // This frame's instance counts into the lines' draws.
  for (InstanceFamily* f : families) {
    if (f->count == 0 || !f->wire_args) continue;
    for (uint32_t b = 0; b < 2 * f->lod_count; ++b) {
      encoder.CopyBufferToBuffer(f->args, uint64_t{b} * 20 + 4, f->wire_args, uint64_t{b} * 20 + 4,
                                 4);
    }
  }
  wgpu::RenderPassColorAttachment c{};
  c.view = colour;
  c.loadOp = wgpu::LoadOp::Load;
  c.storeOp = wgpu::StoreOp::Store;
  const wgpu::RenderPassDepthStencilAttachment depth = depth_load(targets, true);
  wgpu::RenderPassDescriptor desc{};
  desc.label = "instances.wire";
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &c;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&desc);
  pass.SetPipeline(pipelines.render(ip.p_wire));
  pass.SetBindGroup(0, frame);
  pass.SetBindGroup(1, light);
  pass.SetBindGroup(3, ip.shadows);
  for (InstanceFamily* f : families) {
    if (f->count == 0 || !f->wire_args) continue;
    pass.SetBindGroup(2, f->draw_group);
    pass.SetIndexBuffer(f->wire_indices, wgpu::IndexFormat::Uint32);
    for (uint32_t b = 0; b < 2 * f->lod_count; ++b)
      pass.DrawIndexedIndirect(f->wire_args, uint64_t{b} * 20);
  }
  pass.End();
}

namespace {

InstanceStats read_stats(gpu::Device& device, const wgpu::Buffer& buffer) {
  constexpr uint64_t kAt = uint64_t{kDrawSlots} * 20;
  constexpr uint64_t kBytes = kStats * 4;
  wgpu::BufferDescriptor desc{};
  desc.label = "instances.stats.readback";
  desc.size = kBytes;
  desc.usage = BU::MapRead | BU::CopyDst;
  wgpu::Buffer rb = device.device.CreateBuffer(&desc);
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  encoder.CopyBufferToBuffer(buffer, kAt, rb, 0, kBytes);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  uint32_t v[kStats] = {};
  gpu::wait(device, rb.MapAsync(wgpu::MapMode::Read, 0, kBytes, wgpu::CallbackMode::WaitAnyOnly,
                                [](wgpu::MapAsyncStatus, wgpu::StringView) {}));
  if (const void* p = rb.GetConstMappedRange(0, kBytes)) std::memcpy(v, p, kBytes);
  rb.Unmap();
  InstanceStats s;
  s.early = v[0];
  s.late = v[1];
  s.frustum = v[2];
  s.small = v[3];
  s.occluded = v[4];
  s.hidden = v[5];
  return s;
}

}  // namespace

InstanceStats read_instance_stats(gpu::Device& device, const InstanceFamily& f) {
  return read_stats(device, f.args);
}

InstanceStats read_caster_stats(gpu::Device& device, const InstanceFamily& f, int slot) {
  if (slot < 0 || static_cast<size_t>(slot) >= f.casters.size()) return {};
  return read_stats(device, f.casters[static_cast<size_t>(slot)].args);
}

}  // namespace ge::render
