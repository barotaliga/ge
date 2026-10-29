// THE SEA OF FOG IN THE VALLEYS (render/valley_fog.hpp, `frame.valley`): its depth along a ray in
// closed form under a level, wavy top, and the light on it. Included by frame.wgsl; `haze` calls
// `valley_fog` before the air's haze, the sky `valley_fog` and the cloud `valley_fog_over`. With
// no fog (`frame.valley[0].y` 0) the colour is returned as it came.
//
// ge:summary The valley fog in WGSL: its depth along a ray in closed form, its wavy, lit top
// ge:keywords render valley-fog sea-of-fog valley_fog valley_fog_over valley_normal optical-depth
// ge:skill render

// render/valley_fog.hpp's kValleyGloom: the light under the deepest fog against its top's.
const kValleyGloom = 0.3;
// The farthest the top's wave is read, m (a ray that never crosses the top reads it there).
const kValleyFar = 20000.0;

// render/valley_fog.hpp's valley_share and valley_column, the top `t` and the edge `e` over the
// eye.
fn valley_share(t: f32, e: f32, y: f32) -> f32 {
  return clamp((t + 0.5 * e - y) / e, 0.0, 1.0);
}

fn valley_column(t: f32, e: f32, y: f32) -> f32 {
  let hi = t + 0.5 * e;
  if (y >= hi) {
    return 0.0;
  }
  if (y >= t - 0.5 * e) {
    return (hi - y) * (hi - y) / (2.0 * e);
  }
  return t - y;
}

// How far along `d` (camera-relative, unit) the ray reaches the top, out to `dist_m` (and
// kValleyFar): where its wave is read and its light is.
fn valley_reach(d: vec3f, dist_m: f32) -> f32 {
  let top = frame.valley[0].x;
  var l = min(dist_m, kValleyFar);
  if (abs(d.y) > 1e-4 && top / d.y > 0.0) {
    l = min(l, top / d.y);
  }
  return l;
}

// The noise the top waves on at `p` (its tile's share), 0..1: two octaves of the cloud's.
fn valley_noise(p: vec2f) -> f32 {
  let a = textureSampleLevel(cloud_noise, linear_repeat, vec3f(p, 0.5), 0.0).r;
  let b = textureSampleLevel(cloud_noise, linear_repeat, vec3f(p * 2.7 + 0.37, 0.25), 0.0).r;
  return 0.65 * a + 0.35 * b;
}

// The billows' normal at `p`: the first octave's slope over two of its texels (its finer detail
// would speckle the top, and flicker at range), laid level from kValleyCalm tiles off to
// kValleyFlat, where the waves are under a pixel.
const kValleyCalm = 3.0;
const kValleyFlat = 10.0;

fn valley_normal(p: vec2f, l: f32) -> vec3f {
  let k = 1.0 - smoothstep(kValleyCalm, kValleyFlat, l * frame.valley[1].x);
  if (k <= 0.0) {
    return vec3f(0.0, 1.0, 0.0);
  }
  let e = 2.0 / 64.0;
  let h = textureSampleLevel(cloud_noise, linear_repeat, vec3f(p, 0.5), 0.0).r;
  let hx = textureSampleLevel(cloud_noise, linear_repeat, vec3f(p + vec2f(e, 0.0), 0.5), 0.0).r;
  let hz = textureSampleLevel(cloud_noise, linear_repeat, vec3f(p + vec2f(0.0, e), 0.5), 0.0).r;
  // Metres of top a metre across: the wave's span over the step.
  let g = 2.0 * 0.65 * frame.valley[0].w * frame.valley[1].x / e * vec2f(hx - h, hz - h);
  return normalize(vec3f(-g.x * k, 1.0, -g.y * k));
}

// THE SUN ON A BILLOW: the light the fog sends back is scattered many times inside it, so a
// billow turned from the sun is lit through it (a wrapped cosine, kValleyWrap of the way round):
// a soft relief, never a black shade; a level top keeps a level face's light.
const kValleyWrap = 0.6;

fn valley_wrap(c: f32, level: f32) -> f32 {
  let w = max((c + kValleyWrap) / (1.0 + kValleyWrap), 0.0);
  let flat = max((level + kValleyWrap) / (1.0 + kValleyWrap), 1e-3);
  return w * max(level, 0.0) / flat;
}

// Along `d` out to `dist_m`: the fog's light (rgb) and the share of what is behind it that is
// seen (a).
fn valley_at(d: vec3f, dist_m: f32) -> vec4f {
  let v = frame.valley[0];
  let l = valley_reach(d, dist_m);
  // The top over the eye where the ray meets it: its mean, raised and lowered on the noise.
  let p = d.xz * (l * frame.valley[1].x) + frame.valley[1].yz;
  let t = v.x + v.w * (2.0 * valley_noise(p) - 1.0);
  let end = d.y * dist_m;
  var tau = 0.0;
  if (abs(d.y) < 1e-4) {
    tau = v.y * dist_m * valley_share(t, v.z, 0.5 * end);
  } else {
    tau = v.y * abs(valley_column(t, v.z, 0.0) - valley_column(t, v.z, end)) / abs(d.y);
  }
  if (tau < 1e-5) {
    return vec4f(0.0, 0.0, 0.0, 1.0);
  }
  // The light on the top where the ray meets it: the sun on its billows past the cloud's
  // shadow, and the sky over them; dimmed under the top by the eye's depth in it.
  let n = valley_normal(p, l);
  let xz = frame.eye.xz + d.xz * l;
  let sun = sun_light(frame.eye.y + t, xz) * valley_wrap(dot(n, frame.sun.xyz), frame.sun.y);
  let lit = frame.valley[1].w * (sun + sky_irradiance(n)) / PI;
  let gloom = max(exp(-v.y * max(t, 0.0)), kValleyGloom);
  return vec4f(lit * gloom, exp(-tau));
}

// A colour at `dist_m` (1e30 for the sky) seen through the fog at screen `uv`.
fn valley_fog(colour: vec3f, uv: vec2f, dist_m: f32) -> vec3f {
  if (frame.valley[0].y <= 0.0) {
    return colour;
  }
  let v = valley_at(view_ray(frame, uv), dist_m);
  return colour * v.a + v.rgb * (1.0 - v.a);
}

// The same over a premultiplied colour (a cloud's light and cover blended over the sky, which the
// sky pass has already seen through it): the cover takes its share of the fog's light.
fn valley_fog_over(c: vec4f, uv: vec2f, dist_m: f32) -> vec4f {
  if (frame.valley[0].y <= 0.0) {
    return c;
  }
  let v = valley_at(view_ray(frame, uv), dist_m);
  return vec4f(c.rgb * v.a + v.rgb * (1.0 - v.a) * c.a, c.a);
}
