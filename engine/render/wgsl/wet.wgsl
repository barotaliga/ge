// THE WET GROUND for any material that the rain soaks: `wet_albedo` and `wet_sheen`, and the
// scene mirrored in it (render/wgsl/mirror.wgsl). Included by the materials that read it, not by
// every draw that reads the frame, so a pass that never draws wet ground carries none of it.
//
// ge:summary WGSL library: the wet ground's darkening and its sheen, the scene mirrored in it
// ge:keywords wet-ground wet_albedo wet_sheen puddle film fresnel rain mirror wgsl library
// ge:skill render
// ge:test tests/render_mirror_test.cpp

#include "render/wgsl/frame.wgsl"
#include "render/wgsl/mirror.wgsl"

// THE WET GROUND (render/view.hpp's `Weather::wet` and `puddles`, in `frame.wet.xy`): a wet
// surface is darker, because the water filling its pores keeps the light inside it longer
// (`wet_albedo`: a porous surface, 1, goes to half its albedo soaked; a sealed one, 0, keeps it),
// and it mirrors the sky in its film (`wet_sheen`: Schlick's Fresnel off water, 0.02 head on, the
// sky's light along the reflected ray and the sun's glint, the film's mirror rough and a puddle's
// flat; the scene mirrored where the last frame drew it: render/wgsl/mirror.wgsl). `flat` is how
// level the surface lies (its normal's y over 0.9 .. 0.98 is a puddle's); `rel` where it is,
// camera-relative.
fn wet_albedo(albedo: vec3f, porous: f32) -> vec3f {
  return albedo * (1.0 - 0.5 * frame.wet.x * clamp(porous, 0.0, 1.0));
}

fn wet_sheen(n: vec3f, v: vec3f, sun: vec3f, flat: f32, rel: vec3f) -> vec3f {
  let wet = frame.wet.x;
  if (wet <= 0.0) {
    return vec3f(0.0);
  }
  let pool = frame.wet.y * smoothstep(0.9, 0.98, flat);
  // A puddle lies level whatever the ground's grain under it.
  let m = normalize(mix(n, vec3f(0.0, 1.0, 0.0), pool));
  let r = reflect(-v, m);
  let cos_v = clamp(dot(m, v), 0.0, 1.0);
  let fresnel = 0.02 + 0.98 * pow(1.0 - cos_v, 5.0);
  // The scene where the picture holds it (render/wgsl/mirror.wgsl), sharp in a puddle and
  // blurred in the film; the sky where it does not, and the sun's glint only there.
  var seen = vec4f(0.0);
  if (fresnel > 0.05) {
    seen = scene_reflection(rel + m * 0.05, r, mix(0.6, 0.0, pool));
  }
  let sky = mix(sky_luminance(normalize(vec3f(r.x, max(r.y, 0.02), r.z))), seen.rgb, seen.a);
  let sharp = mix(60.0, 1500.0, pool);
  let glint = pow(max(dot(r, frame.sun.xyz), 0.0), sharp) * mix(4.0, 60.0, pool) * (1.0 - seen.a);
  return (sky + sun * glint / PI) * fresnel * wet * mix(0.5, 1.0, pool);
}
