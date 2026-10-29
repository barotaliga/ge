// THE EXPLOSIONS DRAWN (render/explosions.hpp): one quad round each staged blast's ball on the
// screen (the whole screen when the eye is inside it); a pixel's ray cut by the ball and by the
// scene's depth and marched front to back through hot gas that glows by its temperature and
// smoke that the sun, the sky and the fire light. A muzzle's flame is the same ball stretched
// along the barrel and carved into prongs round it. Premultiplied over the picture; the motion
// target's reactive channel only.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/blackbody.wgsl"

struct Blast {
  centre: vec4f,  // camera-relative, m; the fireball's radius
  state: vec4f,   // the core's temperature, K; the fire share; the smoke; the age, s
  look: vec4f,    // the noise's offset; the bound's radius, m
  more: vec4f,    // 1 when the eye is inside the bound; the prongs' depth; the glow
  shape: vec4f,   // the look's billows; fray; the noise's tiles per radius; roll, radii/s
  soot: vec4f,    // the smoke's single-scattering albedo times its tint
  axis: vec4f,    // the stretch's axis (unit); the stretch (1: a ball)
  side: vec4f,    // the first prong's way (unit, square to the axis); the prongs (0: none)
}

struct ExplosionParams {
  info: vec4f,  // the near plane, m; the steps; the table's last row's luminance, cd/m^2; count
  table: array<vec4f, 64>,
}

@group(1) @binding(0) var<uniform> xp: ExplosionParams;
@group(1) @binding(1) var<storage, read> blasts: array<Blast>;
@group(1) @binding(2) var scene_depth: texture_depth_2d;

override kSteps: u32 = 10u;

// How thick the gas is: a full ball is about 2 kExtinction optical depths across. The fray's
// noise is kFine times finer than the billows'.
const kExtinction = 4.0;
const kFine = 3.0;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) blast: u32,
}

const kCorners = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
                                 vec2f(-1.0, -1.0), vec2f(1.0, 1.0), vec2f(-1.0, 1.0));

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> VertexOut {
  var out: VertexOut;
  out.blast = iid;
  let b = blasts[iid];
  let c = kCorners[vid];
  if (b.more.x > 0.5) {
    out.position = vec4f(c, 0.5, 1.0);
    return out;
  }
  let centre = b.centre.xyz;
  let d = length(centre);
  let to_eye = -centre / d;
  var side = cross(vec3f(0.0, 1.0, 0.0), to_eye);
  if (dot(side, side) < 1e-6) {
    side = cross(vec3f(1.0, 0.0, 0.0), to_eye);
  }
  side = normalize(side);
  let up = cross(to_eye, side);
  // The cone from the eye that just holds the ball, cut at its centre.
  let r = b.look.w;
  let half = r * d / sqrt(max(d * d - r * r, 1e-6));
  out.position = frame.view_proj * vec4f(centre + (side * c.x + up * c.y) * half, 1.0);
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // b only: the reactive mask
}

// A per-pixel offset in 0..1 that differs frame to frame (interleaved gradient noise).
fn jitter(px: vec2f) -> f32 {
  let p = px + vec2f(5.588238, 5.588238) * f32(u32(frame.time.y) % 64u);
  return fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715))));
}

// The light of gas at `kelvin`, in the frame's units.
fn fire_light(kelvin: f32) -> vec3f {
  let x = blackbody_coord(kelvin);
  let i = u32(x);
  let lo = xp.table[i];
  let hi = xp.table[min(i + 1u, kBlackbodyRows - 1u)];
  return blackbody_mix(lo, hi, x - f32(i), 1.0) * xp.info.z * frame.world.y / kSunLux;
}

