// THE FALLING SNOW DRAWN (render/snowfall.hpp): one quad a flake, the first `info.y` of them. A
// flake is a streak along its motion over the shutter, at least `size` long (and a pixel and a
// half wide, thinned to keep its light where it is smaller), faded out toward the
// box's faces (the wrap is never seen) and right at the lens, lit by the sun past the ground's
// horizon and the sky, hazed, softened against the depth. Premultiplied over the picture; the
// motion target's reactive channel only.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/terrain_common.wgsl"
#include "render/wgsl/snowfall_types.wgsl"

@group(1) @binding(0) var<uniform> ground: TerrainParams;
@group(1) @binding(1) var horizon0: texture_2d<f32>;
@group(1) @binding(2) var horizon1: texture_2d<f32>;

@group(2) @binding(0) var<uniform> sp: SnowfallParams;
@group(2) @binding(1) var<storage, read> flakes: array<vec4f>;
@group(2) @binding(2) var scene_depth: texture_depth_2d;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) quad: vec2f,
  @location(1) rel: vec3f,
  @location(2) @interpolate(flat) look: vec2f,  // opacity, thinning
}

const kCorners = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
                                 vec2f(-1.0, -1.0), vec2f(1.0, 1.0), vec2f(-1.0, 1.0));
const kShutter = 1.0 / 60.0;

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  var out: VertexOut;
  let f = flakes[iid];
  let rel = f.xyz - frame.eye.xyz;
  let dist = length(rel);
  // Faded toward the box's faces and away right at the lens.
  let edge = max(abs(rel.x), max(abs(rel.y), abs(rel.z))) / (0.5 * sp.eye.w);
  let fade = (1.0 - smoothstep(0.7, 1.0, edge)) * smoothstep(0.25, 0.8, dist);
  if (iid >= sp.info.y || fade <= 0.0) {
    out.position = vec4f(2.0, 2.0, 2.0, 1.0);
    return out;
  }
  let c = kCorners[vid];
  let size = sp.step.w;
  let to_eye = -rel / max(dist, 1e-4);
  let motion = flake_velocity(sp, iid, f.w) * kShutter;
  let flat_motion = motion - dot(motion, to_eye) * to_eye;
  let len = length(flat_motion);
  var along = vec3f(0.0);
  if (len > size) {
    along = flat_motion / len;
  } else {
    let side = cross(vec3f(0.0, 1.0, 0.0), to_eye);
    along = side / max(length(side), 1e-4);
  }
  let across = normalize(cross(to_eye, along));
  let centre = rel - flat_motion * 0.5 * select(0.0, 1.0, len > size);
  // A far flake is narrower than a pixel: widened to a pixel and a half, and as much thinner, so
  // it covers the same light and does not shimmer in and out between pixels.
  let c0 = frame.view_proj * vec4f(centre, 1.0);
  let c1 = frame.view_proj * vec4f(centre + across, 1.0);
  let px_per_m = length((c1.xy / c1.w - c0.xy / c0.w) * frame.size.xy * 0.5);
  let widen = max(1.0, 1.5 / max(size * px_per_m, 1e-4));
  let width = size * widen;
  let half_len = max(len, width) * 0.5;
  let corner = centre + along * (c.x * half_len) + across * (c.y * width * 0.5);
  out.position = frame.view_proj * vec4f(corner, 1.0);
  out.quad = c;
  out.rel = corner;
  // A streak spreads a flake over its length, but the eye keeps a moving flake brighter than its
  // share of the shutter: thinned by the root of its length, not the length.
  out.look = vec2f(fade / widen, sqrt(width / max(len, width)));
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let across = 1.0 - in.quad.y * in.quad.y;
  let ends = 1.0 - smoothstep(0.6, 1.0, abs(in.quad.x));
  var a = in.look.x * in.look.y * across * ends;
  let d = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  let behind = sp.step.y / max(d, 1e-7);
  let here = sp.step.y / max(in.position.z, 1e-7);
  a *= clamp((behind - here) / max(sp.step.z, 1e-3), 0.0, 1.0);
  if (a <= 0.002) {
    discard;
  }
  let y = in.rel.y + frame.eye.y;
  let xz = in.rel.xz + frame.eye.xz;
  let lit = ground_shadow(ground, horizon0, horizon1, linear_clamp, frame.sun.xyz, xz);
  let c = 0.9 / PI * (sun_light(y, xz) * lit + sky_irradiance(vec3f(0.0, 1.0, 0.0)));
  let hazed = haze(c, in.position.xy * frame.size.zw, length(in.rel));
  var out: FragmentOut;
  out.colour = vec4f(hazed * a, a);
  out.motion = vec4f(0.0, 0.0, a, 0.0);
  return out;
}
