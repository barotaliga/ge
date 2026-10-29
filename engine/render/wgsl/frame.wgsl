// GROUP 0 OF EVERY DRAW: the frame block and the sky's tables (docs/rendering.md § Geometry and
// resources, bind group frequency), with the shading functions that read them. One copy of
// each idea: the haze, the sun's light at a point and the sky's light are here and nowhere
// else. render/sky.hpp's `frame_layout` is the C++ side of these bindings.
//
// THE WEATHER (`frame.weather`, `frame.deck`: render/view.hpp's `Weather`) enters here and
// nowhere else, so every material, particle and the sky see the same day: the cloud takes its
// share off the sun's direct light (`sun_light`); a DECK spreads a grey over its share of the
// dome (`sky_luminance`), its light the clear day's on the ground over pi times what the deck
// lets down, darker overhead, a pale patch where the sun is behind it, and the sky's light on a
// surface becomes that grey's (`sky_irradiance`), with no direction but up: the flat light; the
// air under it scatters its grey, not the sun's (`haze`); and a fog or a fall closes everything,
// the sky too, into its grey veil at its visual range (`veil_luminance`, `veil_sky`).
//
// THE NIGHT (`frame.moon`, `frame.night`: render/night.hpp) enters here too: the dark sky's own
// glow on the dome and its light on the ground, and the EYE, which adapts to a night only part
// of the way (`exposure`: 0.6 decades of gain a decade of light under 14 lx on white), so a
// full moon's snow reads a fifth as bright as the noon's and a moonless night's an eighth of that,
// and loses its colour (`night_vision`) into the rods' blue grey as the light falls under 3 lx.
// At night the sky is TWO skies, the twilight's (the sun's own table) and the moon's (its own
// table at the moon's height, lit by its light), summed wherever the sky's light is read; the
// KEY LIGHT (`frame.sun`, the one direct light) is the moon once the sun is gone, and the haze
// is the brighter sky's light scattered, the other's added as the sky behind it.
#include "render/wgsl/frame_types.wgsl"
#include "render/wgsl/atmosphere.wgsl"

@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var linear_clamp: sampler;
@group(0) @binding(2) var transmittance_lut: texture_2d<f32>;
@group(0) @binding(3) var multiscatter_lut: texture_2d<f32>;
@group(0) @binding(4) var sky_view_lut: texture_2d<f32>;
@group(0) @binding(5) var aerial_volume: texture_3d<f32>;

struct SkyLight {
  irradiance_up: vec4f,  // the sun's sky's irradiance on an up-facing surface, per unit sun
  sun_at_eye: vec4f,     // the sun's transmittance to the eye
  moon_up: vec4f,        // the moon's sky's, per unit of its light (zero by day)
  moon_at_eye: vec4f,    // the moon's transmittance to the eye
  eye: vec4f,            // the eye's adaptation (render/eye.hpp): x the stops it has opened
                         // past the sky's exposure; zero (the eye at rest) unless it runs
}
@group(0) @binding(6) var<storage, read> sky_light: SkyLight;
@group(0) @binding(7) var moon_view_lut: texture_2d<f32>;
// The cloud's noise (render/clouds.hpp: 64^3, 0..1) and its wrapping sampler.
@group(0) @binding(8) var cloud_noise: texture_3d<f32>;
@group(0) @binding(9) var linear_repeat: sampler;

// A world height (m) as a radius from the planet's centre, km.
fn radius_at(world_y: f32) -> f32 {
  return kGroundRadius + max(frame.world.x + world_y * 1e-3, 0.0);
}

// THE CLOUD'S LAYERS (render/clouds.hpp): layer `i`'s six rows of `frame.clouds`.
struct CloudRows {
  shape: vec4f,  // base, top (world m), the noise's cut, one over the cell (1/m)
  wind: vec4f,   // the heading (cos, sin), the drift and the age in the noise's tile
  look: vec4f,   // one over the stretch, the dome (1 a heap), the density (1/m), the shadow's depth
  form: vec4f,   // the anvil, the detail's bite, its comb along the wind, the patches
  glow: vec4f,   // its particles' radius (um), their sizes' spread, the 22 degree halo, the deck
  thick: vec4f,  // the thinnest patches' share of the extinction, one over a patch's cells, the bias
}

