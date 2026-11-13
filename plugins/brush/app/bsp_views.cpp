// The map drawn again for its water (bsp_views.hpp).
//
// ge:summary The map drawn again for its water: the planar mirror in a pool's plane, the view up
// ge:keywords bsp water planar-mirror view-up bsp-views encode_bsp_views
// ge:keywords mirror-skirt mirror_skirt waterline clip-plane white-line
// ge:skill brush render water
#include "brush/app/bsp_views.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ge::brush::app {

namespace r = ge::render;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kUpSize = 512;     // the view up's texels a side, at most
constexpr float kMirrorSkirt = 0.1f;  // m past the level the mirror's cut lies (prepare_bsp_views)

void make_view(gpu::Device& device, BspView& v, const char* label, uint32_t w, uint32_t h) {
  v.width = w;
  v.height = h;
  const auto usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  v.colour = r::make_texture(device, label, w, h, 1, r::kHdrFormat, usage);
  v.motion = r::make_texture(device, label, w, h, 1, r::kMotionFormat, usage);
  v.mask = r::make_texture(device, label, w, h, 1, wgpu::TextureFormat::R8Unorm, usage);
  v.depth = r::make_texture(device, label, w, h, 1, r::kDepthFormat, usage);
  v.colour_view = v.colour.CreateView();
  v.motion_view = v.motion.CreateView();
  v.mask_view = v.mask.CreateView();
  v.depth_view = v.depth.CreateView();
}

// The view's block off the frame's: its own camera, its size, no motion, no cull, the clip plane.
void stage(BspView& v, const r::FrameUniforms& main, const float vp[16], const float inv[16],
           const float eye[3], const float plane[4]) {
  v.staged = main;
  std::memcpy(v.staged.view_proj, vp, sizeof v.staged.view_proj);
  std::memcpy(v.staged.prev_view_proj, vp, sizeof v.staged.prev_view_proj);
  std::memcpy(v.staged.inv_view_proj, inv, sizeof v.staged.inv_view_proj);
  for (int k = 0; k < 3; ++k) v.staged.eye[k] = eye[k];
  v.staged.size[0] = static_cast<float>(v.width);
  v.staged.size[1] = static_cast<float>(v.height);
  v.staged.size[2] = 1.0f / static_cast<float>(v.width);
  v.staged.size[3] = 1.0f / static_cast<float>(v.height);
  std::memset(v.staged.planes, 0, sizeof v.staged.planes);
  for (int k = 0; k < 4; ++k) v.staged.planes[5][k] = plane[k];
  std::memset(v.staged.eye_delta, 0, sizeof v.staged.eye_delta);
  std::memset(v.staged.jitter, 0, sizeof v.staged.jitter);
}

void draw_view(const BspDraw& d, const BspView& v, const gpu::PipelineSet& pipelines,
               r::FrameRecorder& rec, wgpu::CommandEncoder& encoder, int part, int parts) {
  wgpu::RenderPassColorAttachment colours[3]{};
  colours[0].view = v.colour_view;
  colours[1].view = v.motion_view;
  colours[2].view = v.mask_view;
  for (auto& c : colours) {
    c.loadOp = wgpu::LoadOp::Clear;
    c.storeOp = wgpu::StoreOp::Store;
    c.clearValue = {0, 0, 0, 0};
  }
  wgpu::RenderPassDepthStencilAttachment depth{};
  depth.view = v.depth_view;
  depth.depthLoadOp = wgpu::LoadOp::Clear;
  depth.depthStoreOp = wgpu::StoreOp::Store;
  depth.depthClearValue = 0.0f;
  wgpu::RenderPassDescriptor desc{};
  desc.colorAttachmentCount = 3;
  desc.colorAttachments = colours;
  desc.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = rec.render_part(encoder, desc, "bsp.views", part, parts);
  // The pipeline the frame's own pass takes (bsp_draw.cpp's encode_bsp).
  const bool lit_only =
      d.staged.info[0] > 0.5f && d.staged.info[1] == 1.0f && d.staged.weather[0] < 0.5f;
  pass.SetPipeline(pipelines.render(lit_only ? d.p_draw_lit : d.p_draw));
  pass.SetBindGroup(0, v.frame.surface);  // the lean part bsp.draw is laid out for
  pass.SetBindGroup(1, d.group);
  pass.SetVertexBuffer(0, d.vertices);
  pass.SetIndexBuffer(d.indices, wgpu::IndexFormat::Uint32);
  const bool v2 = d.options.mode == Mode::v2;
  const Kind drawn[] = {kSolid, kSky, kWarp, kTele, kWater, kSlime, kLava};
  for (Kind k : drawn) {
    const bool own = (k == kWater && !d.options.warped_water) ||
                     (k == kSlime && !d.options.warped_slime) ||
                     (k == kLava && !d.options.warped_lava);
    if ((v2 && own) || d.range[k][1] == 0) continue;
    pass.DrawIndexed(d.range[k][1], 1, d.range[k][0], 0, 0);
    ++rec.current().draws;
  }
  pass.End();
}

}  // namespace

