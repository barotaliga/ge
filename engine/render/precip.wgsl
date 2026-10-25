// WHAT FALLS, DRAWN (render/precip.hpp): one quad a particle (the first `info.y` of them), then one
// a splash (the next `info.z` instances).
//
// - A PARTICLE is a streak along its motion over the shutter: a flake's over 1/60 s at least `size`
//   long, a drop's over 1/30 s (the eye smears a drop into a line), each widened to a pixel and a
//   half where it is thinner and thinned as much, faded out toward the box's faces (the wrap is
//   never seen) and right at the lens, softened against the depth. A flake is lit white by the sun
//   past the ground's horizon and the sky; a drop is clear water, a faint grey line that takes the
//   sky's light and catches the sun.
// - A SPLASH is a drop landing: each instance a crown that lives `splash.y` s, rising and spreading
//   from a spot drawn afresh each life out of the screen and set on the surface under it off the
//   depth (so it lands on any ground, a roof or a deck alike), kept where the surface faces up and
//   within `splash.x` m, at an even density over the ground (a far spot is kept by its share of the
//   ground a pixel covers).
//
// Under a SHELTER map (`sp.shelter_info.x` 1) a particle and a splash are kept by how open the
// way they fell is: the share of four taps of the map round the point that see the sky along the
// fall. Premultiplied over the picture; the motion target's reactive channel only.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/terrain_common.wgsl"
#include "render/wgsl/precip_types.wgsl"

@group(1) @binding(0) var<uniform> ground: TerrainParams;
@group(1) @binding(1) var horizon0: texture_2d<f32>;
@group(1) @binding(2) var horizon1: texture_2d<f32>;

@group(2) @binding(0) var<uniform> sp: PrecipParams;
@group(2) @binding(1) var<storage, read> particles: array<vec4f>;
@group(2) @binding(2) var scene_depth: texture_depth_2d;
@group(2) @binding(3) var shelter_maps: texture_depth_2d_array;
@group(2) @binding(4) var shelter_cmp: sampler_comparison;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) quad: vec2f,
  @location(1) rel: vec3f,
  // opacity, thinning, kind (0 a flake, 1 a drop, 2 a splash), a splash's age
  @location(2) @interpolate(flat) look: vec4f,
}

const kCorners = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
                                 vec2f(-1.0, -1.0), vec2f(1.0, 1.0), vec2f(-1.0, 1.0));
const kShutter = 1.0 / 60.0;
const kDropShutter = 1.0 / 20.0;

fn culled() -> VertexOut {
  var out: VertexOut;
  out.position = vec4f(2.0, 2.0, 2.0, 1.0);
  return out;
}

// How open the way the fall came to the world point `p` is: 1 with no shelter map or off its
// edge, else the share of four taps a texel apart that see the sky along the fall.
fn open_to_fall(p: vec3f) -> f32 {
  if (sp.shelter_info.x < 0.5) {
    return 1.0;
  }
  let q = p - sp.shelter_o.xyz;
  let half = sp.shelter_o.w;
  let uv = vec2f(0.5 + 0.5 * dot(q, sp.shelter_x.xyz) / half,
                 0.5 - 0.5 * dot(q, sp.shelter_y.xyz) / half);
  if (any(uv < vec2f(0.0)) || any(uv > vec2f(1.0))) {
    return 1.0;
  }
  let depth = sp.shelter_info.y;
  let z = (dot(q, sp.shelter_z.xyz) + depth) / (2.0 * depth) - 0.002;
  let layer = i32(sp.shelter_info.z);
  let d = 0.5 * sp.shelter_info.w / half;
  var seen = 0.0;
  for (var k = 0; k < 4; k++) {
    let o = vec2f(select(-d, d, (k & 1) == 1), select(-d, d, (k & 2) == 2));
    seen += textureSampleCompareLevel(shelter_maps, shelter_cmp, uv + o, layer, z);
  }
  return seen * 0.25;
}

