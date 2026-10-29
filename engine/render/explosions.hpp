// EXPLOSIONS DRAWN (docs/rendering.md § Explosions): each live blast's fireball and smoke as a
// small volume marched per pixel, its sparks and debris on the particle pool, its flash handed to
// the local lights by the game (render/explosion.hpp says what each is at each moment).
//
// - THE VOLUME is a ball of radius 1.3 times the fireball's, drawn as ONE QUAD round its outline
//   on the screen (the cone from the eye that just holds it; the whole screen when the eye is
//   inside it). A pixel's ray is cut by the ball and by the scene's depth (so a wall in front of
//   it, or the floor through it, clips it exactly, not by a soft fade), and marched in
//   `ExplosionSettings::steps` steps from a jittered start (the TAA resolve averages the jitter
//   away). Each step reads the cloud's tileable noise twice (render/clouds.hpp, frame.wgsl's 8:
//   the value noise and the billows), turned and drifted by the blast's seed and its age, and
//   carves the ball's edge with it into rolling billows. The gas there is hot where it is deep
//   in the ball and still burning: it EMITS the blackbody light of its temperature
//   (render/blackbody.hpp's table, a perfect radiator's luminance, so the fireball sits in the
//   sun's units and is white-hot, then orange, then a dull red); and it SCATTERS as smoke of
//   its look's albedo and tint (soot, dust, steam), lit by the sun (its sunward side brighter),
//   the sky and the fire inside it; the look's billows, fray and billow size shape its edge.
//   Emission and absorption are integrated front to back; the march stops once the ray is
//   nearly opaque.
// - THE COST is a pixel's steps where a fireball covers the screen, and nothing elsewhere: no
//   texture of its own, two fetches a step, one draw for every blast. At most kMaxExplosions are
//   drawn a frame (the nearest kept), back to front, premultiplied over the picture; each writes
//   the motion target's reactive channel so TAA does not smear it.
// - THE SPARKS: `throw_blast` turns a blast's births into particles (glowing sparks that cool,
//   dark debris) and hands them to the pool, once, the frame the blast goes off.
//
// ge:summary Explosions drawn: the fireball volume marched per pixel, sorted, and its sparks
// ge:keywords render explosions fireball volume raymarch billboard impostor smoke emission
// ge:keywords blackbody sparks debris create_explosions prepare_explosions encode_explosions
// ge:keywords throw_blast spark_particle explosion-cost
// ge:skill render
// ge:test tests/render_explosion_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/explosion.hpp"
#include "render/frame.hpp"
#include "render/particles.hpp"
#include "render/view.hpp"

namespace ge::render {

inline constexpr uint32_t kMaxExplosions = 32;  // drawn a frame (the nearest kept)
inline constexpr double kBlastBound = 1.3;      // the volume's radius over the fireball's

struct ExplosionSettings {
  uint32_t steps = 10;  // the march's steps through a ball
};

// explosions.wgsl's `Blast`: one drawn this frame.
struct GpuBlast {
  float centre[4];  // camera-relative, m; the fireball's radius
  float state[4];   // the core's temperature, K; the fire share; the smoke; the age, s
  float look[4];    // the noise's offset (its tile's units); the bound's radius, m
  float more[4];    // 1 when the eye is inside the bound; 0; 0; 0
  float shape[4];   // the look's billows; fray; the noise's tiles per radius; roll, radii/s
  float soot[4];    // the smoke's albedo times its tint; 0
};
static_assert(sizeof(GpuBlast) == 96, "match explosions.wgsl's Blast");

struct Explosions {
  ExplosionSettings settings;
  wgpu::Buffer params, blasts;
  wgpu::BindGroupLayout layout;
  wgpu::BindGroup group;
  uint32_t p_draw = 0;
  float table[64][4];            // render/blackbody.hpp's rows, made once
  float hot_cd = 0;              // a perfect radiator's luminance at the table's last row, cd/m^2
  std::vector<GpuBlast> staged;  // kMaxExplosions
  std::vector<std::pair<float, uint32_t>> order;  // scratch: the live ones by distance
  uint32_t drawn = 0;                             // this frame's
  uint32_t culled = 0;  // alive but off the screen, or past kMaxExplosions
};

// The pass's buffers and pipeline (into `pipelines`, before the load ends). `frame` is group 0's
// layout.
void create_explosions(gpu::Device& device, gpu::PipelineSet& pipelines, Explosions& ex,
                       const wgpu::BindGroupLayout& frame, const ExplosionSettings& settings = {});
// The group over a frame's depth (again after a resize).
void bind_explosion_targets(gpu::Device& device, Explosions& ex, const FrameTargets& targets);

// This frame's blasts: each one alive at `view.time_s` and on the screen of a `width` x `height`
// render, at most kMaxExplosions of the nearest, staged far to near. Allocates nothing.
void stage_explosions(Explosions& ex, std::span<const Blast> blasts, const FrameView& view,
                      uint32_t width, uint32_t height);
// Stage (above) and upload.
void prepare_explosions(gpu::Device& device, Explosions& ex, std::span<const Blast> blasts,
                        const FrameView& view, uint32_t width, uint32_t height);

// The pass "explosions" (after the particles, before the resolve): one quad a staged blast.
void encode_explosions(const Explosions& ex, const gpu::PipelineSet& pipelines, FrameRecorder& rec,
                       wgpu::CommandEncoder& encoder, const FrameTargets& targets,
                       const wgpu::BindGroup& frame);

// One thrown thing as the particle pool flies it.
Particle spark_particle(const Spark& s);
// Every birth of `blast` into the pool's births (call once, the frame it goes off). The count.
size_t throw_blast(Particles& particles, const Blast& blast);

}  // namespace ge::render
