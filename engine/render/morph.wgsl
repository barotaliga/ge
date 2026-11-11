// MORPH MESHES DRAWN (render/morph.hpp): one draw a model instance, its vertices pulled by index
// from the shared buffers, its two frames blended (and the last frame's two, for its motion),
// its skin read from the atlas texel by texel with each texel's mark resolved: a tinted texel's
// colour comes off the instance's ramp at the texel's step, a glowing one lights itself, a hole
// (mark 2) is not drawn: the texel the point falls in decides, sharp or smooth. Sharp
// (the texel the point falls in) or smooth (the four nearest, each resolved, blended).
// A WARPED draw (MorphWarp: shown under 1) dissolves by render/wgsl/warp.wgsl's field, the band at
// its cut turned to ash, pinched and shimmering as its knobs say.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/warp.wgsl"

struct Model {
  first_vertex: u32,  // the model's frame 0, vertex 0, in vertices (two vec4 each)
  vertex_count: u32,
  first_index: u32,
  first_uv: u32,
  rect: vec4u,  // its skin in the atlas: x, y, w, h (texels)
}

struct Draw {
  now_place: vec4f,  // camera-relative position, m; scale
  now_rotation: vec4f,
  last_place: vec4f,  // the last frame's, relative to this frame's eye; its scale
  last_rotation: vec4f,
  frames: vec4u,  // now a, now b, last a, last b
  blends: vec4f,  // now's share, last's share, glow, outdoor
  info: vec4u,    // model, tint row A, tint row B, 1 if smooth
  ambient: vec4f,
  shade: vec4f,
  shade_dir: vec4f,
  warp: Warp,  // render/wgsl/warp.wgsl (MorphWarp)
  burn: vec4f,  // charred, embers, etched, seed (MorphBurn)
}

const kTintSteps = 16u;

@group(1) @binding(0) var<storage, read> models: array<Model>;
@group(1) @binding(1) var<storage, read> vertices: array<vec4f>;
@group(1) @binding(2) var<storage, read> uvs: array<vec2f>;
@group(1) @binding(3) var<storage, read> indices: array<u32>;
@group(1) @binding(4) var<storage, read> draws: array<Draw>;
@group(1) @binding(5) var<storage, read> ramps: array<vec4f>;
@group(1) @binding(6) var atlas: texture_2d<f32>;

fn rotate(q: vec4f, v: vec3f) -> vec3f {
  let t = 2.0 * cross(q.xyz, v);
  return v + q.w * t + cross(q.xyz, t);
}

struct Posed {
  position: vec3f,
  normal: vec3f,
}

fn posed(m: Model, a: u32, b: u32, t: f32, idx: u32) -> Posed {
  let va = 2u * (m.first_vertex + a * m.vertex_count + idx);
  let vb = 2u * (m.first_vertex + b * m.vertex_count + idx);
  var p: Posed;
  p.position = mix(vertices[va].xyz, vertices[vb].xyz, t);
  let n = mix(vertices[va + 1u].xyz, vertices[vb + 1u].xyz, t);
  p.normal = n / max(length(n), 1e-6);
  return p;
}

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) rel: vec3f,
  @location(1) last_rel: vec3f,
  @location(2) normal: vec3f,
  @location(3) uv: vec2f,
  @location(4) @interpolate(flat) draw: u32,
  @location(5) local: vec4f,  // the point in the model's frame times its scale; along the axis, m
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) k: u32) -> VertexOut {
  let d = draws[k];
  let m = models[d.info.x];
  let idx = indices[m.first_index + vid];
  let now = posed(m, d.frames.x, d.frames.y, d.blends.x, idx);
  let last = posed(m, d.frames.z, d.frames.w, d.blends.y, idx);
  var out: VertexOut;
  let off = rotate(d.now_rotation, now.position * d.now_place.w);
  let last_off = rotate(d.last_rotation, last.position * d.last_place.w);
  out.rel = d.now_place.xyz + warp_vertex(d.warp, frame.time.x, off);
  out.last_rel = d.last_place.xyz + warp_vertex(d.warp, frame.time.x, last_off);
  out.local = vec4f(now.position * d.now_place.w, dot(off, d.warp.b.xyz));
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  out.normal = rotate(d.now_rotation, now.normal);
  out.uv = uvs[m.first_uv + idx];
  out.draw = k;
  return out;
}

fn srgb_decode(c: vec3f) -> vec3f {
  let lo = c / 12.92;
  let hi = pow((c + 0.055) / 1.055, vec3f(2.4));
  return select(hi, lo, c <= vec3f(0.04045));
}

// A texel resolved: linear rgb, and a 1 in w when it lights itself.
fn texel(p: vec2i, d: Draw) -> vec4f {
  let c = textureLoad(atlas, p, 0);
  let mark = u32(round(c.a * 255.0));
  let region = mark >> 6u;
  if (region != 0u) {
    let row = select(d.info.y, d.info.z, region == 2u);
    return vec4f(ramps[row * kTintSteps + (mark & 15u)].rgb, 0.0);
  }
  return vec4f(srgb_decode(c.rgb), select(0.0, 1.0, mark == 1u));
}