fn cloud_layer(i: i32) -> CloudRows {
  let r = i * 6;
  return CloudRows(frame.clouds[r], frame.clouds[r + 1], frame.clouds[r + 2], frame.clouds[r + 3],
                   frame.clouds[r + 4], frame.clouds[r + 5]);
}

// render/clouds.hpp's kCloudTileCells: the noise's base lattice across its tile.
const kCloudTileCells = 8.0;

// The noise's tile coordinates of the world point (x, z) for a layer: across the wind, along it
// (stretched, carried by the drift), and its age.
fn cloud_uvw(l: CloudRows, xz: vec2f) -> vec3f {
  let across = xz.x * l.wind.x - xz.y * l.wind.y;
  let along = xz.x * l.wind.y + xz.y * l.wind.x;
  let s = l.shape.w / kCloudTileCells;
  return vec3f(across * s, along * s * l.look.x - l.wind.z, l.wind.w);
}

// How far past the cut the noise climbs to a full column (render/clouds.hpp's kColumnRamp): a
// sheet of a full cover is full nearly everywhere, not as patchy as the noise.
const kColumnRamp = 0.3;

// How full a layer's column is over `xz`, 0..1 (render/clouds.hpp's `cloud_column`): where the
// noise passes the cut, how far past it; with PATCHES (form.w), only where a read at four times
// the cell passes its middle too, so the cells gather into islands with clear sky between.
fn cloud_column_of(l: CloudRows, uvw: vec3f, n: f32) -> f32 {
  var c = clamp((n - l.shape.z) / max(min(1.0 - l.shape.z, kColumnRamp), 1e-3), 0.0, 1.0);
  if (l.form.w > 0.0) {
    let m = textureSampleLevel(cloud_noise, linear_repeat, vec3f(uvw.xy * 0.25, uvw.z + 0.5), 0.0).r;
    c *= mix(1.0, clamp((m - 0.5) * 4.0 + 0.5, 0.0, 1.0), l.form.w);
  }
  return c;
}

// HOW THICK a layer is over its column `c` at the noise's point `uvw` (render/clouds.hpp's
// `cloud_thickness`): the share of a full cell's extinction it holds there, thick.x (its thinnest
// patches) .. 1. A read of the noise a patch wide (thick.y), set off from the column's, moved by
// the sky's bias (thick.z), and a share of the column itself, so the thin patches gather where the
// cloud thins toward its gaps and its cores are thick.
fn cloud_thickness(l: CloudRows, uvw: vec3f, c: f32) -> f32 {
  if (l.thick.x >= 1.0) {
    return 1.0;
  }
  let q = vec3f(uvw.xy * l.thick.y + vec2f(0.37, 0.71), uvw.z + 0.25);
  let m = textureSampleLevel(cloud_noise, linear_repeat, q, 0.0).r;
  let t = clamp((m - 0.5) * 2.5 + 0.5 + l.thick.z + 0.5 * (c - 1.0), 0.0, 1.0);
  return mix(l.thick.x, 1.0, t * t * (3.0 - 2.0 * t));
}

// The optical depth's share a layer's column over `xz` takes off the key light: how full it is
// times how thick (the shadow's read).
fn cloud_cast(l: CloudRows, xz: vec2f) -> f32 {
  let uvw = cloud_uvw(l, xz);
  let c = cloud_column_of(l, uvw, textureSampleLevel(cloud_noise, linear_repeat, uvw, 0.0).r);
  if (c <= 0.0) {
    return 0.0;
  }
  return c * cloud_thickness(l, uvw, c);
}

