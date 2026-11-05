// THE FAR LAND'S DRAW (render/far_land.hpp): the polar mesh's vertices pulled from a storage
// buffer, each pixel's ground picked by its height, its slope, its moisture and the air's
// temperature there (the valley's cooled at the lapse rate): rock (banded where the kind has
// strata), grass or dry grass by the moisture, forest under the tree line, sand at the shore,
// snow where the air is under freezing and the slope holds it. Lit by the key light past the
// ranges' own horizons and by the sky by the share of it each point sees; the water a mirror of
// the sky with the sun's glint (ice once its air is well under freezing; a kind's lava a crust
// with its light through the breaks). Then the frame's haze, and past the aerial volume's reach
// the kind's own far air closing toward the sky at the horizon.
//
// ge:summary The far land's draw: ground by height, slope and frost; water, ice or lava; haze
// ge:keywords render far-land wgsl snowline lapse tree-line strata horizon-shadow water-glint
// ge:keywords lava far-haze sky-share
// ge:skill render
#include "render/wgsl/frame.wgsl"

struct FarParams {
  eye: vec4f,      // the eye from the site's middle (y the world's), m; w the time, s
  site: vec4f,     // the valley floor's y, the water's y (world m), 1 for lava, its glow
  climate: vec4f,  // the valley's temperature (C), the lapse (K/m), the tree line's y, strata m
  far: vec4f,      // the far air's visual range m, the towers' share, the site's world x, z
  colours: array<vec4f, 9>,  // rock, rock2, grass, dry, forest, snow, sand, deep, shallow
};

struct FarVertex {
  pos: vec4f,       // from the site's middle (y the world's); w the moisture, -1 water
  n: vec4f,         // the normal; w the share of the sky it sees
  horizon: vec2u,   // eight bearings' horizon elevations, a byte each (0 .. pi/2)
  pad: vec2u,
};

@group(1) @binding(0) var<uniform> land: FarParams;
@group(1) @binding(1) var<storage, read> far_vertices: array<FarVertex>;

const kFarHalfPi = 1.5707963;
const kFarIce = -1.0;  // C: the water's air under this, it is ice
const kFarLavaWhite = 1.2;  // the lava's breaks at most this many times the display's white

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) rel: vec3f,    // camera-relative, m
  @location(1) local: vec2f,  // from the site's middle on the plan, m
  @location(2) n: vec3f,
  @location(3) moist: f32,
  @location(4) open: f32,
  @location(5) h0: vec4f,     // the horizons, rad: bearings 0 .. 3
  @location(6) h1: vec4f,     // bearings 4 .. 7
};

