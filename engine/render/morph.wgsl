// MORPH MESHES DRAWN (render/morph.hpp): one draw a model instance, its vertices pulled by index
// from the shared buffers, its two frames blended (and the last frame's two, for its motion),
// its skin read from the atlas texel by texel with each texel's mark resolved: a tinted texel's
// colour comes off the instance's ramp at the texel's step, a glowing one lights itself. Sharp
// (the texel the point falls in) or smooth (the four nearest, each resolved, blended).
#include "render/wgsl/frame.wgsl"

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
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) k: u32) -> VertexOut {
  let d = draws[k];
  let m = models[d.info.x];
  let idx = indices[m.first_index + vid];
  let now = posed(m, d.frames.x, d.frames.y, d.blends.x, idx);
  let last = posed(m, d.frames.z, d.frames.w, d.blends.y, idx);
  var out: VertexOut;
  out.rel = d.now_place.xyz + rotate(d.now_rotation, now.position * d.now_place.w);
  out.last_rel = d.last_place.xyz + rotate(d.last_rotation, last.position * d.last_place.w);
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
  let s = skin_at(in.uv, m, d);
  let y = in.rel.y + frame.eye.y;
  let outdoor = d.blends.w;
  let sun = sun_light_open(y) * max(dot(n, frame.sun.xyz), 0.0);
  let light = outdoor * (sun + sky_irradiance(n)) + d.ambient.rgb +
              d.shade.rgb * max(dot(n, d.shade_dir.xyz), 0.0);
  var c = s.rgb / PI * light + s.rgb * (s.w * d.blends.z * white_luminance());
  // The lamps on its skin (a torch carried past, a muzzle's flash), on a matte face.
  c += local_lights(matte(s.rgb), in.rel, n, -normalize(in.rel), in.position.xyz);
  var out: FragmentOut;
  out.colour = vec4f(haze(c, in.position.xy * frame.size.zw, length(in.rel)), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.last_rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}
