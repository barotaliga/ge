// THE CLOUD PASS (render/clouds.hpp): each pixel's ray marched through every layer of
// `frame.clouds` it crosses, nearest first, up to what the depth target holds, and the cloud laid
// over the HDR target premultiplied. A layer's density at a point is its column (frame.wgsl's
// `cloud_column`) shaped by the height in its slab (a heap's top grows into the cell, a sheet
// thickens about its middle) and eaten at its edge by the noise read again at a quarter of the
// cell. The light on it is the key light through the air (`sun_light_open`), through the other
// layers above (their shadow at the layer), through its own cloud toward the light (four reads
// at growing steps: Beer's law and a slower lobe for the light scattered on), and the sky's and
// the snow's light from above and below; far cloud is hazed and melts into the sky behind it.
// Marched for every pixel (`fs_main`), or at a quarter of them (`fs_march` into a half-size target)
// and laid over the frame by a depth-aware upsample (`fs_upsample`); spread over four frames
// (`cloud_info.z`), a quarter of the half-size pixels march each frame and the rest are the last
// frame's, reprojected.
#include "render/wgsl/frame.wgsl"

@group(1) @binding(0) var scene_depth: texture_depth_2d;

// The primary steps a heap and a sheet take through their slab at the top of the quality ladder
// (`cloud_info.y` takes a share of them, never under four), the farthest a ray marches in
// one layer (m), and where far cloud starts and ends melting into the sky (m).
const kHeapSteps = 28;
const kSheetSteps = 10;
const kMaxMarch = 30000.0;
const kFadeNear = 25000.0;
const kFadeFar = 55000.0;
// The sky's distance (a ray that meets nothing), and the upsample's depth test: how sharply a
// half-size pixel's weight falls with its distance's miss (a share of the nearer), and the miss
// past which no half-size pixel stands for this one.
const kSky = 1e9;
// The share a ray's cloud lets through under which the layers behind it are no longer marched.
const kThroughGone = 1e-3;
// The share under which a layer's march stops and takes the rest of its path as thick as the
// ray's so far: a hundredth, and toward the key light a millionth. Near the light the glow is
// seen as the disc is, so even a thousandth of a cloud's glow shows, and a ray stopped at a
// hundredth (its rest guessed thick) beside one marched on through the clear air past a thin
// wisp differed by that much: the cloud's edge across the moon was a line a pixel wide. Out to
// kMarchNear rad from the light the march goes on to the millionth, by kMarchFar it is back to
// the hundredth, so only the rays round the disc pay for the longer march.
const kMarchGone = 0.01;
const kMarchGoneNear = 1e-6;
const kMarchNear = 0.1;
const kMarchFar = 0.3;
const kDepthSharp = 40.0;
const kDepthMiss = 0.1;
// The detail's scale (cells of the column a detail cell); how deep it eats the edge is the
// layer's (form.y).
const kDetailScale = 10.0;
// The share of the extinction the light scattered on through a cloud dies at (1 - g).
const kReduced = 0.15;
// The orders of scattering past the first, summed, over the one lobe that stands for them.
const kOrders = 3.0;
// The 22 degree halo's peak in an ice layer, per steradian (the forward lobe is about 0.4 there).
const kHalo = 0.5;
// THE GLOW ROUND THE DISC: half of what a particle much larger than the light's wavelength takes
// out of a beam is diffracted round its edge, into a lobe a few degrees wide (the aureole), with
// coloured rings out from it where the particles are of one size (the corona: blue inside, red
// outside each). A layer's particles diffract as their radius (`glow.x`, um), over five sizes a
// share `glow.y` either side of it, which washes the outer rings out: a thin altocumulus's rings,
// an altostratus's or an ice veil's plain bright glare (the `diffraction` below).
//
// How a glow round the MOON is seen beside its disc: as the disc's brightness as seen
// (frame.wgsl's `as_seen`) times its ratio to the disc's own to this power, the law sky.wgsl's
// faint parts of the moon are seen by (never brighter than it is).
const kGlowSeen = 0.35;
// The most the sun's glow adds, against the picture's white, and the glow (against the white)
// whose colour is a share 1 - 1/e of the way to white.
const kSunGlowTop = 8.0;
// The core of a particle's glow as a Gaussian's width (rad um): a round edge's lobe's half width
// at half its peak, 0.257 wavelengths over the radius, at 0.55 um.
const kGlowCore = 0.141;
const kSunGlowWhite = 1.5;
// THE DISC DRAWN LARGER (render/view.hpp's sun_size and moon_size): a glow is a point's pattern
// spread over the disc, so off a disc drawn larger than the real one it stands off the drawn
// limb, its core filling the face. The real discs' radii, rad: the sun's, and the moon's at its
// nearest, so a disc no larger than these is the point the pattern was cut for.
const kSunDisc = 0.00465;
const kMoonDiscMost = 0.0049;