// THE CLOUD'S SHADOW at a world point: each layer whose middle is above it, read where the beam
// toward the key light crosses that middle, takes exp(-column * its depth) off the light.
fn cloud_shadow(p: vec3f) -> f32 {
  let n = i32(frame.cloud_info.x);
  if (n == 0 || frame.sun.y < 0.02) {
    return 1.0;
  }
  var tau = 0.0;
  for (var i = 0; i < n; i++) {
    let l = cloud_layer(i);
    let mid = 0.5 * (l.shape.x + l.shape.y);
    if (mid > p.y) {
      let at = p.xz + frame.sun.xz * ((mid - p.y) / frame.sun.y);
      tau += cloud_cast(l, at) * l.look.w;
    }
  }
  return exp(-tau);
}

// THE SHAFTS: the share of the sun's light the air along a view ray `d` gets, out to `far` m (at
// most kShaftReach), averaged over kShaftSteps points dithered per pixel `px` and per frame:
// where the cloud's shadow crosses the air, the air scatters less of the sun, and the sky reads
// as beams between the heaps. 1 with no layer, or with the sun low.
const kShaftSteps = 6;
const kShaftReach = 20000.0;
const kShaftShare = 0.7;

fn shafts(d: vec3f, far: f32, px: vec2f) -> f32 {
  if (i32(frame.cloud_info.x) == 0 || frame.sun.y < 0.02 || frame.weather.y > 0.99) {
    return 1.0;
  }
  let reach = min(far, kShaftReach);
  let j = fract(52.9829189 * fract(dot(px, vec2f(0.06711056, 0.00583715)) + 0.618034 * frame.time.y));
  var lit = 0.0;
  for (var i = 0; i < kShaftSteps; i++) {
    let a = (f32(i) + j) / f32(kShaftSteps);
    lit += cloud_shadow(frame.eye.xyz + d * (reach * a * a));
  }
  return lit / f32(kShaftSteps);
}

// The sun's light reaching a point at `world_y` (m) over the world's `xz`: illuminance times the
// transmittance, times what the cloud lets through (the deck's mean share, and each layer's
// shadow there).
fn sun_light(world_y: f32, xz: vec2f) -> vec3f {
  return sun_light_open(world_y) * cloud_shadow(vec3f(xz.x, world_y, xz.y));
}

// The same before any layer's shadow: the light on the cloud itself, which marches its own.
fn sun_light_open(world_y: f32) -> vec3f {
  return frame.sun.w * frame.weather.x *
         transmittance_to_top(transmittance_lut, linear_clamp, radius_at(world_y), frame.sun.y);
}

const kLuma = vec3f(0.2126, 0.7152, 0.0722);

// The key light's transmittance to the eye: the sun's, or the moon's when it is the key.
fn key_at_eye() -> vec3f {
  if (frame.moon_light.w > 0.5) {
    return sky_light.moon_at_eye.rgb;
  }
  return sky_light.sun_at_eye.rgb;
}

// The clear sky's irradiance on an up-facing surface: the sun's sky and, at night, the moon's.
fn clear_sky_up() -> vec3f {
  return sky_light.irradiance_up.rgb * frame.night_sun.w + sky_light.moon_up.rgb * frame.moon.w;
}

// THE DECK'S GREY at the horizon: the clear day's light on a level face (the sun's and the
// sky's) over pi, as the luminance of a white face under it, times the deck's tint and light.
fn deck_horizon() -> vec3f {
  let sun = key_at_eye() * frame.sun.w * max(frame.sun.y, 0.0);
  let sky = clear_sky_up();
  return frame.deck.rgb * (dot(sun + sky, kLuma) / PI);
}

// The deck's grey overhead over its grey at the horizon: its own darker grey, lost into the
// horizon's grey as the snow in the air hides it.
fn deck_zenith_share() -> f32 {
  return mix(frame.deck.w, 1.0, frame.weather.w);
}

// The deck's luminance along a direction: the horizon's grey up to its grey overhead, and a pale
// patch toward the sun behind it, as bright as the sun gets through.
fn deck_luminance(d: vec3f) -> vec3f {
  let h = deck_horizon();
  let up = sqrt(clamp(d.y, 0.0, 1.0));
  let toward = max(dot(d, frame.sun.xyz), 0.0);
  let halo = 1.5 * frame.weather.x * pow(toward, 8.0) * smoothstep(-0.05, 0.1, frame.sun.y);
  return h * (mix(1.0, deck_zenith_share(), up) + halo);
}