void mirror_matrices(const float vp[16], const float inv[16], float out[16], float inv_out[16]) {
  // view_proj times diag(1, -1, 1, 1): its second column turned over; the inverse's second row.
  std::memcpy(out, vp, 16 * sizeof(float));
  std::memcpy(inv_out, inv, 16 * sizeof(float));
  for (int k = 0; k < 4; ++k) {
    out[4 + k] = -out[4 + k];
    inv_out[k * 4 + 1] = -inv_out[k * 4 + 1];
  }
}

void up_matrices(double fov_deg, double near_m, float out[16], float inv_out[16]) {
  // clip = (f x, f z, near, y) for a camera-relative (x, y, z): looking along +y, the screen's
  // right +x and its up +z; the depth near / y, reversed, with no far plane.
  const double f = 1 / std::tan(fov_deg * kPi / 360);
  std::memset(out, 0, 16 * sizeof(float));
  std::memset(inv_out, 0, 16 * sizeof(float));
  out[0 * 4 + 0] = static_cast<float>(f);
  out[2 * 4 + 1] = static_cast<float>(f);
  out[3 * 4 + 2] = static_cast<float>(near_m);
  out[1 * 4 + 3] = 1;
  // (X, Y, Z) on the screen back to (X / f, 1, Y / f) over Z / near.
  inv_out[0 * 4 + 0] = static_cast<float>(1 / f);
  inv_out[3 * 4 + 1] = 1;
  inv_out[1 * 4 + 2] = static_cast<float>(1 / f);
  inv_out[2 * 4 + 3] = static_cast<float>(1 / near_m);
}

void up_place(const float box[4], float x, float z, float out[2]) {
  const float at[2] = {x, z};
  for (int k = 0; k < 2; ++k) {
    const float lo = box[k], hi = box[k + 2];
    const float in = std::min(0.25f, 0.5f * (hi - lo));
    out[k] = std::clamp(at[k], lo + in, hi - in);
  }
}

void create_bsp_views(gpu::Device& device, BspViews& views, const wgpu::BindGroupLayout& layout,
                      const r::Sky& sky) {
  for (BspView* v : {&views.mirror, &views.up}) {
    v->uniforms =
        r::make_buffer(device, "bsp.views", sizeof(r::FrameUniforms), wgpu::BufferUsage::Uniform);
    v->frame = r::frame_group(device, layout, v->uniforms, sky);
  }
}

void create_bsp_view_targets(gpu::Device& device, BspViews& views, uint32_t width,
                             uint32_t height) {
  make_view(device, views.mirror, "bsp.views.mirror", std::max(1u, width / 2),
            std::max(1u, height / 2));
  const uint32_t up = std::clamp(height / 2, 64u, kUpSize);
  make_view(device, views.up, "bsp.views.up", up, up);
}