// How far past the real disc's the key light's drawn disc reaches, rad (0: drawn as it is).
fn disc_swell() -> f32 {
  if (frame.moon_light.w < 0.5) {
    return max(kSunDisc * frame.style[2].w - kSunDisc, 0.0);
  }
  return max(frame.moon_light.z - kMoonDiscMost, 0.0);
}

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
}

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: VertexOut;
  out.position = vec4f(p, 0.0, 1.0);
  out.uv = vec2f(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
  return out;
}

// The cloud's extinction (1/m) of layer `l` at the world point `p`; `detail` false for the light's
// cheaper reads.
// THE FEATHER: no cloud ends in a cut. A dense layer's density climbing from nothing to its full
// over its edge is optically all of it in the edge's first few percent (a scud of optical depth
// 30 is opaque a thirtieth of the way in), so against the moon's glow it ends in a line a pixel
// wide. The density's share is taken to a power so the edge reaches an optical depth of one a
// kFeather share of the way in, whatever the layer's depth: ln(tau) / ln(1 / kFeather), never
// under one, so a thin layer (its whole depth under 1 / kFeather) keeps its edge as it is.
const kFeather = 0.3;

fn feather(l: CloudRows, dens: f32) -> f32 {
  let tau = l.look.z * (l.shape.y - l.shape.x);
  if (tau * kFeather <= 1.0 || dens <= 0.0) {
    return dens;
  }
  return pow(dens, log(tau) / log(1.0 / kFeather));
}

fn cloud_density(l: CloudRows, p: vec3f, detail: bool) -> f32 {
  let h = (p.y - l.shape.x) / (l.shape.y - l.shape.x);
  if (h <= 0.0 || h >= 1.0) {
    return 0.0;
  }
  let uvw = cloud_uvw(l, p.xz);
  let n = textureSampleLevel(cloud_noise, linear_repeat, uvw, 0.0).r;
  let c = cloud_column_of(l, uvw, n);
  // AN ANVIL (form.x): near the slab's top the same noise cut far lower, so the tower's top
  // spreads out flat to several times its width, thinning toward its rim; its base hangs in
  // pouches (mammatus), a coarse read of the billows lowering it cell by cell.
  var anvil = 0.0;
  if (l.form.x > 0.0 && h > 0.55) {
    let cut = l.shape.z * 0.72;
    let wide = clamp((n - cut) / max(1.0 - cut, 1e-3) * 2.0, 0.0, 1.0);
    let pouch = textureSampleLevel(cloud_noise, linear_repeat, vec3f(uvw.xy * 6.0, uvw.z), 0.0).g;
    let lo = 0.8 - 0.07 * wide - 0.12 * smoothstep(0.3, 0.8, pouch) * wide;
    let band = smoothstep(lo, lo + 0.04, h) * (1.0 - smoothstep(0.95 - 0.12 * (1.0 - wide), 1.0, h));
    anvil = l.form.x * sqrt(wide) * band;
  }
  if (c <= 0.0 && anvil <= 0.0) {
    return 0.0;
  }
  // A heap: a flat base, its top the taller the fuller the column, so a dome over the column's
  // core. A sheet: about its middle, a lens thickest where the column is fullest. Both soft
  // inside a wide band, for the detail to bite into.
  let heap = clamp((sqrt(c) * 1.15 - h) * 1.6, 0.0, 1.0) * clamp(h * 10.0, 0.0, 1.0);
  let sheet = clamp((c - abs(2.0 * h - 1.0)) * 1.6, 0.0, 1.0);
  let occ = max(mix(sheet, heap, l.look.y), anvil);
  if (occ <= 0.0) {
    return 0.0;
  }
  var dens = occ;
  if (detail) {
    var q = uvw * kDetailScale;
    q.z = p.y * l.shape.w / kCloudTileCells * kDetailScale + l.wind.w;
    // Combed along the wind (form.z): the detail drawn out into hairs, fine across it.
    q.x *= 1.0 + 3.0 * l.form.z;
    q.y /= 1.0 + 4.0 * l.form.z;
    // Two reads, the second at twice the scale. A heap's and a sheet's edge is eaten at the seams
    // between the billows (one minus the cells' noise: their bulges kept, the cauliflower); ice's
    // (combed, form.z) by the value noise stretched to its full range, in wisps.
    let d1 = textureSampleLevel(cloud_noise, linear_repeat, q, 0.0);
    let d2 = textureSampleLevel(cloud_noise, linear_repeat, q * 2.03, 0.0);
    let wisp = smoothstep(0.3, 0.7, 0.65 * d1.r + 0.35 * d2.r);
    let billow = 1.0 - smoothstep(0.25, 0.75, 0.65 * d1.g + 0.35 * d2.g);
    let d = mix(billow, wisp, l.form.z);
    // Eaten harder toward the top, where a heap boils.
    let e = l.form.y * mix(0.7, 1.4, h);
    // A heap's edge is crisp (its density climbs fast past the bite), a sheet's soft.
    dens = clamp((occ - e * d) / (1.0 - e * d) * mix(1.0, 3.0, l.look.y), 0.0, 1.0);
    // Ice is strands all through, not only at its edge: the combed noise cut sharp.
    dens *= mix(1.0, smoothstep(0.42, 0.58, 0.7 * d1.r + 0.3 * d2.r), l.form.z);
  }
  return feather(l, dens) * l.look.z * cloud_thickness(l, uvw, c);
}