fn far_unpack(w: u32) -> vec4f {
  return vec4f(f32(w & 0xffu), f32((w >> 8u) & 0xffu), f32((w >> 16u) & 0xffu),
               f32(w >> 24u)) * (kFarHalfPi / 255.0);
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VertexOut {
  let v = far_vertices[vid];
  var out: VertexOut;
  out.rel = v.pos.xyz - land.eye.xyz;
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  out.local = v.pos.xz;
  out.n = v.n.xyz;
  out.moist = v.pos.w;
  out.open = v.n.w;
  out.h0 = far_unpack(v.horizon.x);
  out.h1 = far_unpack(v.horizon.y);
  return out;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
};

fn far_colour(i: i32) -> vec3f {
  return land.colours[i].rgb;
}

// The cloud noise as a detail at plan `xz` over `span` m, faded to its mean past `fade` m where a
// pixel spans more than a texel.
fn far_detail(xz: vec2f, span: f32, seed: f32, dist: f32, fade: f32) -> f32 {
  let n = textureSampleLevel(cloud_noise, linear_repeat, vec3f(xz / span, seed), 0.0).r;
  return mix(n, 0.5, smoothstep(0.5 * fade, fade, dist));
}

// The mesh's normal bent by the relief its cells are too coarse to hold: the cloud noise's slope
// over 900 m and 220 m, faded out where a pixel spans its texels.
fn far_relief(n: vec3f, xz: vec2f, dist: f32) -> vec3f {
  var g = vec2f(0.0);
  let spans = array<f32, 2>(900.0, 220.0);
  let fades = array<f32, 2>(40000.0, 9000.0);
  for (var k = 0; k < 2; k++) {
    let e = spans[k] / 64.0;
    let a = far_detail(xz, spans[k], 0.17 + 0.3 * f32(k), dist, fades[k]);
    let bx = far_detail(xz + vec2f(e, 0.0), spans[k], 0.17 + 0.3 * f32(k), dist, fades[k]);
    let bz = far_detail(xz + vec2f(0.0, e), spans[k], 0.17 + 0.3 * f32(k), dist, fades[k]);
    g += vec2f(bx - a, bz - a) * (0.35 * spans[k] / e) / spans[k] * 8.0;
  }
  return normalize(n - vec3f(g.x, 0.0, g.y) * 0.35);
}

// The key light's share past the ranges: its elevation against the horizon toward it.
fn far_shadow(in: VertexOut) -> f32 {
  let s = frame.sun.xyz;
  let e = asin(clamp(s.y, -1.0, 1.0));
  var b = atan2(s.x, s.z) / (2.0 * PI) * 8.0;
  b = b - floor(b / 8.0) * 8.0;
  let k0 = u32(floor(b)) % 8u;
  let k1 = (k0 + 1u) % 8u;
  let h = array<f32, 8>(in.h0.x, in.h0.y, in.h0.z, in.h0.w, in.h1.x, in.h1.y, in.h1.z, in.h1.w);
  let a = mix(h[k0], h[k1], fract(b));
  return smoothstep(a - 0.015, a + 0.025, e);
}

// The ground's albedo at a pixel.
fn far_ground(in: VertexOut, n: vec3f, y: f32, xz: vec2f, dist: f32) -> vec3f {
  let d1 = far_detail(xz, 2600.0, 0.31, dist, 60000.0);
  let d2 = far_detail(xz, 280.0, 0.73, dist, 4000.0);
  let slope = 1.0 - n.y;
  // The rock: two tones by a noise, or by bands where the kind has strata.
  var rock = mix(far_colour(0), far_colour(1), smoothstep(0.35, 0.65, d2 + 0.3 * (d1 - 0.5)));
  let strata = land.climate.w;
  if (strata > 0.0) {
    let band = 0.5 + 0.5 * sin((y + 60.0 * d1 + 0.3 * strata * d2) / strata * 2.0 * PI);
    rock = mix(rock, far_colour(1), 0.55 * smoothstep(0.35, 0.75, band));
  }
  // The cover on what is not too steep: grass by the moisture, the forest under the tree line,
  // the sand by the water.
  let wet = clamp(in.moist + 0.5 * (d1 - 0.5) + 0.2 * (d2 - 0.5), 0.0, 1.0);
  var cover = mix(far_colour(3), far_colour(2), smoothstep(0.35, 0.6, wet));
  let tree_y = land.climate.z + 250.0 * (d1 - 0.5);
  let tree = (1.0 - smoothstep(tree_y - 120.0, tree_y, y)) * (1.0 - smoothstep(0.3, 0.5, slope)) *
             smoothstep(0.45, 0.62, wet);
  cover = mix(cover, far_colour(4), tree);
  if (land.site.y > -1e29) {
    let shore = 1.0 - smoothstep(1.0, 6.0 + 12.0 * d2, y - land.site.y);
    cover = mix(cover, far_colour(6), shore * (1.0 - tree));
  }
  // The cover thins to bare rock on what is steep, and from 500 m over the tree line (a kind
  // with no forest grows its cover wherever the slope holds it).
  let bare = smoothstep(land.climate.z + 500.0, land.climate.z + 1000.0, y + 300.0 * (d1 - 0.5));
  let gentle = (1.0 - smoothstep(0.22, 0.42, slope + 0.15 * (d2 - 0.5))) * (1.0 - bare);
  var albedo = mix(rock, cover, gentle);
  // The snow: where the air is under freezing, on what is not too steep to hold it; on the
  // forest a dusting through the crowns.
  let t = land.climate.x - land.climate.y * (y - land.site.x) + 5.0 * (d1 - 0.5) + 2.0 * (d2 - 0.5);
  let cold = smoothstep(1.0, -1.5, t);
  let holds = 1.0 - smoothstep(0.62, 0.88, slope + 0.25 * (d2 - 0.5));
  let snow = cold * holds * (1.0 - 0.55 * tree * gentle);
  albedo = mix(albedo, far_colour(5), snow);
  return albedo;
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let dist = max(length(in.rel), 1e-3);
  let v = -in.rel / dist;
  let xz = in.local + land.far.zw;
  let y = in.rel.y + land.eye.y;
  let key = sun_light(y, xz) * far_shadow(in);
  var c: vec3f;
  if (in.moist < -0.5) {
    // THE WATER (or its ice, or the kind's lava).
    let t = land.climate.x - land.climate.y * (y - land.site.x);
    let up = vec3f(0.0, 1.0, 0.0);
    if (land.site.z > 0.5) {
      let crack = far_detail(xz + vec2f(0.4, 0.25) * land.eye.w, 90.0, 0.11, dist, 30000.0);
      let breaks = smoothstep(0.52, 0.7, crack);
      let crust = far_colour(7) / PI * (key * max(frame.sun.y, 0.0) + sky_irradiance(up));
      // Its light, held to a few times the display's white for the eye's exposure: at night the
      // eye adapted to the moon would see 5000 cd/m^2 as a white-out over the whole frame.
      let glow = min(land.site.w, kFarLavaWhite / max(exposure(), 1e-6));
      c = crust + far_colour(8) * glow * (0.12 + 0.88 * breaks);
    } else if (t < kFarIce) {
      let ice = mix(far_colour(5), far_colour(8), 0.35);
      c = ice / PI * (key * max(frame.sun.y, 0.0) + sky_irradiance(up));
      let f = 0.02 + 0.98 * pow(1.0 - max(v.y, 0.0), 5.0);
      c = mix(c, sky_luminance(reflect(-v, up)), 0.6 * f);
    } else {
      // Ripples, calmer with distance (a far pixel spans many: their mean is level).
      let calm = 1.0 - smoothstep(150.0, 3000.0, dist);
      let w1 = far_detail(mat2x2f(0.8, 0.6, -0.6, 0.8) * xz + vec2f(0.7, 0.3) * land.eye.w, 47.0, 0.47, dist, 4000.0);
      let w2 = far_detail(mat2x2f(0.6, -0.8, 0.8, 0.6) * xz - vec2f(0.2, 0.6) * land.eye.w, 19.0, 0.59, dist, 2000.0);
      let n = normalize(vec3f((w1 - 0.5) * 0.1 * calm, 1.0, (w2 - 0.5) * 0.1 * calm));
      var r = reflect(-v, n);
      r.y = abs(r.y);
      let f = 0.02 + 0.98 * pow(1.0 - max(dot(n, v), 0.0), 5.0);
      let body = mix(far_colour(7), far_colour(8), 0.25 * calm) / PI *
                 (key * max(frame.sun.y, 0.0) + sky_irradiance(up));
      let glint = key * pow(max(dot(r, frame.sun.xyz), 0.0), 600.0) * 40.0 * f;
      c = body * (1.0 - f) + sky_luminance(r) * f + glint;
    }
  } else {
    // THE GROUND.
    let n = far_relief(normalize(in.n), xz, dist);
    let albedo = far_ground(in, n, y, xz, dist);
    let open = mix(0.35, 1.0, in.open);
    c = albedo / PI * (key * max(dot(n, frame.sun.xyz), 0.0) + sky_irradiance(n) * open);
  }
  // The frame's haze, then the kind's far air past the aerial volume's reach.
  let uv = in.position.xy * frame.size.zw;
  c = haze(c, uv, dist);
  let beyond = max(dist - kAerialRangeKm * 1000.0, 0.0);
  let veil = 1.0 - exp(-beyond / max(land.far.x, 1.0));
  let d = -v;
  let toward = normalize(vec3f(d.x, max(d.y, 0.0) + 0.015, d.z));
  c = mix(c, sky_luminance(toward), veil);
  var out: FragmentOut;
  out.colour = vec4f(min(c, vec3f(60000.0)), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}
