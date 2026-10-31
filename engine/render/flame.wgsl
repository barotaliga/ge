// THE FLAMES DRAWN (render/flame.hpp): one quad a flame, facing the eye over its bounding sphere
// (the whole screen when the eye is inside it), and per pixel a march through the gas: a
// teardrop torn by rising turbulence, its temperature hottest on the axis low down, every
// temperature's colour and brightness off the blackbody table. Optically thin: added to the
// picture, the march stopped at the scene's depth. Writes the motion target's reactive channel
// so the resolve does not smear the fire.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/blackbody.wgsl"

struct GpuFlame {
  base: vec4f,   // camera-relative base, m; the height at this level
  shape: vec4f,  // radius; the hottest gas, K; the core's luminance, cd/m^2 (negative: times a
                 // white face's under the scene's light); the seed's phase
  wind: vec4f,   // the tip's offset a metre of height (x, z); the rise, m/s; the blue base
  bound: vec4f,  // the bounding sphere's camera-relative centre and radius
}

@group(1) @binding(0) var<uniform> flame_table: array<vec4f, kBlackbodyRows>;
@group(1) @binding(1) var<storage, read> flames: array<GpuFlame>;
@group(1) @binding(2) var scene_depth: texture_depth_2d;

const kFlameSteps = 28u;
const kFlameFilm = 0.5;  // how fast each primary saturates past white
const kFlameKnee = 0.4;   // the gas shows kFlameGlare x log2(1 + its light / this), in the
const kFlameGlare = 0.38; // picture's units (white is about 1): every doubling a step brighter

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) id: u32,
}

const kFlameCorners = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
                                      vec2f(-1.0, -1.0), vec2f(1.0, 1.0), vec2f(-1.0, 1.0));

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  var out: VertexOut;
  out.id = iid;
  let b = flames[iid].bound;
  let corner = kFlameCorners[vid];
  let dist = length(b.xyz);
  if (dist < b.w * 1.2) {
    // The eye inside (or at) the sphere: the whole screen, every pixel's ray tested.
    out.position = vec4f(corner, 0.5, 1.0);
    return out;
  }
  let to_eye = -b.xyz / dist;
  var side = cross(vec3f(0.0, 1.0, 0.0), to_eye);
  if (dot(side, side) < 1e-6) {
    side = vec3f(1.0, 0.0, 0.0);
  }
  let right = normalize(side);
  let up = cross(to_eye, right);
  // The sphere's silhouette seen in perspective is wider than its radius.
  let r = b.w * 1.05 * dist / sqrt(max(dist * dist - b.w * b.w, 1e-6));
  let p = b.xyz + to_eye * b.w + (right * corner.x + up * corner.y) * r;
  out.position = frame.view_proj * vec4f(p, 1.0);
  return out;
}

fn flame_hash(p: vec3i) -> f32 {
  var h = (u32(p.x) * 0x8da6b343u) ^ (u32(p.y) * 0xd8163841u) ^ (u32(p.z) * 0xcb1ab31fu);
  h ^= h >> 16u;
  h *= 0x7feb352du;
  h ^= h >> 15u;
  h *= 0x846ca68bu;
  h ^= h >> 16u;
  return f32(h) * (1.0 / 4294967295.0);
}

// Smooth 3D value noise, 0..1.
fn flame_noise(p: vec3f) -> f32 {
  let i = vec3i(floor(p));
  let f = fract(p);
  let u = f * f * (3.0 - 2.0 * f);
  let a = mix(flame_hash(i), flame_hash(i + vec3i(1, 0, 0)), u.x);
  let b = mix(flame_hash(i + vec3i(0, 1, 0)), flame_hash(i + vec3i(1, 1, 0)), u.x);
  let c = mix(flame_hash(i + vec3i(0, 0, 1)), flame_hash(i + vec3i(1, 0, 1)), u.x);
  let d = mix(flame_hash(i + vec3i(0, 1, 1)), flame_hash(i + vec3i(1, 1, 1)), u.x);
  return mix(mix(a, b, u.y), mix(c, d, u.y), u.z);
}