// Henyey-Greenstein's phase function, per steradian.
// The Bessel function J1 (rational fits under 8, the asymptotic form over it).
fn bessel_j1(x: f32) -> f32 {
  if (x < 8.0) {
    let y = x * x;
    let a = x * (72362614232.0 + y * (-7895059235.0 + y * (242396853.1 + y * (-2972611.439 +
            y * (15704.4826 + y * -30.16036606)))));
    let b = 144725228442.0 + y * (2300535178.0 + y * (18583304.74 + y * (99447.43394 +
            y * (376.9991397 + y))));
    return a / b;
  }
  let z = 8.0 / x;
  let y = z * z;
  let xx = x - 2.356194491;
  let a = 1.0 + y * (0.00183105 + y * (-3.516396496e-5 + y * (2.457520174e-6 + y * -2.40337019e-7)));
  let b = 0.04687499995 + y * (-2.002690873e-4 + y * (8.449199096e-6 + y * (-8.8228987e-7 +
          y * 1.05787412e-7)));
  return sqrt(0.636619772 / x) * (cos(xx) * a - z * sin(xx) * b);
}

// The diffraction lobe per steradian (it sums to one over the sphere) at `theta` rad off the
// light, red, green and blue, for particles of radius `a` um spread `spread` either side (five
// sizes): a round edge's Airy pattern, (k a)^2 / 4 pi (2 J1(k a theta) / (k a theta))^2.
fn diffraction(theta: f32, a: f32, spread: f32) -> vec3f {
  let wave = vec3f(0.61, 0.55, 0.465);
  let sizes = array<f32, 5>(1.0 - spread, 1.0 - 0.5 * spread, 1.0, 1.0 + 0.5 * spread, 1.0 + spread);
  let shares = array<f32, 5>(0.1, 0.2, 0.4, 0.2, 0.1);
  var out = vec3f(0.0);
  for (var i = 0; i < 5; i++) {
    let ka = 2.0 * PI * a * sizes[i] / wave;
    let u = max(ka * theta, vec3f(1e-4));
    let j = vec3f(bessel_j1(u.x), bessel_j1(u.y), bessel_j1(u.z));
    let airy = 2.0 * j / u;
    out += shares[i] * ka * ka / (4.0 * PI) * airy * airy;
  }
  return out;
}

fn hg(c: f32, g: f32) -> f32 {
  let g2 = g * g;
  return (1.0 - g2) / (4.0 * PI * pow(max(1.0 + g2 - 2.0 * g * c, 1e-4), 1.5));
}

// The other layers' shadow at `p`: those whose middle is above it, but `skip`.
fn shadow_from(p: vec3f, skip: i32) -> f32 {
  let n = i32(frame.cloud_info.x);
  if (frame.sun.y < 0.02) {
    return 1.0;
  }
  var tau = 0.0;
  for (var i = 0; i < n; i++) {
    let l = cloud_layer(i);
    let mid = 0.5 * (l.shape.x + l.shape.y);
    if (i != skip && mid > p.y) {
      tau += cloud_cast(l, p.xz + frame.sun.xz * ((mid - p.y) / frame.sun.y)) * l.look.w;
    }
  }
  return exp(-tau);
}

// The spread's history: how many frames a stamp counts before it wraps (`cloud_info.w` is this
// frame's, 1..kStamps; 0 a target never written).
const kStamps = 4096.0;

// A pixel's own offset, 0..1, new each frame (interleaved gradient noise): TAA resolves it.
fn dither(px: vec2f) -> f32 {
  let q = px + 5.588238 * f32(u32(frame.time.y) % 64u);
  return fract(52.9829189 * fract(dot(q, vec2f(0.06711056, 0.00583715))));
}

