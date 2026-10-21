// THE ATMOSPHERE: an Earth-like planet's air (Rayleigh, Mie and ozone), its phase functions and
// the parametrisations of the sky's lookup tables (the scheme of Hillaire, 2020: a
// transmittance table, a multiple-scattering table, a sky-view table and an aerial-perspective
// volume). Distances are KILOMETRES and positions are relative to the planet's centre, with the
// local up along +y. Every luminance is per unit of the sun's illuminance: a pass multiplies by
// `Frame.sun.w`. The tables are built by render/sky_tables.wgsl and read through frame.wgsl.
// Requires frame_types.wgsl (PI).

const kGroundRadius = 6360.0;  // km
const kTopRadius = 6460.0;     // km
const kRayleighScattering = vec3f(5.802e-3, 13.558e-3, 33.1e-3);  // per km at the ground
const kRayleighHeight = 8.0;                                        // km
const kMieScattering = 3.996e-3;
const kMieAbsorption = 4.40e-3;
const kMieHeight = 1.2;
const kMieG = 0.8;
const kOzoneAbsorption = vec3f(0.650e-3, 1.881e-3, 0.085e-3);
const kOzoneCentre = 25.0;  // km, a tent 30 km wide
const kOzoneHalfWidth = 15.0;
// The ground's albedo the multiple scattering sees: snow country.
const kGroundAlbedo = vec3f(0.6);

// The tables' sizes (render/sky.hpp states the same).
const kTransmittanceSize = vec2f(256.0, 64.0);
const kMultiscatterSize = vec2f(32.0, 32.0);
const kSkyViewSize = vec2f(192.0, 108.0);
const kAerialSize = vec3f(32.0, 32.0, 32.0);
const kAerialRangeKm = 8.0;  // the volume's far slice

struct Medium {
  rayleigh: vec3f,    // Rayleigh scattering, per km
  mie: vec3f,         // Mie scattering, per km
  scattering: vec3f,  // both
  extinction: vec3f,  // scattering and absorption
}

fn medium_at(height_km: f32) -> Medium {
  let h = max(height_km, 0.0);
  let r = exp(-h / kRayleighHeight);
  let m = exp(-h / kMieHeight);
  let o = max(0.0, 1.0 - abs(h - kOzoneCentre) / kOzoneHalfWidth);
  var s: Medium;
  s.rayleigh = kRayleighScattering * r;
  s.mie = vec3f(kMieScattering * m);
  s.scattering = s.rayleigh + s.mie;
  s.extinction = s.scattering + vec3f(kMieAbsorption * m) + kOzoneAbsorption * o;
  return s;
}

fn rayleigh_phase(c: f32) -> f32 {
  return 3.0 / (16.0 * PI) * (1.0 + c * c);
}

// Cornette-Shanks.
fn mie_phase(c: f32) -> f32 {
  let g = kMieG;
  let g2 = g * g;
  let d = max(1.0 + g2 - 2.0 * g * c, 1e-4);
  return 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + c * c)) / ((2.0 + g2) * d * sqrt(d));
}

// Distance along a unit ray from `o` to the sphere of `radius` round the centre, the nearest
// one ahead; -1 when it misses or lies behind.
fn ray_sphere(o: vec3f, d: vec3f, radius: f32) -> f32 {
  let b = dot(o, d);
  let c = dot(o, o) - radius * radius;
  let disc = b * b - c;
  if (disc < 0.0) {
    return -1.0;
  }
  let s = sqrt(disc);
  let t0 = -b - s;
  let t1 = -b + s;
  if (t0 >= 0.0) {
    return t0;
  }
  if (t1 >= 0.0) {
    return t1;
  }
  return -1.0;
}

// How far a ray from radius `r` at cosine `mu` to the zenith runs inside the atmosphere: to the
// ground when it meets it, else to the top.
fn ray_length(r: f32, mu: f32) -> f32 {
  let o = vec3f(0.0, r, 0.0);
  let d = vec3f(sqrt(max(1.0 - mu * mu, 0.0)), mu, 0.0);
  let g = ray_sphere(o, d, kGroundRadius);
  if (g > 0.0) {
    return g;
  }
  return max(ray_sphere(o, d, kTopRadius), 0.0);
}

// ── The transmittance table: (r, mu) of a ray to the top of the atmosphere (Bruneton's map) ──

fn transmittance_uv(r: f32, mu: f32) -> vec2f {
  let h = sqrt(kTopRadius * kTopRadius - kGroundRadius * kGroundRadius);
  let rho = sqrt(max(r * r - kGroundRadius * kGroundRadius, 0.0));
  let disc = r * r * (mu * mu - 1.0) + kTopRadius * kTopRadius;
  let d = max(-r * mu + sqrt(max(disc, 0.0)), 0.0);
  let d_min = kTopRadius - r;
  let d_max = rho + h;
  return vec2f((d - d_min) / (d_max - d_min), rho / h);
}

fn transmittance_r_mu(uv: vec2f) -> vec2f {
  let h = sqrt(kTopRadius * kTopRadius - kGroundRadius * kGroundRadius);
  let rho = h * uv.y;
  let r = sqrt(rho * rho + kGroundRadius * kGroundRadius);
  let d_min = kTopRadius - r;
  let d_max = rho + h;
  let d = d_min + uv.x * (d_max - d_min);
  var mu = 1.0;
  if (d > 0.0) {
    mu = (h * h - rho * rho - d * d) / (2.0 * r * d);
  }
  return vec2f(r, clamp(mu, -1.0, 1.0));
}

