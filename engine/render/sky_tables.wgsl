// THE SKY'S TABLES, built by compute (render/sky.hpp; docs/rendering.md § Sky): the
// transmittance and multiple-scattering tables once at load, the sky-view table when the sun or
// the eye's height has moved past a threshold, the aerial-perspective volume every frame for the
// camera, and the sky's light (the irradiance an up-facing surface gets from the sky, and the
// sun's transmittance at the eye) after the sky-view table. Each entry point reads the tables
// before it through group 0 (`table_a`, `table_b`) and writes one output in group 1.
#include "render/wgsl/frame_types.wgsl"
#include "render/wgsl/atmosphere.wgsl"

@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var lut_sampler: sampler;
@group(0) @binding(2) var table_a: texture_2d<f32>;
@group(0) @binding(3) var table_b: texture_2d<f32>;

@group(1) @binding(0) var out_2d: texture_storage_2d<rgba16float, write>;
@group(1) @binding(1) var out_3d: texture_storage_3d<rgba16float, write>;

struct SkyLight {
  irradiance_up: vec4f,  // the sky's irradiance on an up-facing surface, per unit sun
  sun_at_eye: vec4f,     // the sun's transmittance to the eye
}
@group(1) @binding(2) var<storage, read_write> sky_light: SkyLight;

fn eye_radius() -> f32 {
  return kGroundRadius + max(frame.eye.w, 0.0);
}

// The transmittance from (r, mu) to the top: 40 steps of optical depth.
@compute @workgroup_size(8, 8)
fn transmittance(@builtin(global_invocation_id) id: vec3u) {
  if (id.x >= u32(kTransmittanceSize.x) || id.y >= u32(kTransmittanceSize.y)) {
    return;
  }
  let uv = vec2f(id.xy) / (kTransmittanceSize - 1.0);
  let rm = transmittance_r_mu(uv);
  let o = vec3f(0.0, rm.x, 0.0);
  let d = vec3f(sqrt(max(1.0 - rm.y * rm.y, 0.0)), rm.y, 0.0);
  let len = max(ray_sphere(o, d, kTopRadius), 0.0);
  let steps = 40;
  let dt = len / f32(steps);
  var depth = vec3f(0.0);
  for (var i = 0; i < steps; i++) {
    let p = o + d * ((f32(i) + 0.5) * dt);
    depth += medium_at(length(p) - kGroundRadius).extinction * dt;
  }
  textureStore(out_2d, id.xy, vec4f(exp(-depth), 1.0));
}

// Psi = L2 / (1 - f_ms) over 64 directions round the sphere, 20 steps each (table_a: the
// transmittance table).
@compute @workgroup_size(8, 8)
fn multiscattering(@builtin(global_invocation_id) id: vec3u) {
  if (id.x >= u32(kMultiscatterSize.x) || id.y >= u32(kMultiscatterSize.y)) {
    return;
  }
  let uv = vec2f(id.xy) / (kMultiscatterSize - 1.0);
  let mu_s = uv.x * 2.0 - 1.0;
  let r = kGroundRadius + uv.y * (kTopRadius - kGroundRadius) + 1e-3;
  let sun = vec3f(sqrt(max(1.0 - mu_s * mu_s, 0.0)), mu_s, 0.0);
  let o = vec3f(0.0, r, 0.0);
  let iso = 1.0 / (4.0 * PI);
  var l2 = vec3f(0.0);
  var fms = vec3f(0.0);
  let n = 8;
  for (var a = 0; a < n; a++) {
    for (var b = 0; b < n; b++) {
      // Uniform on the sphere: cos(theta) uniform in [-1, 1], phi uniform.
      let ct = 1.0 - 2.0 * (f32(a) + 0.5) / f32(n);
      let st = sqrt(max(1.0 - ct * ct, 0.0));
      let phi = 2.0 * PI * (f32(b) + 0.5) / f32(n);
      let d = vec3f(st * cos(phi), ct, st * sin(phi));
      let ground = ray_sphere(o, d, kGroundRadius);
      var len = max(ray_sphere(o, d, kTopRadius), 0.0);
      if (ground > 0.0) {
        len = ground;
      }
      let steps = 20;
      let dt = len / f32(steps);
      var through = vec3f(1.0);
      var lum = vec3f(0.0);
      var f = vec3f(0.0);
      for (var i = 0; i < steps; i++) {
        let p = o + d * ((f32(i) + 0.3) * dt);
        let pr = length(p);
        let m = medium_at(pr - kGroundRadius);
        let ext = max(m.extinction, vec3f(1e-7));
        let step_t = exp(-ext * dt);
        let sun_t = transmittance_to_top(table_a, lut_sampler, pr, dot(p / pr, sun));
        let s = m.scattering * sun_t * iso;
        lum += through * (s - s * step_t) / ext;
        f += through * (m.scattering - m.scattering * step_t) / ext;
        through *= step_t;
      }
      if (ground > 0.0) {
        let p = o + d * ground;
        let up = normalize(p);
        let sun_t = transmittance_to_top(table_a, lut_sampler, kGroundRadius, dot(up, sun));
        lum += through * sun_t * max(dot(up, sun), 0.0) * kGroundAlbedo / PI;
      }
      l2 += lum;
      fms += f;
    }
  }
  let count = f32(n * n);
  l2 /= count;
  fms /= count;
  textureStore(out_2d, id.xy, vec4f(l2 / max(vec3f(1.0) - fms, vec3f(1e-3)), 1.0));
}

