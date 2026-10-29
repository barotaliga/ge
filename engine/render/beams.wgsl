// THE BEAMS DRAWN (render/beams.hpp): every segment of every beam's channel a ribbon facing the
// eye, its glowing sheath and white-hot core added over the picture, rippling along its length
// within a strike; and the BURNS they leave, a scorch's soot laid over the picture and a fresh
// one's ember glowing in it, in one blend (the colour added, the picture under it kept by alpha).
//
// - A RIBBON is the segment's own length and a ramp of `ramp_m` past each end; the ramps of two
//   segments meeting at a joint cross-fade (each falls as the other rises), so a channel's glow is
//   even along its length with no bead at a joint and no notch on the outside of a bend.
// - THE SHEATH's width is metric but held to a thirtieth of the screen's height at most, so the
//   beam's root beside the eye glows round the core instead of fogging the view.
// - THE CORE is a Gaussian `core_m` across, at least about a pixel and a half wide (its light
//   kept as it widens: a far beam is a fainter line, never a gap), whitened toward its middle so
//   a bright core burns white and holds its colour at the rim. THE SHEATH falls off over
//   `glow_m`, a Gaussian over an exponential tail.
// - Both are softened where the ribbon nears what is behind it (the depth, read), so a beam
//   landing on a wall sinks into it instead of being cut by it.
// - A BURN's mask comes from the atlas the game handed the pass; its soot darkens what is there
//   (alpha), its ember adds a glow from white-yellow through orange to a dull red as it cools,
//   brightest in the mark's heart and in its cracks (the mask's relief).
// All light in cd/m^2 at the frame's pre-exposure, as every emissive surface is.
#include "render/wgsl/frame.wgsl"

struct BeamLookGpu {
  core: vec4f,   // rgb; the core's luminance at this frame's level (cd/m^2)
  glow: vec4f,   // rgb; the sheath's
  shape: vec4f,  // core width, sheath width (m), the shimmer, the shot's seed (0..1)
  more: vec4f,   // the joints' ramp (m), the softening against the depth (m), 0, 0
}

struct BeamParams {
  info: vec4f,  // beams, segments, burn vertices, the frame's time (s)
  more: vec4f,  // the near plane (m), a fresh ember's luminance (cd/m^2), 0, 0
  looks: array<BeamLookGpu, 8>,
}

struct BeamSeg {
  a: vec4f,  // camera-relative start; its width share
  b: vec4f,  // camera-relative end; its width share
  c: vec4f,  // its glow share, distance along the beam at each end, the beam's index
}

struct BurnVert {
  at: vec4f,  // camera-relative position; its fade on a turned triangle
  uv: vec4f,  // the atlas's uv; the soot's darkness; the ember's luminance (cd/m^2)
}

@group(1) @binding(0) var<uniform> bp: BeamParams;
@group(1) @binding(1) var<storage, read> segs: array<BeamSeg>;
@group(1) @binding(2) var<storage, read> burns: array<BurnVert>;
@group(1) @binding(3) var scene_depth: texture_depth_2d;
@group(1) @binding(4) var burn_atlas: texture_2d<f32>;

const kBeamCorners = array<vec2f, 6>(vec2f(-1.0, -1.0), vec2f(1.0, -1.0), vec2f(1.0, 1.0),
                                     vec2f(-1.0, -1.0), vec2f(1.0, 1.0), vec2f(-1.0, 1.0));

struct BeamOut {
  @builtin(position) position: vec4f,
  @location(0) ribbon: vec3f,  // m along the segment from its start, m across, the half width
  @location(1) @interpolate(flat) seg: vec4f,  // its length, the ramp, metres a pixel, its index
  @location(2) along_s: f32,                   // distance along the beam, m
}

@vertex
fn beam_vs(@builtin(vertex_index) vid: u32, @builtin(instance_index) iid: u32) -> BeamOut {
  var out: BeamOut;
  if (f32(iid) >= bp.info.y) {
    out.position = vec4f(2.0, 2.0, 2.0, 1.0);
    return out;
  }
  let s = segs[iid];
  let look = bp.looks[u32(s.c.w)];
  let c = kBeamCorners[vid];
  let d = s.b.xyz - s.a.xyz;
  let len = max(length(d), 1e-4);
  let along = d / len;
  let end = c.x * 0.5 + 0.5;  // 0 at the start, 1 at the end
  let base = mix(s.a.xyz, s.b.xyz, end);
  let to_eye = -normalize(base);
  var across = cross(along, to_eye);
  let al = length(across);
  across = select(across / max(al, 1e-6), normalize(cross(vec3f(0.0, 1.0, 0.0), to_eye)),
                  al < 1e-4);
  let width = mix(s.a.w, s.b.w, end);
  // Metres a pixel here: the ribbon is at least a few pixels wide.
  let clip0 = frame.view_proj * vec4f(base, 1.0);
  let clip1 = frame.view_proj * vec4f(base + across, 1.0);
  let px = length((clip1.xy / clip1.w - clip0.xy / clip0.w) * frame.size.xy * 0.5);
  let m_px = 1.0 / max(px, 1e-4);
  // The sheath at its width, but never wider on the screen than a thirtieth of its height (a
  // beam's root by the eye would otherwise fog the view) nor narrower than three pixels.
  let hw = max(min(look.shape.y * 0.5 * width, 0.033 * frame.size.y * m_px), 3.0 * m_px);
  let ramp = look.more.x;
  let x = select(len + ramp, -ramp, c.x < 0.0);
  let corner = s.a.xyz + along * x + across * (c.y * hw);
  out.position = frame.view_proj * vec4f(corner, 1.0);
  out.ribbon = vec3f(x, c.y * hw, hw);
  out.seg = vec4f(len, ramp, m_px, f32(iid));
  out.along_s = mix(s.c.y, s.c.z, (x / len));
  return out;
}

