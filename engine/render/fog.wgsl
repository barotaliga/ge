// THE FOG IN AN AREA (render/fog.hpp): "fog.inject" lights the medium in every froxel, blended
// with the last frame's light at the same world point; "fog.integrate" gathers each column front
// to back into the volume frame.wgsl's `local_fog` reads.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/terrain_common.wgsl"
#include "render/wgsl/sun_shadow.wgsl"

// render/fog.cpp's Params.
struct FogParams {
  grid: vec4u,  // froxels across, down, slices; the volumes in reach
  look: vec4f,  // range (m), the history's share (0: none), this frame's depth jitter (0..1),
                // 1 with the ground's heights and horizon (0: no ground)
}

// render/fog.cpp's Volume: one FogVolume, camera-relative.
struct FogVol {
  centre: vec4f,  // camera-relative centre, m; w: 0 a box, 1 an ellipsoid, + 2 hugging the ground
  half: vec4f,    // half its size along its own axes, m; w: the soft edge, m
  turn: vec4f,    // cos, sin of its yaw; its floor's extinction (1/m); the falloff (1/m)
  albedo: vec4f,  // the scattered share; w: the phase's asymmetry
  glow: vec4f,    // its own light, cd/m2 a metre of extinction; w: the noise's share
  drift: vec4f,   // the noise's offset, tiles; w: one over its tile (1/m)
}

@group(1) @binding(0) var<uniform> fp: FogParams;
@group(1) @binding(1) var<storage, read> vols: array<FogVol>;
@group(1) @binding(2) var history: texture_3d<f32>;
@group(1) @binding(3) var media_out: texture_storage_3d<rgba16float, write>;
@group(2) @binding(0) var<uniform> ground: TerrainParams;
@group(2) @binding(1) var horizon0: texture_2d<f32>;
@group(2) @binding(2) var horizon1: texture_2d<f32>;
@group(2) @binding(3) var heights: texture_2d<f32>;
@group(3) @binding(0) var<uniform> shadow: SunShadow;
@group(3) @binding(1) var shadow_maps: texture_depth_2d_array;
@group(3) @binding(2) var shadow_cmp: sampler_comparison;

// The medium at a camera-relative point, summed over the volumes: the extinction (1/m), the
// scattering (rgb, 1/m), its own light (rgb, the frame's units a metre) and the scattering's
// mean asymmetry.
struct FogMedium {
  extinction: f32,
  scatter: vec3f,
  glow: vec3f,
  g: f32,
}

// How high a camera-relative point stands over the ground under it, m (with no ground, over y = 0
// of the world, which no volume hugging the ground uses).
fn over_ground(rel: vec3f) -> f32 {
  let world = rel + frame.eye.xyz;
  if (fp.look.w < 0.5) {
    return world.y;
  }
  return world.y - field_height(heights, ground, world.xz);
}

// One volume's extinction at a camera-relative point `above` m over the ground. A volume hugging
// the ground stands on it: its own height is measured up from the ground under the point, so a
// mist lies the same depth over a slope as over a flat.
fn volume_density(v: FogVol, rel: vec3f, above: f32, noisy: bool) -> f32 {
  let o = rel - v.centre.xyz;
  let h = v.half.xyz;
  let hug = v.centre.w > 1.5;
  // Into its own frame: x its right, z its heading (heading 0 = +z, clockwise from above).
  let q = vec3f(o.x * v.turn.x - o.z * v.turn.y, select(o.y, above - h.y, hug),
                o.x * v.turn.y + o.z * v.turn.x);
  var inside: f32;
  if (v.centre.w - select(0.0, 2.0, hug) < 0.5) {
    let e = h - abs(q);
    inside = min(e.x, min(e.y, e.z));
  } else {
    inside = (1.0 - length(q / h)) * min(h.x, min(h.y, h.z));
  }
  if (inside <= 0.0) {
    return 0.0;
  }
  let w = clamp(inside / max(v.half.w, 1e-3), 0.0, 1.0);
  var d = v.turn.z * exp(-v.turn.w * (q.y + h.y)) * w * w * (3.0 - 2.0 * w);
  if (!noisy) {
    // The noise's mean share kept: what a coarse march toward the light sees.
    return d * (1.0 - 0.5 * v.glow.w);
  }
  if (v.glow.w > 0.0) {
    // Two octaves of the cloud's tiled noise, carried on the wind, carve the medium into wisps.
    let uvw = q * v.drift.w + v.drift.xyz;
    let n = 0.65 * textureSampleLevel(cloud_noise, linear_repeat, uvw, 0.0).r +
            0.35 * textureSampleLevel(cloud_noise, linear_repeat, uvw * 2.73 + 0.31, 0.0).r;
    d *= mix(1.0, smoothstep(0.32, 0.68, n), v.glow.w);
  }
  return d;
}

fn fog_medium_at(rel: vec3f, above: f32) -> FogMedium {
  var m = FogMedium(0.0, vec3f(0.0), vec3f(0.0), 0.0);
  var gw = 0.0;
  for (var i = 0u; i < fp.grid.w; i++) {
    let v = vols[i];
    let d = volume_density(v, rel, above, true);
    if (d <= 0.0) {
      continue;
    }
    m.extinction += d;
    m.scatter += d * v.albedo.rgb;
    m.glow += d * v.glow.rgb;
    m.g += d * v.albedo.w;
    gw += d;
  }
  m.g = m.g / max(gw, 1e-9);
  return m;
}