// What a pixel's ray takes through one layer: its light (in front of nothing), its own
// transmittance, and the mean distance its light comes from.
struct LayerLook {
  light: vec3f,
  through: f32,
  dist: f32,
}

fn march(i: i32, d: vec3f, t0: f32, t1: f32, j: f32) -> LayerLook {
  let l = cloud_layer(i);
  let eye = frame.eye.xyz;
  let thick = l.shape.y - l.shape.x;
  let full = select(kSheetSteps, kHeapSteps, l.look.y > 0.5);
  // A long path through the slab (seen far off, or from inside it) takes up to three times the
  // steps, spaced closest where the ray enters: a dense sheet is opaque within its first
  // hundred metres, and the light it shows comes from there.
  let longer = clamp((t1 - t0) / (2.0 * thick), 1.0, 3.0);
  let steps = max(4, i32(round(f32(full) * longer * frame.cloud_info.y)));
  let span = t1 - t0;
  // How far the steps bunch toward the entry: a sheet's, on a long path (a heap's puffs lie all
  // along it).
  let bunch = 0.5 * (longer - 1.0) * (1.0 - l.look.y);
  let L = frame.sun.xyz;
  let c = dot(d, L);
  // The phase: a forward and a back lobe, and a wider one for the light scattered on.
  let ph = 0.75 * hg(c, 0.6) + 0.25 * hg(c, -0.25);
  let ph_ms = hg(c, 0.2);
  // ICE (a layer with a halo, glow.z) bends the light 22 degrees through its hexagonal plates: a
  // ring round the sun, red inside, blue outside.
  let theta = 2.0 * asin(min(0.5 * length(d - L), 1.0));
  let off = (theta - vec3f(0.3787, 0.3840, 0.3910)) / 0.016;
  let halo = l.glow.z * kHalo * exp(-off * off);
  // The glow round the disc, off its droplets or crystals, off the drawn limb.
  let off_limb = max(theta - disc_swell(), 0.0);
  let glow = 0.5 * diffraction(off_limb, l.glow.x, l.glow.y);
  let gone = mix(kMarchGoneNear, kMarchGone, smoothstep(kMarchNear, kMarchFar, theta));
  // The light at the layer: the key's through the air and the layers above at its middle; the
  // sky's from above, the snow's from below.
  let mid_p = eye + d * (0.5 * (t0 + t1));
  let mid_y = 0.5 * (l.shape.x + l.shape.y);
  let key = sun_light_open(mid_y) * shadow_from(vec3f(mid_p.x, mid_y, mid_p.z), i);
  let sky = (clear_sky_up() + dark_light()) * mix(1.0, deck_zenith_share(), frame.weather.y);
  let ground = kGroundAlbedo * (sun_light_open(0.0) * max(L.y, 0.0) * 0.6 + sky);
  // A DECK's light scattered on comes in through its top, which the sun lights at a slant: its
  // light on the top is the sun's height's sine of the beam's (what lets a deck under a low sun
  // show grey, the sun bright in it); a heap's sides and a thin layer's edges face the sun at any
  // height (a high veil's pink at sunset).
  let top_lit = mix(1.0, max(L.y, 0.05), l.glow.w);
  // The light's reads, as shares of the slab's depth.
  let ds = array<f32, 4>(0.05, 0.1, 0.2, 0.4);
  var out: LayerLook;
  out.light = vec3f(0.0);
  out.through = 1.0;
  var weight = 0.0;
  var wdist = 0.0;
  // How much of the key's light the ray's steps scatter, for the glow.
  var glow_lit = 0.0;
  for (var k = 0; k < steps; k++) {
    let a = (f32(k) + j) / f32(steps);
    let t = t0 + span * (a + bunch * (a * a - a));
    let dt = span * (1.0 + bunch * (2.0 * a - 1.0)) / f32(steps);
    let p = eye + d * t;
    let sigma = cloud_density(l, p, true);
    if (sigma <= 0.0) {
      continue;
    }
    var tau = 0.0;
    var at = 0.0;
    for (var s = 0; s < 4; s++) {
      let step = ds[s] * thick;
      at += 0.5 * step;
      tau += cloud_density(l, p + L * at, false) * step;
      at += 0.5 * step;
    }
    let h = clamp((p.y - l.shape.x) / thick, 0.0, 1.0);
    // The cloud over the point: one read halfway to the slab's top, for how much of the sky's
    // light from above still reaches it.
    let over = (1.0 - h) * thick;
    let tau_up = cloud_density(l, p + vec3f(0.0, 0.5 * over, 0.0), false) * over;
    // Thin cloud seen with the light behind the eye is darker at its edge (the powder).
    let powder = mix(1.0, 1.0 - exp(-sigma * 120.0), clamp(0.5 - 0.5 * c, 0.0, 1.0));
    // The light scattered on mostly keeps going forward (a droplet's lobe, g about 0.85), so it
    // dies only at the reduced extinction sigma (1 - g); and its many orders together leave a
    // thick cloud, which absorbs almost nothing, as bright as fresh snow in the light: what keeps
    // a heap's body white.
    // Light scattered through a thick cloud leaves it as a diffusion does, falling as one over
    // the depth rather than exponentially (a deck's base is grey, not black); so does the sky's
    // light from above.
    // The light scattered on reaches a point the short way: in a sheet through the cloud over it
    // (a deck is lit from its whole top), in a heap through the cloud toward the light, which
    // falls off as the heap's body shades it (a heap backlit is dark, rimmed with light).
    let tau_ms = min(tau, mix(tau_up / max(L.y, 0.4), tau, l.look.y));
    let ms = mix(1.0 / (1.0 + kReduced * tau_ms), exp(-kReduced * tau_ms), l.look.y);
    let direct = key * (exp(-tau) * (ph + halo) + kOrders * ph_ms * ms * top_lit) * powder;
    let ambient = sky * (mix(0.35, 1.0, h) / (2.0 * PI) / (1.0 + kReduced * tau_up)) +
                  ground * ((1.0 - h) * 0.3 / (2.0 * PI));
    let step_t = exp(-sigma * dt);
    let gain = out.through * (1.0 - step_t);
    out.light += (direct + ambient) * gain;
    glow_lit += (1.0 - step_t) * powder;
    weight += gain;
    wdist += gain * t;
    out.through *= step_t;
    if (out.through < gone) {
      // The rest of the path taken as thick as the ray's so far: a hundredth of the sun would
      // still shine through as a disc far brighter than the cloud.
      let tau_ray = -log(out.through);
      out.through *= exp(-tau_ray * max(t1 - t - dt, 0.0) / max(t + dt - t0, 1.0));
      break;
    }
  }
  out.dist = select(0.5 * (t0 + t1), wdist / max(weight, 1e-6), weight > 0.0);
  // THE GLOW comes nearly straight from the disc, so its light crosses the whole of the cloud on
  // the ray, wherever on it it was bent: whatever step bent it, the rest of the ray's depth
  // toward the light and the depth back to the eye add to the ray's whole, so it is lit by the
  // light the steps scatter times the ray's own transmittance (tau exp(-tau): brightest through a
  // thin patch about one deep, gone behind a thick core).
  // Through a thicker cloud the light is bent again and again: the orders past the first (the
  // diffracted half of each extinction, Poisson in their count) spread the glow ever wider, a
  // Gaussian of the first's core widened as the root of its order, and take the disc's place: a
  // bright smudge where the sun stands, its disc lost in it.
  let tau_ray = -log(max(out.through, 1e-30));
  let tf = 0.5 * tau_ray;
  let many = exp(-0.5 * tau_ray) * (1.0 - exp(-tf) * (1.0 + tf));
  let s = kGlowCore / l.glow.x * sqrt(max(2.0, tf));
  let spread = many * exp(-0.5 * off_limb * off_limb / (s * s)) / (2.0 * PI * s * s);
  out.light += glow_seen(key * (glow * (glow_lit * out.through) + spread));
  return out;
}