// The sky round the eye (table_a: transmittance, table_b: multiple scattering), 30 steps a ray.
@compute @workgroup_size(8, 8)
fn sky_view(@builtin(global_invocation_id) id: vec3u) {
  if (id.x >= u32(kSkyViewSize.x) || id.y >= u32(kSkyViewSize.y)) {
    return;
  }
  let uv = vec2f(f32(id.x) / (kSkyViewSize.x - 1.0), (f32(id.y) + 0.5) / kSkyViewSize.y);
  let r = eye_radius();
  let dir = skyview_dir(uv, r);
  let sin_z = sqrt(max(1.0 - dir.x * dir.x, 0.0));
  let sin_a = sqrt(max(1.0 - dir.y * dir.y, 0.0));
  // The sun in the x-y plane at its own zenith angle; the view at its azimuth from it.
  let mu_s = frame.sun.y;
  let sun = vec3f(sqrt(max(1.0 - mu_s * mu_s, 0.0)), mu_s, 0.0);
  let d = vec3f(sin_z * dir.y, dir.x, sin_z * sin_a);
  let len = ray_length(r, dir.x);
  let s = integrate_scattering(vec3f(0.0, r, 0.0), d, sun, len, 30, table_a, table_b, lut_sampler);
  textureStore(out_2d, id.xy, vec4f(s.luminance, 1.0));
}

// The aerial-perspective volume: for each of 32 x 32 columns over the screen, the light
// scattered in and the mean transmittance from the eye to each of 32 slices, the slices
// spaced as the square of their index out to kAerialRangeKm (table_a: transmittance,
// table_b: multiple scattering).
@compute @workgroup_size(8, 8)
fn aerial(@builtin(global_invocation_id) id: vec3u) {
  if (id.x >= u32(kAerialSize.x) || id.y >= u32(kAerialSize.y)) {
    return;
  }
  let uv = (vec2f(id.xy) + 0.5) / kAerialSize.xy;
  let d = view_ray(frame, uv);
  let o = vec3f(0.0, eye_radius(), 0.0);
  let sun = frame.sun.xyz;
  let c = dot(d, sun);
  let ph_r = rayleigh_phase(c);
  let ph_m = mie_phase(c);
  var lum = vec3f(0.0);
  var through = vec3f(1.0);
  var t = 0.0;
  let slices = u32(kAerialSize.z);
  for (var k = 0u; k < slices; k++) {
    let f = (f32(k) + 1.0) / kAerialSize.z;
    let end = kAerialRangeKm * f * f;
    let sub = 2;
    let dt = (end - t) / f32(sub);
    for (var i = 0; i < sub; i++) {
      let p = o + d * (t + (f32(i) + 0.5) * dt);
      let r = length(p);
      let mu_s = dot(p / r, sun);
      let m = medium_at(r - kGroundRadius);
      let sun_t = transmittance_to_top(table_a, lut_sampler, r, mu_s);
      let ms = multiscatter(table_b, lut_sampler, r, mu_s);
      let source = sun_t * (m.rayleigh * ph_r + m.mie * ph_m) + ms * m.scattering;
      let ext = max(m.extinction, vec3f(1e-7));
      let step_t = exp(-ext * dt);
      lum += through * (source - source * step_t) / ext;
      through *= step_t;
    }
    t = end;
    textureStore(out_3d, vec3u(id.xy, k), vec4f(lum, dot(through, vec3f(1.0 / 3.0))));
  }
}

// The sky's light, once a frame after the sky view (table_a: the sky-view table, table_b:
// transmittance): the cosine-weighted integral of the sky over the upper hemisphere.
@compute @workgroup_size(1)
fn sky_light_main() {
  let r = eye_radius();
  let sun = frame.sun.xyz;
  var e = vec3f(0.0);
  let n_el = 8;
  let n_az = 16;
  let d_el = 0.5 * PI / f32(n_el);
  let d_az = 2.0 * PI / f32(n_az);
  for (var a = 0; a < n_el; a++) {
    let zenith = (f32(a) + 0.5) * d_el;
    let cz = cos(zenith);
    let sz = sin(zenith);
    for (var b = 0; b < n_az; b++) {
      let phi = (f32(b) + 0.5) * d_az;
      let d = vec3f(sz * cos(phi), cz, sz * sin(phi));
      let l = textureSampleLevel(table_a, lut_sampler, skyview_uv(r, cz, azimuth_cos(d, sun)), 0.0).rgb;
      e += l * cz * sz * d_el * d_az;
    }
  }
  sky_light.irradiance_up = vec4f(e, 1.0);
  sky_light.sun_at_eye = vec4f(transmittance_to_top(table_b, lut_sampler, r, sun.y), 1.0);
}