// The medium's optical depth from `rel` along `toward` (unit): six steps doubling from a metre,
// 63 m in all, the noise's mean in place of the noise. Toward the key light it is a fog's own
// shadow: the floor of a deep bank under its lit top, the far side of a mist from the sun.
fn depth_along(rel: vec3f, toward: vec3f) -> f32 {
  var tau = 0.0;
  var t = 0.0;
  var step = 1.0;
  for (var k = 0; k < 6; k++) {
    let p = rel + toward * (t + 0.5 * step);
    let above = over_ground(p);
    for (var i = 0u; i < fp.grid.w; i++) {
      tau += volume_density(vols[i], p, above, false) * step;
    }
    t += step;
    step *= 2.0;
  }
  return tau;
}

// THE KEY LIGHT SCATTERED toward the eye, a unit of extinction (render/clouds.wgsl's model):
// the first scattering, dimmed by the medium toward the light (`tau`), on the volume's lobe; and
// the orders past it on one gentle lobe, as a diffusion that falls as one over the reduced depth
// (most of it goes on forward, so it dies at kFogReduced of the extinction) rather than
// exponentially, reaching the point the short way: through the medium toward the light or, a
// sheet lit from its whole top as a deck is, down through the medium over it (`tau_up`, over the
// light's height's sine). Unlike the cloud, a fog may be thin: the orders are counted only as far
// as the medium round the point is thick enough to bounce the light again, so a thin dust
// scatters once and a deep bank glows white through its lit top.
const kFogOrders = 3.0;
const kFogReduced = 0.15;

fn key_scatter(c: f32, g: f32, tau: f32, tau_up: f32, sun_y: f32) -> f32 {
  let first = exp(-tau) * henyey_greenstein(c, g);
  let thick = 1.0 - exp(-max(tau, tau_up));
  let tau_ms = min(tau, tau_up / max(sun_y, 0.4));
  let orders = kFogOrders * henyey_greenstein(c, 0.2) * thick / (1.0 + kFogReduced * tau_ms);
  return first + orders;
}

const kHorizonReach = 400.0;

fn henyey_greenstein(c: f32, g: f32) -> f32 {
  let k = 1.0 + g * g - 2.0 * g * c;
  return (1.0 - g * g) / (4.0 * PI * k * sqrt(k));
}

// THE INJECT: one froxel's lit medium, the light it scatters toward the eye a metre (rgb) and its
// extinction (a).
@compute @workgroup_size(4, 4, 4)
fn inject(@builtin(global_invocation_id) id: vec3u) {
  if (any(id >= fp.grid.xyz)) {
    return;
  }
  let size = vec3f(fp.grid.xyz);
  let uv = (vec2f(id.xy) + 0.5) / size.xy;
  let s = (f32(id.z) + fp.look.z) / size.z;
  let dist = fp.look.x * s * s;
  let d = view_ray(frame, uv);
  let rel = d * dist;
  let above = over_ground(rel);
  let m = fog_medium_at(rel, above);
  var lit = vec4f(0.0);
  if (m.extinction > 0.0) {
    let world = rel + frame.eye.xyz;
    var key = sun_light(world.y, world.xz);
    if (frame.sun.y > 0.0) {
      // The ground's horizon is a point's on the snow; one standing over it sees the sun over a
      // horizon lower by its height over a ridge kHorizonReach off.
      let flat = length(frame.sun.xz);
      let raised = vec3f(frame.sun.x, frame.sun.y + max(above, 0.0) / kHorizonReach * flat,
                         frame.sun.z);
      key *= ground_shadow(ground, horizon0, horizon1, linear_clamp, raised, world.xz) *
             sun_shadow(shadow, shadow_maps, shadow_cmp, rel, vec3f(0.0),
                        vec2f(id.xy) + vec2f(f32(id.z) * 7.0), frame.time.y);
    } else {
      key = vec3f(0.0);
    }
    // The sky's light: its irradiance on a level face as an even radiance over the upper half,
    // scattered evenly (E / 2 pi).
    let sky = sky_irradiance(vec3f(0.0, 1.0, 0.0)) / (2.0 * PI);
    let phase = key_scatter(dot(d, frame.sun.xyz), m.g, depth_along(rel, frame.sun.xyz),
                            depth_along(rel, vec3f(0.0, 1.0, 0.0)), frame.sun.y);
    let glow = m.glow * (frame.world.y / kSunLux);
    lit = vec4f(m.scatter * (key * phase + sky) + glow, m.extinction);
  }
  // The last frame's light at the same world point, where that point was in its volume.
  if (fp.look.y > 0.0) {
    let back = rel + frame.eye_delta.xyz;
    let clip = frame.prev_view_proj * vec4f(back, 1.0);
    if (clip.w > 0.0) {
      let puv = vec2f(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
      let pw = sqrt(length(back) / fp.look.x);
      if (all(puv >= vec2f(0.0)) && all(puv <= vec2f(1.0)) && pw <= 1.0) {
        let last = textureSampleLevel(history, linear_clamp, vec3f(puv, pw), 0.0);
        lit = mix(lit, last, fp.look.y);
      }
    }
  }
  textureStore(media_out, id, lit);
}