// The glow round the key light as seen. Round the sun it dazzles: eased toward kSunGlowTop of the
// picture's white and whitened as it brightens, so its middle is a white glare and its rings show
// only as pastel where it is about as bright as the cloud (each channel clipped on its own would
// band a glare hundreds of times the white into rainbows). Round the moon, seen beside the disc as the
// disc is seen (kGlowSeen), so its rings show.
fn glow_seen(l: vec3f) -> vec3f {
  let lum = dot(l, kLuma);
  if (lum <= 0.0) {
    return l;
  }
  if (frame.moon_light.w < 0.5) {
    let white = 1.0 / exposure();
    let top = kSunGlowTop * white;
    let seen = top * (1.0 - exp(-lum / top));
    let hue = mix(l / lum, vec3f(1.0), 1.0 - exp(-lum / (kSunGlowWhite * white)));
    return hue * seen;
  }
  let disc = frame.moon_light.x * frame.moon_east.w * dot(key_at_eye(), kLuma);
  let seen = as_seen(disc) * pow(lum / max(disc, 1e-30), kGlowSeen);
  return l * (min(seen, lum) / lum);
}

// A pixel's cloud: its ray (at `uv`) marched through every layer to the scene at depth `z`,
// the dither's pixel `px`. Its light premultiplied, and 1 - its transmittance.
fn cloud_at(uv: vec2f, px: vec2f, z: f32) -> vec4f {
  return cloud_look(uv, px, z).cloud;
}