// The sky's irradiance on a surface of normal `n` (the up-facing integral, falling off toward
// a surface that faces the ground), with the ground's bounce for the half that faces down. Under
// a deck, the deck's: pi times its grey, the overhead share weighed as a level face sees it.
fn sky_irradiance(n: vec3f) -> vec3f {
  let clear = clear_sky_up();
  let h = deck_horizon();
  let deck = PI * h * mix(1.0, deck_zenith_share(), 0.8);
  let up = mix(clear, deck, frame.weather.y) + dark_light();
  let sun = key_at_eye() * frame.sun.w * frame.weather.x * max(frame.sun.y, 0.0);
  let ground = (sun + up) * kGroundAlbedo;
  return up * (0.5 + 0.5 * n.y) + ground * (0.5 - 0.5 * n.y) * 0.5;
}

// THE DARK SKY's glow (the airglow, the zodiacal light and the stars too faint to see one by
// one), a little green: its luminance along `d`, brighter toward the horizon as the glowing
// layer 90 km up is seen edge on (van Rhijn's law, held to three times the zenith's by the air
// it shines through), hidden by the deck's share.
const kDarkTint = vec3f(0.92, 1.03, 0.98);

fn dark_sky(d: vec3f) -> vec3f {
  if (frame.night.x <= 0.0) {
    return vec3f(0.0);
  }
  let z = clamp(d.y, 0.0, 1.0);
  let edge = min(inverseSqrt(max(1.0 - 0.9724 * (1.0 - z * z), 1e-4)), 3.0);
  return kDarkTint * (frame.night.x * edge * (1.0 - frame.weather.y));
}

// The dark sky's light on a level face, through what the deck lets down of it, and the moon's
// direct light while the sun is still the key, spread from above.
fn dark_light() -> vec3f {
  return kDarkTint * (frame.night.y * (1.0 - 0.7 * frame.weather.y)) +
         vec3f(frame.world.z * frame.weather.x);
}

// The sun's sky and the moon's along a camera-relative direction, clear.
fn sun_sky(d: vec3f) -> vec3f {
  let r = kGroundRadius + max(frame.eye.w, 0.0);
  let uv = skyview_uv(r, d.y, azimuth_cos(d, frame.night_sun.xyz));
  return textureSampleLevel(sky_view_lut, linear_clamp, uv, 0.0).rgb * frame.night_sun.w;
}

fn moon_sky(d: vec3f) -> vec3f {
  let r = kGroundRadius + max(frame.eye.w, 0.0);
  let uv = skyview_uv(r, d.y, azimuth_cos(d, frame.moon.xyz));
  return textureSampleLevel(moon_view_lut, linear_clamp, uv, 0.0).rgb * frame.moon.w;
}

// The sky's luminance along a camera-relative direction: the sky-view tables' (the sun's, and
// the moon's at night), under the deck's share of the dome its grey, and the dark sky's glow.
fn sky_luminance(d: vec3f) -> vec3f {
  var clear = sun_sky(d);
  if (frame.moon.w > 0.0) {
    clear += moon_sky(d);
  }
  if (frame.weather.y <= 0.0) {
    return clear + dark_sky(d);
  }
  return mix(clear, deck_luminance(d), frame.weather.y) + dark_sky(d);
}

// THE VEIL: the light a fog's droplets or a fall's flakes scatter toward the eye where they are
// thick enough to hide what is behind them. They are far larger than light's wavelengths, so
// they scatter every colour alike: the veil is grey, a white face's light under this sun and sky
// a little dimmed (kVeilAlbedo), never the blue of the clear air beyond. The sky is seen through
// a slab of it kVeilDepth deep over the eye, so looking up through a thick fall the blue is lost
// too, and toward the horizon (kVeilReach) all of it.
const kVeilAlbedo = 0.85;
const kVeilDepth = 800.0;
const kVeilReach = 20000.0;

fn veil_luminance() -> vec3f {
  return vec3f(white_luminance() * kVeilAlbedo);
}

