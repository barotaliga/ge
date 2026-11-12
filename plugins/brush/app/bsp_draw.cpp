// The BSP renderer (bsp_draw.hpp): the map's buffers and atlases, its three pipelines, the lights
// picked each frame and the shadow cameras over them.
//
// ge:summary The BSP renderer: buffers, atlases, pipelines, the lights a frame and their shadows
// ge:keywords bsp brush render renderer classic v2 point-light cube-shadow sun-shadow lava-light
// ge:skill brush render
#include "brush/app/bsp_draw.hpp"
#include "prof/zones.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

#include "brush/app/bsp.wgsl.hpp"
#include "brush/app/bsp_fix.wgsl.hpp"
#include "brush/app/bsp_flames.hpp"
#include "brush/app/bsp_shadow.wgsl.hpp"
#include "render/view.hpp"
#include "water/glow.hpp"

namespace ge::brush::app {

namespace r = ge::render;

namespace {

wgpu::Texture depth_array(gpu::Device& device, std::string_view label, uint32_t size,
                          uint32_t layers, wgpu::TextureFormat format = r::kDepthFormat) {
  wgpu::TextureDescriptor desc{};
  desc.label = wgpu::StringView(label.data(), label.size());
  desc.size = {size, size, layers};
  desc.format = format;
  desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  return device.device.CreateTexture(&desc);
}

wgpu::TextureView layer_view(const wgpu::Texture& t, uint32_t k) {
  wgpu::TextureViewDescriptor desc{};
  desc.dimension = wgpu::TextureViewDimension::e2D;
  desc.baseArrayLayer = k;
  desc.arrayLayerCount = 1;
  return t.CreateView(&desc);
}

// All the layers to sample: the depth alone (a depth and stencil map's stencil is the drawing's).
wgpu::TextureView array_view(const wgpu::Texture& t, uint32_t layers) {
  wgpu::TextureViewDescriptor desc{};
  desc.dimension = wgpu::TextureViewDimension::e2DArray;
  desc.arrayLayerCount = layers;
  desc.aspect = wgpu::TextureAspect::DepthOnly;
  return t.CreateView(&desc);
}

void upload(gpu::Device& device, const wgpu::Texture& t, const Atlas& a) {
  for (uint32_t k = 0; k < a.mips; ++k) {
    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = t;
    dst.mipLevel = k;
    wgpu::TexelCopyBufferLayout layout{};
    const uint32_t w = a.width >> k, h = a.height >> k;
    layout.bytesPerRow = uint32_t(a.levels[k].size() / std::max(h, 1u));  // 4 bytes a texel, or 1
    layout.rowsPerImage = h;
    const wgpu::Extent3D size{w, h, 1};
    device.queue.WriteTexture(&dst, a.levels[k].data(), a.levels[k].size(), &layout, &size);
  }
}

void set4(float out[4], double a, double b, double c, double d) {
  out[0] = float(a);
  out[1] = float(b);
  out[2] = float(c);
  out[3] = float(d);
}

// An orthographic camera looking along `dir` (unit) at `centre`, `half` m across, `depth` m deep.
GpuJob ortho_job(const double dir[3], const double centre[3], double half, double depth) {
  const double ref[3] = {0, std::fabs(dir[1]) < 0.99 ? 1.0 : 0.0,
                         std::fabs(dir[1]) < 0.99 ? 0.0 : 1.0};
  double x[3] = {ref[1] * dir[2] - ref[2] * dir[1], ref[2] * dir[0] - ref[0] * dir[2],
                 ref[0] * dir[1] - ref[1] * dir[0]};
  const double l = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
  for (double& c : x) c /= l;
  const double y[3] = {dir[1] * x[2] - dir[2] * x[1], dir[2] * x[0] - dir[0] * x[2],
                       dir[0] * x[1] - dir[1] * x[0]};
  GpuJob j{};
  set4(j.o, centre[0], centre[1], centre[2], half);
  set4(j.x, x[0], x[1], x[2], 0);
  set4(j.y, y[0], y[1], y[2], 0);
  set4(j.z, dir[0], dir[1], dir[2], 0);
  set4(j.info, 1, depth, 0, 0);
  return j;
}

}  // namespace

bool parse_mode(std::string_view word, Mode* out) {
  if (word == "classic")
    *out = Mode::classic;
  else if (word == "v2")
    *out = Mode::v2;
  else
    return false;
  return true;
}

namespace {

// A grid of lights over each pool of `kind`'s top, each its share of a glow of `rgb` cd/m^2.
std::vector<PointLight> pool_lights(const Scene& scene, Kind kind, const double rgb[3]) {
  std::vector<PointLight> out;
  for (const LiquidPatch& p : scene.liquids) {
    if (p.kind != kind) continue;
    const double w = p.box[2] - p.box[0], d = p.box[3] - p.box[1];
    // A light every 2.5 m or so, at most 64 a pool.
    double step = 2.5;
    while ((std::floor(w / step) + 1) * (std::floor(d / step) + 1) > 64) step *= 1.25;
    const int nx = std::max(1, int(w / step)), nz = std::max(1, int(d / step));
    const double cell = (w / nx) * (d / nz) * std::min(1.0, p.area / std::max(w * d, 1e-6));
    for (int i = 0; i < nx; ++i) {
      for (int j = 0; j < nz; ++j) {
        PointLight l;
        l.pos[0] = p.box[0] + (i + 0.5) * w / nx;
        l.pos[1] = p.level + 0.25;
        l.pos[2] = p.box[1] + (j + 0.5) * d / nz;
        l.physical = true;
        // A flat glowing patch seen from the side: half its normal intensity, on average.
        l.value = 0.5 * cell;
        l.reach = 14;
        for (int k = 0; k < 3; ++k) l.colour[k] = rgb[k];
        l.flicker = Flicker::steady;
        out.push_back(l);
      }
    }
  }
  return out;
}

}  // namespace

std::vector<PointLight> lava_lights(const Scene& scene, double core_c, double cover) {
  // The pool's mean glow: the open melt's share at the core, the crust's at a dull red.
  const water::Glow melt = water::glow_of(core_c + 273.15, 0.9);
  const water::Glow crust = water::glow_of(core_c - 380 + 273.15, 0.9);
  const double m[3] = {melt.colour.r, melt.colour.g, melt.colour.b};
  const double c[3] = {crust.colour.r, crust.colour.g, crust.colour.b};
  double rgb[3];
  for (int k = 0; k < 3; ++k) {
    rgb[k] = (1 - cover) * melt.luminance * m[k] + cover * crust.luminance * c[k];
  }
  return pool_lights(scene, kLava, rgb);
}

std::vector<PointLight> slime_lights(const Scene& scene, double luminance) {
  // A sickly green, its luminance carried by the green.
  const double rgb[3] = {0.35 * luminance, 1.2 * luminance, 0.25 * luminance};
  return pool_lights(scene, kSlime, rgb);
}

std::vector<PointLight> portal_lights(const Scene& scene) {
  std::vector<PointLight> out;
  uint32_t seed = 0x5eed7e1eu;
  for (const Portal& p : scene.portals) {
    PointLight l;
    for (int k = 0; k < 3; ++k) l.pos[k] = p.centre[k] + 0.8 * p.normal[k];
    // A classic light's value by the panel's size: a bright lamp's for a doorway's teleporter.
    l.value = std::clamp(100 * p.area, 200.0, 450.0);
    l.reach = 9;
    l.colour[0] = 0.8;
    l.colour[1] = 0.82;
    l.colour[2] = 1.0;
    l.flicker = Flicker::pulse;
    l.seed = seed;
    seed = seed * 1664525u + 1013904223u;
    out.push_back(l);
  }
  return out;
}

void create_bsp(gpu::Device& device, gpu::PipelineSet& pipelines, BspDraw& d, const Scene& scene,
                const wgpu::BindGroupLayout& frame) {
  d.scene = &scene;
  // The indices by kind, so a mode draws the ranges it wants, each kind's faces in the map's
  // order; a face is a chunk of the cull (bsp_cull.hpp).
  std::vector<uint32_t> sorted;
  sorted.reserve(scene.indices.size());
  std::vector<std::array<uint32_t, 4>> chunk_of;  // first, count, kind, surface
  for (int k = 0; k < kKinds; ++k) {
    d.range[k][0] = uint32_t(sorted.size());
    for (size_t f = 0; f < scene.face_runs.size(); ++f) {
      const auto [first, count] = scene.face_runs[f];
      if (count < 3 || scene.vertices[scene.indices[first]].kind != uint32_t(k)) continue;
      chunk_of.push_back({uint32_t(sorted.size()), count, uint32_t(k), uint32_t(f)});
      sorted.insert(sorted.end(), scene.indices.begin() + first,
                    scene.indices.begin() + first + count);
    }
    d.range[k][1] = uint32_t(sorted.size()) - d.range[k][0];
  }
  d.vertices = r::make_buffer(device, "bsp.vertices", scene.vertices.size() * sizeof(DrawVertex),
                              wgpu::BufferUsage::Vertex);
  device.queue.WriteBuffer(d.vertices, 0, scene.vertices.data(),
                           scene.vertices.size() * sizeof(DrawVertex));
  d.indices = r::make_buffer(device, "bsp.indices", std::max<size_t>(sorted.size(), 3) * 4,
                             wgpu::BufferUsage::Index | wgpu::BufferUsage::Storage);
  if (!sorted.empty()) device.queue.WriteBuffer(d.indices, 0, sorted.data(), sorted.size() * 4);

  const auto sampled = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  d.atlas = r::make_texture(device, "bsp.atlas", scene.textures.width, scene.textures.height, 1,
                            wgpu::TextureFormat::RGBA8UnormSrgb, sampled, scene.textures.mips);
  upload(device, d.atlas, scene.textures);
  {
    // The relief (draw_data.hpp's Scene::surface), or a 1 x 1 flat stand-in nothing reads.
    const bool has = !scene.surface.levels.empty();
    const uint32_t w = has ? scene.surface.width : 1, h = has ? scene.surface.height : 1;
    d.surface = r::make_texture(device, "bsp.surface", w, h, 1, wgpu::TextureFormat::RGBA8Unorm,
                                sampled, has ? scene.surface.mips : 1);
    if (has) {
      upload(device, d.surface, scene.surface);
    } else {
      Atlas flat;
      flat.width = flat.height = 1;
      flat.levels.push_back({kFlatSurface[0], kFlatSurface[1], kFlatSurface[2], kFlatSurface[3]});
      upload(device, d.surface, flat);
    }
  }
  {
    // The cover (draw_data.hpp's Scene::cover), or a 1 x 1 solid stand-in the shader skips.
    const bool has = !scene.cover.levels.empty();
    const uint32_t w = has ? scene.cover.width : 1, h = has ? scene.cover.height : 1;
    d.cover = r::make_texture(device, "bsp.cover", w, h, 1, wgpu::TextureFormat::R8Unorm, sampled,
                              has ? scene.cover.mips : 1);
    if (has) {
      upload(device, d.cover, scene.cover);
    } else {
      Atlas solid;
      solid.width = solid.height = 1;
      solid.levels.push_back({255});
      upload(device, d.cover, solid);
    }
  }
  {
    // The map's sky box (draw_data.hpp's Scene::sky_box), or a 1 x 1 stand-in the shader skips.
    const bool has = !scene.sky_box.levels.empty();
    const uint32_t w = has ? scene.sky_box.width : 1, h = has ? scene.sky_box.height : 1;
    d.sky_box = r::make_texture(device, "bsp.sky_box", w, h, 1,
                                wgpu::TextureFormat::RGBA8UnormSrgb, sampled, 1);
    if (has) {
      upload(device, d.sky_box, scene.sky_box);
    } else {
      Atlas none;
      none.width = none.height = 1;
      none.levels.push_back({0, 0, 0, 255});
      upload(device, d.sky_box, none);
    }
  }
  d.light = r::make_texture(device, "bsp.lightmaps", scene.light.width, scene.light.height, 1,
                            wgpu::TextureFormat::RGBA8Unorm, sampled);
  upload(device, d.light, scene.light);
  {
    // The rebaked light (rebake.hpp), or 1 x 1 stand-ins that no vertex reads (lm2 is -1).
    const Rebaked& rb = scene.rebaked;
    const bool has = rb.width > 0 && rb.height > 0;
    const uint32_t w = has ? rb.width : 1, h = has ? rb.height : 1;
    d.rebake_rgb =
        r::make_texture(device, "bsp.rebake", w, h, 1, wgpu::TextureFormat::RGB9E5Ufloat, sampled);
    d.rebake_dir = r::make_texture(device, "bsp.rebake.dir", w, h, 1,
                                   wgpu::TextureFormat::RGBA8Unorm, sampled);
    const bool bounced = has && rb.bounce.size() == rb.rgb9e5.size();
    d.rebake_bounce = r::make_texture(device, "bsp.rebake.bounce", bounced ? w : 1, bounced ? h : 1,
                                      1, wgpu::TextureFormat::RGB9E5Ufloat, sampled);
    const uint32_t zero = 0;
    const uint8_t flat[4] = {128, 128, 128, 0};
    wgpu::TexelCopyBufferLayout layout{};
    layout.bytesPerRow = w * 4;
    layout.rowsPerImage = h;
    const wgpu::Extent3D size{w, h, 1};
    wgpu::TexelCopyTextureInfo dst{};
    dst.texture = d.rebake_rgb;
    device.queue.WriteTexture(&dst, has ? static_cast<const void*>(rb.rgb9e5.data()) : &zero,
                              size_t(w) * h * 4, &layout, &size);
    dst.texture = d.rebake_dir;
    device.queue.WriteTexture(&dst, has ? static_cast<const void*>(rb.dir.data()) : flat,
                              size_t(w) * h * 4, &layout, &size);
    dst.texture = d.rebake_bounce;
    if (bounced) {
      device.queue.WriteTexture(&dst, rb.bounce.data(), size_t(w) * h * 4, &layout, &size);
    } else {
      wgpu::TexelCopyBufferLayout one{};
      one.bytesPerRow = 4;
      one.rowsPerImage = 1;
      const wgpu::Extent3D px{1, 1, 1};
      device.queue.WriteTexture(&dst, &zero, 4, &one, &px);
    }
  }

  d.cube_slots = std::min(kMaxShadowed, int(device.limits.maxTextureArrayLayers / 6));
  const uint32_t cube_layers = uint32_t(d.cube_slots) * 6;
  d.cubes = depth_array(device, "bsp.cubes", kCubeSize, cube_layers);
  for (uint32_t k = 0; k < cube_layers; ++k) d.cube_layers.push_back(layer_view(d.cubes, k));
  d.cubes_view = array_view(d.cubes, cube_layers);
  {
    // One atlas, a row a light and a column a face, so a frame draws them all in one pass.
    wgpu::TextureDescriptor desc{};
    desc.label = "bsp.casters";
    desc.size = {kCasterSize * 6, kCasterSize * uint32_t(kCasterLights + 1), 1};
    desc.format = r::kDepthFormat;
    desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    d.caster_cubes = device.device.CreateTexture(&desc);
    d.caster_target = layer_view(d.caster_cubes, 0);
    d.caster_cubes_view = array_view(d.caster_cubes, 1);
  }
  d.caster_vertices = r::make_buffer(device, "bsp.casters", sizeof(float) * 3 * kMaxCasterVerts,
                                     wgpu::BufferUsage::Vertex);
  d.orthos = depth_array(device, "bsp.orthos", kOrthoSize, kOrthos, kOrthoFormat);
  for (uint32_t k = 0; k < kOrthos; ++k) d.ortho_layers[k] = layer_view(d.orthos, k);
  d.orthos_view = array_view(d.orthos, kOrthos);
  {
    wgpu::SamplerDescriptor s{};
    s.magFilter = s.minFilter = wgpu::FilterMode::Linear;
    s.addressModeU = s.addressModeV = wgpu::AddressMode::ClampToEdge;
    d.light_sampler = device.device.CreateSampler(&s);
    s.compare = wgpu::CompareFunction::LessEqual;
    d.compare = device.device.CreateSampler(&s);
  }
  d.uniform = r::make_buffer(device, "bsp.uniform", sizeof(GpuBsp), wgpu::BufferUsage::Uniform);
  // The animated textures' frames, or one unread stand-in.
  d.frames = r::make_buffer(device, "bsp.frames", std::max<size_t>(scene.frames.size(), 1) * 16,
                            wgpu::BufferUsage::Storage);
  if (!scene.frames.empty())
    device.queue.WriteBuffer(d.frames, 0, scene.frames.data(), scene.frames.size() * 16);
  // The brush models' runs, all at the map's place.
  d.mover_rows.assign(std::max<size_t>(scene.model_vertices.size(), 1), GpuMover{});
  for (size_t k = 0; k < scene.model_vertices.size(); ++k) {
    d.mover_rows[k].range[0] = scene.model_vertices[k][0];
    d.mover_rows[k].range[1] = scene.model_vertices[k][1];
  }
  d.movers = r::make_buffer(device, "bsp.movers", d.mover_rows.size() * sizeof(GpuMover),
                            wgpu::BufferUsage::Storage);
  device.queue.WriteBuffer(d.movers, 0, d.mover_rows.data(),
                           d.mover_rows.size() * sizeof(GpuMover));
  d.lights =
      r::make_buffer(device, "bsp.lights", sizeof(GpuLight) * 512, wgpu::BufferUsage::Storage);
  d.grid = r::make_buffer(device, "bsp.grid", sizeof(uint32_t) * (kGridCells + 1 + kMaxGridRefs),
                          wgpu::BufferUsage::Storage);
  d.grid_rows.assign(kGridCells + 1 + kMaxGridRefs, 0);
  d.jobs = r::make_buffer(device, "bsp.jobs",
                          sizeof(GpuJob) * (kMaxShadowed * 6 + kOrthos + kCasterLights * 6 + 1),
                          wgpu::BufferUsage::Storage);

  using r::Slot;
  const auto both = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
  d.layout = r::make_layout(
      device, "bsp",
      {Slot::uniform, Slot::texture, Slot::texture, Slot::sampler, Slot::storage_read,
       Slot::depth_array, Slot::compare, Slot::depth_array, Slot::texture, Slot::texture,
       Slot::storage_read, Slot::storage_read, Slot::texture, Slot::texture, Slot::texture,
       Slot::storage_read, Slot::depth_array, Slot::texture},
      both);
  d.group = r::make_group(
      device, "bsp", d.layout,
      {d.uniform, d.atlas.CreateView(), d.light.CreateView(), d.light_sampler, d.lights,
       d.cubes_view, d.compare, d.orthos_view, d.rebake_rgb.CreateView(), d.rebake_dir.CreateView(),
       d.frames, d.movers, d.surface.CreateView(), d.rebake_bounce.CreateView(),
       d.cover.CreateView(), d.grid, d.caster_cubes_view, d.sky_box.CreateView()});
  d.fix_layout = r::make_layout(device, "bsp.sky", {Slot::texture, Slot::uniform},
                                wgpu::ShaderStage::Fragment);
  d.shadow_layout = r::make_layout(device, "bsp.shadows", {Slot::storage_read}, both);
  d.shadow_group = r::make_group(device, "bsp.shadows", d.shadow_layout, {d.jobs});

  wgpu::VertexAttribute attrs[9] = {};
  const uint64_t offsets[9] = {0, 12, 20, 28, 40, 56, 64, 72, 76};
  const wgpu::VertexFormat formats[9] = {
      wgpu::VertexFormat::Float32x3, wgpu::VertexFormat::Float32x2, wgpu::VertexFormat::Float32x2,
      wgpu::VertexFormat::Float32x3, wgpu::VertexFormat::Float32x4, wgpu::VertexFormat::Uint32,
      wgpu::VertexFormat::Float32x2, wgpu::VertexFormat::Uint32,    wgpu::VertexFormat::Uint32};
  for (uint32_t i = 0; i < 9; ++i) {
    attrs[i].shaderLocation = i;
    attrs[i].offset = offsets[i];
    attrs[i].format = formats[i];
  }
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = sizeof(DrawVertex);
  vbl.attributeCount = 9;
  vbl.attributes = attrs;
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::bsp, "bsp");
    wgpu::ColorTargetState colours[3] = {};
    colours[0].format = r::kHdrFormat;
    colours[1].format = r::kMotionFormat;
    colours[2].format = wgpu::TextureFormat::R8Unorm;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 3;
    fragment.targets = colours;
    wgpu::DepthStencilState depth{};
    depth.format = r::kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Greater;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "bsp.draw";
    desc.layout = r::make_pipeline_layout(device, "bsp.draw", {frame, d.layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.vertex.bufferCount = 1;
    desc.vertex.buffers = &vbl;
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.cullMode = wgpu::CullMode::None;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    d.p_draw = pipelines.add_render(device, desc);
    // The same with only the real-time lights' path (bsp.wgsl's kLitOnly), for the frames that
    // take no other.
    wgpu::ConstantEntry lit_only{};
    lit_only.key = "kLitOnly";
    lit_only.value = 1;
    fragment.constantCount = 1;
    fragment.constants = &lit_only;
    desc.label = "bsp.draw.lit";
    d.p_draw_lit = pipelines.add_render(device, desc);
  }
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::bsp_fix, "bsp.sky");
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 0;
    wgpu::DepthStencilState depth{};
    depth.format = r::kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Always;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "bsp.sky";
    desc.layout = r::make_pipeline_layout(device, "bsp.sky", {d.fix_layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    d.p_fix = pipelines.add_render(device, desc);
  }
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::bsp_fix, "bsp.veil");
    // The sky's pixels times the veil the fragment writes: dst * src (unclamped, so over 1 lifts).
    wgpu::BlendState blend{};
    blend.color.srcFactor = wgpu::BlendFactor::Zero;
    blend.color.dstFactor = wgpu::BlendFactor::Src;
    blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;
    wgpu::ColorTargetState target{};
    target.format = r::kHdrFormat;
    target.blend = &blend;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_veil";
    fragment.targetCount = 1;
    fragment.targets = &target;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "bsp.veil";
    desc.layout = r::make_pipeline_layout(device, "bsp.veil", {d.fix_layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.fragment = &fragment;
    d.p_veil = pipelines.add_render(device, desc);
  }
  {
    const wgpu::ShaderModule module = gpu::create_shader(device, wgsl::bsp_shadow, "bsp.shadows");
    wgpu::VertexBufferLayout pos{};
    pos.arrayStride = sizeof(DrawVertex);
    pos.attributeCount = 1;
    pos.attributes = attrs;
    wgpu::FragmentState fragment{};
    fragment.module = module;
    fragment.entryPoint = "fs_main";
    fragment.targetCount = 0;
    wgpu::DepthStencilState depth{};
    depth.format = r::kDepthFormat;
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Less;
    wgpu::RenderPipelineDescriptor desc{};
    desc.label = "bsp.shadows";
    desc.layout = r::make_pipeline_layout(device, "bsp.shadows", {d.shadow_layout});
    desc.vertex.module = module;
    desc.vertex.entryPoint = "vs_main";
    desc.vertex.bufferCount = 1;
    desc.vertex.buffers = &pos;
    desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    desc.primitive.cullMode = wgpu::CullMode::None;
    desc.depthStencil = &depth;
    desc.fragment = &fragment;
    d.p_shadow = pipelines.add_render(device, desc);
    // The moving casters: the same shader over their bare positions.
    wgpu::VertexAttribute at{};
    at.format = wgpu::VertexFormat::Float32x3;
    at.offset = 0;
    at.shaderLocation = 0;
    wgpu::VertexBufferLayout bare{};
    bare.arrayStride = sizeof(float) * 3;
    bare.attributeCount = 1;
    bare.attributes = &at;
    desc.label = "bsp.casters";
    desc.vertex.buffers = &bare;
    d.p_caster = pipelines.add_render(device, desc);
    // THE ORTHOGRAPHIC MAPS' THREE STEPS (encode_bsp_shadows): the solid faces as a cube face's,
    // into the depth and stencil format; the sky faces nearer the sky than them marking the
    // stencil (no depth written); and every texel left unmarked filled at the near plane, so
    // whatever no sky face stands over along the camera's way is covered at any depth.
    depth.format = kOrthoFormat;
    desc.label = "bsp.ortho";
    desc.vertex.buffers = &pos;
    d.p_ortho = pipelines.add_render(device, desc);
    depth.depthWriteEnabled = false;
    depth.stencilFront.passOp = wgpu::StencilOperation::Replace;
    depth.stencilBack = depth.stencilFront;
    desc.label = "bsp.ortho_sky";
    d.p_ortho_sky = pipelines.add_render(device, desc);
    depth.depthWriteEnabled = true;
    depth.depthCompare = wgpu::CompareFunction::Always;
    depth.stencilFront.compare = wgpu::CompareFunction::Equal;  // unmarked: the reference 0
    depth.stencilFront.passOp = wgpu::StencilOperation::Keep;
    depth.stencilBack = depth.stencilFront;
    desc.label = "bsp.ortho_shut";
    desc.vertex.entryPoint = "vs_shut";
    desc.vertex.bufferCount = 0;
    desc.vertex.buffers = nullptr;
    desc.fragment = nullptr;  // the near plane's depth, straight from the triangle
    d.p_ortho_shut = pipelines.add_render(device, desc);
  }
  create_bsp_cull(device, pipelines, d.cull, scene, chunk_of, d.indices, frame);
  d.lava = lava_lights(scene, 1150, 0.3);
  d.slime = slime_lights(scene);
  d.portals = portal_lights(scene);
}

void create_bsp_targets(gpu::Device& device, BspDraw& d, const r::FrameTargets& targets) {
  d.mask = r::make_texture(
      device, "bsp.mask", targets.width, targets.height, 1, wgpu::TextureFormat::R8Unorm,
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding);
  d.mask_view = d.mask.CreateView();
  d.fix_group = r::make_group(device, "bsp.sky", d.fix_layout, {d.mask_view, d.uniform});
  create_bsp_cull_targets(device, d.cull, targets);
  d.culled_frames = 0;  // the new pyramid holds no frame yet
}

// Whether two shadow cameras are the same to the bit (the floats as the GPU gets them).
static bool same_job(const GpuJob& a, const GpuJob& b) {
  const auto eq = [](const float(&x)[4], const float(&y)[4]) {
    return std::equal(std::begin(x), std::end(x), std::begin(y));
  };
  return eq(a.o, b.o) && eq(a.x, b.x) && eq(a.y, b.y) && eq(a.z, b.z) && eq(a.info, b.info);
}

void prepare_bsp(gpu::Device& device, BspDraw& d, const r::FrameView& view, const Options& o) {
  const Scene& s = *d.scene;
  d.options = o;
  d.options.shadowed = std::clamp(o.shadowed, 0, d.cube_slots);
  const bool lights = o.mode == Mode::v2 && o.lighting == Lighting::lights;
  // The lights near enough to the eye to reach what it sees, nearest first.
  struct Pick {
    const PointLight* l;
    double d;
  };
  std::vector<Pick> picks;
  if (lights) {
    GE_ZONE("bsp.pick");
    const auto consider = [&](const PointLight& l) {
      const double reach = l.physical ? l.reach : l.reach * 1.5;
      const double dx = l.pos[0] - view.eye[0], dy = l.pos[1] - view.eye[1],
                   dz = l.pos[2] - view.eye[2];
      const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
      if (dist < reach + 40) picks.push_back({&l, dist - reach});
    };
    for (const PointLight& l : s.lights) consider(l);
    // The pools' lights at the options' strengths, copied before any is picked.
    const auto scaled = [](const std::vector<PointLight>& from, double k,
                           std::vector<PointLight>& to) {
      to.clear();
      if (k <= 0) return;
      for (PointLight l : from) {
        for (double& c : l.colour) c *= k;
        to.push_back(l);
      }
    };
    scaled(d.lava, o.lava_lights ? o.lava_glow : 0, d.lava_points);
    scaled(d.slime, o.slime_glow, d.slime_points);
    for (const PointLight& l : d.lava_points) consider(l);
    for (const PointLight& l : d.slime_points) consider(l);
    if (o.portal_lights) {
      for (const PointLight& l : d.portals) consider(l);
    }
    // The dynamic lights as point lights of the classic's brightness: the radius its reach.
    d.dynamic_points.clear();
    for (const DynamicLight& dl : d.dynamic) {
      PointLight l;
      for (int k = 0; k < 3; ++k) l.pos[k] = dl.pos[k];
      // A light entity of this value reaches the radius; its colour scaled so that half way out
      // it gives half its middle's light, as the classic's straight fall-off does.
      l.value = std::max(dl.radius, 0.01) / kUnitM;
      l.reach = dl.radius / 1.5;
      for (int k = 0; k < 3; ++k)
        l.colour[k] = 0.5 * dl.colour[k] * 256 / l.value * o.dynamic_strength * dl.boost;
      d.dynamic_points.push_back(l);
    }
    for (const PointLight& l : d.dynamic_points) consider(l);
    std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) { return a.d < b.d; });
    if (picks.size() > size_t(std::min(o.most, 512))) picks.resize(size_t(std::min(o.most, 512)));
  }
  d.frame_lights.clear();
  // The fires' lights swell and dim with their flames' puffs (bsp_flames.hpp's fire_flicker).
  FireLook look;
  look.bonfires = o.bonfires;
  look.pace = o.fire_pace;
  d.frame_jobs.assign(size_t(kMaxShadowed) * 6 + kOrthos + size_t(kCasterLights) * 6 + 1, GpuJob{});
  // The lights that cast shadows, by their index in frame_lights: the nearest of those the eye's
  // cluster can see whose reach comes into the view first, then the nearest of the others it can
  // see (behind the eye), then the nearest of the rest. Nearness alone (the distance less the
  // reach) let a map's big lights behind walls take the shadowed few from the room in view: on a
  // map of a few score lights, the room ahead lit flat, its shadows popping in a few steps on. And
  // a light left without a slot shines through the walls and frames in front of it: start's
  // corridor sees 57 lights ahead, so the two lamps before the NORMAL hall's slipgate went
  // unshadowed from there and its frame's shadow on the wall behind (the map's own lightmap has
  // it) appeared only once the player walked in.
  std::vector<size_t> casters, aside, unseen;
  const Map& map = s.map;
  // Whether a light's reach comes into the view: its sphere against the cone round the look that
  // holds the whole picture at the widest screen a window has (kWidestAspect).
  double ahead[3];
  r::look_dir(view.heading, view.pitch, ahead);
  const double half_y = std::tan(view.fov_y / 2);
  const double cone = std::atan(half_y * std::sqrt(1 + kWidestAspect * kWidestAspect));
  const auto in_view = [&](const GpuLight& g) {
    const double to[3] = {g.pos[0] - view.eye[0], g.pos[1] - view.eye[1], g.pos[2] - view.eye[2]};
    const double dist = std::sqrt(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
    const double reach = g.pos[3];
    if (dist <= reach) return true;
    const double along = (to[0] * ahead[0] + to[1] * ahead[1] + to[2] * ahead[2]) / dist;
    const double off = std::acos(std::clamp(along, -1.0, 1.0));
    return off - std::asin(reach / dist) <= cone;
  };
  const auto cluster_of = [&](const double p[3]) {
    const float f[3] = {float(p[0] / kUnitM), float(p[2] / kUnitM), float(p[1] / kUnitM)};
    return map.leaves.empty() ? -1 : map.leaves[leaf_at(map, f)].cluster;
  };
  const int32_t eye_cluster = cluster_of(view.eye);
  for (const Pick& p : picks) {
    const PointLight& l = *p.l;
    GpuLight g{};
    double near = 0;  // m: what its light is softened over near it (`near_soft`)
    if (l.physical) {
      set4(g.pos, l.pos[0], l.pos[1], l.pos[2], l.reach);
      set4(g.colour, l.colour[0] * l.value, l.colour[1] * l.value, l.colour[2] * l.value, 1);
    } else {
      // The classic's light at half its reach, value / 2 of 128, matched by an inverse square:
      // the strength a metre off is that times the half reach squared.
      const double half = l.value * kUnitM / 2;
      const double at1 = l.value / 256.0 * half * half * l.gain * o.strength *
                         fire_flicker(l, view.time_s, look) * fire_light_gain(l, o.bonfires);
      set4(g.pos, l.pos[0], l.pos[1] + fire_light_lift(l, o.bonfires), l.pos[2], l.reach * 1.5);
      set4(g.colour, l.colour[0] * at1, l.colour[1] * at1, l.colour[2] * at1, 0);
      near = o.near_soft * half;
    }
    // A fire lights the wall behind it from its whole flame: its light softened over its height.
    const double soft = std::max(fire_light_size(l, o.bonfires), l.soft_m);
    const double spread = std::max(soft, near);
    // An aimed light: full inside info.w's cosine, none outside spot.w's.
    set4(g.info, -1, spread * spread, 0, l.spot ? l.cone_cos[0] : -1);
    // A dynamic light moves or goes out from one frame to the next, so a cube of it would be drawn
    // again every frame: it lights without a shadow.
    const bool dynamic = !d.dynamic_points.empty() && p.l >= d.dynamic_points.data() &&
                         p.l < d.dynamic_points.data() + d.dynamic_points.size();
    // Whether it throws a figure's shadow (Options::fill_shadows): a source he can see.
    const bool figure = o.fill_shadows || dynamic || (l.seen && !l.physical);
    // Its shadows as soft as its source is big: a flame's over its size, a bulb's sharp; z 1 when
    // it throws a figure's.
    set4(g.more, -1, std::max(soft * kFlameWidthShare, kPointSourceM), figure ? 1 : 0, 0);
    if (l.spot)
      set4(g.spot, l.dir[0], l.dir[1], l.dir[2], l.cone_cos[1]);
    else
      set4(g.spot, 0, -1, 0, -2);
    if (!l.physical && !dynamic) {
      // A light in solid or outside the map (no cluster) counts as seen: nothing says it is not.
      const int32_t at = cluster_of(l.pos);
      const bool seen = eye_cluster < 0 || at < 0 || cluster_sees(map, eye_cluster, at);
      (!seen ? unseen : in_view(g) ? casters : aside).push_back(d.frame_lights.size());
    }
    d.frame_lights.push_back(g);
  }
  casters.insert(casters.end(), aside.begin(), aside.end());
  casters.insert(casters.end(), unseen.begin(), unseen.end());
  if (int(casters.size()) > d.options.shadowed) casters.resize(size_t(d.options.shadowed));
  // Each caster keeps the slot that already holds its cube; the rest, nearest first, take the
  // slot left unused longest (a light that steps out of the nearest few and back in finds its cube
  // still there), at most `fresh_cubes` a frame: one over the budget lights without a shadow until
  // a later frame draws its cube, so a doorway into a room of torches costs no frame more than
  // the budget's cubes. The map's first frame draws them all.
  const auto key_of = [](const GpuLight& g) {
    return std::array<float, 4>{g.pos[0], g.pos[1], g.pos[2], g.pos[3]};
  };
  ++d.shadow_frame;
  std::array<bool, kMaxShadowed> taken{};
  std::vector<size_t> homeless;
  for (const size_t c : casters) {
    const std::array<float, 4> key = key_of(d.frame_lights[c]);
    int slot = -1;
    for (int k = 0; k < d.cube_slots && slot < 0; ++k)
      if (d.slot_held[size_t(k)] && !taken[size_t(k)] && d.slot_key[size_t(k)] == key) slot = k;
    if (slot < 0) {
      homeless.push_back(c);
      continue;
    }
    taken[size_t(slot)] = true;
    d.slot_used[size_t(slot)] = d.shadow_frame;
    d.frame_lights[c].info[0] = float(slot * 6);
  }
  d.fresh_slots.clear();
  d.shadows_waiting = 0;
  // The first frame after the map is made draws every cube: it is the load's, not play's (unless
  // the caller warms the map up over frames of its own: `first_all` off).
  const size_t budget = d.shadow_frame == 1 && o.first_all ? size_t(d.cube_slots)
                                                           : size_t(std::max(o.fresh_cubes, 0));
  for (const size_t c : homeless) {
    if (d.fresh_slots.size() >= budget) {
      ++d.shadows_waiting;  // info[0] stays -1: lit, unshadowed, this frame
      continue;
    }
    int slot = -1;
    for (int k = 0; k < d.cube_slots; ++k) {
      if (taken[size_t(k)]) continue;
      if (slot < 0 || d.slot_used[size_t(k)] < d.slot_used[size_t(slot)]) slot = k;
    }
    taken[size_t(slot)] = true;  // casters <= cube_slots: one is always free
    d.slot_key[size_t(slot)] = key_of(d.frame_lights[c]);
    d.slot_held[size_t(slot)] = true;
    d.slot_used[size_t(slot)] = d.shadow_frame;
    d.fresh_slots.push_back(slot);
    d.frame_lights[c].info[0] = float(slot * 6);
  }
  // Every held slot's six faces, so the jobs buffer always says what its cubes hold.
  for (int k = 0; k < d.cube_slots; ++k) {
    if (!d.slot_held[size_t(k)]) continue;
    const std::array<float, 4>& key = d.slot_key[size_t(k)];
    for (int f = 0; f < 6; ++f) {
      const int i = f / 2;
      const double sign = f % 2 ? -1.0 : 1.0;
      GpuJob j{};
      set4(j.o, key[0], key[1], key[2], key[3]);
      double ax[3] = {0, 0, 0}, ay[3] = {0, 0, 0}, az[3] = {0, 0, 0};
      ax[(i + 1) % 3] = 1;
      ay[(i + 2) % 3] = 1;
      az[i] = sign;
      set4(j.x, ax[0], ax[1], ax[2], 0);
      set4(j.y, ay[0], ay[1], ay[2], 0);
      set4(j.z, az[0], az[1], az[2], 0);
      set4(j.info, 0, 0, 0, 0);
      d.frame_jobs[size_t(k) * 6 + size_t(f)] = j;
    }
  }
  const int shadowed = int(casters.size());
  d.shadowed = shadowed;
  // The moving casters as capsules: the nearest the eye, each its box's vertical capsule.
  {
    const std::vector<Capsule> caps =
        lights && o.capsules ? shadow_capsules(d.caster_boxes, view.eye) : std::vector<Capsule>{};
    // w: the share of the bounce a figure's shadow dims (Options::fill_shadows; bsp.wgsl).
    set4(d.staged.capsule_info, double(caps.size()), o.soft_casters ? 0 : 1,
         std::clamp(o.view_shadows, 0, 2), o.fill_shadows ? kIndirectShadow : 0);
    for (size_t c = 0; c < caps.size(); ++c) {
      set4(d.staged.capsules[c * 2], caps[c].a[0], caps[c].a[1], caps[c].a[2], caps[c].r);
      set4(d.staged.capsules[c * 2 + 1], caps[c].b[0], caps[c].b[1], caps[c].b[2], 0);
    }
  }
  // The moving casters: the shadowed lights that light one of them most, each a cube of them
  // this frame (the options' count of them at most).
  d.caster_lights = 0;
  d.caster_count = 0;
  const int cast_by = std::clamp(o.caster_lights, 0, kCasterLights);
  const bool casting = lights && !d.casters.empty() && cast_by > 0;
  if (casting) {
    d.caster_count = uint32_t(std::min<size_t>(d.casters.size() / 3, kMaxCasterVerts) / 3 * 3);
    // Only the lights that throw a figure's shadow get a cube of the casters.
    std::vector<size_t> figure;
    for (const size_t c : casters)
      if (d.frame_lights[c].more[2] > 0.5f) figure.push_back(c);
    for (const size_t c : caster_lights(d.frame_lights, figure, d.caster_boxes, view.eye)) {
      if (d.caster_lights >= cast_by) break;
      GpuLight& g = d.frame_lights[c];
      const int n = d.caster_lights++;
      g.more[0] = float(n);
      for (int f = 0; f < 6; ++f) {
        GpuJob j = d.frame_jobs[size_t(g.info[0]) + size_t(f)];  // the world cube's face
        set4(j.o, g.pos[0], g.pos[1], g.pos[2], g.pos[3]);
        d.frame_jobs[size_t(kMaxShadowed) * 6 + kOrthos + size_t(n) * 6 + size_t(f)] = j;
      }
    }
  }
  // The classic's and v2's lightmaps: the dynamic lights added to the lightmaps' light.
  if (!lights) {
    for (size_t i = 0; i < d.dynamic.size() && i < size_t(kMaxDynamic); ++i) {
      const DynamicLight& dl = d.dynamic[i];
      GpuLight g{};
      set4(g.pos, dl.pos[0], dl.pos[1], dl.pos[2], std::max(dl.radius, 0.01));
      const double k = o.dynamic_strength * dl.boost;
      set4(g.colour, dl.colour[0] * k, dl.colour[1] * k, dl.colour[2] * k, 0);
      set4(g.info, -1, 0, 1, 0);
      set4(g.spot, 0, -1, 0, -2);
      set4(g.more, -1, kPointSourceM, 0, 0);
      d.frame_lights.push_back(g);
    }
  }
  // The sun's map and the zenith's over the whole map.
  const double centre[3] = {(s.mins[0] + s.maxs[0]) / 2, (s.mins[1] + s.maxs[1]) / 2,
                            (s.mins[2] + s.maxs[2]) / 2};
  const double ext[3] = {s.maxs[0] - s.mins[0], s.maxs[1] - s.mins[1], s.maxs[2] - s.mins[2]};
  const double radius = 0.5 * std::sqrt(ext[0] * ext[0] + ext[1] * ext[1] + ext[2] * ext[2]) + 1;
  double sun[3];
  r::sun_direction(view, sun);
  const double toward[3] = {-sun[0], -sun[1], -sun[2]};
  const double down[3] = {0, -1, 0};
  d.staged.ortho[0] = ortho_job(toward, centre, radius, radius);
  // The moving casters along the sun, round the eye, the depth as deep as the map's.
  d.sun_caster = casting && sun[1] > 0;
  d.staged.sun_caster = ortho_job(toward, view.eye, kSunCasterHalfM, 2 * radius);
  d.staged.sun_caster.info[2] = d.sun_caster ? 1.0f : 0.0f;
  d.frame_jobs.back() = d.staged.sun_caster;
  d.staged.ortho[1] = ortho_job(down, centre, radius, radius);
  // The fall's: along the way the rain and the snow come, kept off the level (a gale's snow
  // driven near flat still comes from the sky).
  double fall[3] = {o.fall[0], std::min(o.fall[1], -0.26), o.fall[2]};
  const double fl = std::sqrt(fall[0] * fall[0] + fall[1] * fall[1] + fall[2] * fall[2]);
  for (double& c : fall) c /= fl;
  d.staged.ortho[2] = ortho_job(fall, centre, radius, radius);
  for (uint32_t k = 0; k < kOrthos; ++k)
    d.frame_jobs[size_t(kMaxShadowed) * 6 + k] = d.staged.ortho[k];
  set4(d.staged.weather, o.weather && o.mode == Mode::v2 ? 1 : 0, std::clamp(o.raining, 0.0, 1.0),
       std::clamp(o.spread, 0.0, 1.0), std::max(o.glow, 0.0));
  // The orthographic maps this frame reads (the sun's and the zenith's lit in real time, the
  // zenith's and the fall's under weather), drawn only when their camera moved.
  {
    const bool v2_lights = o.mode == Mode::v2 && o.lighting == Lighting::lights;
    const bool weather_on = o.mode == Mode::v2 && o.weather;
    const bool fog_sun = o.mode == Mode::v2 && o.fog_sun;
    const bool need[kOrthos] = {v2_lights || fog_sun, v2_lights || weather_on, weather_on};
    for (uint32_t k = 0; k < kOrthos; ++k) {
      const bool same = d.ortho_held[k] && same_job(d.ortho_drawn[k], d.staged.ortho[k]);
      d.ortho_fresh[k] = need[k] && !same;
      if (d.ortho_fresh[k]) {
        d.ortho_drawn[k] = d.staged.ortho[k];
        d.ortho_held[k] = true;
      }
    }
  }

  set4(d.staged.info, o.mode == Mode::v2 ? 1 : 0,
       o.lighting == Lighting::lights    ? 1
       : o.lighting == Lighting::rebaked ? 2
                                         : 0,
       double(d.frame_lights.size()), o.ambient);
  set4(d.staged.info2, view.time_s, kCubeSize, kOrthoSize, o.brightness);
  // The bounce's share; negative: the rebake's bounce in its place (this share where it has none).
  const bool rebounce = o.rebake_bounce && o.mode == Mode::v2 && !s.rebaked.bounce.empty();
  const double share = o.lighting == Lighting::lights ? o.bounce : 0;
  set4(d.staged.info3, o.minlight, rebounce && share > 0 ? -share : share, o.sky_light,
       o.crisp ? 0 : 1);
  // The relief (v2, a scene with a surface atlas): on, the parallax's depth and reach.
  const bool relief = o.relief && o.mode == Mode::v2 && !s.surface.levels.empty();
  const double parallax = relief ? std::max(o.parallax, 0.0) : 0;
  // The map's sky box: on, and half a texel of the atlas (each face's edge is inset by it).
  const bool box = !s.sky_box.levels.empty() && s.sky_box_face > 0;
  const double inset = box ? 0.5 / double(s.sky_box_face) : 0;
  set4(d.staged.sky_box, box ? 1 : 0, inset, inset, 0);
  set4(d.staged.info4, o.bump, o.pack_sky ? 1 : 0, parallax > 0 ? o.parallax_reach : 0,
       relief ? o.specular : 0);
  set4(d.staged.info5, 1.0 / double(s.light_layers), s.light_layers,
       relief ? std::max(o.relief_strength, 1e-3) : 0, parallax);
  if (lights) {
    GE_ZONE("bsp.grid");
    d.grid_refs = bin_light_grid(d.frame_lights, view.eye, d.grid_rows, d.staged.grid);
  } else {
    d.grid_refs = 0;
    set4(d.staged.grid, 0, 0, 0, 0);
  }
  if (o.cull) {
    GE_ZONE("bsp.cull");
    prepare_bsp_cull(device, d.cull, s, view.eye, d.culled_frames > 0, o.pvs);
    ++d.culled_frames;
  } else {
    d.culled_frames = 0;
  }
  for (int k = 0; k < kLightStyles; ++k)
    d.staged.styles[k] = float(style_level(d.light_styles[size_t(k)], view.time_s));
  const double veil = o.sky_light * o.sky_seen;
  set4(d.staged.veil, veil * o.sky_tint[0], veil * o.sky_tint[1], veil * o.sky_tint[2], 1);
  if (s.sky_texture < s.map.textures.size()) {
    // The sky texture's rectangle: any vertex of a sky face carries it.
    for (size_t k = 0; k < s.vertices.size(); ++k) {
      if (s.vertices[k].kind == kSky) {
        std::memcpy(d.staged.sky, s.vertices[k].rect, sizeof d.staged.sky);
        break;
      }
    }
  }
  GE_ZONE("bsp.upload");
  device.queue.WriteBuffer(d.uniform, 0, &d.staged, sizeof d.staged);
  if (!d.frame_lights.empty()) {
    device.queue.WriteBuffer(d.lights, 0, d.frame_lights.data(),
                             d.frame_lights.size() * sizeof(GpuLight));
  }
  if (d.staged.grid[3] > 0) {
    device.queue.WriteBuffer(d.grid, 0, d.grid_rows.data(),
                             (size_t(kGridCells) + 1 + d.grid_refs) * sizeof(uint32_t));
  }
  device.queue.WriteBuffer(d.jobs, 0, d.frame_jobs.data(), d.frame_jobs.size() * sizeof(GpuJob));
  if (d.caster_lights > 0 || d.sun_caster)  // the sun's tile draws them too
    device.queue.WriteBuffer(d.caster_vertices, 0, d.casters.data(),
                             size_t(d.caster_count) * 3 * sizeof(float));
  if (d.movers_dirty) {
    device.queue.WriteBuffer(d.movers, 0, d.mover_rows.data(),
                             d.mover_rows.size() * sizeof(GpuMover));
    d.movers_dirty = false;
  }
}

