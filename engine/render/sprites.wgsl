// SPRITES DRAWN (render/sprites.hpp): one quad a sprite, its corners off the right and up the
// CPU staged for it (every facing rule is the CPU's), its texels read from the atlas sharp or
// smooth (premultiplied, so an edge never darkens), cut out at half coverage (`fs_cutout`: depth
// and its own motion written) or laid premultiplied over the picture (`fs_blend`: an added
// sprite's alpha out is 0, so the same blend adds it; the motion target's reactive channel only).
#include "render/wgsl/frame.wgsl"

struct Sprite {
  centre: vec4f,  // camera-relative position, m
  last: vec4f,    // the frame before's, relative to this frame's eye
  right: vec4f,   // unit
  up: vec4f,      // unit
  extent: vec4f,  // the quad along right (left, right) and up (bottom, top), m
  colour: vec4f,  // linear rgb times the texels; a: the opacity
  light: vec4f,   // glow, outdoor
  ambient: vec4f,
  info: vec4u,    // frame, blend (0 cutout, 1 over, 2 add), 1 if smooth
}

@group(1) @binding(0) var<storage, read> rects: array<vec4u>;  // x, y, w, h (texels)
@group(1) @binding(1) var<storage, read> sprites: array<Sprite>;
@group(1) @binding(2) var atlas: texture_2d<f32>;

struct VertexOut {
  @builtin(position) position: vec4f,
  @location(0) rel: vec3f,
  @location(1) last_rel: vec3f,
  @location(2) uv: vec2f,  // 0..1 across the frame, v down
  @location(3) @interpolate(flat) sprite: u32,
}

@vertex
fn vs_main(@builtin(vertex_index) vid: u32, @builtin(instance_index) k: u32) -> VertexOut {
  let s = sprites[k];
  // Two triangles: 0 1 2, 0 2 3 over the corners bottom-left, top-left, top-right, bottom-right.
  let corner = array<u32, 6>(0u, 1u, 2u, 0u, 2u, 3u)[vid];
  let fx = select(0.0, 1.0, corner == 2u || corner == 3u);
  let fy = select(0.0, 1.0, corner == 1u || corner == 2u);
  let along = mix(s.extent.x, s.extent.y, fx) * s.right.xyz + mix(s.extent.z, s.extent.w, fy) * s.up.xyz;
  var out: VertexOut;
  out.rel = s.centre.xyz + along;
  out.last_rel = s.last.xyz + along;
  out.position = frame.view_proj * vec4f(out.rel, 1.0);
  out.uv = vec2f(fx, 1.0 - fy);
  out.sprite = k;
  return out;
}

fn srgb_decode(c: vec3f) -> vec3f {
  let lo = c / 12.92;
  let hi = pow((c + 0.055) / 1.055, vec3f(2.4));
  return select(hi, lo, c <= vec3f(0.04045));
}

// A texel: linear colour premultiplied by its coverage, and the coverage.
fn texel(p: vec2i) -> vec4f {
  let c = textureLoad(atlas, p, 0);
  return vec4f(srgb_decode(c.rgb) * c.a, c.a);
}

fn frame_at(uv: vec2f, s: Sprite) -> vec4f {
  let r = rects[s.info.x];
  let lo = vec2i(r.xy);
  let hi = vec2i(r.xy + r.zw) - 1;
  let at = uv * vec2f(r.zw);
  if (s.info.z == 0u) {
    return texel(clamp(lo + vec2i(floor(at)), lo, hi));
  }
  let f = at - 0.5;
  let base = vec2i(floor(f));
  let w = f - floor(f);
  let p00 = clamp(lo + base, lo, hi);
  let p11 = clamp(lo + base + 1, lo, hi);
  let a = texel(p00);
  let b = texel(vec2i(p11.x, p00.y));
  let c = texel(vec2i(p00.x, p11.y));
  let e = texel(p11);
  return mix(mix(a, b, w.x), mix(c, e, w.x), w.y);
}

// The sprite's premultiplied colour, lit, and its coverage times its opacity.
fn shaded(in: VertexOut) -> vec4f {
  let s = sprites[in.sprite];
  let t = frame_at(in.uv, s);
  let to_eye = -normalize(in.rel);
  let y = in.rel.y + frame.eye.y;
  // The face turned to the eye, lit as a thin card: the sun through it half as well as on it.
  let sun = sun_light_open(y) * (0.5 + 0.5 * dot(to_eye, frame.sun.xyz)) * 0.5;
  let light = s.light.y * (sun + sky_irradiance(to_eye)) + s.ambient.rgb;
  let c = t.rgb * s.colour.rgb * (light / PI + s.light.x * white_luminance());
  return vec4f(c, t.a) * s.colour.a;
}

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // rg: the screen motion (uv), b: reactive
}

@fragment
fn fs_cutout(in: VertexOut) -> FragmentOut {
  let p = shaded(in);
  if (p.a < 0.5) {
    discard;
  }
  let uv = in.position.xy * frame.size.zw;
  var out: FragmentOut;
  out.colour = vec4f(haze(p.rgb / p.a, uv, length(in.rel)), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.last_rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}

@fragment
fn fs_blend(in: VertexOut) -> FragmentOut {
  let s = sprites[in.sprite];
  let p = shaded(in);
  let uv = in.position.xy * frame.size.zw;
  let dist = length(in.rel);
  var out: FragmentOut;
  if (s.info.y == 2u) {
    // Added light fades with the air and adds none of its own.
    out.colour = vec4f(haze(p.rgb, uv, dist) - haze(vec3f(0.0), uv, dist), 0.0);
  } else {
    // Over: the colour behind the haze, premultiplied by the coverage.
    let straight = select(vec3f(0.0), p.rgb / p.a, p.a > 1e-4);
    out.colour = vec4f(haze(straight, uv, dist) * p.a, p.a);
  }
  out.motion = vec4f(0.0, 0.0, min(p.a + max(max(out.colour.r, out.colour.g), out.colour.b), 1.0), 0.0);
  return out;
}