// The cloud, and the mean distance its light comes from (0: no cloud on the ray).
struct CloudLook {
  cloud: vec4f,
  dist: f32,
}

fn cloud_look(uv: vec2f, px: vec2f, z: f32) -> CloudLook {
  let n = i32(frame.cloud_info.x);
  let d = view_ray(frame, uv);
  let scene = scene_dist(uv, z);
  // Each layer's span along the ray.
  var t0 = array<f32, 4>(0.0, 0.0, 0.0, 0.0);
  var t1 = array<f32, 4>(0.0, 0.0, 0.0, 0.0);
  for (var i = 0; i < n; i++) {
    let l = cloud_layer(i);
    let ey = frame.eye.y;
    var a = 0.0;
    var b = 0.0;
    if (abs(d.y) < 1e-5) {
      if (ey > l.shape.x && ey < l.shape.y) {
        b = kMaxMarch;
      }
    } else {
      let ta = (l.shape.x - ey) / d.y;
      let tb = (l.shape.y - ey) / d.y;
      a = max(min(ta, tb), 0.0);
      b = max(ta, tb);
    }
    t0[i] = a;
    t1[i] = min(min(b, scene), a + kMaxMarch);
  }
  let j = dither(px);
  var light = vec3f(0.0);
  var through = 1.0;
  var wdist = 0.0;
  var weight = 0.0;
  var done = array<bool, 4>(false, false, false, false);
  for (var k = 0; k < n; k++) {
    // The nearest layer not yet marched.
    var i = -1;
    for (var m = 0; m < n; m++) {
      if (!done[m] && (i < 0 || t0[m] < t0[i])) {
        i = m;
      }
    }
    done[i] = true;
    if (t1[i] <= t0[i] || t0[i] > kFadeFar) {
      continue;
    }
    var look = march(i, d, t0[i], t1[i], j);
    let alpha = 1.0 - look.through;
    // Not cut at a thin share: round the moon a wisp's glow is seen as the disc is, far over the
    // picture's white, so even a ten-thousandth of cover shows.
    if (alpha <= 1e-7) {
      continue;
    }
    // Under a deck a sheet is the deck: its light mostly the deck's grey along the ray (what the
    // ground is lit by, so the two agree), a little of its own march kept for its texture.
    let lid = 0.5 * frame.weather.y * (1.0 - cloud_layer(i).look.y);
    look.light = mix(look.light, deck_luminance(d) * alpha, lid);
    // Seen through the air to where its light comes from, and lost into the sky far off.
    let fade = 1.0 - smoothstep(kFadeNear, kFadeFar, look.dist);
    let hazed = haze_air(look.light / alpha, uv, look.dist) * alpha;
    // The layers behind a nearly opaque one fade out with what it lets through rather than stop
    // at a share of it: a glow behind it as bright as the moon's, times even a hundredth, is
    // brighter than the cloud in front, and a stop there cut it along a line.
    let behind = smoothstep(kThroughGone, 10.0 * kThroughGone, through);
    light += through * behind * fade * hazed;
    wdist += through * alpha * look.dist;
    weight += through * alpha;
    through *= 1.0 - fade * alpha;
    if (through < kThroughGone) {
      break;
    }
  }
  var out: CloudLook;
  // Over the moon's disc the post pass leaves the picture to the cones (frame.wgsl's
  // night_vision), for the disc's own light: the cloud's in front of it is the rods' as anywhere.
  out.cloud = vec4f(mix(light, rods(light), on_moon(d)), 1.0 - through);
  out.dist = select(0.0, wdist / max(weight, 1e-6), weight > 0.0);
  return out;
}

// Where the ray `d` first crosses a layer's middle (m), or `far`: the spread's first guess of the
// distance a pixel's cloud lies at.
fn layer_guess(d: vec3f, far: f32) -> f32 {
  let n = i32(frame.cloud_info.x);
  var t = far;
  for (var i = 0; i < n; i++) {
    let l = cloud_layer(i);
    let mid = (0.5 * (l.shape.x + l.shape.y) - frame.eye.y) / d.y;
    if (abs(d.y) > 1e-5 && mid > 0.0) {
      t = min(t, mid);
    }
  }
  return t;
}

