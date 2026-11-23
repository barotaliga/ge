// MORPH MESHES DRAWN (render/morph.hpp): one draw a model instance, its vertices pulled by index
// from the shared buffers, its two frames blended (and the last frame's two, for its motion),
// its skin read from the atlas texel by texel with each texel's mark resolved: a tinted texel's
// colour comes off the instance's ramp at the texel's step, a glowing one lights itself, a hole
// (mark 2) is not drawn: the texel the point falls in decides, sharp or smooth. Sharp
// (the texel the point falls in) or smooth (the four nearest, each resolved, blended).
// A WARPED draw (MorphWarp: shown under 1) dissolves by render/wgsl/warp.wgsl's field, the band at
// its cut turned to ash, pinched and shimmering as its knobs say. A SEE-THROUGH draw (MorphDraw's
// alpha under 1, `shade_dir.w` the share left out) drops that share of its pixels by a 4x4
// ordered pattern shifted every frame (`see_through_gone`), which the temporal resolve blends.
// A SHELL (MorphDraw::shell_push, `skin.z` set) is the model pushed out along its normals and one
// flat colour, no skin and no light. A SHADOW (MorphDraw::flat, `flat.w` 1) is a shell squashed
// onto a level floor, each point moved across by the lean for its height above the floor.
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
  shade: vec4f,  // w: the share a face turned away loses
  shade_dir: vec4f,
  warp: Warp,  // render/wgsl/warp.wgsl (MorphWarp)
  burn: vec4f,  // charred, embers, etched, seed (MorphBurn)
  // x: the model whose skin it wears (MorphDraw::skin); y: its slot + 1 (0: none); z: a shell's
  // colour, a byte a channel from bit 0, bit 24 set (0: not a shell); w: its push's bits, m
  skin: vec4u,
  // A SHADOW (MorphDraw::flat, w 1): x the floor's height from the eye, m; yz the lean (x, z) a
  // m above it
  flat: vec4f,
  blood: array<vec4f, 12>,  // MorphBlood: a wound two: at (rest frame, m), size; run, wet, seed
}

fn is_shell(d: Draw) -> bool {
  return d.skin.z != 0u;
}

fn shell_colour(d: Draw) -> vec3f {
  return vec3f(f32(d.skin.z & 255u), f32((d.skin.z >> 8u) & 255u), f32((d.skin.z >> 16u) & 255u)) /
         255.0;
}

const kTintSteps = 16u;

@group(1) @binding(0) var<storage, read> models: array<Model>;
@group(1) @binding(1) var<storage, read> vertices: array<vec4f>;
@group(1) @binding(2) var<storage, read> uvs: array<vec2f>;
@group(1) @binding(3) var<storage, read> indices: array<u32>;
@group(1) @binding(4) var<storage, read> draws: array<Draw>;
@group(1) @binding(5) var<storage, read> ramps: array<vec4f>;
@group(1) @binding(6) var atlas: texture_2d<f32>;
@group(1) @binding(7) var slots: texture_2d_array<f32>;  // the skins worn by slot (MorphDraw::slot)

const kSlotSize = 512u;  // render/morph.hpp's kMorphSlotSize

// The rectangle the draw's skin is read from: its worn model's in the atlas, or a whole slot.
fn skin_rect(m: Model, d: Draw) -> vec4u {
  return select(m.rect, vec4u(0u, 0u, kSlotSize, kSlotSize), d.skin.y != 0u);
}

fn skin_load(p: vec2i, d: Draw) -> vec4f {
  if (d.skin.y != 0u) {
    return textureLoad(slots, p, i32(d.skin.y - 1u), 0);
  }
  return textureLoad(atlas, p, 0);
}

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