void set_movers(BspDraw& d, std::span<const Mover> movers) {
  for (const Mover& m : movers) {
    if (m.model == 0 || m.model >= d.mover_rows.size()) continue;
    GpuMover& g = d.mover_rows[m.model];
    const float o[3] = {float(m.offset[0]), float(m.offset[1]), float(m.offset[2])};
    const uint32_t hidden = m.hidden ? 1 : 0;
    if (g.range[2] == hidden && g.offset[0] == o[0] && g.offset[1] == o[1] && g.offset[2] == o[2])
      continue;
    g.range[2] = hidden;
    for (int k = 0; k < 3; ++k) g.offset[k] = o[k];
    d.movers_dirty = true;
    if (m.model < d.cull.moves.size()) {
      d.cull.moves[m.model] = {o[0], o[1], o[2], float(hidden)};
      d.cull.moved = true;
    }
  }
}

void set_dynamic_lights(BspDraw& d, std::span<const DynamicLight> lights) {
  d.dynamic.assign(lights.begin(), lights.end());
}

void set_shadow_casters(BspDraw& d, std::span<const float> triangles,
                        std::span<const CasterBox> boxes) {
  d.casters.assign(triangles.begin(), triangles.end());
  d.caster_boxes.assign(boxes.begin(), boxes.end());
}

