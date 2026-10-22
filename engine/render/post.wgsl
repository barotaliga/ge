// THE POST PASS (render/post.hpp; docs/rendering.md § Post): the HDR frame to the display, in
// one full-screen pass: the bloom's glow mixed in, the exposure from the sky's brightness (never
// a histogram, so it never pumps), the tone curve, the colour grade's table, a dither against
// banding and the output's encoding. Linear light ends here.
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

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
  var c = textureLoad(hdr, vec2i(in.position.xy), 0).rgb;
  if (kBloom > 0.0) {
    let uv = in.position.xy / vec2f(textureDimensions(hdr));
    let b = textureSampleLevel(bloom, lin, uv, 0.0);
    c = mix(c, b.rgb / max(b.a, 1.0), kBloom);
  }
  let t = clamp(tone(c * exposure()), vec3f(0.0), vec3f(1.0));
  var o = graded(linear_to_srgb(t));
  if (!kEncodeSrgb) {
    o = srgb_to_linear(o);
  }
  o += vec3f(dither(in.position.xy) / 255.0);
  return vec4f(o, 1.0);
}
