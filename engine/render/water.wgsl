// THE WATER'S SURFACE FROM ABOVE (render/water.hpp; docs/water.md § Drawn): `fs_main`, each pixel's
// light over the shared half (render/wgsl/water_surface.wgsl: the geometry, the waves, the wakes,
// the rain's rings, the gusts and the stir, the planar mirror's read), and the caustics on the bed
// seen through it (render/wgsl/water_body.wgsl's `caustic_at`).
#include "render/wgsl/water_surface.wgsl"

const kFoamTaps = 10;     // moments looked back at
const kFoamStep = 0.3;    // s between them
const kFoamLife = 2.0;    // s: a patch's share falls by e in this
const kFoamEdge = 0.6;    // the Jacobian's span either side of the fold's threshold, in its spreads
const kSurfFrom = 0.15;     // m: the least Hs that surfs white
const kWetDark = 0.3;       // the share of its light wet ground loses (render/water_shore.wgsl's)
const kFoamShortest = 1.5;  // m: the shortest wave that folds (render/water.hpp's kFoamShortest)

// The waves' fold and height at `xz` at `t`: the Jacobian of the surface's horizontal motion (the
// choppy displacement x' = x + (k / |k|) a cos(phi) of the laid components and the detail band's
// down to kFoamShortest m), J = 1 where the water
// neither gathers nor spreads, under 0 where the surface folds over itself; and the height.
fn fold_at(xz: vec2f, t: f32) -> vec2f {
  var jxx = 0.0;
  var jxz = 0.0;
  var jzz = 0.0;
  var h = 0.0;
  let n = u32(body.info.z + body.info.w);
  for (var i = 0u; i < n; i++) {
    let w = waves[i];
    let k = w.more.y;
    let keep = smoothstep(kFoamShortest, 2.0 * kFoamShortest, 6.2831853 / k);
    if (keep <= 0.0) {
      continue;
    }
    let a = keep * w.dir.z * body.info.y;
    let phi = dot(w.dir.xy, xz) - w.dir.w * t + w.more.x;
    let s = sin(phi);
    let c = cos(phi);
    let q = a * s / k;
    jxx += w.dir.x * w.dir.x * q;
    jxz += w.dir.x * w.dir.y * q;
    jzz += w.dir.y * w.dir.y * q;
    h += a * s - 0.5 * k * a * a * (c * c - s * s);
  }
  return vec2f((1.0 - jxx) * (1.0 - jzz) - jxz * jxz, h);
}

// The surf's share where a wave `h` m high stands over `depth` m of water (water/waves.cpp's
// `breaking_at`: past 0.78 of the depth, eased in from 0.6 of that).
fn surf_share(h: f32, depth: f32) -> f32 {
  if (h <= 0.0) {
    return 0.0;
  }
  return smoothstep(0.0, 1.0, 2.0 * h / (0.78 * max(depth, 1e-3)) - 0.6);
}

// THE FOAM's share at a pixel's point, 0..1 (render/water.hpp's `foam_of`): the whitecaps where
// the surface folds and the surf where a wave stands too high for the depth, each kept as it
// fades over the last few seconds while it drifts downwind: the waves' own past worked out again
// at the places the drift brought the water from, so a crest leaves a trail, the same on its first
// frame as on its thousandth. `depth` is the water's under the point (m; huge where unknown); a
// pixel wider than a sixteenth of the folds' wavelength takes the cover's mean, and the surf now.
fn foam_at(xz: vec2f, t: f32, foot: f32, depth: f32) -> f32 {
  let hs = body.surf.x;
  let caps = body.foam.y > 0.0;
  // Wavelets under kSurfFrom m lap a shore without breaking white.
  let surfs = hs > kSurfFrom && depth < 4.0 * hs;
  if (!caps && !surfs) {
    return 0.0;
  }
  let drift = body.foam.zw;
  let far = foot * 16.0 > body.surf.z;
  var f = select(0.0, body.foam.y, far);
  let taps = select(kFoamTaps, 1, far);
  for (var j = 0; j < taps; j++) {
    let tau = f32(j) * kFoamStep;
    let fh = fold_at(xz - drift * tau, t - tau);
    var share = 0.0;
    if (caps && !far) {
      let edge = kFoamEdge * body.surf.w;
      share = smoothstep(body.foam.x + edge, body.foam.x - edge, fh.x);
    }
    if (surfs) {
      share = max(share, surf_share(fh.y, depth) * smoothstep(kSurfFrom, 2.5 * kSurfFrom, hs));
    }
    f = max(f, share * exp(-tau / kFoamLife));
  }
  return f;
}

