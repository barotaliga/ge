// THE SNOW'S MATERIAL (mountain/app/snow.hpp): the terrain's `fs_main` for a snow mountain, over
// render/wgsl/terrain_draw.wgsl. Every term is light: nothing here moves the ground.
//
// - THE GROUND MAP (group 3, `snow_ground`): the packed share (the groomed piste), the rock share
//   (too steep to hold snow) and the piste's direction as the doubled angle (a comb line has no
//   front and back), faded to nought off the piste.
// - THE SNOW LIGHT MODEL (render/wgsl/snow_light.wgsl, the engine's for any material): the
//   micro-relief bent into the baked normal as a slope, the rock its bare share; Lambert, the
//   wrap's blue terminator, the glare (a harder streak on the packed piste), the wet sheen and the
//   glitter, marked reactive so TAA does not smear it.
// - THE MOUNTAIN'S OWN, over it: on the piste the groomer's CORDUROY across its direction (a comb
//   14 cm a rib, worn in patches) and the machine's LANES a swath wide, each a shade of its own,
//   faded by the pixel's own footprint before they alias.
// - THE TRACKS (group 3, the trail maps of trails.hpp): every groove and berm a skier has left,
//   bent into the normal as a slope taken across the pixel's own footprint (so a groove narrower
//   than a pixel fades instead of shimmering), the sky's light dimmed in a deep trough, and the
//   pressed floor of a track smooth: no grain, no glitter, no comb. The fine window where it
//   reaches, the coarse map beyond, blended over the window's outer rim.

#include "render/wgsl/terrain_draw.wgsl"
#include "render/wgsl/snow_light.wgsl"

struct SnowParams {
  look: vec4f,   // glitter, corduroy, relief, wrap (each a gain; 1 is the tuned look)
  look2: vec4f,  // glare, lanes, tracks, 0
}

struct TrailRect {
  x0: f32,
  z0: f32,
  span: f32,
  texel: f32,
}

@group(3) @binding(0) var<uniform> snow: SnowParams;
@group(3) @binding(1) var snow_ground: texture_2d<f32>;
@group(3) @binding(2) var trail_fine: texture_2d<f32>;
@group(3) @binding(3) var trail_coarse: texture_2d<f32>;
@group(3) @binding(4) var<uniform> fine_rect: TrailRect;
@group(3) @binding(5) var<uniform> coarse_rect: TrailRect;

const kTrailDepth = 1.0;  // trails.hpp's kTrailDepth
const kTrailBerm = 0.15;  // kTrailBerm

// What the tracks leave at a point: the groove's depth and the berm's height (m), the pressed
// share, and the surface's slope (dh/dx, dh/dz) they make.
struct Track {
  depth: f32,
  berm: f32,
  pressed: f32,
  slope: vec2f,
}

// A berm stands only beside a groove, never in one: the ring round a stamp's end crosses the next
// stamp's groove, and the maps keep the two channels' maxima apart.
fn trail_texel(map: texture_2d<f32>, r: TrailRect, p: vec2f) -> vec3f {
  let v = textureSampleLevel(map, linear_clamp, (p - vec2f(r.x0, r.z0)) / r.span, 0.0);
  let depth = v.r * v.r * kTrailDepth;
  return vec3f(depth, v.g * v.g * kTrailBerm * (1.0 - smoothstep(0.0, 0.004, depth)), v.b);
}

// One map at p: the centre, and the slope across the larger of a texel and the pixel's footprint.
fn trail_of(map: texture_2d<f32>, r: TrailRect, p: vec2f, foot: f32) -> Track {
  let c = trail_texel(map, r, p);
  let e = max(r.texel, foot);
  let xp = trail_texel(map, r, p + vec2f(e, 0.0));
  let xm = trail_texel(map, r, p - vec2f(e, 0.0));
  let zp = trail_texel(map, r, p + vec2f(0.0, e));
  let zm = trail_texel(map, r, p - vec2f(0.0, e));
  let h = vec4f(xp.y - xp.x, xm.y - xm.x, zp.y - zp.x, zm.y - zm.x);
  var slope = vec2f(h.x - h.y, h.z - h.w) / (2.0 * e);
  let steep = length(slope);
  if (steep > 1.5) {
    slope *= 1.5 / steep;
  }
  return Track(c.x, c.y, c.z, slope);
}

// How far inside a map's square p lies: 1 inside, falling to 0 over the outer 6 % of the span.
fn inside(r: TrailRect, p: vec2f) -> f32 {
  let uv = (p - vec2f(r.x0, r.z0)) / r.span;
  return smoothstep(0.0, 0.06, min(min(uv.x, uv.y), min(1.0 - uv.x, 1.0 - uv.y)));
}

