// THE CLOUD PASS (render/clouds.hpp): each pixel's ray marched through every layer of
// `frame.clouds` it crosses, nearest first, up to what the depth target holds, and the cloud laid
// over the HDR target premultiplied. A layer's density at a point is its column (frame.wgsl's
// `cloud_column`) shaped by the height in its slab (a heap's top grows into the cell, a sheet
// thickens about its middle) and eaten at its edge by the noise read again at a quarter of the
// cell. The light on it is the key light through the air (`sun_light_open`), through the other
// layers above (their shadow at the layer), through its own cloud toward the light (four reads
// at growing steps: Beer's law and a slower lobe for the light scattered on), and the sky's and
// the snow's light from above and below; far cloud is hazed and melts into the sky behind it.
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
// The detail's scale (cells of the column a detail cell), and how deep it eats the edge.
const kDetailScale = 4.0;
const kErode = 0.6;
// The share of the extinction the light scattered on through a cloud dies at (1 - g).
const kReduced = 0.15;
// The orders of scattering past the first, summed, over the one lobe that stands for them.
const kOrders = 3.0;

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
fn cloud_density(l: CloudRows, p: vec3f, detail: bool) -> f32 {
  let h = (p.y - l.shape.x) / (l.shape.y - l.shape.x);
  if (h <= 0.0 || h >= 1.0) {
    return 0.0;
  }
  let c = cloud_column(l, p.xz);
  if (c <= 0.0) {
    return 0.0;
  }
  // A heap: a flat base, its top the taller the fuller the column. A sheet: about its middle.
  let heap = clamp((sqrt(clamp(c * 2.5, 0.0, 1.0)) - h) * 3.0, 0.0, 1.0) * clamp(h * 12.0, 0.0, 1.0);
  let sheet = clamp((c - abs(2.0 * h - 1.0)) * 3.0, 0.0, 1.0);
  let occ = mix(sheet, heap, l.look.y);
  if (occ <= 0.0) {
    return 0.0;
  }
  var dens = occ;
  if (detail) {
    var q = cloud_uvw(l, p.xz) * kDetailScale;
    q.z = p.y * l.shape.w / kCloudTileCells * kDetailScale + l.wind.w;
    // Two reads, the second at twice the scale, stretched to the noise's full range.
    let d1 = textureSampleLevel(cloud_noise, linear_repeat, q, 0.0).r;
    let d2 = textureSampleLevel(cloud_noise, linear_repeat, q * 2.03, 0.0).r;
    let d = smoothstep(0.3, 0.7, 0.65 * d1 + 0.35 * d2);
    // Eaten harder toward the top, where a heap boils.
    let e = kErode * mix(0.7, 1.4, h);
    dens = clamp((occ - e * d) / (1.0 - e * d), 0.0, 1.0);
  }
  return dens * l.look.z;
}

// Henyey-Greenstein's phase function, per steradian.
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
      tau += cloud_column(l, p.xz + frame.sun.xz * ((mid - p.y) / frame.sun.y)) * l.look.w;
    }
  }
  return exp(-tau);
}

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
  let steps = max(4, i32(round(f32(full) * frame.cloud_info.y)));
  let dt = (t1 - t0) / f32(steps);
  let L = frame.sun.xyz;
  let c = dot(d, L);
  // The phase: a forward and a back lobe, and a wider one for the light scattered on.
  let ph = 0.75 * hg(c, 0.6) + 0.25 * hg(c, -0.25);
  let ph_ms = hg(c, 0.2);
  // The light at the layer: the key's through the air and the layers above at its middle; the
  // sky's from above, the snow's from below.
  let mid_p = eye + d * (0.5 * (t0 + t1));
  let mid_y = 0.5 * (l.shape.x + l.shape.y);
  let key = sun_light_open(mid_y) * shadow_from(vec3f(mid_p.x, mid_y, mid_p.z), i);
  let sky = (clear_sky_up() + dark_light()) * mix(1.0, deck_zenith_share(), frame.weather.y);
  let ground = kGroundAlbedo * (sun_light_open(0.0) * max(L.y, 0.0) * 0.6 + sky);
  // The light's reads, as shares of the slab's depth.
  let ds = array<f32, 4>(0.05, 0.1, 0.2, 0.4);
  var out: LayerLook;
  out.light = vec3f(0.0);
  out.through = 1.0;
  var weight = 0.0;
  var wdist = 0.0;
  for (var k = 0; k < steps; k++) {
    let t = t0 + (f32(k) + j) * dt;
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
    // Thin cloud seen with the light behind the eye is darker at its edge (the powder).
    let powder = mix(1.0, 1.0 - exp(-sigma * 120.0), clamp(0.5 - 0.5 * c, 0.0, 1.0));
    // The light scattered on mostly keeps going forward (a droplet's lobe, g about 0.85), so it
    // dies only at the reduced extinction sigma (1 - g); and its many orders together leave a
    // thick cloud, which absorbs almost nothing, as bright as fresh snow in the light: what keeps
    // a heap's body white.
    let direct = key * (exp(-tau) * ph + kOrders * exp(-kReduced * tau) * ph_ms) * powder;
    let ambient = sky * (mix(0.35, 1.0, h) / (2.0 * PI)) + ground * ((1.0 - h) * 0.3 / (2.0 * PI));
    let step_t = exp(-sigma * dt);
    let gain = out.through * (1.0 - step_t);
    out.light += (direct + ambient) * gain;
    weight += gain;
    wdist += gain * t;
    out.through *= step_t;
    if (out.through < 0.01) {
      break;
    }
  }
  out.dist = select(0.5 * (t0 + t1), wdist / max(weight, 1e-6), weight > 0.0);
  return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let n = i32(frame.cloud_info.x);
  let d = view_ray(frame, in.uv);
  // How far the ray goes before the scene: reversed-z, 0 is the sky at infinity.
  let z = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  var scene = 1e9;
  if (z > 0.0) {
    let ndc = vec2f(in.uv.x * 2.0 - 1.0, 1.0 - in.uv.y * 2.0);
    let q = frame.inv_view_proj * vec4f(ndc, z, 1.0);
    scene = length(q.xyz / q.w);
  }
  // Each layer's span along the ray.
  var t0 = array<f32, 3>(0.0, 0.0, 0.0);
  var t1 = array<f32, 3>(0.0, 0.0, 0.0);
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
  let j = dither(in.position.xy);
  var light = vec3f(0.0);
  var through = 1.0;
  var done = array<bool, 3>(false, false, false);
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
    let look = march(i, d, t0[i], t1[i], j);
    let alpha = 1.0 - look.through;
    if (alpha <= 1e-4) {
      continue;
    }
    // Seen through the air to where its light comes from, and lost into the sky far off.
    let fade = 1.0 - smoothstep(kFadeNear, kFadeFar, look.dist);
    let hazed = haze(look.light / alpha, in.uv, look.dist) * alpha;
    light += through * fade * hazed;
    through *= 1.0 - fade * alpha;
    if (through < 0.01) {
      break;
    }
  }
  return vec4f(light, 1.0 - through);
}