fn flame_fbm(p: vec3f) -> f32 {
  return 0.5 * flame_noise(p) + 0.3 * flame_noise(p * 2.03 + vec3f(17.1, 3.3, 9.7)) +
         0.2 * flame_noise(p * 4.11 + vec3f(3.7, 41.2, 27.5));
}

// The blackbody's colour times its brightness relative to the flame's hottest gas.
fn flame_colour(kelvin: f32, hottest: f32) -> vec3f {
  let x = blackbody_coord(kelvin);
  let i = u32(x);
  let y = blackbody_coord(hottest);
  let j = u32(y);
  let top = mix(flame_table[j].w, flame_table[min(j + 1u, 63u)].w, fract(y));
  // A cooling gas dims steeply; the eye's response is gentler than the radiance.
  let c = blackbody_mix(flame_table[i], flame_table[min(i + 1u, 63u)], fract(x), top);
  let rel = clamp(dot(c, vec3f(0.2126, 0.7152, 0.0722)), 1e-12, 1.0);
  return c * pow(rel, -0.55);
}

// The teardrop's radius at height `y` (of the flame's): narrow at the fuel, widest an eighth up,
// a point at the tip.
fn flame_profile(y: f32) -> f32 {
  return 1.05 * sqrt(clamp(y / 0.12, 0.0, 1.0)) * pow(clamp(1.0 - y, 0.0, 1.0), 0.6);
}