// The transmittance from radius `r` toward cosine `mu`, through the top of the atmosphere;
// zero when the planet is in the way.
fn transmittance_to_top(lut: texture_2d<f32>, s: sampler, r: f32, mu: f32) -> vec3f {
  let horizon = -sqrt(max(1.0 - (kGroundRadius / r) * (kGroundRadius / r), 0.0));
  if (mu < horizon) {
    return vec3f(0.0);
  }
  let uv = transmittance_uv(r, mu);
  // Texel centres: the table's edges are its first and last samples.
  let t = (uv * (kTransmittanceSize - 1.0) + 0.5) / kTransmittanceSize;
  return textureSampleLevel(lut, s, t, 0.0).rgb;
}

// ── The multiple-scattering table: (cosine of the sun's zenith, height) ──

fn multiscatter_uv(r: f32, mu_s: f32) -> vec2f {
  return vec2f(mu_s * 0.5 + 0.5, clamp((r - kGroundRadius) / (kTopRadius - kGroundRadius), 0.0, 1.0));
}

fn multiscatter(lut: texture_2d<f32>, s: sampler, r: f32, mu_s: f32) -> vec3f {
  let uv = multiscatter_uv(r, mu_s);
  // Texel centres: the table's edges are its first and last samples.
  let t = (uv * (kMultiscatterSize - 1.0) + 0.5) / kMultiscatterSize;
  return textureSampleLevel(lut, s, t, 0.0).rgb;
}

// ── The sky-view table: the sky round the eye, by zenith angle and azimuth from the sun ──
// v packs the zenith angle with its rows crowded at the horizon (the top half is the sky, the
// bottom half under the horizon); u = sqrt of (1 - cos of the azimuth from the sun) / 2,
// crowded round the sun.

fn horizon_zenith(r: f32) -> f32 {
  return PI - acos(clamp(sqrt(max(r * r - kGroundRadius * kGroundRadius, 0.0)) / r, -1.0, 1.0));
}

// uv -> (cos of the view's zenith angle, cos of its azimuth from the sun's).
fn skyview_dir(uv: vec2f, r: f32) -> vec2f {
  let zh = horizon_zenith(r);
  var zenith: f32;
  if (uv.y < 0.5) {
    let c = 1.0 - 2.0 * uv.y;
    zenith = zh * (1.0 - c * c);
  } else {
    let c = 2.0 * uv.y - 1.0;
    zenith = zh + (PI - zh) * c * c;
  }
  return vec2f(cos(zenith), 1.0 - 2.0 * uv.x * uv.x);
}

fn skyview_uv(r: f32, cos_zenith: f32, cos_azimuth: f32) -> vec2f {
  let zh = horizon_zenith(r);
  let zenith = acos(clamp(cos_zenith, -1.0, 1.0));
  var v: f32;
  if (zenith < zh) {
    v = 0.5 * (1.0 - sqrt(max(1.0 - zenith / zh, 0.0)));
  } else {
    v = 0.5 + 0.5 * sqrt(clamp((zenith - zh) / (PI - zh), 0.0, 1.0));
  }
  let u = sqrt(clamp(0.5 - 0.5 * cos_azimuth, 0.0, 1.0));
  // Texel centres along u; v keeps its ends so the horizon row stays sharp.
  return vec2f((u * (kSkyViewSize.x - 1.0) + 0.5) / kSkyViewSize.x, v);
}

// The cosine of the azimuth between a direction and the sun, round the local up (+y).
fn azimuth_cos(d: vec3f, sun: vec3f) -> f32 {
  let a = vec2f(d.x, d.z);
  let b = vec2f(sun.x, sun.z);
  let la = length(a);
  let lb = length(b);
  if (la < 1e-5 || lb < 1e-5) {
    return 1.0;
  }
  return clamp(dot(a, b) / (la * lb), -1.0, 1.0);
}

// ── Integrating the in-scattered light along a ray ──

struct Scattered {
  luminance: vec3f,      // per unit of sun illuminance
  transmittance: vec3f,  // along the ray
}

// March from `o` (km, planet-centred) along unit `d` for `length_km` in `steps` steps, the sun
// along `sun`, with the multiple scattering of the table (single scattering plus Psi).
fn integrate_scattering(o: vec3f, d: vec3f, sun: vec3f, length_km: f32, steps: i32,
                        tlut: texture_2d<f32>, mslut: texture_2d<f32>, s: sampler) -> Scattered {
  var out: Scattered;
  out.luminance = vec3f(0.0);
  out.transmittance = vec3f(1.0);
  let c = dot(d, sun);
  let ph_r = rayleigh_phase(c);
  let ph_m = mie_phase(c);
  let dt = length_km / f32(steps);
  for (var i = 0; i < steps; i++) {
    let t = (f32(i) + 0.3) * dt;
    let p = o + d * t;
    let r = length(p);
    let up = p / r;
    let mu_s = dot(up, sun);
    let m = medium_at(r - kGroundRadius);
    let sun_t = transmittance_to_top(tlut, s, r, mu_s);
    let ms = multiscatter(mslut, s, r, mu_s);
    let source = sun_t * (m.rayleigh * ph_r + m.mie * ph_m) + ms * m.scattering;
    let ext = max(m.extinction, vec3f(1e-7));
    let step_t = exp(-ext * dt);
    out.luminance += out.transmittance * (source - source * step_t) / ext;
    out.transmittance *= step_t;
  }
  return out;
}
