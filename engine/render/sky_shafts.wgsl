// THE SHAFTS' VOLUME (render/sky.hpp; frame.wgsl's `shafts` reads it): for each of a grid of the
// screen's rays, the share of the sun's light the air along it gets out to each slice's distance,
// worked out once a frame where every pixel's haze and sky once marched it again.
//
// The share out to a distance R is the mean of the cloud's shadow over the ray's points at R a^2,
// a uniform over 0..1: the points bunched toward the eye, where the air the eye sees is nearest.
// Over the volume's reach (kShaftReach) a slice k ends at a = (k + 1) / slices, so the share at
// its end is the mean of the shadow at the points a = 0 .. (k + 1) / slices along the whole reach:
// one walk out along each ray (kShaftSub points a slice) keeps the running mean and writes it at
// each slice's end. A distance's slice is read at sqrt(distance / reach), as the aerial volume is.
//
// Its group 0 holds only what `cloud_shadow` reads (the frame block, the cloud's noise and its
// sampler), at the frame group's bindings, so the pass writes the volume the frame group samples.
#include "render/wgsl/frame.wgsl"

@group(1) @binding(1) var shafts_out: texture_storage_3d<rgba16float, write>;

// The points marched a slice.
const kShaftSub = 2u;

@compute @workgroup_size(8, 8)
fn shafts_main(@builtin(global_invocation_id) id: vec3u) {
  let size = vec3u(kShaftSize);
  if (id.x >= size.x || id.y >= size.y) {
    return;
  }
  let lit = i32(frame.cloud_info.x) > 0 && frame.sun.y >= 0.02 && frame.weather.y <= 0.99;
  let uv = (vec2f(id.xy) + 0.5) / kShaftSize.xy;
  let d = view_ray(frame, uv);
  let n = size.z * kShaftSub;
  var sum = 0.0;
  for (var k = 0u; k < size.z; k++) {
    if (lit) {
      for (var m = 0u; m < kShaftSub; m++) {
        let a = (f32(k * kShaftSub + m) + 0.5) / f32(n);
        sum += cloud_shadow(frame.eye.xyz + d * (kShaftReach * a * a));
      }
    }
    let share = select(1.0, sum / f32((k + 1u) * kShaftSub), lit);
    textureStore(shafts_out, vec3u(id.xy, k), vec4f(share, 0.0, 0.0, 1.0));
  }
}