// The last frame's uv of the camera-relative point at `t` along `d` (a direction past kSky/2).
fn prev_uv_of(d: vec3f, t: f32) -> vec2f {
  var rel = vec4f(d, 0.0);
  if (t < 0.5 * kSky) {
    rel = vec4f(d * t, 1.0);
  }
  let prev = frame.prev_view_proj * (rel + vec4f(frame.eye_delta.xyz * rel.w, 0.0));
  return vec2f(prev.x / prev.w * 0.5 + 0.5, 0.5 - prev.y / prev.w * 0.5);
}

// How far the ray at `uv` goes before the scene at depth `z` (reversed-z: 0 is the sky).
fn scene_dist(uv: vec2f, z: f32) -> f32 {
  if (z <= 0.0) {
    return kSky;
  }
  let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  let q = frame.inv_view_proj * vec4f(ndc, z, 1.0);
  return length(q.xyz / q.w);
}

// THE FULL PICTURE: every pixel marched (the top of the ladder, and the reference).
@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  // Seen through the fog in areas in front of it (the cloud lies beyond the volume's range).
  let z = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  return local_fog_over(cloud_at(in.uv, in.position.xy, z), in.uv, 1e30);
}

// THE QUARTER PICTURE, its march: a pixel of the half-size target for each two by two of the
// frame's, marched to the FARTHEST of the four's depths, so a ridge's edge keeps the cloud behind
// it; beside it the distance it marched to (for the upsample), the distance its cloud lies at and
// the frame's stamp (for the next frame's reprojection).
//
// SPREAD over four frames (`cloud_info.z`), one of each two by two of the half-size pixels marches
// a frame, in turn, in a pass of its own over a target a quarter of the half-size one
// (`fs_fresh`), so the pixels that march lie side by side and a GPU's lanes all march together
// (marching one pixel in four within one pass costs as much as marching all four). Then the
// half-size pass (`fs_march`) takes the fresh pixel where it is its turn, and elsewhere the last
// frame's half-size picture where its cloud stood then (its distance guessed at the nearest
// layer's middle, read back from the last frame and reprojected again, then read bilinear over
// the taps that were written the frame before and marched to the same scene within kDepthMiss);
// a pixel no tap stands for (off the screen, newly seen) marches after all.
@group(1) @binding(1) var history: texture_2d<f32>;
@group(1) @binding(2) var history_dist: texture_2d<f32>;
@group(1) @binding(3) var fresh: texture_2d<f32>;
@group(1) @binding(4) var fresh_dist: texture_2d<f32>;

// The march's turn order over a two by two, a frame at a time: diagonals first.
const kTurn = array<vec2i, 4>(vec2i(0, 0), vec2i(1, 1), vec2i(1, 0), vec2i(0, 1));
// The share of the bilinear footprint the valid taps must hold for the history to stand.
const kHistoryHeld = 0.5;

fn turn_of() -> vec2i {
  return kTurn[u32(frame.cloud_info.w) % 4u];
}

// The half-size pixel `q`'s depth: the farthest of its two by two, and its uv.
fn quarter_depth(q: vec2i) -> f32 {
  let size = vec2i(textureDimensions(scene_depth)) - 1;
  var z = 1.0;
  for (var k = 0; k < 4; k++) {
    z = min(z, textureLoad(scene_depth, min(q * 2 + vec2i(k & 1, k >> 1), size), 0));
  }
  return z;
}

// The half-size pixel `q`'s centre in uv (as its own pass's full-screen triangle reads it).
fn quarter_uv(q: vec2i) -> vec2f {
  let half = (vec2i(textureDimensions(scene_depth)) + 1) / 2;
  return (vec2f(q) + 0.5) / vec2f(half);
}

// The half-size targets keep a pixel's TRANSMITTANCE in alpha, not one minus it: a half float
// holds a small number finely but one near 1 only in steps of 1/2048, and the sun, tens of
// thousands of times the white, would shine through a thick deck in the last step.
fn kept(c: vec4f) -> vec4f {
  return vec4f(c.rgb, 1.0 - c.a);
}

struct FreshOut {
  @location(0) cloud: vec4f,
  @location(1) dist: vec2f,
}

@fragment
fn fs_fresh(in: VertexOut) -> FreshOut {
  let q = vec2i(in.position.xy) * 2 + turn_of();
  let z = quarter_depth(q);
  let uv = quarter_uv(q);
  let look = cloud_look(uv, vec2f(q) + 0.5, z);
  var out: FreshOut;
  out.cloud = kept(look.cloud);
  out.dist = vec2f(scene_dist(uv, z), look.dist);
  return out;
}

struct MarchOut {
  @location(0) cloud: vec4f,
  @location(1) dist: vec4f,
}

// The last frame's half-size picture at `uv`, bilinear over the taps written the frame before
// that marched to a scene within kDepthMiss of `scene`: the cloud, the distance it lies at
// (`dist.y`, 0 for none) and the share of the footprint those taps hold.
struct Held {
  cloud: vec4f,
  dist: vec2f,
  share: f32,
}