fn hash2(n: u32) -> vec2f {
  var h = n * 747796405u + 2891336453u;
  h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
  let a = (h >> 22u) ^ h;
  var g = a * 747796405u + 2891336453u;
  g = ((g >> ((g >> 28u) + 4u)) ^ g) * 277803737u;
  let b = (g >> 22u) ^ g;
  return vec2f(f32(a & 0xffffffu), f32(b & 0xffffffu)) / 16777216.0;
}

// The camera-relative point a pixel's depth shows.
fn unproject(px: vec2i) -> vec3f {
  let d = textureLoad(scene_depth, px, 0);
  let uv = (vec2f(px) + 0.5) * frame.size.zw;
  let ndc = vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, max(d, 1e-7), 1.0);
  let p = frame.inv_view_proj * ndc;
  return p.xyz / p.w;
}

fn splash_vertex(s: u32, vid: u32) -> VertexOut {
  let life = sp.splash.y;
  // Each splash has its own clock offset, so the landings never pulse together.
  let off = hash2(s ^ 0x9e3779b9u).x;
  let t = sp.step.x / life + off;
  let cycle = u32(i32(floor(t)) & 0x7fffffff);
  let age = fract(t);
  let pick = hash2(s * 7919u + cycle * 104729u);
  let size = vec2i(frame.size.xy);
  let px = clamp(vec2i(pick * frame.size.xy), vec2i(2), size - vec2i(3));
  if (textureLoad(scene_depth, px, 0) <= 0.0) {
    return culled();  // the sky
  }
  let rel = unproject(px);
  let dist = length(rel);
  let reach = sp.splash.x;
  let to_eye = -rel / max(dist, 1e-4);
  // The surface's normal off the depth's neighbours, turned to face the eye (a surface the depth
  // shows always does).
  var n = normalize(cross(unproject(px + vec2i(2, 0)) - rel, unproject(px + vec2i(0, 2)) - rel));
  n *= sign(dot(n, to_eye) + 1e-6);
  let facing = abs(dot(n, to_eye));
  // An even density over the ground: a pixel at distance d covers d^2 / cos of it.
  let keep = dist * dist / (reach * reach * max(facing, 0.08));
  if (n.y < 0.6 || dist > reach || dist < 0.6 || hash2(cycle ^ (s * 31u)).y > keep) {
    return culled();
  }
  // Only where the fall reaches: lifted a little toward the sky, off the surface it lands on.
  let reached = open_to_fall(rel + frame.eye.xyz - sp.shelter_z.xyz * 0.08 + n * 0.02);
  if (reached < hash2(cycle * 13u + s).x) {
    return culled();
  }
  let c = kCorners[vid];
  let side = normalize(cross(vec3f(0.0, 1.0, 0.0), to_eye));
  let rise = sin(3.14159 * sqrt(age));
  let w = sp.splash.z * (0.5 + 0.9 * age);
  let h = sp.splash.z * 0.9 * rise + 0.004;
  // Stood a hair toward the eye, so the surface it lands on does not cut it.
  let foot = rel + to_eye * 0.03;
  let corner = foot + side * (c.x * w) + vec3f(0.0, 1.0, 0.0) * ((c.y * 0.5 + 0.5) * h);
  var out: VertexOut;
  out.position = frame.view_proj * vec4f(corner, 1.0);
  out.quad = c;
  out.rel = corner;
  let fade = 1.0 - smoothstep(0.6 * reach, reach, dist);
  out.look = vec4f(fade * (1.0 - age), 1.0, 2.0, age);
  return out;
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  if (iid >= sp.info.y) {
    let s = iid - sp.info.y;
    if (s >= sp.info.z) {
      return culled();
    }
    return splash_vertex(s, vid);
  }
  var out: VertexOut;
  let f = particles[iid];
  let rel = f.xyz - frame.eye.xyz;
  let dist = length(rel);
  // Faded toward the box's faces and away right at the lens.
  let edge = max(abs(rel.x), max(abs(rel.y), abs(rel.z))) / (0.5 * sp.eye.w);
  var fade = (1.0 - smoothstep(0.7, 1.0, edge)) * smoothstep(0.25, 0.8, dist);
  if (fade > 0.0) {
    fade *= open_to_fall(f.xyz);
  }
  if (fade <= 0.0) {
    return culled();
  }
  let drop = is_drop(sp, iid);
  let c = kCorners[vid];
  let size = select(sp.step.w, sp.fall.w, drop);
  let to_eye = -rel / max(dist, 1e-4);
  let motion = particle_velocity(sp, iid, f.w, f.xyz) * select(kShutter, kDropShutter, drop);
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
  // A far particle is narrower than a pixel: widened to a pixel and a half, and thinner by the root
  // of the widening, so it does not shimmer in and out between pixels and a far fall still reads
  // as a fall (the eye holds a small bright point brighter than its share of the pixel).
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
  // share of the shutter: thinned by the root of its length, not the length. A drop is far
  // thinner than a pixel and still seen: the eye holds a thin bright line, so it is thinned by
  // the root of its widening, never by its length.
  if (drop) {
    out.look = vec4f(fade * min(1.0, 1.6 / sqrt(widen)), 1.0, 1.0, 0.0);
  } else {
    out.look = vec4f(fade / sqrt(widen), sqrt(width / max(len, width)), 0.0, 0.0);
  }
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let kind = in.look.z;
  var a = in.look.x * in.look.y;
  if (kind < 1.5) {
    let across = 1.0 - in.quad.y * in.quad.y;
    let ends = 1.0 - smoothstep(0.6, 1.0, abs(in.quad.x));
    a *= across * ends;
  } else {
    // The crown: a rim of spray round the landing, its sides thrown higher than its middle,
    // thinning as it spreads.
    let u = abs(in.quad.x);
    let v = in.quad.y * 0.5 + 0.5;
    let rim = smoothstep(0.25, 0.75, u) * (1.0 - smoothstep(0.85, 1.0, u));
    let top = mix(0.35, 1.0, rim);
    a *= rim * (1.0 - smoothstep(top * 0.7, top, v)) * 0.8 + (1.0 - v) * 0.25 * (1.0 - u);
  }
  let d = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  let behind = sp.step.y / max(d, 1e-7);
  let here = sp.step.y / max(in.position.z, 1e-7);
  let soft = select(sp.step.z, 0.02, kind > 1.5);
  a *= clamp((behind - here) / max(soft, 1e-3), 0.0, 1.0);
  if (a <= 0.002) {
    discard;
  }
  let y = in.rel.y + frame.eye.y;
  let xz = in.rel.xz + frame.eye.xz;
  let lit = ground_shadow(ground, horizon0, horizon1, linear_clamp, frame.sun.xyz, xz);
  let sun = sun_light(y, xz) * lit;
  let sky = sky_irradiance(vec3f(0.0, 1.0, 0.0));
  // A flake (and a splash's spray) is white ice lit by the sun and the sky. Seen in the near air
  // it scatters toward the eye the light of the whole sky ahead, so against a fog's or a fall's
  // grey veil it is never lost: kept well brighter than the veil.
  var c = max(0.9 / PI * (sun + sky), veil_luminance() * 1.6);
  if (kind > 0.5 && kind < 1.5) {
    // A drop is a lens: it bends most of the sky toward the eye (brighter than the ground it
    // falls past) and catches the sun along it.
    let v = normalize(in.rel);
    let toward = max(dot(v, frame.sun.xyz), 0.0);
    c = max(1.15 / PI * sky + sun * (0.1 + 0.8 * pow(toward, 12.0)) / PI,
            veil_luminance() * 1.25);
  }
  let hazed = haze(c, in.position.xy * frame.size.zw, length(in.rel));
  var out: FragmentOut;
  out.colour = vec4f(hazed * a, a);
  out.motion = vec4f(0.0, 0.0, a, 0.0);
  return out;
}