// What of the sky along `d` (camera-relative, unit) the fog or the fall leaves: its colour
// through the slab, the veil for the rest.
fn veil_sky(d: vec3f, colour: vec3f) -> vec3f {
  if (frame.weather.z <= 0.0) {
    return colour;
  }
  let path = min(kVeilDepth / max(d.y, 1e-3), kVeilReach);
  return mix(veil_luminance(), colour, exp(-frame.weather.z * path));
}

// THE REACH'S MIST (FrameView::reach_m): what the sky pass draws along `d` (camera-relative,
// unit) while there is a reach, the sky with a direction under the horizon drawn as the
// horizon's, so a surface the mist has closed is the sky behind it and the ground past the reach
// is never missed: the far land lost in a bright haze, never a dark sea under it.
fn mist_dir(d: vec3f) -> vec3f {
  if (frame.mist.y <= 0.0 || d.y >= 0.0) {
    return d;
  }
  return normalize(vec3f(d.x, 1e-4, d.z));
}

fn mist_sky(d: vec3f) -> vec3f {
  let h = mist_dir(d);
  return veil_sky(h, sky_luminance(h));
}

// THE HAZE: a surface's colour seen through `dist_m` of air at screen `uv` (0..1): its colour
// times the air's transmittance plus the light the air scatters in, from the aerial-perspective
// volume (under a deck, the deck's grey in its place); then a fog or a fall closing it into its
// grey veil, so a surface at the visual range keeps 2 % of its contrast against it; then the
// reach's mist closing it into the sky behind it. Every world material calls this, and none has
// its own fog.
fn haze(colour: vec3f, uv: vec2f, dist_m: f32) -> vec3f {
  let c = haze_air(colour, uv, dist_m);
  if (frame.mist.y <= 0.0) {
    return c;
  }
  let t = clamp((dist_m - frame.mist.x) / (frame.mist.y - frame.mist.x), 0.0, 1.0);
  return mix(c, mist_sky(view_ray(frame, uv)), t * t * (3.0 - 2.0 * t));
}

// The haze without the reach's mist: the air and the weather alone (the cloud's, which is the
// sky's own and never misted).
fn haze_air(colour: vec3f, uv: vec2f, dist_m: f32) -> vec3f {
  let s = sqrt(clamp(dist_m * 1e-3 / kAerialRangeKm, 0.0, 1.0));
  let w = max(s - 0.5 / kAerialSize.z, 0.5 / kAerialSize.z);
  let v = textureSampleLevel(aerial_volume, linear_clamp, vec3f(uv, w), 0.0);
  // Nearer than the first slice: fade from no air to the first slice's.
  let k = clamp(s * kAerialSize.z, 0.0, 1.0);
  // The volume is the light of the haze's sky (the sun's, or the moon's once its sky outshines
  // the twilight's); the other sky's is the air's veil over the sky behind it.
  var air = v.rgb * select(frame.night_sun.w, frame.moon.w, frame.world.w > 0.5);
  if (frame.moon.w > 0.0) {
    var b = view_ray(frame, uv);
    b = normalize(vec3f(b.x, max(b.y, 0.01), b.z));
    air += (1.0 - v.a) * select(moon_sky(b), sun_sky(b), frame.world.w > 0.5);
  }
  let lit = shafts(view_ray(frame, uv), dist_m, uv * frame.size.xy);
  air *= mix(1.0, lit, kShaftShare * (1.0 - frame.weather.y));
  let inscatter = mix(air, (1.0 - v.a) * deck_horizon(), frame.weather.y);
  var c = colour * mix(1.0, v.a, k) + inscatter * k;
  if (frame.weather.z > 0.0) {
    c = mix(veil_luminance(), c, exp(-frame.weather.z * dist_m));
  }
  return c;
}

