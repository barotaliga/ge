// THE POST PASS (render/post.hpp; docs/rendering.md § Post): the HDR frame to the display, in
// one full-screen pass: the bloom's glow mixed in, the night's loss of colour (frame.wgsl's
// night_vision), the exposure from the sky's brightness (never a histogram, so it never pumps),
// the tone curve, the colour grade's table, a dither against banding and the output's encoding.
// Linear light ends here. The lens and the film (render/post.hpp § The lens and the film) are
// each an `override` switch: off, the compiler drops the branch and the pass is what it was.
#include "render/wgsl/frame.wgsl"
#include "gpu/wgsl/colour.wgsl"
#include "render/wgsl/dof_coc.wgsl"

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

// PostLook's strengths, 0..1: a = (sharpen, chromatic, vignette, grain), b = (dirt, motion
// blur's shutter, -, -).
struct Look {
  a: vec4f,
  b: vec4f,
  c: vec4f,  // depth of field: the focus (m), the sharp share, the widest circle (of the height)
}
@group(1) @binding(6) var<uniform> look: Look;
// Motion blur's inputs at the render size (render/motion_blur.hpp): the motion target (rg the
// screen motion a frame, uv), the depth, and each tile's neighbours' fastest motion.
@group(1) @binding(7) var motion: texture_2d<f32>;
@group(1) @binding(8) var depth: texture_depth_2d;
@group(1) @binding(9) var near_max: texture_2d<f32>;
// Depth of field's half-size gather (render/dof.hpp): rgb the light, a the texel's circle.
@group(1) @binding(10) var dof: texture_2d<f32>;

override kSharpen: bool = false;
override kChromatic: bool = false;
override kVignette: bool = false;
override kGrain: bool = false;
override kDirt: bool = false;
override kLens: bool = false;
override kMotionBlur: bool = false;
override kDof: bool = false;

const kBlurTaps: i32 = 6;     // the gather's most taps along the motion
const kBlurMost: f32 = 0.04;  // the longest streak, a share of the picture's height
const kTile: i32 = 16;        // render/motion_blur.hpp's kTile

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
// ringing. Everything is done on the light squashed into 0..1 (x / (1 + x) of the exposed
// luminance, the colour scaled with it) and mapped back, so the sun's thousands do not swamp the
// headroom and a light thousands of times brighter than its neighbours (a lightning channel)
// cannot pull them below black, a dark rim round it.
fn squash(c: vec3f, e: f32) -> vec3f {
  return c * e / (1.0 + dot(c, kLuma) * e);
}