fn held_at(uv: vec2f, scene: f32) -> Held {
  let hsize = vec2i(textureDimensions(history));
  let last = select(frame.cloud_info.w - 1.0, kStamps, frame.cloud_info.w <= 1.0);
  let f = uv * vec2f(hsize) - 0.5;
  let i0 = vec2i(floor(f));
  let w = fract(f);
  var out: Held;
  out.cloud = vec4f(0.0);
  out.dist = vec2f(0.0);
  out.share = 0.0;
  var cloudy = 0.0;
  for (var k = 0; k < 4; k++) {
    let o = vec2i(k & 1, k >> 1);
    let q = i0 + o;
    if (any(q < vec2i(0)) || any(q >= hsize)) {
      continue;
    }
    let h = textureLoad(history_dist, q, 0);
    let miss = abs(h.x - scene) / max(min(h.x, scene), 1.0);
    if (h.z != last || miss > kDepthMiss) {
      continue;
    }
    let bw = select(1.0 - w.x, w.x, o.x == 1) * select(1.0 - w.y, w.y, o.y == 1);
    out.cloud += textureLoad(history, q, 0) * bw;
    out.share += bw;
    if (h.y > 0.0) {
      out.dist.y += h.y * bw;
      cloudy += bw;
    }
  }
  if (out.share > 0.0) {
    out.cloud /= out.share;
  }
  out.dist.y = select(0.0, out.dist.y / max(cloudy, 1e-6), cloudy > 0.0);
  return out;
}

@fragment
fn fs_march(in: VertexOut) -> MarchOut {
  let px = vec2i(in.position.xy);
  let z = quarter_depth(px);
  let scene = scene_dist(in.uv, z);
  let stamp = frame.cloud_info.w;
  var out: MarchOut;
  if (frame.cloud_info.z > 0.0) {
    if (all((px & vec2i(1)) == turn_of())) {
      let f = vec2i(in.position.xy) / 2;
      out.cloud = textureLoad(fresh, f, 0);
      out.dist = vec4f(textureLoad(fresh_dist, f, 0).xy, stamp, 0.0);
      return out;
    }
    let d = view_ray(frame, in.uv);
    var t = layer_guess(d, scene);
    for (var r = 0; r < 2; r++) {
      let held = held_at(prev_uv_of(d, t), scene);
      if (held.share < kHistoryHeld) {
        break;
      }
      if (r == 1 || held.dist.y <= 0.0) {
        out.cloud = held.cloud;
        out.dist = vec4f(scene, held.dist.y, stamp, 0.0);
        return out;
      }
      t = min(held.dist.y, scene);
    }
  }
  let look = cloud_look(in.uv, in.position.xy, z);
  out.cloud = kept(look.cloud);
  out.dist = vec4f(scene, look.dist, stamp, 0.0);
  return out;
}

// THE QUARTER PICTURE, laid over the frame: each pixel takes the four half-size pixels round it,
// each weighted by its share of the bilinear footprint and by how near the distance it marched to
// is to this pixel's own; a pixel none of the four matches (the near side of an edge) is marched
// itself.
@group(1) @binding(1) var quarter: texture_2d<f32>;
@group(1) @binding(2) var quarter_dist: texture_2d<f32>;

@fragment
fn fs_upsample(in: VertexOut) -> @location(0) vec4f {
  let z = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  let own = scene_dist(in.uv, z);
  let size = vec2i(textureDimensions(quarter)) - 1;
  let f = in.position.xy * 0.5 - 0.5;
  let i0 = vec2i(floor(f));
  let w = fract(f);
  var sum = vec4f(0.0);
  var weight = 0.0;
  var best = 1e9;
  for (var k = 0; k < 4; k++) {
    let o = vec2i(k & 1, k >> 1);
    let q = clamp(i0 + o, vec2i(0), size);
    let bw = select(1.0 - w.x, w.x, o.x == 1) * select(1.0 - w.y, w.y, o.y == 1);
    let qd = textureLoad(quarter_dist, q, 0).r;
    let miss = abs(qd - own) / max(min(qd, own), 1.0);
    best = min(best, miss);
    let dw = bw * exp(-miss * kDepthSharp) + 1e-5 * bw;
    sum += textureLoad(quarter, q, 0) * dw;
    weight += dw;
  }
  if (best > kDepthMiss) {
    return local_fog_over(cloud_at(in.uv, in.position.xy, z), in.uv, 1e30);
  }
  return local_fog_over(kept(sum / max(weight, 1e-6)), in.uv, 1e30);
}