void set_light_style(BspDraw& d, int style, std::string_view pattern) {
  if (style >= 0 && style < kLightStyles) d.light_styles[size_t(style)] = pattern;
}

void encode_bsp_shadows(const BspDraw& d, const gpu::PipelineSet& pipelines, r::FrameRecorder& rec,
                        wgpu::CommandEncoder& encoder) {
  const bool v2 = d.options.mode == Mode::v2;
  const bool lights = v2 && d.options.lighting == Lighting::lights;
  // The ranges the shadows see: the solid faces (the sky's do not stop the sun).
  const auto draw_solid = [&](wgpu::RenderPassEncoder& pass, uint32_t instance) {
    pass.DrawIndexed(d.range[kSolid][1], 1, d.range[kSolid][0], 0, instance);
  };
  // Only the maps prepare_bsp found stale: the lights new to a slot, the orthographic cameras
  // that moved (the zenith's also for the lightning, and the fall's, with the weather on); timed
  // together as "bsp.shadows".
  const int cube_passes = lights ? int(d.fresh_slots.size()) * 6 : 0;
  const int caster_passes = lights && (d.caster_lights > 0 || d.sun_caster) ? 1 : 0;
  int parts = cube_passes + caster_passes;
  for (uint32_t k = 0; k < kOrthos; ++k) parts += d.ortho_fresh[k] ? 1 : 0;
  if (parts == 0) rec.compute(encoder, "bsp.shadows").End();
  {
    int part = 0;
    const auto shadow_pass = [&](const wgpu::TextureView& view, uint32_t job, bool sky) {
      wgpu::RenderPassDepthStencilAttachment depth{};
      depth.view = view;
      depth.depthLoadOp = wgpu::LoadOp::Clear;
      depth.depthStoreOp = wgpu::StoreOp::Store;
      depth.depthClearValue = 1.0f;
      wgpu::RenderPassDescriptor desc{};
      desc.depthStencilAttachment = &depth;
      wgpu::RenderPassEncoder pass = rec.render_part(encoder, desc, "bsp.shadows", part++, parts);
      rec.current().draws += sky ? 2 : 1;
      pass.SetPipeline(pipelines.render(d.p_shadow));
      pass.SetBindGroup(0, d.shadow_group);
      pass.SetVertexBuffer(0, d.vertices);
      pass.SetIndexBuffer(d.indices, wgpu::IndexFormat::Uint32);
      draw_solid(pass, job);
      if (sky && d.range[kSky][1] > 0)
        pass.DrawIndexed(d.range[kSky][1], 1, d.range[kSky][0], 0, job);
      pass.End();
    };
    const uint32_t first_ortho = uint32_t(kMaxShadowed * 6);
    if (lights)
      for (const int slot : d.fresh_slots)
        for (int f = 0; f < 6; ++f)
          shadow_pass(d.cube_layers[size_t(slot) * 6 + size_t(f)], uint32_t(slot * 6 + f), true);
    // An orthographic map (the sun's, the zenith's, the fall's) sees its level's sky only through
    // the sky faces: the solid faces' depth, the sky faces that stand nearer marking the stencil,
    // and every texel left unmarked shut at the near plane (kOrthoFormat). A closed level has no
    // sky anywhere, so no light from it and no weather; its seams and cracks let none in.
    const auto ortho_pass = [&](const wgpu::TextureView& view, uint32_t job) {
      wgpu::RenderPassDepthStencilAttachment depth{};
      depth.view = view;
      depth.depthLoadOp = wgpu::LoadOp::Clear;
      depth.depthStoreOp = wgpu::StoreOp::Store;
      depth.depthClearValue = 1.0f;
      depth.stencilLoadOp = wgpu::LoadOp::Clear;
      depth.stencilStoreOp = wgpu::StoreOp::Discard;
      depth.stencilClearValue = 0;
      wgpu::RenderPassDescriptor desc{};
      desc.depthStencilAttachment = &depth;
      wgpu::RenderPassEncoder pass = rec.render_part(encoder, desc, "bsp.shadows", part++, parts);
      rec.current().draws += 3;
      pass.SetPipeline(pipelines.render(d.p_ortho));
      pass.SetBindGroup(0, d.shadow_group);
      pass.SetVertexBuffer(0, d.vertices);
      pass.SetIndexBuffer(d.indices, wgpu::IndexFormat::Uint32);
      draw_solid(pass, job);
      if (d.range[kSky][1] > 0) {
        pass.SetPipeline(pipelines.render(d.p_ortho_sky));
        pass.SetStencilReference(1);
        pass.DrawIndexed(d.range[kSky][1], 1, d.range[kSky][0], 0, job);
      }
      pass.SetPipeline(pipelines.render(d.p_ortho_shut));
      pass.SetStencilReference(0);
      pass.Draw(3);
      pass.End();
    };
    for (uint32_t k = 0; k < kOrthos; ++k)
      if (d.ortho_fresh[k]) ortho_pass(d.ortho_layers[k], first_ortho + k);
    // The moving casters, every frame: each lit cube's six faces of them alone, every face its
    // tile of the atlas in one pass (a pass a face cost 0.1 ms each on a tiled GPU, whatever it
    // drew).
    if (caster_passes > 0) {
      const uint32_t first_caster = first_ortho + kOrthos;
      wgpu::RenderPassDepthStencilAttachment depth{};
      depth.view = d.caster_target;
      depth.depthLoadOp = wgpu::LoadOp::Clear;
      depth.depthStoreOp = wgpu::StoreOp::Store;
      depth.depthClearValue = 1.0f;
      wgpu::RenderPassDescriptor desc{};
      desc.depthStencilAttachment = &depth;
      wgpu::RenderPassEncoder pass = rec.render_part(encoder, desc, "bsp.shadows", part++, parts);
      pass.SetPipeline(pipelines.render(d.p_caster));
      pass.SetBindGroup(0, d.shadow_group);
      pass.SetVertexBuffer(0, d.caster_vertices);
      const float size = float(kCasterSize);
      for (int n = 0; n < d.caster_lights; ++n)
        for (int f = 0; f < 6; ++f) {
          pass.SetViewport(float(f) * size, float(n) * size, size, size, 0, 1);
          ++rec.current().draws;
          pass.Draw(d.caster_count, 1, 0, first_caster + uint32_t(n * 6 + f));
        }
      if (d.sun_caster) {
        pass.SetViewport(0, float(kCasterLights) * size, size, size, 0, 1);
        ++rec.current().draws;
        pass.Draw(d.caster_count, 1, 0, first_caster + uint32_t(kCasterLights * 6));
      }
      pass.End();
    }
  }
}

