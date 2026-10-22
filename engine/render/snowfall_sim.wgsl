// THE FALLING SNOW'S FLIGHT (render/snowfall.hpp): one thread a flake. It moves at the air's
// velocity plus its own fall and flutter, and its offset from the eye is wrapped into the box.
#include "render/wgsl/snowfall_types.wgsl"

@group(0) @binding(0) var<uniform> sp: SnowfallParams;
@group(0) @binding(1) var<storage, read_write> flakes: array<vec4f>;

@compute @workgroup_size(64)
fn sim(@builtin(global_invocation_id) id: vec3u) {
  let i = id.x;
  if (i >= sp.info.x) {
    return;
  }
  let f = flakes[i];
  let p = f.xyz + flake_velocity(sp, i, f.w) * sp.wind.w;
  let box = sp.eye.w;
  var d = p - sp.eye.xyz;
  d -= box * floor(d / box + 0.5);
  flakes[i] = vec4f(sp.eye.xyz + d, f.w);
}
