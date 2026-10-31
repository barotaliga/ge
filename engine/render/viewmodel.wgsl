// THE VIEW-MODEL PASS (render/viewmodel.hpp): the held weapon, its vertices pulled by index from
// the shared buffers, its two frames blended, placed into the view's frame by its placement and
// projected by its own lens into its own depth range; lit per pixel as viewmodel::shade_held
// lights it (the ambient from above a little more than below, a wrapped Lambert and a Blinn
// highlight from the probe's direct light and each light the game kept, the floor), with the
// frame's sun through its shadow maps and its sky by the draw's outdoor share, and the frame's
// clustered lamps, all in the held unit (1: a white surface lit white), then put on the HDR target
// at the held white. `fs_resolved` writes the same resolved as viewmodel::resolve does.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/sun_shadow.wgsl"

struct HeldModel {
  at: vec4u,    // the model's frame 0, vertex 0 (two vec4 each); its vertex count; its first
                // index; its first attribute
  rect: vec4u,  // its skin in the atlas: x, y, w, h (texels); w 0: no skin, a colour a vertex
  more: vec4u,  // its first triangle's flag; 0; 0; 0
}

struct HeldDraw {
  now: array<vec4f, 3>,   // the placement's rows: the held frame into the view's
  last: array<vec4f, 3>,  // a frame ago
  lens: vec4f,            // 1 / tan(x / 2), 1 / tan(y / 2), the gun's near plane, the depth floor
  info: vec4u,            // frame a, frame b, model, flags (1: lamps, 2: hide the flame, 4: the hands)
  mix: vec4f,             // the frames' share, outdoor, white (lx; 0: the eye's), floor
  ambient: vec4f,         // rgb; gloss
  direct: vec4f,          // rgb; shininess
  toward: vec4f,          // view, unit; the held lights' count
  right: vec4f,           // the view's right (camera-relative world); the lens ratio
  up: vec4f,
  forward: vec4f,
  lights: array<vec4f, 16>,  // a light: at (view), range; rgb, radius
}

@group(1) @binding(0) var<storage, read> models: array<HeldModel>;
@group(1) @binding(1) var<storage, read> vertices: array<vec4f>;
@group(1) @binding(2) var<storage, read> attrs: array<vec4f>;
@group(1) @binding(3) var<storage, read> indices: array<u32>;
@group(1) @binding(4) var<storage, read> flags: array<u32>;
@group(1) @binding(5) var<storage, read> draws: array<HeldDraw>;
@group(1) @binding(6) var atlas: texture_2d<f32>;
@group(2) @binding(0) var<uniform> shadow: SunShadow;
@group(2) @binding(1) var shadow_maps: texture_depth_2d_array;
@group(2) @binding(2) var shadow_cmp: sampler_comparison;

const kWrap = 0.25;  // viewmodel/light.cpp's
const kKnee = 0.8;   // viewmodel/raster.cpp's resolve

fn place(m: array<vec4f, 3>, p: vec3f, w: f32) -> vec3f {
  let h = vec4f(p, w);
  return vec3f(dot(m[0], h), dot(m[1], h), dot(m[2], h));
}

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) view: vec3f,       // the point in the view's frame, m
  @location(1) last_view: vec3f,  // the same a frame ago
  @location(2) normal: vec3f,     // view
  @location(3) attr: vec3f,       // uv, or the colour
  @location(4) @interpolate(flat) draw: u32,
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) k: u32) -> VertexOut {
  let d = draws[k];
  let m = models[d.info.z];
  let idx = indices[m.at.z + vid];
  let va = 2u * (m.at.x + d.info.x * m.at.y + idx);
  let vb = 2u * (m.at.x + d.info.y * m.at.y + idx);
  let pos = mix(vertices[va].xyz, vertices[vb].xyz, d.mix.x);
  let nb = mix(vertices[va + 1u].xyz, vertices[vb + 1u].xyz, d.mix.x);
  var out: VertexOut;
  out.view = place(d.now, pos, 1.0);
  out.last_view = place(d.last, pos, 1.0);
  out.normal = place(d.now, nb / max(length(nb), 1e-6), 0.0);
  out.attr = attrs[m.at.w + idx].xyz;
  out.draw = k;
  // The gun's own lens and the frame's jitter; depth near / z (reversed, infinite far), which the
  // viewport squeezes into the gun's range.
  let p = out.view;
  let jitter = vec2f(2.0 * frame.jitter.z, -2.0 * frame.jitter.w);
  out.position = vec4f(p.xy * d.lens.xy + jitter * p.z, d.lens.z, p.z);
  // The model's own flash (flag 1) and the hands holding it (flag 2), each clipped away when the
  // draw hides it (info.w 2 and 4).
  let tri = flags[m.more.x + vid / 3u];
  if (((d.info.w >> 1u) & tri) != 0u) {
    out.position = vec4f(0.0, 0.0, -1.0, 1.0);
  }
  return out;
}

fn srgb_decode(c: vec3f) -> vec3f {
  let lo = c / 12.92;
  let hi = pow((c + 0.055) / 1.055, vec3f(2.4));
  return select(hi, lo, c <= vec3f(0.04045));
}

fn power(x: f32, n: i32) -> f32 {
  var r = 1.0;
  var b = x;
  var e = n;
  loop {
    if (e <= 0) {
      break;
    }
    if ((e & 1) != 0) {
      r *= b;
    }
    b *= b;
    e = e >> 1u;
  }
  return r;
}