void encode_bsp(const BspDraw& d, const gpu::PipelineSet& pipelines, r::FrameRecorder& rec,
                wgpu::CommandEncoder& encoder, const r::FrameTargets& targets,
                const wgpu::BindGroup& frame) {
  const bool v2 = d.options.mode == Mode::v2;
  encode_bsp_shadows(d, pipelines, rec, encoder);
  // The map: every kind's faces, or (culled) a phase's, drawn into the targets; the late phase
  // over what the early one drew.
  const bool culled = d.options.cull && d.cull.chunks > 0;
  const auto draw_pass = [&](std::string_view name, int phase) {
    wgpu::RenderPassColorAttachment colours[3]{};
    colours[0].view = targets.hdr_view;
    colours[1].view = targets.motion_view;
    colours[2].view = d.mask_view;
    const auto load = phase == 1 ? wgpu::LoadOp::Load : wgpu::LoadOp::Clear;
    for (auto& c : colours) {
      c.loadOp = load;
      c.storeOp = wgpu::StoreOp::Store;
      c.clearValue = {0, 0, 0, 0};
    }
    wgpu::RenderPassDepthStencilAttachment depth{};
    depth.view = targets.depth_view;
    depth.depthLoadOp = load;
    depth.depthStoreOp = wgpu::StoreOp::Store;
    depth.depthClearValue = 0.0f;
    wgpu::RenderPassDescriptor desc{};
    desc.colorAttachmentCount = 3;
    desc.colorAttachments = colours;
    desc.depthStencilAttachment = &depth;
    wgpu::RenderPassEncoder pass = rec.render(encoder, desc, name);
    // v2 lit in real time with no weather (the uniform as bsp.wgsl reads it): the lean pipeline.
    const bool lit_only =
        d.staged.info[0] > 0.5f && d.staged.info[1] == 1.0f && d.staged.weather[0] < 0.5f;
    pass.SetPipeline(pipelines.render(lit_only ? d.p_draw_lit : d.p_draw));
    pass.SetBindGroup(0, frame);
    pass.SetBindGroup(1, d.group);
    pass.SetVertexBuffer(0, d.vertices);
    pass.SetIndexBuffer(culled ? d.cull.dst : d.indices, wgpu::IndexFormat::Uint32);
    const Kind drawn[] = {kSolid, kSky, kWarp, kTele, kWater, kSlime, kLava};
    for (Kind k : drawn) {
      const bool own = (k == kWater && !d.options.warped_water) ||
                       (k == kSlime && !d.options.warped_slime) ||
                       (k == kLava && !d.options.warped_lava);
      if (v2 && own) continue;
      if (d.range[k][1] == 0) continue;
      if (culled)
        pass.DrawIndexedIndirect(d.cull.args, cull_args_at(phase, k));
      else
        pass.DrawIndexed(d.range[k][1], 1, d.range[k][0], 0, 0);
      ++rec.current().draws;
    }
    pass.End();
  };
  if (culled) {
    encode_bsp_cull(d.cull, pipelines, rec, encoder, frame, 0);
    draw_pass("bsp", 0);
    r::encode_hiz(d.cull.hiz, pipelines, rec, encoder);
    encode_bsp_cull(d.cull, pipelines, rec, encoder, frame, 1);
    draw_pass("bsp.late", 1);
  } else {
    draw_pass("bsp", 0);
  }
  if (v2) {
    wgpu::RenderPassDepthStencilAttachment depth{};
    depth.view = targets.depth_view;
    depth.depthLoadOp = wgpu::LoadOp::Load;
    depth.depthStoreOp = wgpu::StoreOp::Store;
    wgpu::RenderPassDescriptor desc{};
    desc.depthStencilAttachment = &depth;
    wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "bsp.sky");
    pass.SetPipeline(pipelines.render(d.p_fix));
    pass.SetBindGroup(0, d.fix_group);
    pass.Draw(3);
    ++rec.current().draws;
    pass.End();
  } else {
    rec.compute(encoder, "bsp.sky").End();
  }
}