fn sharpened(p: vec2i, c: vec3f, e: f32) -> vec3f {
  let n = squash(at(p + vec2i(0, -1)), e);
  let s = squash(at(p + vec2i(0, 1)), e);
  let w = squash(at(p + vec2i(-1, 0)), e);
  let r = squash(at(p + vec2i(1, 0)), e);
  let m = squash(c, e);
  let q = vec4f(dot(n, kLuma), dot(s, kLuma), dot(w, kLuma), dot(r, kLuma));
  let mq = dot(m, kLuma);
  let lo = min(mq, min(min(q.x, q.y), min(q.z, q.w)));
  let hi = max(mq, max(max(q.x, q.y), max(q.z, q.w)));
  let amp = sqrt(clamp(min(lo, 1.0 - hi) / max(hi, 1e-4), 0.0, 1.0));
  let k = -amp / mix(8.0, 5.0, look.a.x);
  let t = max((m + k * (n + s + w + r)) / (1.0 + 4.0 * k), vec3f(0.0));
  return t / (max(1.0 - dot(t, kLuma), 1e-3) * e);
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

// MOTION BLUR (a scatter-as-gather reconstruction): taps along the fastest motion near the
// pixel, each weighted by a cone of its own motion (does its streak reach this pixel?) where it
// stands in front, by the pixel's own cone where it stands behind, and by both streaks' full
// length where neither is decided; the pixel itself weighs one over its own streak, so a still
// pixel under a fast neighbour keeps itself. Depth is compared as a share of the distance
// (reversed-z: a nearer point's depth is larger), so the soft edge is 5 % at any range.
fn cone(d: f32, l: f32) -> f32 {
  return clamp(1.0 - d / max(l, 1e-3), 0.0, 1.0);
}

fn cylinder(d: f32, l: f32) -> f32 {
  return 1.0 - smoothstep(0.95 * l, 1.05 * l, d);
}

// 1 where depth `a` stands in front of or level with depth `b`, falling to 0 a 20th behind it.
fn in_front(a: f32, b: f32) -> f32 {
  return clamp(1.0 + (a - b) / (0.05 * max(b, 1e-8)), 0.0, 1.0);
}

fn blurred(uv: vec2f, c: vec3f, size: vec2f, px: vec2f) -> vec3f {
  let render = vec2f(textureDimensions(motion));
  let last = vec2i(render) - 1;
  let tiles = vec2i(textureDimensions(near_max)) - 1;
  let rp = clamp(vec2i(uv * render), vec2i(0), last);
  let shutter = look.b.y;
  let most = kBlurMost * size.y;
  let tile = textureLoad(near_max, min(rp / kTile, tiles), 0);
  var vn = tile.xy * size * shutter;
  let ln = length(vn);
  if (ln < 0.5) {
    return c;
  }
  vn *= min(ln, most) / ln;  // in output pixels, clamped to the longest streak
  let lr = min(ln, most);
  let j = dither(px);  // -0.5 .. 0.5: the taps' offset, so their gaps blend into noise
  // Fewer taps for a short streak: one every four pixels, three at least.
  let taps = clamp(i32(ceil(lr / 4.0)), 3, kBlurTaps);
  // THE FAST PATH: the tile and its neighbours all moving within half of one another (the
  // camera turning over open ground) have no edge between speeds to keep: the plain average
  // along the streak, one bilinear tap each and no motion or depth read.
  let slowest = min(tile.z * (size.y / render.y) * shutter, most);
  if (slowest > 0.5 * lr) {
    var plain = c;
    for (var i = 0; i < taps; i++) {
      let t = (f32(i) + 0.5 + j) / f32(taps) * 2.0 - 1.0;
      plain += textureSampleLevel(hdr, lin, uv + vn * (0.5 * t) / size, 0.0).rgb;
    }
    return plain / f32(taps + 1);
  }
  let lc = min(length(textureLoad(motion, rp, 0).xy * size * shutter), most);
  let dc = textureLoad(depth, rp, 0);
  let w0 = 1.0 / max(0.5 * lc, 1.0);
  var sum = c * w0;
  var weight = w0;
  for (var i = 0; i < taps; i++) {
    let t = (f32(i) + 0.5 + j) / f32(taps) * 2.0 - 1.0;  // -1 .. 1 along the streak
    let suv = uv + vn * (0.5 * t) / size;
    let sp = clamp(vec2i(suv * render), vec2i(0), last);
    let ls = min(length(textureLoad(motion, sp, 0).xy * size * shutter), most);
    let ds = textureLoad(depth, sp, 0);
    let d = abs(t) * 0.5 * lr;
    // A streak reaches half its length each way: its reach is half of it.
    let rs = 0.5 * ls;
    let rc = 0.5 * lc;
    let w = in_front(ds, dc) * cone(d, rs) + in_front(dc, ds) * cone(d, rc) +
            cylinder(d, rs) * cylinder(d, rc) * 2.0;
    sum += textureSampleLevel(hdr, lin, suv, 0.0).rgb * w;
    weight += w;
  }
  return sum / weight;
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

// DEPTH OF FIELD: the half-size gather mixed in by this pixel's own circle, read off the
// full-size depth, so a sharp subject's edge is as sharp as its pixels; a circle under a pixel
// and a half is left sharp.
fn defocused(uv: vec2f, c: vec3f, size: vec2f) -> vec3f {
  let dims = vec2i(textureDimensions(depth));
  let z = textureLoad(depth, clamp(vec2i(uv * vec2f(dims)), vec2i(0), dims - 1), 0);
  let own = dof_circle(dof_distance(frame.inv_view_proj, uv, z), look.c.x, look.c.y);
  let px = own * look.c.z * size.y;
  return mix(c, textureSampleLevel(dof, lin, uv, 0.0).rgb, clamp(px / 1.5, 0.0, 1.0));
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
  if (kMotionBlur) {
    c = blurred(uv, c, size, in.position.xy);
  }
  if (kDof) {
    c = defocused(uv, c, size);
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