// viewmodel::light_falloff.
fn falloff(dist: f32, range_m: f32, radius_m: f32) -> f32 {
  if (range_m <= 0.0 || dist >= range_m) {
    return 0.0;
  }
  let q = dist / range_m;
  let q4 = q * q * q * q;
  let w = (1.0 - q4) * (1.0 - q4);
  return w / (dist * dist + radius_m * radius_m);
}

struct Shade {
  diffuse: vec3f,
  specular: vec3f,
}

fn add_light(s: ptr<function, Shade>, dir: vec3f, rgb: vec3f, n: vec3f, v: vec3f, d: HeldDraw) {
  let nl = dot(n, dir);
  let wrap = (nl + kWrap) / (1.0 + kWrap);
  if (wrap <= 0.0) {
    return;
  }
  let h = normalize(dir + v);
  let nh = dot(n, h);
  var spec = 0.0;
  if (nl > 0.0 && nh > 0.0) {
    spec = d.ambient.w * power(nh, i32(d.direct.w)) * nl;
  }
  (*s).diffuse += rgb * wrap;
  (*s).specular += rgb * spec;
}

fn to_view(d: HeldDraw, w: vec3f) -> vec3f {
  return vec3f(dot(w, d.right.xyz), dot(w, d.up.xyz), dot(w, d.forward.xyz));
}

fn to_world(d: HeldDraw, v: vec3f) -> vec3f {
  return d.right.xyz * v.x + d.up.xyz * v.y + d.forward.xyz * v.z;
}

// The radiance (frame units) of a white surface the held unit's 1 lights.
fn held_white(d: HeldDraw) -> f32 {
  if (d.mix.z > 0.0) {
    return d.mix.z * frame.world.y / (kSunLux * PI);
  }
  return white_luminance();
}

// The pixel in the held unit: the surface's colour times its light, or a glowing texel's own.
fn held_colour(in: VertexOut) -> vec3f {
  let d = draws[in.draw];
  let m = models[d.info.z];
  var base = in.attr;
  if (m.rect.z > 0u) {
    let at = fract(in.attr.xy) * vec2f(m.rect.zw);
    let t = clamp(vec2u(at), vec2u(0u), m.rect.zw - 1u);
    let c = textureLoad(atlas, vec2i(m.rect.xy + t), 0);
    base = srgb_decode(c.rgb);
    if (c.a > 0.5) {
      return base;
    }
  }
  let n = normalize(in.normal);
  let p = in.view;
  let v = normalize(-p);
  var s: Shade;
  s.diffuse = d.ambient.rgb * (0.8 + 0.2 * n.y);
  s.specular = vec3f(0.0);
  add_light(&s, d.toward.xyz, d.direct.rgb, n, v, d);
  let count = u32(d.toward.w);
  for (var i = 0u; i < count; i++) {
    let at = d.lights[2u * i];
    let rgb = d.lights[2u * i + 1u];
    let to = at.xyz - p;
    let a = falloff(length(to), at.w, rgb.w);
    if (a > 0.0) {
      add_light(&s, normalize(to), rgb.rgb * a, n, v, d);
    }
  }
  // The frame's light, from irradiance (frame units) into the held unit.
  let per = 1.0 / (PI * held_white(d));
  let rel = to_world(d, p);
  let nw = to_world(d, n);
  let outdoor = d.mix.y;
  if (outdoor > 0.0) {
    // The sun's shadow where the gun is seen: the point moved across onto its screen ray.
    let seen = to_world(d, vec3f(p.xy * d.right.w, p.z));
    let lit = sun_shadow(shadow, shadow_maps, shadow_cmp, seen, nw, in.position.xy, frame.time.y);
    let sun = sun_light_open(rel.y + frame.eye.y) * (lit * outdoor * per);
    add_light(&s, to_view(d, frame.sun.xyz), sun, n, v, d);
    s.diffuse += sky_irradiance(nw) * (outdoor * per);
  }
  if ((d.info.w & 1u) != 0u && light_grid.info.x > 0u) {
    // The cluster the gun's pixel looks into, at the gun's depth along the view.
    let cell = light_grid.cells[cluster_index(vec3f(in.position.xy, light_grid.grid.z / p.z))];
    for (var k = 0u; k < cell.y; k++) {
      var l: vec3f;
      let e = lamp_irradiance(light_grid.refs[cell.x + k], rel, &l);
      add_light(&s, to_view(d, l), e * per, n, v, d);
    }
  }
  s.diffuse = max(s.diffuse, vec3f(d.mix.w));
  return base * s.diffuse + s.specular;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // rg: the screen motion (uv), b: reactive (0: a solid surface)
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  let d = draws[in.draw];
  var out: FragmentOut;
  out.colour = vec4f(held_colour(in) * held_white(d), 1.0);
  // Where the point was a frame ago through the gun's lens, unjittered.
  let q = in.last_view;
  let was = vec2f(q.x * d.lens.x / q.z * 0.5 + 0.5, 0.5 - q.y * d.lens.y / q.z * 0.5);
  out.motion = vec4f(in.position.xy * frame.size.zw - frame.jitter.zw - was, 0.0, 0.0);
  return out;
}

// viewmodel::resolve at an exposure of 1: the roll-off past the knee, then the sRGB curve.
@fragment
fn fs_resolved(in: VertexOut) -> @location(0) vec4f {
  var c = held_colour(in);
  let e = max(c - kKnee, vec3f(0.0));
  c = select(c, kKnee + e / (1.0 + e / (1.0 - kKnee)), c > vec3f(kKnee));
  c = clamp(c, vec3f(0.0), vec3f(1.0));
  let lo = 12.92 * c;
  let hi = 1.055 * pow(c, vec3f(1.0 / 2.4)) - 0.055;
  return vec4f(select(hi, lo, c <= vec3f(0.0031308)), 1.0);
}
