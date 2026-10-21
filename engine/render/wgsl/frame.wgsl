// GROUP 0 OF EVERY DRAW: the frame block and the sky's tables (docs/rendering.md § Geometry and
// resources, bind group frequency), with the shading functions that read them. One copy of
// each idea: the haze, the sun's light at a point and the sky's light are here and nowhere
// else. render/sky.hpp's `frame_layout` is the C++ side of these bindings.
#include "render/wgsl/frame_types.wgsl"
#include "render/wgsl/atmosphere.wgsl"

@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var linear_clamp: sampler;
@group(0) @binding(2) var transmittance_lut: texture_2d<f32>;
@group(0) @binding(3) var multiscatter_lut: texture_2d<f32>;
@group(0) @binding(4) var sky_view_lut: texture_2d<f32>;
@group(0) @binding(5) var aerial_volume: texture_3d<f32>;

struct SkyLight {
  irradiance_up: vec4f,  // the sky's irradiance on an up-facing surface, per unit sun
  sun_at_eye: vec4f,     // the sun's transmittance to the eye
}
@group(0) @binding(6) var<storage, read> sky_light: SkyLight;

// A world height (m) as a radius from the planet's centre, km.
fn radius_at(world_y: f32) -> f32 {
  return kGroundRadius + max(frame.world.x + world_y * 1e-3, 0.0);
}

// The sun's light reaching a point at `world_y` (m): illuminance times the transmittance.
fn sun_light(world_y: f32) -> vec3f {
  return frame.sun.w * transmittance_to_top(transmittance_lut, linear_clamp, radius_at(world_y), frame.sun.y);
}

// The sky's irradiance on a surface of normal `n` (the up-facing integral, falling off toward
// a surface that faces the ground), with the ground's bounce for the half that faces down.
fn sky_irradiance(n: vec3f) -> vec3f {
  let up = sky_light.irradiance_up.rgb * frame.sun.w;
  let ground = (sky_light.sun_at_eye.rgb * frame.sun.w * max(frame.sun.y, 0.0) + up) * kGroundAlbedo;
  return up * (0.5 + 0.5 * n.y) + ground * (0.5 - 0.5 * n.y) * 0.5;
}

// THE HAZE: a surface's colour seen through `dist_m` of air at screen `uv` (0..1): its colour
// times the air's transmittance plus the light the air scatters in, from the aerial-perspective
// volume. Every world material calls this, and none has its own fog.
fn haze(colour: vec3f, uv: vec2f, dist_m: f32) -> vec3f {
  let s = sqrt(clamp(dist_m * 1e-3 / kAerialRangeKm, 0.0, 1.0));
  let w = max(s - 0.5 / kAerialSize.z, 0.5 / kAerialSize.z);
  let v = textureSampleLevel(aerial_volume, linear_clamp, vec3f(uv, w), 0.0);
  // Nearer than the first slice: fade from no air to the first slice's.
  let k = clamp(s * kAerialSize.z, 0.0, 1.0);
  return colour * mix(1.0, v.a, k) + v.rgb * k * frame.sun.w;
}

// The sky's luminance along a camera-relative direction, from the sky-view table.
fn sky_luminance(d: vec3f) -> vec3f {
  let r = kGroundRadius + max(frame.eye.w, 0.0);
  let uv = skyview_uv(r, d.y, azimuth_cos(d, frame.sun.xyz));
  return textureSampleLevel(sky_view_lut, linear_clamp, uv, 0.0).rgb * frame.sun.w;
}