// A point (relative to the eye) laid on the shadow's floor, moved across by its height above it.
fn flatten(f: vec4f, p: vec3f) -> vec3f {
  let h = max(p.y - f.x, 0.0);
  return vec3f(p.x + f.y * h, f.x, p.z + f.z * h);
}

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) rel: vec3f,
  @location(1) last_rel: vec3f,
  @location(2) normal: vec3f,
  @location(3) uv: vec2f,
  @location(4) @interpolate(flat) draw: u32,
  @location(5) local: vec4f,  // the point in the model's frame times its scale; along the axis, m
  @location(6) rest: vec3f,   // the point in the model's frame 0 times its scale (the blood's), m
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) k: u32) -> VertexOut {
  let d = draws[k];
  let m = models[d.info.x];
  let idx = indices[m.first_index + vid];
  let now = posed(m, d.frames.x, d.frames.y, d.blends.x, idx);
  let last = posed(m, d.frames.z, d.frames.w, d.blends.y, idx);
  var out: VertexOut;
  var off = rotate(d.now_rotation, now.position * d.now_place.w);
  var last_off = rotate(d.last_rotation, last.position * d.last_place.w);
  if (is_shell(d)) {  // pushed out along its normals
    let push = bitcast<f32>(d.skin.w);
    off += rotate(d.now_rotation, now.normal) * push;
    last_off += rotate(d.last_rotation, last.normal) * push;
  }
  out.rel = d.now_place.xyz + warp_vertex(d.warp, frame.time.x, off);
  out.last_rel = d.last_place.xyz + warp_vertex(d.warp, frame.time.x, last_off);
  out.local = vec4f(now.position * d.now_place.w, dot(off, d.warp.b.xyz));
  out.rest = vertices[2u * (m.first_vertex + idx)].xyz * d.now_place.w;
  if (d.flat.w > 0.0) {  // squashed onto the floor, leaning
    out.rel = flatten(d.flat, out.rel);
    out.last_rel = flatten(d.flat, out.last_rel);
  }
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
  let c = skin_load(p, d);
  let mark = u32(round(c.a * 255.0));
  let region = mark >> 6u;
  if (region != 0u) {
    let row = select(d.info.y, d.info.z, region == 2u);
    return vec4f(ramps[row * kTintSteps + (mark & 15u)].rgb, 0.0);
  }
  return vec4f(srgb_decode(c.rgb), select(0.0, 1.0, mark == 1u));
}