// A vector with its part along `axis` shrunk by `stretch`.
fn squeeze(v: vec3f, axis: vec3f, stretch: f32) -> vec3f {
  let along = dot(v, axis);
  return v - along * axis + axis * (along / stretch);
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let b = blasts[in.blast];
  let uv = in.position.xy * frame.size.zw;
  let ray = view_ray(frame, uv);
  let centre = b.centre.xyz;
  let bound = b.look.w;
  // The ball along the ray, squeezed along its axis by the stretch (a flame's bound is the
  // ellipsoid round it, so the march spends its steps on the flame, not the air beside it).
  let axis = b.axis.xyz;
  let stretch = max(b.axis.w, 1.0);
  let o2 = squeeze(-centre, axis, stretch);
  let d2 = squeeze(ray, axis, stretch);
  let held = bound / stretch;
  let qa = dot(d2, d2);
  let qb = dot(o2, d2);
  let disc = qb * qb - qa * (dot(o2, o2) - held * held);
  if (disc <= 0.0) {
    discard;
  }
  let s = sqrt(disc);
  let near = xp.info.x;
  let t0 = max((-qb - s) / qa, near);
  var t1 = (-qb + s) / qa;
  // The scene in front of it.
  let dz = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  if (dz > 0.0) {
    let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    let p = frame.inv_view_proj * vec4f(ndc, dz, 1.0);
    t1 = min(t1, length(p.xyz / p.w));
  }
  if (t1 <= t0) {
    discard;
  }

  let radius = b.centre.w;
  let inv = 1.0 / radius;
  let core_k = b.state.x;
  let fire = b.state.y;
  let smoke = b.state.z;
  let age = b.state.w;
  let presence = fire + 0.7 * smoke;
  let flow = b.look.xyz + vec3f(0.0, -age * b.shape.w * b.shape.z, 0.0);
  let prongs = b.side.w;
  let side_a = b.side.xyz;
  let side_b = cross(axis, side_a);
  let dt = (t1 - t0) / f32(kSteps);
  let sun = frame.sun.xyz;
  let world_c = centre + frame.eye.xyz;
  // The light the smoke gets: the sun at the ball's height, the sky's from above, the fire's
  // from inside it (the core's glow, by how much still burns).
  let sun_in = sun_light(world_c.y, world_c.xz);
  let sky_in = sky_irradiance(vec3f(0.0, 1.0, 0.0));
  let fire_in = fire_light(mix(kBlackbodyLoK, core_k, 0.85)) * fire * 0.35;

  var trans = 1.0;
  var light = vec3f(0.0);
  var t = t0 + dt * jitter(in.position.xy);
  for (var i = 0u; i < kSteps; i++) {
    let q = (ray * t - centre) * inv;
    // Squeezed along the axis by the stretch (a flame's length over its width; 1 for a ball).
    let along = dot(q, axis);
    let across = q - along * axis;
    let qs = across + axis * (along / stretch);
    let r = length(qs);
    // Two reads of the noise: the big billows, and a finer fray turned across them.
    let n = textureSampleLevel(cloud_noise, linear_repeat, qs * b.shape.z + flow, 0.0).rg;
    let m = textureSampleLevel(cloud_noise, linear_repeat, qs.zxy * (b.shape.z * kFine) -
                               flow * 1.7, 0.0).r;
    // The ball's edge carved into billows: the cells' tops bulge, the fbm frays them.
    var shape = 1.0 - r + b.shape.x * (n.g - 0.5) + b.shape.y * (1.4 * (n.r - 0.5) + (m - 0.5));
    // A flame's prongs: petals round the axis, cut deeper away from it.
    if (prongs > 0.5) {
      let phi = atan2(dot(across, side_b), dot(across, side_a));
      let petal = 0.5 + 0.5 * cos(prongs * phi);
      shape -= b.more.y * (1.0 - petal) * smoothstep(0.05, 0.5, length(across));
    }
    let density = clamp(shape * 2.5, 0.0, 1.0) * presence;
    if (density > 0.0) {
      let sigma = density * kExtinction * inv;
      let a = 1.0 - exp(-sigma * dt);
      // Hot deep inside and while it burns, cooler toward the billows' tips.
      // (A flame is hotter at its base, by the muzzle.)
      let base = 1.0 - 0.45 * clamp(along / stretch, 0.0, 1.0) * step(1.001, stretch);
      let heat = clamp(shape * 2.2 + 0.9 * (n.r - 0.5) + 0.6 * (m - 0.5), 0.0, 1.0) * fire * base;
      let kelvin = kBlackbodyLoK + (core_k - kBlackbodyLoK) * heat;
      let burning = fire / max(presence, 1e-4);
      let sunlit = mix(0.3, 1.0, clamp(0.5 + 0.5 * dot(q, sun) / max(r, 1e-3), 0.0, 1.0));
      let scatter = b.soot.rgb / PI * (sun_in * sunlit + sky_in) +
                    b.soot.rgb * fire_in * clamp(1.0 - r, 0.0, 1.0);
      let source = fire_light(kelvin) * b.more.z * burning * heat + scatter * (1.0 - burning * heat);
      light += trans * a * source;
      trans *= 1.0 - a;
      if (trans < 0.01) {
        break;
      }
    }
    t += dt;
  }
  let alpha = 1.0 - trans;
  if (alpha <= 0.002) {
    discard;
  }
  let hazed = haze(light / alpha, uv, length(centre)) * alpha;
  var out: FragmentOut;
  out.colour = vec4f(hazed, alpha);
  out.motion = vec4f(0.0, 0.0, alpha, 0.0);
  return out;
}