// The foam's LACE: its share `f` cut into a pattern carried with the drift, so a patch is threads
// and holes that open as it fades, never a white sheet; a pixel too wide for the threads takes
// the share's mean.
fn foam_lace(xz: vec2f, t: f32, f: f32, foot: f32) -> f32 {
  if (f <= 0.0) {
    return 0.0;
  }
  // Ridges of noise at two sizes: threads that meet round holes, as the bubbles' film drains.
  let p = xz - body.foam.zw * t;
  let r1 = 1.0 - abs(2.0 * value_noise(vec3f(p * 0.9, t * 0.05)).x - 1.0);
  let r2 = 1.0 - abs(2.0 * value_noise(vec3f(p * 3.1 + 7.1, t * 0.09)).x - 1.0);
  let n = 0.6 * r1 * r1 + 0.4 * r2;
  let lace = smoothstep(1.0 - f, 1.0 - f + 0.15, n);
  return mix(lace, f, smoothstep(0.08, 0.5, foot));
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  // The pixel's footprint on the water, m (before anything can discard).
  let foot = max(length(dpdx(in.xz)), length(dpdy(in.xz)));
  if (any(in.xz < body.box.xy) || any(in.xz > body.box.zw)) {
    discard;
  }
  // Inside the shore's mask (water.hpp's `shore_mask`): a lake's box beyond its shore is dry.
  let cells = vec2f(textureDimensions(shore));
  let on_box = (in.xz - body.box.xy) / max(body.box.zw - body.box.xy, vec2f(1e-3));
  if (textureLoad(shore, vec2i(clamp(on_box * cells, vec2f(0.0), cells - 1.0)), 0).r < 0.5) {
    discard;
  }
  let t = frame.time.x;
  let sl = surface_slope(in.xz, t, foot);
  let lost = sl.z;
  let n = normalize(vec3f(-sl.x, 1.0, -sl.y));
  let dist = length(in.rel);
  let uv = in.position.xy * frame.size.zw;
  // The planar mirror where the game drew one: the reflected ray read where its image lands, the
  // sky's model where the mirror shows the sky, and the sun's glitter only there.
  var seen = vec4f(0.0);
  var sun_seen = 1.0;
  if (body.view.x > 0.5 && body.view.x < 1.5) {
    let v = -in.rel / dist;
    let r0 = reflect(-v, n);
    let r = normalize(vec3f(r0.x, max(r0.y, 0.01), r0.z));
    var leg = 0.0;
    let m = mirror_along(in.rel, r, uv, dist, &leg);
    if (m.a >= 0.0) {
      seen = vec4f(mix(sky_luminance(r), m.rgb, m.a), 1.0);
      sun_seen = 1.0 - m.a;
    } else {
      // Off the mirror's picture: the room's walls, lit as the room is; no sun.
      seen = vec4f(vec3f(white_luminance() * body.emission.w * kRoomAlbedo), 1.0);
      sun_seen = 0.0;
    }
  }
  // A body under a roof (a cave's pool) sees the sky through `open` of its dome (rough.w) and the
  // room's own light, a share of a white face's (emission.w), over the rest; the day lights the
  // water itself by its own share (air.x).
  // THE CAUSTICS on what is seen under the surface (its bed, a rock): the sun's part of its light
  // gathered and spread by the waves over it, as far as the sun reaches the water.
  var bed_light = 1.0;
  let behind = scene_rel(vec2i(in.position.xy), uv);
  let bed_depth = body.info.x - (behind.y + frame.eye.y);
  if (behind.w > 0.5 && bed_depth > 0.0 && body.sun.x > 0.0) {
    let at = behind.xyz + frame.eye.xyz;
    let share = sun_share(body.info.x, in.xz) * body.sun.x * exp(-body.kd.g * bed_depth);
    // The pixel's footprint on the bed: the surface's, grown over the way on down to the bed.
    let bed_foot = foot * length(behind.xyz) / max(dist, 0.1);
    bed_light = 1.0 + (caustic_at(at.xz, bed_depth, sun_down(), t, 1.0, bed_foot) - 1.0) * share;
  }
  // The bed is as wet as the shore's band where the water shoals out onto it, so the waterline shows
  // no dry seam between the band and the water (the copy was taken before the band darkened it).
  if (behind.w > 0.5 && body.surf.y >= 0.0) {
    bed_light *= 1.0 - kWetDark * (1.0 - smoothstep(0.05, 0.3, bed_depth));
  }
  // THE FOAM: the whitecaps and the surf (their trails with them), and a line of it where the water
  // laps the shore, drawn as lace.
  let depth = select(1e6, bed_depth, behind.w > 0.5);
  var foam = foam_at(in.xz, t, foot, depth);
  let hs = body.surf.x;
  if (hs > kSurfFrom && behind.w > 0.5) {
    foam = max(foam, (1.0 - smoothstep(0.0, 0.02 + 0.3 * hs, bed_depth)) *
                         smoothstep(kSurfFrom, 2.5 * kSurfFrom, hs));
  }
  foam = foam_lace(in.xz, t, foam, foot);
  let c = shade_water_lit(n, in.rel, in.xz, in.position.xyz, body.info.x, lost, body.rough.z,
                          Optics(body.c.rgb, body.kd.rgb, body.deep.rgb, body.emission.rgb), foam,
                          body.rough.w, body.emission.w, body.air.x, seen, sun_seen, bed_light);
  var out: FragmentOut;
  out.colour = vec4f(haze(c, uv, dist), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}

