// THE POST PASS (render/post.hpp; docs/rendering.md § Post): the HDR frame to the display, in
// one full-screen pass: the bloom's glow mixed in, the night's loss of colour (frame.wgsl's
// night_vision), the exposure from the sky's brightness (never a histogram, so it never pumps),
// the tone curve, the colour grade's table, a dither against banding and the output's encoding.
// Linear light ends here. The lens and the film (render/post.hpp § The lens and the film) are
// each an `override` switch: off, the compiler drops the branch and the pass is what it was.
#include "render/wgsl/frame.wgsl"
#include "gpu/wgsl/colour.wgsl"

// Whether the output is a UNORM target this pass encodes to sRGB (an *Srgb target encodes on
// store by itself).
override kEncodeSrgb: bool = true;

// The share of the bloom mixed in (render/bloom.hpp); 0 reads none.
override kBloom: f32 = 0.0;

@group(1) @binding(0) var hdr: texture_2d<f32>;
@group(1) @binding(1) var bloom: texture_2d<f32>;  // rgb summed over `a` levels
@group(1) @binding(2) var lin: sampler;
@group(1) @binding(3) var grade: texture_3d<f32>;
@group(1) @binding(4) var lens: texture_2d<f32>;  // render/lens.hpp's light, pre-exposure
@group(1) @binding(5) var dirt: texture_2d<f32>;  // the lens's dirt, 0..1

// PostLook's strengths, 0..1: a = (sharpen, chromatic, vignette, grain), b = (dirt, -, -, -).
struct Look {
  a: vec4f,
  b: vec4f,
}
@group(1) @binding(6) var<uniform> look: Look;

override kSharpen: bool = false;
override kChromatic: bool = false;
override kVignette: bool = false;
override kGrain: bool = false;
override kDirt: bool = false;
override kLens: bool = false;

struct VertexOut {
  @builtin(position) position: vec4f,
}

@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOut {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: VertexOut;
  out.position = vec4f(p, 0.0, 1.0);
  return out;
}

// The tone curve: linear below 0.8 and a smooth shoulder to white above (the Khronos PBR
// neutral operator), so snow keeps its colour and only highlights roll off.
fn tone(c: vec3f) -> vec3f {
  let start = 0.8 - 0.04;
  let desat = 0.15;
  let x = min(c.r, min(c.g, c.b));
  let offset = select(0.04, x - 6.25 * x * x, x < 0.08);
  var col = c - offset;
  let peak = max(col.r, max(col.g, col.b));
  if (peak < start) {
    return col;
  }
  let d = 1.0 - start;
  let np = 1.0 - d * d / (peak + d - start);
  col *= np / peak;
  let g = 1.0 - 1.0 / (desat * (peak - np) + 1.0);
  return mix(col, vec3f(np), g);
}

// Interleaved gradient noise: a fixed screen-space dither, -0.5 .. 0.5 of one 8-bit step.
fn dither(p: vec2f) -> f32 {
  return fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715)))) - 0.5;
}

// The grade's table at an sRGB-encoded colour: texel centres span 0..1.
fn graded(e: vec3f) -> vec3f {
  let n = f32(textureDimensions(grade).x);
  let at = clamp(e, vec3f(0.0), vec3f(1.0)) * ((n - 1.0) / n) + 0.5 / n;
  return textureSampleLevel(grade, lin, at, 0.0).rgb;
}

// A texel of the frame, its coordinate clamped to the picture.
fn at(p: vec2i) -> vec3f {
  return textureLoad(hdr, clamp(p, vec2i(0), vec2i(textureDimensions(hdr)) - 1), 0).rgb;
}

// CONTRAST-ADAPTIVE SHARPENING over the four neighbours: the negative lobe's weight is scaled by
// how much headroom the neighbourhood leaves (its darkest over its brightest, against 0 and 1), so
// a flat area is sharpened most and an edge already at full contrast barely at all: no halo, no
// ringing. The headroom is judged on the luminance squashed into 0..1 (x / (1 + x) of the exposed
// light, so the sun's thousands do not swamp it), one number a pixel; the lobe is applied to the
// colour itself, in linear light.
fn sharpened(p: vec2i, c: vec3f, e: f32) -> vec3f {
  let n = at(p + vec2i(0, -1));
  let s = at(p + vec2i(0, 1));
  let w = at(p + vec2i(-1, 0));
  let r = at(p + vec2i(1, 0));
  let l = vec4f(dot(n, kLuma), dot(s, kLuma), dot(w, kLuma), dot(r, kLuma)) * e;
  let q = l / (1.0 + l);
  let m = dot(c, kLuma) * e;
  let mq = m / (1.0 + m);
  let lo = min(mq, min(min(q.x, q.y), min(q.z, q.w)));
  let hi = max(mq, max(max(q.x, q.y), max(q.z, q.w)));
  let amp = sqrt(clamp(min(lo, 1.0 - hi) / max(hi, 1e-4), 0.0, 1.0));
  let k = -amp / mix(8.0, 5.0, look.a.x);
  return max((c + k * (n + s + w + r)) / (1.0 + 4.0 * k), vec3f(0.0));
}