void encode_bsp_veil(const BspDraw& d, const gpu::PipelineSet& pipelines,
                     render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                     const render::FrameTargets& targets) {
  const Options& o = d.options;
  const float* k = d.staged.veil;
  if (o.mode != Mode::v2 || (k[0] == 1 && k[1] == 1 && k[2] == 1)) {
    rec.compute(encoder, "bsp.veil").End();
    return;
  }
  wgpu::RenderPassColorAttachment colour{};
  colour.view = targets.hdr_view;
  colour.loadOp = wgpu::LoadOp::Load;
  colour.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &colour;
  wgpu::RenderPassEncoder pass = rec.render(encoder, desc, "bsp.veil");
  pass.SetPipeline(pipelines.render(d.p_veil));
  pass.SetBindGroup(0, d.fix_group);
  pass.Draw(3);
  ++rec.current().draws;
  pass.End();
}

r::PrecipShelter bsp_shelter(const BspDraw& d) {
  r::PrecipShelter h;
  h.maps = d.orthos_view;
  h.layer = 2;
  const GpuJob& j = d.staged.ortho[2];
  for (int k = 0; k < 3; ++k) {
    h.o[k] = j.o[k];
    h.x[k] = j.x[k];
    h.y[k] = j.y[k];
    h.z[k] = j.z[k];
  }
  h.half = j.o[3];
  h.depth = j.info[1];
  h.size = kOrthoSize;
  h.spread = d.staged.weather[2];
  return h;
}

}  // namespace ge::brush::app