// THE WET GROUND (render/view.hpp's `Weather::wet` and `puddles`, in `frame.mist.zw`): a wet
// surface is darker, because the water filling its pores keeps the light inside it longer
// (`wet_albedo`: a porous surface, 1, goes to half its albedo soaked; a sealed one, 0, keeps it),
// and it mirrors the sky in its film (`wet_sheen`: Schlick's Fresnel off water, 0.02 head on, the
// sky's light along the reflected ray and the sun's glint, the film's mirror rough and a puddle's
// flat). `flat` is how level the surface lies (its normal's y over 0.9 .. 0.98 is a puddle's).
fn wet_albedo(albedo: vec3f, porous: f32) -> vec3f {
  return albedo * (1.0 - 0.5 * frame.mist.z * clamp(porous, 0.0, 1.0));
}

fn wet_sheen(n: vec3f, v: vec3f, sun: vec3f, flat: f32) -> vec3f {
  let wet = frame.mist.z;
  if (wet <= 0.0) {
    return vec3f(0.0);
  }
  let pool = frame.mist.w * smoothstep(0.9, 0.98, flat);
  // A puddle lies level whatever the ground's grain under it.
  let m = normalize(mix(n, vec3f(0.0, 1.0, 0.0), pool));
  let r = reflect(-v, m);
  let cos_v = clamp(dot(m, v), 0.0, 1.0);
  let fresnel = 0.02 + 0.98 * pow(1.0 - cos_v, 5.0);
  let sky = sky_luminance(normalize(vec3f(r.x, max(r.y, 0.02), r.z)));
  let sharp = mix(60.0, 1500.0, pool);
  let glint = pow(max(dot(r, frame.sun.xyz), 0.0), sharp) * mix(4.0, 60.0, pool);
  return (sky + sun * glint / PI) * fresnel * wet * mix(0.5, 1.0, pool);
}

// What a white surface lit by the sun and the sky maps to before the tone curve; under a deck,
// lower: a grey day's snow is a light grey, not the sunlit white, or the flat light reads as a
// white-out where there is none.
const kWhiteKey = 0.78;
const kDeckKey = 0.6;

// One unit of light is the sun's illuminance outside the air, lx (render/night.hpp's kSunLux);
// the frame stores light at the pre-exposure's scale (`frame.world.y`), so an absolute reading
// divides it out.
const kSunLux = 128000.0;
// THE EYE AT NIGHT: under this light on white, lx, it no longer adapts all the way but makes up
// kAdaptGain decades of gain a decade darker; and the light on white no night is darker than
// (an overcast night with no moon), lx.
const kAdaptLux = 14.0;
const kAdaptGain = 0.6;
const kDarkestLux = 1e-4;

// A white Lambertian face's luminance under the key light and the sky, the sun's units.
fn white_luminance() -> f32 {
  let sun = key_at_eye() * frame.sun.w * frame.weather.x * max(frame.sun.y, 0.0);
  let sky = sky_irradiance(vec3f(0.0, 1.0, 0.0));
  return max(dot((sun + sky) / PI, kLuma), kDarkestLux * frame.world.y / (PI * kSunLux));
}

// THE EXPOSURE at `white`, a white Lambertian surface's luminance the eye is adapted to: the
// scale that maps it to kWhiteKey; under kAdaptLux on white, the eye's partial adaptation leaves
// the night darker than the day.
fn exposure_at(white: f32) -> f32 {
  var e = mix(kWhiteKey, kDeckKey, frame.weather.y) / white;
  let lux = white * PI * kSunLux / frame.world.y;
  if (lux < kAdaptLux) {
    e *= pow(lux / kAdaptLux, 1.0 - kAdaptGain);
  }
  return e;
}

// THE SKY'S EXPOSURE: the eye adapted to a white surface under this sun and sky, read off the
// sky's light (never a histogram, so it never pumps). What the eye's meter exposes at.
fn sky_exposure() -> f32 {
  return exposure_at(white_luminance());
}

// THE EXPOSURE the picture is shown at: the sky's, with the eye opened `sky_light.eye.x` stops
// past it where render/eye.hpp's pass metered a darker world than the sky lights (a hall, a
// cave); the white the eye adapts to is that much dimmer, so the night's partial adaptation still
// holds in the dark. The post pass applies it; TAA-U weighs its samples in the light it gives.
fn exposure() -> f32 {
  let open = sky_light.eye.x;
  if (open == 0.0) {
    return sky_exposure();
  }
  return exposure_at(white_luminance() * exp2(-open));
}

