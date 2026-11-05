// THE SHORE'S WET BAND (render/water.hpp; docs/water.md § The shore): `fs_shore` over the shared
// half (render/wgsl/water_surface.wgsl), drawn in the "water" pass before the bodies as a
// full-screen triangle multiplying the frame's colour. Each pixel's opaque point (the depth copy)
// that stands a little over a body's level and near its water is darkened: wet up to most of the
// run-up (the waves' reach over the last minutes, a calm lake's lap included), fading over the rest
// of it, and darker still where the waves run up it now.
#include "render/wgsl/water_surface.wgsl"

struct ShoreVertex {
  @builtin(position) position: vec4f,
}

@vertex
fn vs_full(@builtin(vertex_index) i: u32) -> ShoreVertex {
  let p = vec2f(f32((i << 1u) & 2u), f32(i & 2u)) * 2.0 - 1.0;
  var out: ShoreVertex;
  out.position = vec4f(p, 0.0, 1.0);
  return out;
}

const kShoreReach = 12.0;  // m past a lake's box its band may lie (render/water.hpp's kShoreReach)
const kWetDark = 0.45;     // the share of its light wet ground loses

struct ShoreOut {
  @location(0) colour: vec4f,  // multiplies the frame's
  @location(1) motion: vec4f,  // not written
}

@fragment
fn fs_shore(in: ShoreVertex) -> ShoreOut {
  let pixel = vec2i(in.position.xy);
  let uv = in.position.xy * frame.size.zw;
  let p = scene_rel(pixel, uv);
  if (p.w < 0.5) {
    discard;
  }
  let world = p.xyz + frame.eye.xyz;
  let above = world.y - body.info.x;
  let runup = body.surf.y;
  if (above < -0.05 || above > 1.2 * runup) {
    discard;
  }
  // Near the water: the shore mask's ramp past the outline (render/water.hpp's `shore_mask`),
  // read at the nearest texel of the box, fading over the way out past the box.
  let xz = world.xz;
  let cells = vec2f(textureDimensions(shore));
  let on_box = (xz - body.box.xy) / max(body.box.zw - body.box.xy, vec2f(1e-3));
  let mask = textureLoad(shore, vec2i(clamp(on_box * cells, vec2f(0.0), cells - 1.0)), 0).r;
  let out_of_box = length(max(body.box.xy - xz, vec2f(0.0)) + max(xz - body.box.zw, vec2f(0.0)));
  let near = clamp(mask * 2.0, 0.0, 1.0) * (1.0 - smoothstep(0.0, kShoreReach, out_of_box));
  if (near <= 0.0) {
    discard;
  }
  // Where the run-ups of the last minutes reached, and where the waves run up it now (their
  // height here, carried up the beach by half again).
  let reached = 1.0 - smoothstep(0.6 * runup, 1.2 * runup, above);
  let now = 1.0 - smoothstep(0.0, 0.02, above - (1.5 * height_at(xz, frame.time.x, 0.5) + 0.01));
  let wet = max(0.75 * reached, now) * near;
  var out: ShoreOut;
  out.colour = vec4f(vec3f(1.0 - kWetDark * wet), 1.0);
  out.motion = vec4f(0.0);
  return out;
}