fn skin_at(uv: vec2f, m: Model, d: Draw) -> vec4f {
  let lo = vec2i(m.rect.xy);
  let hi = vec2i(m.rect.xy + m.rect.zw) - 1;
  let at = uv * vec2f(m.rect.zw);
  if (d.info.w == 0u) {
    return texel(clamp(lo + vec2i(floor(at)), lo, hi), d);
  }
  // Bilinear between the four nearest texels' centres, each resolved first.
  let f = at - 0.5;
  let base = vec2i(floor(f));
  let w = f - floor(f);
  let p00 = clamp(lo + base, lo, hi);
  let p11 = clamp(lo + base + 1, lo, hi);
  let a = texel(p00, d);
  let b = texel(vec2i(p11.x, p00.y), d);
  let c = texel(vec2i(p00.x, p11.y), d);
  let e = texel(p11, d);
  return mix(mix(a, b, w.x), mix(c, e, w.x), w.y);
}

// Whether the texel the point falls in is a hole (kMarkHole).
fn hole_at(uv: vec2f, m: Model) -> bool {
  let lo = vec2i(m.rect.xy);
  let hi = vec2i(m.rect.xy + m.rect.zw) - 1;
  let p = clamp(lo + vec2i(floor(uv * vec2f(m.rect.zw))), lo, hi);
  return u32(round(textureLoad(atlas, p, 0).a * 255.0)) == 2u;
}

// THE BURN (MorphBurn) on a texel's albedo `a` at the model-frame point `p` (m): the char's soot
// in patches spreading as it grows, the etch's bleach in pits; and the embers' own light in the
// char's cracks (w: 1 where a crack glows, its light added by the caller).
const kCharSoot = 0.05;  // the albedo the char leaves of the skin's
fn morph_burn(b: vec4f, a: vec3f, p: vec3f, t: f32) -> vec4f {
  let q = p * 9.0 + b.w * 37.0;
  let n = warp_noise(q) * 0.65 + warp_noise(q * 2.7 + 5.3) * 0.35;
  var c = a;
  var glow = 0.0;
  if (b.z > 0.0) {
    // The acid: the skin bleached toward a sick yellow and pitted, where its patches reach.
    let e = smoothstep(1.05 - b.z, 1.25 - b.z, n);
    let pits = smoothstep(0.55, 0.75, warp_noise(q * 4.1 + 1.7));
    let bleach = mix(vec3f(dot(c, vec3f(0.3, 0.6, 0.1))), vec3f(0.55, 0.52, 0.22), 0.5);
    c = mix(c, bleach * (1.0 - 0.6 * pits), e * min(b.z * 1.3, 1.0));
  }
  if (b.x > 0.0) {
    // The char: soot over the patches its level reaches, edged by a brown scorch.
    let reach = 1.15 - 1.1 * b.x;
    let soot = smoothstep(reach - 0.05, reach + 0.08, n);
    let scorch = smoothstep(reach - 0.2, reach, n) * (1.0 - soot);
    c = mix(c, c * vec3f(0.45, 0.32, 0.22), scorch * 0.8);
    c = mix(c, c * kCharSoot, soot);
    // The embers: the cracks of the char glowing while it is hot, flickering.
    if (b.y > 0.0) {
      let crack = 1.0 - abs(warp_noise(q * 3.3 + vec3f(0.0, t * 0.7, 0.0)) * 2.0 - 1.0);
      let flicker = 0.6 + 0.4 * warp_noise(q * 0.8 + vec3f(t * 3.0));
      glow = soot * smoothstep(0.82, 0.97, crack) * b.y * flicker;
    }
  }
  return vec4f(c, glow);
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // rg: the screen motion (uv), b: reactive (0: a solid surface)
}

@fragment
fn fs_main(in: VertexOut, @builtin(front_facing) front: bool) -> FragmentOut {
  let d = draws[in.draw];
  let m = models[d.info.x];
  var n = normalize(in.normal);
  if (!front) {
    n = -n;
  }
  if (hole_at(in.uv, m)) {
    discard;
  }
  let burn = warp_burn(d.warp, in.local.xyz, in.local.w);
  if (burn < 0.0) {
    discard;
  }
  var s = skin_at(in.uv, m, d);
  var ember = 0.0;
  if (any(d.burn.xyz > vec3f(0.0))) {
    let burnt = morph_burn(d.burn, s.rgb, in.local.xyz, frame.time.x);
    s = vec4f(burnt.rgb, s.w);
    ember = burnt.w;
  }
  let y = in.rel.y + frame.eye.y;
  let outdoor = d.blends.w;
  let sun = sun_light_open(y) * max(dot(n, frame.sun.xyz), 0.0);
  var own = d.ambient.rgb + d.shade.rgb * max(dot(n, d.shade_dir.xyz), 0.0);
  if (d.ambient.w > 0.5) {
    own = own * (PI * white_luminance());  // in the scene's white (MorphDraw::light_in_white)
  }
  let light = outdoor * (sun + sky_irradiance(n)) + own;
  var c = s.rgb / PI * light + s.rgb * (s.w * d.blends.z * white_luminance());
  // The lamps on its skin (a torch carried past, a muzzle's flash), on a matte face.
  c += local_lights(matte(s.rgb), in.rel, n, -normalize(in.rel), in.position.xyz);
  // The embers' light: a deep orange-red, a few times the picture's white at their brightest.
  c += vec3f(1.0, 0.32, 0.06) * (ember * 3.0 * white_luminance());
  c = warp_shade(d.warp, c, burn);
  let gone = 1.0 - clamp(d.warp.a.x, 0.0, 1.0);
  var out: FragmentOut;
  out.colour = vec4f(haze(c, in.position.xy * frame.size.zw, length(in.rel)), 1.0);
  let reactive = select(0.0, 1.0, gone > 0.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.last_rel, 1.0), in.position.xy), reactive, 0.0);
  return out;
}