// THE MOON AS SEEN (sky.wgsl's disc, and the clouds' glow round it): its lit surface is hundreds
// to tens of thousands of times as bright as the snow the eye has adapted to (the more so in the
// twilight and in the dark of an eclipse), which no display holds; the eye sees it as a bright
// thing whatever its light. A brightness is drawn as it is while it is a small share of what the
// picture shows as white (a moon by day, pale in the blue), and eased toward kMoonTop of that
// white above it, so a full moon at midnight, a crescent in the twilight and the sliver left in
// an eclipse all read bright, their seas and shading multiplied on as they are.
const kMoonTop = 0.45;

fn as_seen(l: f32) -> f32 {
  let top = kMoonTop / exposure();
  let x = l / top;
  return top * select(x * (1.0 - 0.5 * x), 1.0 - exp(-x), x > 1e-3);
}

// THE RODS' SHARE of the eye's seeing at `luminance` (a radiance in white_luminance's units, so
// the light on a white face is white_luminance()): 0 over 3 lx on white (the cones), 1 under
// 0.01 lx.
fn rods_at(luminance: f32) -> f32 {
  let lux = max(luminance, 1e-30) * PI * kSunLux / frame.world.y;
  return clamp((0.5 - log2(lux) * 0.30103) / 2.5, 0.0, 1.0);
}

fn rod_share() -> f32 {
  return rods_at(white_luminance());
}

// The share of the pixel looking along `d` that the moon's disc covers, seen through the
// weather's deck and fog: 1 inside, 0 outside, over the soft edge render/sky.wgsl draws it with.
// It reads no depth, so a ridge or a cloud layer in front of the disc counts as the disc: a
// ridge there is a dark silhouette, and the cloud pass gives a cloud's own light there to the
// rods itself (render/clouds.wgsl), so neither shows it.
// Other worlds' moons (render/moons.hpp) count by the circle that bounds each.
fn on_moon(d: vec3f) -> f32 {
  var on = 0.0;
  let rad = frame.moon_light.z;
  let c = dot(d, frame.moon.xyz);
  if (rad > 0.0 && c > 0.0) {
    on = 1.0 - smoothstep(0.95, 1.0, sqrt(max(1.0 - c * c, 0.0)) / sin(rad));
  }
  for (var k = 0; k < 4; k++) {
    let o = frame.moons[k * 6];
    let ck = dot(d, o.xyz);
    if (o.w > 0.0 && ck > 0.0) {
      on = max(on, 1.0 - smoothstep(0.95, 1.0, sqrt(max(1.0 - ck * ck, 0.0)) / sin(o.w)));
    }
  }
  if (on <= 0.0) {
    return 0.0;
  }
  let clear = (1.0 - frame.weather.y) * exp(-frame.weather.z * 10000.0);
  return on * clear;
}

// NIGHT VISION: a colour as the rods see it, its luminance in a blue grey, by their share. A part
// of the picture bright enough for the cones keeps its colour however dark the scene round it:
// the share is the scene's or the pixel's own, whichever is less, so a bright star keeps its
// tint over a blue-grey night. The moon's disc (`d` the pixel's view ray) is drawn far dimmer
// than it is, as the eye sees it against the scene, but its true brightness is always the
// cones' (even an eclipse's umbra is near 1 cd/m2), so it keeps its colour: a full moon its warm
// white, an eclipse its copper red.
const kRodTint = vec3f(0.78, 0.93, 1.32);

fn rods(c: vec3f) -> vec3f {
  let s = rod_share();
  if (s <= 0.0) {
    return c;
  }
  let l = dot(c, kLuma);
  return mix(c, kRodTint * l, 0.85 * min(s, rods_at(l)));
}

// The post pass's: over the moon's disc the sky pass has already given the sky in front of it
// to the rods (render/sky.wgsl), so the disc's pixels are left as they are.
fn night_vision(c: vec3f, d: vec3f) -> vec3f {
  return mix(rods(c), c, on_moon(d));
}