fn beam_hash(x: f32) -> f32 {
  return fract(sin(x * 12.9898) * 43758.5453);
}

// Smooth value noise along a line.
fn beam_noise(x: f32) -> f32 {
  let i = floor(x);
  let f = x - i;
  let u = f * f * (3.0 - 2.0 * f);
  return mix(beam_hash(i), beam_hash(i + 1.0), u);
}

struct BeamFrag {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,  // b only: the reactive mask
}

@fragment
fn beam_fs(in: BeamOut) -> BeamFrag {
  let s = segs[u32(in.seg.w)];
  let look = bp.looks[u32(s.c.w)];
  let len = in.seg.x;
  let ramp = max(in.seg.y, 1e-4);
  let x = in.ribbon.x;
  // The joints' cross-fade: up over the ramp round the start, down over the ramp round the end.
  let w_in = clamp(0.5 + x / (2.0 * ramp), 0.0, 1.0);
  let w_out = clamp(0.5 + (len - x) / (2.0 * ramp), 0.0, 1.0);
  let joint = min(w_in, w_out);
  let width = mix(s.a.w, s.b.w, clamp(x / len, 0.0, 1.0));
  let y = abs(in.ribbon.y);
  let hw = in.ribbon.z;
  // The core: at least a pixel and a half across, its light kept; and never wider on the screen
  // than a hundredth of its height, its brightness kept, so the root by the eye reads as a thick
  // line and not a white blot.
  let core_hw = look.shape.x * 0.5 * width;
  let core_eff = clamp(core_hw, 0.75 * in.seg.z, max(0.01 * frame.size.y, 0.75) * in.seg.z);
  let core_keep = min(core_hw / core_eff, 1.0);
  let cy = y / core_eff;
  let core = exp(-2.0 * cy * cy) * core_keep;
  // The sheath.
  let gy = y / max(hw, 1e-5);
  let sheath = (exp(-5.0 * gy * gy) + 0.3 * exp(-3.5 * gy)) * max(1.0 - gy * gy, 0.0);
  // The ripple along the beam inside a strike: two octaves running down it.
  let t = bp.info.w;
  let seed = look.shape.w * 97.0;
  let n = beam_noise(in.along_s * 5.0 - t * 37.0 + seed) * 0.65 +
          beam_noise(in.along_s * 13.0 + t * 61.0 + seed * 3.0) * 0.35;
  let ripple = 1.0 + look.shape.z * (n - 0.5) * 1.6;
  let glow = s.c.x * joint * max(ripple, 0.0);
  // The core whitened as it brightens.
  let core_rgb = mix(look.core.rgb, vec3f(1.0), smoothstep(0.25, 0.9, core));
  var c = core_rgb * (core * look.core.w) + look.glow.rgb * (sheath * look.glow.w);
  c *= glow * frame.world.y / kSunLux;
  // Softened where it nears what is behind it.
  let d = textureLoad(scene_depth, vec2i(in.position.xy), 0);
  let behind = bp.more.x / max(d, 1e-7);
  let here = bp.more.x / max(in.position.z, 1e-7);
  let soft = clamp((behind - here) / max(look.more.y, 1e-3), 0.0, 1.0);
  c *= soft;
  var out: BeamFrag;
  // A half float's ceiling: past it the target holds infinity and the tone curve makes it black.
  out.colour = vec4f(min(c, vec3f(60000.0)), 0.0);
  out.motion = vec4f(0.0, 0.0, clamp((core + sheath) * glow * soft, 0.0, 1.0), 0.0);
  return out;
}

struct BurnOut {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
  @location(1) look: vec3f,  // the fade, the soot, the ember
}

@vertex
fn burn_vs(@builtin(vertex_index) vid: u32) -> BurnOut {
  var out: BurnOut;
  if (f32(vid) >= bp.info.z) {
    out.position = vec4f(2.0, 2.0, 2.0, 1.0);
    return out;
  }
  let v = burns[vid];
  out.position = frame.view_proj * vec4f(v.at.xyz, 1.0);
  out.uv = v.uv.xy;
  out.look = vec3f(v.at.w, v.uv.z, v.uv.w);
  return out;
}

// A cooling ember's colour by its heat (1 fresh, 0 cold): yellow-white, orange, a dull red.
fn burn_ember(heat: f32) -> vec3f {
  let red = vec3f(0.55, 0.04, 0.0);
  let orange = vec3f(1.0, 0.32, 0.03);
  let white = vec3f(1.0, 0.78, 0.45);
  return select(mix(orange, white, (heat - 0.55) / 0.45), mix(red, orange, heat / 0.55),
                heat < 0.55);
}

@fragment
fn burn_fs(in: BurnOut) -> @location(0) vec4f {
  let m = textureSampleLevel(burn_atlas, linear_clamp, in.uv, 0.0);
  let cover = m.a * in.look.x;
  if (cover <= 0.002) {
    discard;
  }
  // The ember: its heat its luminance over a fresh one's, hottest in the heart and the cracks.
  let heart = cover * cover;
  let crack = 0.6 + 0.8 * clamp(1.0 - m.g, 0.0, 1.0);
  let ember = in.look.z * heart * crack;
  let heat = clamp(in.look.z / max(bp.more.y, 1e-3), 0.0, 1.0);
  let glow = burn_ember(heat) * ember * frame.world.y / kSunLux;
  return vec4f(min(glow, vec3f(60000.0)), 1.0 - in.look.y * cover);
}