fn skin_at(uv: vec2f, m: Model, d: Draw) -> vec4f {
  let r = skin_rect(m, d);
  let lo = vec2i(r.xy);
  let hi = vec2i(r.xy + r.zw) - 1;
  let at = uv * vec2f(r.zw);
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
// Whether this pixel is one of the share `gone` (0..1) a see-through draw leaves out this frame: a
// 4x4 ordered threshold, the pattern shifted by the frame number so each pixel takes its turn.
fn see_through_gone(gone: f32, pixel: vec2f, frame_number: f32) -> bool {
  if (gone <= 0.0) {
    return false;
  }
  let k = u32(frame_number) % 16u;
  let p = (vec2u(pixel) + vec2u(k % 4u, k / 4u)) % 4u;
  let bayer = array<f32, 16>(0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
                             3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
  return (bayer[p.y * 4u + p.x] + 0.5) / 16.0 < gone;
}

fn hole_at(uv: vec2f, m: Model, d: Draw) -> bool {
  let r = skin_rect(m, d);
  let lo = vec2i(r.xy);
  let hi = vec2i(r.xy + r.zw) - 1;
  let p = clamp(lo + vec2i(floor(uv * vec2f(r.zw))), lo, hi);
  return u32(round(skin_load(p, d).a * 255.0)) == 2u;
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

// THE BLOOD (MorphBlood; render/morph.cpp's `wound_cover` is the CPU twin): how much of the skin at
// the rest-frame point `p` wound `k` covers: its soaked patch and the runs down from it.
fn wound_cover(d: Draw, k: u32, p: vec3f) -> f32 {
  let a = d.blood[2u * k];
  let b = d.blood[2u * k + 1u];
  let size = a.w;
  let run = b.x;
  let seed = b.z;
  let dv = p - a.xyz;
  let n = warp_noise(vec3f(p.x * 14.0 + seed * 41.0, p.y * 14.0, p.z * 14.0));
  let r = length(dv) / size;
  var cover = 1.0 - smoothstep(0.7, 1.0, r + (n - 0.5) * 0.5);
  let below = -dv.y;
  if (run > 0.0 && below > 0.0 && below < run + size * 0.3) {
    for (var j = 0; j < 2; j++) {
      let fj = f32(j);
      let share = select(0.55, 1.0, j == 0);
      let reach = run * select(0.7, 1.0, j == 0);
      let side = select(size * 0.55, 0.0, j == 0) * select(-1.0, 1.0, seed - 0.5 > 0.0);
      let wx = (warp_noise(vec3f(below * 7.0, seed * 13.0 + fj * 5.0, 1.0)) - 0.5) * size * 0.7 + side;
      let wz = (warp_noise(vec3f(below * 7.0, seed * 13.0 + fj * 5.0, 7.0)) - 0.5) * size * 0.7;
      let lat = length(vec2f(dv.x - wx, dv.z - wz));
      let width = size * 0.5 * share * (1.0 - 0.5 * clamp(below / reach, 0.0, 1.0)) + 0.004;
      var stream = 1.0 - smoothstep(width * 0.6, width, lat);
      let past = below - reach;
      let bead = width * 1.3;
      if (past > 0.0) {
        stream = 1.0 - smoothstep(bead * 0.6, bead, length(vec2f(lat, past)));
      }
      cover = max(cover, stream);
    }
  }
  return cover;
}

// The blood over a texel's albedo `a`: fresh a deep wet red, dried a near black brown.
const kWoundWet = vec3f(0.04, 0.0016, 0.0013);
const kWoundDry = vec3f(0.013, 0.0035, 0.0025);
fn morph_blood(d: Draw, a: vec3f, p: vec3f) -> vec3f {
  var c = a;
  for (var k = 0u; k < 6u; k++) {
    if (d.blood[2u * k].w <= 0.0) {
      continue;
    }
    let cover = wound_cover(d, k, p);
    if (cover > 0.0) {
      c = mix(c, mix(kWoundDry, kWoundWet, d.blood[2u * k + 1u].y), cover * 0.94);
    }
  }
  return c;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // rg: the screen motion (uv), b: reactive (0: a solid surface)
}

@fragment
fn fs_main(in: VertexOut, @builtin(front_facing) front: bool) -> FragmentOut {
  let d = draws[in.draw];
  let m = models[d.skin.x];  // only its skin's rectangle is read here
  var n = normalize(in.normal);
  if (!front) {
    n = -n;
  }
  if (see_through_gone(d.shade_dir.w, in.position.xy, frame.time.y)) {
    discard;
  }
  if (is_shell(d)) {  // one flat colour in the scene's white, whatever the light
    var shell: FragmentOut;
    let c = shell_colour(d) * white_luminance();
    shell.colour = vec4f(haze(c, in.position.xy * frame.size.zw, length(in.rel)), 1.0);
    shell.motion = vec4f(screen_motion(frame, vec4f(in.last_rel, 1.0), in.position.xy), 0.0, 0.0);
    return shell;
  }
  if (hole_at(in.uv, m, d)) {
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
  if (d.blood[0].w > 0.0) {
    s = vec4f(morph_blood(d, s.rgb, in.rest), s.w);
  }
  let y = in.rel.y + frame.eye.y;
  let outdoor = d.blends.w;
  let sun = sun_light_open(y) * max(dot(n, frame.sun.xyz), 0.0);
  // Toward the light it gains the shade; turned away it loses `shade.w` of it (MorphDraw::shade_back).
  let toward = dot(n, d.shade_dir.xyz);
  var own = d.ambient.rgb + d.shade.rgb * (max(toward, 0.0) + d.shade.w * min(toward, 0.0));
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