void prepare_bsp_views(gpu::Device& device, BspViews& views, const r::FrameUniforms& main, int body,
                       double level, bool below, const float box[4], double up_level) {
  views.body = body;
  views.below = below;
  views.level = level;
  views.mirror.on = body >= 0 && views.mirror.width > 0;
  views.up.on = views.mirror.on && below;
  if (!views.mirror.on) return;
  const auto h = static_cast<float>(level);
  {
    float vp[16], inv[16];
    mirror_matrices(main.view_proj, main.inv_view_proj, vp, inv);
    const float eye[3] = {main.eye[0], 2 * h - main.eye[1], main.eye[2]};
    // Over the surface the mirror shows what stands over the water; under it, what lies below.
    // The cut sits kMirrorSkirt past the level, so a wall's foot is in the mirror's picture down
    // to the waterline and a little past it: cut at the level itself, the half-size picture's last
    // row along the wall comes out empty and the water reads the room's flat light there, a thin
    // bright line where the water meets the wall.
    const float cut = h + (below ? kMirrorSkirt : -kMirrorSkirt);
    const float plane[4] = {0, below ? -1.0f : 1.0f, 0, below ? cut : -cut};
    stage(views.mirror, main, vp, inv, eye, plane);
    device.queue.WriteBuffer(views.mirror.uniforms, 0, &views.mirror.staged,
                             sizeof views.mirror.staged);
  }
  if (views.up.on) {
    float vp[16], inv[16];
    up_matrices(kUpFov, 0.05, vp, inv);
    const auto top = static_cast<float>(up_level > -1e299 ? up_level : level);
    const float plane[4] = {0, 1, 0, -top};
    // From the surface straight over the eye, so the rays the surface bends out start about where
    // the view's do (a deep eye's own place would look through the pool's walls); from a flooded
    // passage, the nearest point of the pool's top.
    float at[2];
    up_place(box, main.eye[0], main.eye[2], at);
    views.up_shift[0] = main.eye[0] - at[0];
    views.up_shift[1] = main.eye[2] - at[1];
    const float eye[3] = {at[0], top, at[1]};
    stage(views.up, main, vp, inv, eye, plane);
    device.queue.WriteBuffer(views.up.uniforms, 0, &views.up.staged, sizeof views.up.staged);
  }
}

void encode_bsp_views(const BspDraw& draw, const BspViews& views, const gpu::PipelineSet& pipelines,
                      r::FrameRecorder& rec, wgpu::CommandEncoder& encoder) {
  const int parts = (views.mirror.on ? 1 : 0) + (views.up.on ? 1 : 0);
  if (parts == 0) {
    rec.compute(encoder, "bsp.views").End();
    return;
  }
  int part = 0;
  if (views.mirror.on) draw_view(draw, views.mirror, pipelines, rec, encoder, part++, parts);
  if (views.up.on) draw_view(draw, views.up, pipelines, rec, encoder, part++, parts);
}

r::WaterViews water_views(const BspViews& views) {
  r::WaterViews w;
  if (!views.mirror.on) return w;
  w.body = views.body;
  w.below = views.below;
  w.level = views.level;
  w.has_mirror = true;
  w.mirror = views.mirror.colour_view;
  w.mirror_depth = views.mirror.depth_view;
  w.has_up = views.up.on;
  w.up = views.up.colour_view;
  float inv[16];
  up_matrices(kUpFov, 0.05, w.up_view_proj, inv);
  // The water reads it relative to the level straight over the eye: moved by the shift, the
  // translation column gains the x and z columns times it.
  for (int k = 0; k < 4; ++k)
    w.up_view_proj[12 + k] +=
        views.up_shift[0] * w.up_view_proj[0 + k] + views.up_shift[1] * w.up_view_proj[8 + k];
  return w;
}

}  // namespace ge::brush::app
