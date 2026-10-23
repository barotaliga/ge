// PARTICLES ON THE GPU (docs/rendering.md § Particles and figures): a fixed pool of small bright
// things thrown into the air (a skier's spray, a landing's puff, the fine powder that hangs),
// flown by a compute pass and drawn as streaks softened against the depth.
//
// - THE POOL is a ring of `ParticleSettings::capacity` slots. A game hands this frame's BIRTHS
//   (`emit_particles`) and `prepare_particles` writes them into the next slots, wrapping, so a
//   pool that is full lets its oldest go first. A slot is dead once its age passes its life.
// - THE FLIGHT ("particles.sim", one thread a slot): gravity scaled by the particle's `fall` (1 a
//   grain, a tenth for the fines that drift), a drag of `drag` a second toward the air (the game's
//   wind, so a powder cloud drifts off downwind), and
//   the ground (the terrain's heights, render/terrain.hpp): a grain that comes down on the snow is
//   gone, a fine one comes to rest on it and fades (one under the snow and rising is coming up out
//   of it: a ski sinks into soft snow, and what it throws starts there). `step_particle` is its CPU
//   twin, which the test holds the pass to.
// - THE DRAW ("particles", after the sky, before the resolve): one quad a slot, a dead one folded
//   away in the vertex stage. A particle is a STREAK along its motion over `streak` s (a fast grain
//   reads as a stroke, the way a camera's shutter sees spray) at least `size` m long, thinned by
//   its length so a fast grain is no brighter than a slow one, faded in over its first tenth and
//   out over its last third, and SOFTENED where it nears what is behind it (the depth, read). It is
//   lit as the instances are, by the sun past the ground's horizon and the shadow maps and by the
//   sky, with a forward lobe so spray seen into a low sun is silver, and hazed. Blended
//   premultiplied over the picture, unsorted (a grain is small and nearly opaque, the fines nearly
//   clear); it writes the motion target's reactive channel (b) so the resolve does not smear it,
//   and nothing else there.
//
// ge:summary GPU particles: a ring pool, births, the compute flight, streaks softened by depth
// ge:keywords render particles spray particle-pool ring births compute streak soft-particles
// ge:keywords premultiplied reactive emit_particles prepare_particles encode_particles_sim
// ge:keywords encode_particles step_particle create_particles read_particles
// ge:skill render
// ge:test tests/render_particles_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "render/frame.hpp"
#include "render/shadows.hpp"
#include "render/terrain.hpp"

namespace ge::render {

// One slot, as particles.wgsl's `Particle` reads it.
struct Particle {
  float p[3];     // world m
  float age;      // s since its birth
  float v[3];     // m/s
  float life;     // s it lives
  float size;     // m across
  float drag;     // 1/s, toward the still air
  float fall;     // gravity's share: 1 a grain, ~0.1 a fine that drifts
  float streak;   // s of motion drawn as its streak
  float albedo;   // its whiteness, 0..1
  float opacity;  // at its densest, 0..1
  float pad[2];
};
static_assert(sizeof(Particle) == 64, "match particles.wgsl's Particle");

inline constexpr double kGravity = 9.81;  // m/s²

// THE FLIGHT of one particle over `dt` s (no ground) in air moving at `air` m/s (null: still): the
// CPU twin of particles_sim.wgsl's `fly`.
void step_particle(Particle& p, float dt, const float air[3] = nullptr);

struct ParticleSettings {
  uint32_t capacity = 16384;  // slots in the ring
  float soft_m = 0.4f;        // the depth over which a particle fades into what is behind it
};

struct Particles {
  ParticleSettings settings;
  wgpu::Buffer slots, params;
  wgpu::BindGroupLayout sim_layout, draw_layout, light_layout;
  wgpu::BindGroup sim_group, draw_group, light_group, shadows;
  uint32_t p_sim = 0, p_draw = 0;
  uint32_t head = 0;             // the next slot a birth takes
  uint64_t born = 0;             // since creation
  std::vector<Particle> births;  // this frame's, waiting for prepare
  float dt = 0;                  // this frame's step
};

// The pool, its pipelines (into `pipelines`, before the load ends) and its groups over the
// terrain's heights and horizon. `frame` is group 0's layout.
void create_particles(gpu::Device& device, gpu::PipelineSet& pipelines, Particles& particles,
                      const wgpu::BindGroupLayout& frame, const Terrain& terrain,
                      const ParticleSettings& settings = {});

// The draw's group over a frame's depth (again after a resize), and the shadows' group.
void bind_particle_targets(gpu::Device& device, Particles& particles, const FrameTargets& targets);
void bind_particle_shadows(Particles& particles, const wgpu::BindGroup& shadows);

// This frame's births, added to the ones waiting.
void emit_particles(Particles& particles, std::span<const Particle> births);

// Forget every particle (a cut to another run): every slot is killed now.
void clear_particles(gpu::Device& device, Particles& particles);

// The frame's uploads: the births into their slots, and the step of `dt` s the sim takes (0 holds
// every particle still, as on a paused frame). `near_m` is the camera's near plane; `air` the air's
// velocity, m/s, that every particle's drag pulls it toward (null: still air).
void prepare_particles(gpu::Device& device, Particles& particles, double dt, double near_m,
                       const double* air = nullptr);

// The two passes: "particles.sim" (before any view is drawn) and "particles" (after the sky).
void encode_particles_sim(const Particles& particles, const gpu::PipelineSet& pipelines,
                          FrameRecorder& rec, wgpu::CommandEncoder& encoder);
void encode_particles(const Particles& particles, const gpu::PipelineSet& pipelines,
                      FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                      const FrameTargets& targets, const wgpu::BindGroup& frame);

// Labs and tests (blocks): every slot as the GPU holds it.
std::vector<Particle> read_particles(gpu::Device& device, const Particles& particles);

// How many slots are alive in `slots`.
uint32_t live_particles(std::span<const Particle> slots);

}  // namespace ge::render