// THE LENS'S LATERAL COLOUR: red magnified a little more and blue a little less than green, so
// they part toward the corners and meet in the middle.
fn fringed(uv: vec2f, c: vec3f) -> vec3f {
  let d = (uv - 0.5) * (0.004 * look.a.y);
  let red = textureSampleLevel(hdr, lin, uv + d, 0.0).r;
  let blue = textureSampleLevel(hdr, lin, uv - d, 0.0).b;
  return vec3f(red, c.g, blue);
}

// The vignette's light at `uv`: 1 in the middle, 1 - 0.6 x strength in the corners, falling as
// the radius cubed (gentle in the middle, like a lens's natural fall-off).
fn vignette(uv: vec2f, size: vec2f) -> f32 {
  let d = (uv - 0.5) * vec2f(size.x / size.y, 1.0);
  let corner = vec2f(0.5 * size.x / size.y, 0.5);
  let r2 = dot(d, d) / dot(corner, corner);
  return 1.0 - 0.6 * look.a.z * r2 * sqrt(r2);
}

// A hash of a pixel and the frame: 0..1, new every frame.
fn hash(p: vec2u, t: u32) -> f32 {
  var h = p.x * 1973u + p.y * 9277u + t * 26699u;
  h = (h ^ (h >> 16u)) * 0x7feb352du;
  h = (h ^ (h >> 15u)) * 0x846ca68bu;
  h = h ^ (h >> 16u);
  return f32(h) / 4294967295.0;
}

// FILM GRAIN on an encoded colour: a triangular noise (two hashes), strongest in the middle tones
// (film's grain is lost in the black and burnt out in the white).
fn grained(o: vec3f, p: vec2f) -> vec3f {
  let q = vec2u(p);
  let t = u32(frame.time.y);
  let n = hash(q, t) + hash(q + vec2u(7u, 3u), t + 1013u) - 1.0;
  let y = dot(o, vec3f(0.2126, 0.7152, 0.0722));
  return o + vec3f(n * 0.04 * look.a.w * 4.0 * y * (1.0 - y));
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  let size = vec2f(textureDimensions(hdr));
  let uv = in.position.xy / size;
  let p = vec2i(in.position.xy);
  let e = exposure();
  let base = textureLoad(hdr, p, 0).rgb;
  var c = base;
  if (kSharpen) {
    c = sharpened(p, base, e);
  }
  if (kChromatic) {
    c = fringed(uv, base) + (c - base);
  }
  var glow = vec3f(0.0);  // the light the dirt shows: the glow past white, and the lens's
  if (kBloom > 0.0) {
    let b = textureSampleLevel(bloom, lin, uv, 0.0);
    let g = b.rgb / max(b.a, 1.0);
    c = mix(c, g, kBloom);
    glow = max(g - vec3f(1.0 / e), vec3f(0.0));
  }
  if (kLens) {
    let l = textureSampleLevel(lens, lin, uv, 0.0).rgb;
    c += l;
    glow += l;
  }
  if (kDirt) {
    c += glow * textureSampleLevel(dirt, lin, uv, 0.0).rgb * look.b.x;
  }
  c = night_vision(c, view_ray(frame, uv));
  if (kVignette) {
    c *= vignette(uv, size);
  }
  let t = clamp(tone(c * e), vec3f(0.0), vec3f(1.0));
  var o = graded(linear_to_srgb(t));
  if (kGrain) {
    o = clamp(grained(o, in.position.xy), vec3f(0.0), vec3f(1.0));
  }
  if (!kEncodeSrgb) {
    o = srgb_to_linear(o);
  }
  o += vec3f(dither(in.position.xy) / 255.0);
  return vec4f(o, 1.0);
}
