// THE PARTICLES' FLIGHT (render/particles.hpp): one thread a slot of the ring. A live particle
// falls by its share of gravity, drags toward the still air, moves and ages; at the ground (the
// terrain's heights) a grain is gone and a fine one comes to rest on the snow. `fly` is
// step_particle's twin in particles.cpp.
#include "render/wgsl/terrain_common.wgsl"
#include "render/wgsl/particle_types.wgsl"

@group(0) @binding(0) var<uniform> pp: ParticleParams;
@group(0) @binding(1) var<storage, read_write> slots: array<Particle>;
@group(0) @binding(2) var<uniform> ground: TerrainParams;
@group(0) @binding(3) var heights: texture_2d<f32>;

fn fly(q: Particle, dt: f32) -> Particle {
  var p = q;
  p.v.y -= kGravity * p.fall * dt;
  p.v *= exp(-p.drag * dt);
  p.p += p.v * dt;
  p.age += dt;
  return p;
}

@compute @workgroup_size(64)
fn sim(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= pp.info.x) {
    return;
  }
  var p = slots[i];
  if (p.age >= p.life || pp.step.x <= 0.0) {
    return;
  }
  p = fly(p, pp.step.x);
  let h = field_height(heights, ground, p.p.xz);
  // Under the snow and falling: it has come down on it. (One under it and rising is still coming
  // up out of the snow it was thrown from: a ski sinks into soft snow.)
  if (p.p.y < h && p.v.y < 0.0) {
    if (p.fall > 0.5) {
      // A grain buries itself in the snow it lands on.
      p.age = p.life;
    } else {
      // A fine one settles and fades where it lies.
      p.p.y = h;
      p.v = vec3f(p.v.x * 0.5, 0.0, p.v.z * 0.5);
      p.age = max(p.age, p.life * 0.7);
    }
  }
  slots[i] = p;
}
