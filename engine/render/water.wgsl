// THE WATER'S SURFACE FROM ABOVE (render/water.hpp; docs/water.md § Drawn): `fs_main`, each pixel's
// light over the shared half (render/wgsl/water_surface.wgsl: the geometry, the waves, the wakes,
// the rain's rings, the gusts and the stir, the planar mirror's read), and the caustics on the bed
// seen through it (render/wgsl/water_body.wgsl's `caustic_at`).
#include "render/wgsl/water_surface.wgsl"

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
  // The pixel's footprint on the water, m (before anything can discard).
  let foot = max(length(dpdx(in.xz)), length(dpdy(in.xz)));
  if (any(in.xz < body.box.xy) || any(in.xz > body.box.zw)) {
    discard;
  }
  // Inside the shore's mask (water.hpp's `shore_mask`): a lake's box beyond its shore is dry.
  let cells = vec2f(textureDimensions(shore));
  let on_box = (in.xz - body.box.xy) / max(body.box.zw - body.box.xy, vec2f(1e-3));
  if (textureLoad(shore, vec2i(clamp(on_box * cells, vec2f(0.0), cells - 1.0)), 0).r < 0.5) {
    discard;
  }
  let t = frame.time.x;
  let sl = surface_slope(in.xz, t, foot);
  let lost = sl.z;
  let n = normalize(vec3f(-sl.x, 1.0, -sl.y));
  let dist = length(in.rel);
  let uv = in.position.xy * frame.size.zw;
  // The planar mirror where the game drew one: the reflected ray read where its image lands, the
  // sky's model where the mirror shows the sky, and the sun's glitter only there.
  var seen = vec4f(0.0);
  var sun_seen = 1.0;
  if (body.view.x > 0.5 && body.view.x < 1.5) {
    let v = -in.rel / dist;
    let r0 = reflect(-v, n);
    let r = normalize(vec3f(r0.x, max(r0.y, 0.01), r0.z));
    var leg = 0.0;
    let m = mirror_along(in.rel, r, uv, dist, &leg);
    if (m.a >= 0.0) {
      seen = vec4f(mix(sky_luminance(r), m.rgb, m.a), 1.0);
      sun_seen = 1.0 - m.a;
    } else {
      // Off the mirror's picture: the room's walls, lit as the room is; no sun.
      seen = vec4f(vec3f(white_luminance() * body.emission.w * kRoomAlbedo), 1.0);
      sun_seen = 0.0;
    }
  }
  // A body under a roof (a cave's pool) sees the sky through `open` of its dome (rough.w) and the
  // room's own light, a share of a white face's (emission.w), over the rest; the day lights the
  // water itself by its own share (air.x).
  // THE CAUSTICS on what is seen under the surface (its bed, a rock): the sun's part of its light
  // gathered and spread by the waves over it, as far as the sun reaches the water.
  var bed_light = 1.0;
  let behind = scene_rel(vec2i(in.position.xy), uv);
  let bed_depth = body.info.x - (behind.y + frame.eye.y);
  if (behind.w > 0.5 && bed_depth > 0.0 && body.sun.x > 0.0) {
    let at = behind.xyz + frame.eye.xyz;
    let share = sun_share(body.info.x, in.xz) * body.sun.x * exp(-body.kd.g * bed_depth);
    // The pixel's footprint on the bed: the surface's, grown over the way on down to the bed.
    let bed_foot = foot * length(behind.xyz) / max(dist, 0.1);
    bed_light = 1.0 + (caustic_at(at.xz, bed_depth, sun_down(), t, 1.0, bed_foot) - 1.0) * share;
  }
  let c = shade_water_lit(n, in.rel, in.xz, in.position.xyz, body.info.x, lost, body.rough.z,
                          Optics(body.c.rgb, body.kd.rgb, body.deep.rgb, body.emission.rgb), 0.0,
                          body.rough.w, body.emission.w, body.air.x, seen, sun_seen, bed_light);
  var out: FragmentOut;
  out.colour = vec4f(haze(c, uv, dist), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}