// The light the gas gives off per metre of the ray at camera-relative `p`, in cd/m^2 per radius.
fn flame_glow(f: GpuFlame, p: vec3f, t: f32) -> vec3f {
  let h = f.base.w;
  let radius = f.shape.x;
  var q = p - f.base.xyz;
  let y = q.y / h;
  if (y < -0.1 || y > 1.4) {
    return vec3f(0.0);
  }
  // Bent downwind, more toward the tip.
  let bend = q.y * clamp(y, 0.0, 1.3);
  q = vec3f(q.x - f.wind.x * bend, q.y, q.z - f.wind.y * bend);
  // Where no eddy can carry the gas: every term below is zero past m = 1.8 (the blue shell's past
  // 1.35), the profile is at most 1.05 radii and the eddies move a point at most 1.35 amp an axis
  // (1.91 amp across), so a point that far out is skipped before any noise is drawn: the picture
  // is the same, and most of a flame's bounding sphere is air.
  let amp = radius * (0.3 + 2.2 * clamp(y, 0.0, 1.2) * clamp(y, 0.0, 1.2));
  let reach = 1.8 * (1.05 * radius + 1e-5);
  if (length(q.xz) - 1.92 * amp >= reach) {
    return vec3f(0.0);
  }
  // The turbulence, carried up with the gas, in cells of a few radii.
  let cell = radius * 2.5;
  let phase = f.shape.w;
  let s = vec3f(q.x, q.y - t * f.wind.z, q.z) / cell + vec3f(phase, phase * 0.37, phase * 0.71);
  // Two scales of eddy: the big sway and a finer churn that grows toward the tip.
  let n1 = flame_fbm(s * 0.8) + 0.35 * (flame_noise(s * 3.1 + vec3f(5.0, 1.0, 9.0)) - 0.5);
  let n2 = flame_fbm(s * 0.8 + vec3f(31.7, 5.1, 12.9)) +
           0.35 * (flame_noise(s * 3.1 + vec3f(41.0, 8.0, 2.0)) - 0.5);
  let xz = q.xz + (vec2f(n1, n2) - 0.5) * 2.0 * amp;
  // The same test with the sway known, before the tip's lick and the soot's streaks are drawn.
  if (length(xz) >= reach) {
    return vec3f(0.0);
  }
  let n3 = flame_fbm(s * 1.6 + vec3f(11.3, 23.9, 2.1));
  // The tip licks up and falls back: the profile read at a height the noise lifts or lowers.
  let ye = y - (n3 - 0.5) * 0.45 * clamp(y * 1.5, 0.0, 1.0);
  let m = length(xz) / (radius * flame_profile(ye) + 1e-5);
  // The gas thins out past its body smoothly (a Gaussian in its radius), so its edge fades over
  // decades of light rather than ending at a line; the body's own share sets its temperature.
  let body = clamp(1.0 - m, 0.0, 1.0);
  // High up the tongues tear loose: the gas is cut where the noise falls under the height.
  let cut = smoothstep(0.0, 0.25, n3 * 1.3 - ye * 0.9 + 0.35);
  var glow = vec3f(0.0);
  if (m < 1.8 && cut > 0.0) {
    let kelvin = f.shape.y * (0.62 + 0.38 * sqrt(body)) * (1.0 - 0.3 * clamp(ye, 0.0, 1.3));
    // The soot glows in streaks drawn up by the flow: noise stretched along the rise.
    let streak = flame_noise(vec3f(s.x * 3.0, s.y * 1.1, s.z * 3.0) + vec3f(7.0, 0.0, 3.0));
    glow = flame_colour(kelvin, f.shape.y) * (exp(-3.5 * m * m) * cut * (0.25 + 1.5 * streak));
  }
  // The blue base: a thin shell round the bottom of a candle's or a gas flame's body.
  if (f.wind.w > 0.0) {
    let shell = smoothstep(0.35, 0.85, m) * (1.0 - smoothstep(0.95, 1.35, m));
    let low = 1.0 - smoothstep(0.0, 0.25, ye);
    glow += vec3f(0.10, 0.22, 1.0) * (0.35 * f.wind.w * shell * low * smoothstep(-0.05, 0.02, y));
  }
  return glow;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // b only: the reactive mask
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let f = flames[in.id];
  let uv = in.position.xy * frame.size.zw;
  let d = view_ray(frame, uv);
  // The ray against the bounding sphere.
  let c = f.bound.xyz;
  let b = dot(d, c);
  let disc = b * b - (dot(c, c) - f.bound.w * f.bound.w);
  if (disc <= 0.0) {
    discard;
  }
  let root = sqrt(disc);
  let t0 = max(b - root, 0.0);
  var t1 = b + root;
  // Stopped at what is in front: the scene's depth along this ray.
  let z = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  if (z > 0.0) {
    let ndc = vec4f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, z, 1.0);
    let w = frame.inv_view_proj * ndc;
    t1 = min(t1, length(w.xyz / w.w));
  }
  if (t1 <= t0) {
    discard;
  }
  let ds = (t1 - t0) / f32(kFlameSteps);
  // A per-pixel, per-frame offset into the first step: the resolve averages the steps' banding.
  let jitter = flame_hash(vec3i(vec2i(in.position.xy), i32(frame.time.y) & 255));
  let t = frame.time.x;
  var sum = vec3f(0.0);
  for (var k = 0u; k < kFlameSteps; k++) {
    let p = d * (t0 + (f32(k) + jitter) * ds);
    sum += flame_glow(f, p, t);
  }
  // Through the core's width the sum is about the core's luminance (a negative: that many white
  // faces' under the scene's own light, taken to cd/m^2).
  var core = f.shape.z;
  if (core < 0.0) {
    core = -core * white_luminance() * kSunLux / frame.world.y;
  }
  let nits = core * sum * ds / (1.2 * f.shape.x);
  // The eye's local adaptation to a bright source: the gas is shown on a log scale of its light
  // (its hue kept), so a core a thousand times the room reads yellow-white and its cooler rim and
  // tongues orange and red rather than all of it clipping to one white.
  let e = exposure();
  let shown = nits * (frame.world.y / kSunLux) * e;
  let peak = max(shown.r, max(shown.g, shown.b));
  let squeezed = kFlameGlare * log2(1.0 + peak / kFlameKnee);
  // And, as an eye or a film does with a source far past white, each primary saturates on its own:
  // the hottest gas runs from orange through yellow to white, its cooler rim stays orange.
  let film = 1.0 - exp(-shown * kFlameFilm);
  let hue = film / max(max(film.r, max(film.g, film.b)), 1e-6);
  let light = hue * squeezed / e;
  let lum = dot(light, vec3f(0.2126, 0.7152, 0.0722));
  if (lum <= 0.0) {
    discard;
  }
  var out: FragmentOut;
  out.colour = vec4f(light, 0.0);
  out.motion = vec4f(0.0, 0.0, clamp(lum * 4.0, 0.0, 1.0), 0.0);
  return out;
}
