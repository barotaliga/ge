// WHAT FALLS, FLOWN (render/precip.hpp): one thread a particle. It moves at its velocity (the air's,
// the eddies', its own fall) and its offset from the eye is wrapped into the box.
#include "render/wgsl/precip_types.wgsl"

@group(0) @binding(0) var<uniform> sp: PrecipParams;
@group(0) @binding(1) var<storage, read_write> particles: array<vec4f>;

@compute @workgroup_size(64)
fn sim(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= sp.info.x) {
    return;
  }
  let f = particles[i];
  let p = f.xyz + particle_velocity(sp, i, f.w, f.xyz) * sp.wind.w;
  let box = sp.eye.w;
  var d = p - sp.eye.xyz;
  d -= box * floor(d / box + 0.5);
  particles[i] = vec4f(sp.eye.xyz + d, f.w);
}