// Where a pixel spans more than a coarse texel the slope a groove makes averages out under it,
// so it fades out by two, and past that the track is its depth and pressed share alone: one
// tap of the coarse map instead of five (or ten in the fine window's rim).
fn track_at(p: vec2f, foot: f32) -> Track {
  let keep = 1.0 - smoothstep(0.25 * coarse_rect.texel, 0.5 * coarse_rect.texel, foot);
  if (keep <= 0.0) {
    let c = trail_texel(trail_coarse, coarse_rect, p);
    return Track(c.x, c.y, c.z, vec2f(0.0));
  }
  let w = inside(fine_rect, p);
  var t: Track;
  if (w >= 1.0) {
    t = trail_of(trail_fine, fine_rect, p, foot);
  } else {
    let coarse = trail_of(trail_coarse, coarse_rect, p, foot);
    if (w <= 0.0) {
      t = coarse;
    } else {
      let fine = trail_of(trail_fine, fine_rect, p, foot);
      t = Track(mix(coarse.depth, fine.depth, w), mix(coarse.berm, fine.berm, w),
                mix(coarse.pressed, fine.pressed, w), mix(coarse.slope, fine.slope, w));
    }
  }
  t.slope *= keep;
  return t;
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let g = ground_at(in);
  let gm = textureSampleLevel(snow_ground, linear_clamp, g.uv, 0.0);
  let packed = gm.r;
  let rock = gm.g;
  let dir = gm.ba * 2.0 - 1.0;
  let along = clamp(length(dir), 0.0, 1.0);
  let p = g.xz;
  // m a pixel at this point (derivatives taken here, in uniform control flow).
  let dp = fwidth(p);
  let foot = max(dp.x, dp.y);
  // The tracks: a track's floor is pressed smooth, so the loose snow's grain and glitter and the
  // groomer's comb give way under it.
  let tk = snow.look2.z;
  let track = track_at(p, foot);
  let pressed = track.pressed * tk;
  let loose = (1.0 - 0.85 * packed) * (1.0 - rock) * (1.0 - 0.8 * pressed);
  let world = g.rel + frame.eye.xyz;

  // THE MICRO-RELIEF, as a slope (render/wgsl/snow_light.wgsl), the rock its bare share.
  let gain = snow.look.z;
  var grad = snow_relief(p, foot, packed, rock, loose);
  grad *= gain;
  grad += track.slope * tk;

  // THE CORDUROY and THE LANES, across the piste's way.
  var shade = 1.0;
  let th = 0.5 * atan2(dir.y, dir.x);
  let across = vec2f(cos(th), -sin(th));
  let on = packed * smoothstep(0.05, 0.5, along) * (1.0 - rock) * (1.0 - 0.7 * pressed);
  if (on > 0.01) {
    // The comb: 14 cm a rib, faded once a pixel spans a third of one; worn in patches a few
    // metres long and chipped a hand across.
    let phase = dot(p, across) * (2.0 * PI / 0.14);
    let aa = 1.0 - smoothstep(0.2, 0.45, foot / 0.14);
    let worn = smoothstep(0.3, 0.7, snow_noise(p * 0.42 + 11.0));
    let chip = snow_noise(p * 2.6 + 5.0);
    let k = on * aa * mix(0.3, 1.0, worn) * mix(0.55, 1.0, chip) * snow.look.y;
    grad += across * cos(phase) * 0.14 * k * 0.5;
    // The lanes: a swath 5.2 m wide each, a shade of its own. Where two passes meet the tiller's
    // edge leaves a low berm, a centimetre or two of snow a hand wide, never a groove: the
    // driver overlaps the last pass, so the seam wanders a little and is lost in stretches.
    let lane = (dot(p, across) + (snow_noise(p * 0.03 + 2.0) - 0.5) * 0.8) / 5.2;
    let u = fract(lane) - 0.5;
    let seam = 1.0 - smoothstep(0.0, 0.02, abs(u));
    let kept = smoothstep(0.35, 0.65, snow_noise(p * 0.06 + 23.0));
    let laa = 1.0 - smoothstep(0.3, 0.8, foot / 5.2 * 12.0);
    let kl = on * snow.look2.y;
    grad -= across * sign(u) * seam * kept * 0.04 * kl * laa;
    shade += (snow_hash1(vec2f(floor(lane), 7.0)) * 2.0 - 1.0) * 0.025 * kl;
  }
  let n = normalize(g.n - vec3f(grad.x, 0.0, grad.y));

  // THE LIGHT (render/wgsl/snow_light.wgsl).
  let v = -g.rel / max(g.dist, 1e-3);
  // Rain on snow: the water in its pores darkens it to slush (rock less, being sealed).
  let albedo = wet_albedo(g.albedo * shade, mix(1.0, 0.4, rock));
  // A deep trough sees less of the sky.
  let cavity = 1.0 - 0.4 * smoothstep(0.0, 0.25, track.depth * tk);
  let lit = snow_light(SnowPoint(albedo, n, g.n, v, world, g.sun, g.dist, foot, packed, rock, loose,
                                 cavity * sky_seen(in.position)),
                       SnowGains(snow.look.x, snow.look.w, snow.look2.x));
  // The lamps: the street's, a headlamp's, a fire's.
  let lamps = local_lights(matte(albedo), g.rel, n, v, in.position.xyz);
  // The lightning's.
  let flash = albedo / PI * flash_on(frame.eye.xyz + g.rel, n) * cavity;
  return ground_out(in, lit.rgb + lamps + flash, clamp(lit.a * 4.0, 0.0, 1.0));
}
