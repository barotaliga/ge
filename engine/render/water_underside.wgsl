// THE WATER'S UNDERSIDE FROM THE AIR UNDER IT ("water.underside", drawn in the "water" pass;
// render/water.hpp's WaterDraw::open_below; docs/water.md § Drawn): a body whose water hangs over
// open air (a classic map's pool with no floor, which a player drops through) shows its bottom as
// a surface of its own, the shared half's geometry at the body's floor (`sun.z`) under the same
// waves as its top, laid out past the top's box and kept only where the opaque map does not stand
// at or in front of the floor's plane (the opening in the ceiling; drawn with no depth test). `fs_underside` lights it as the top is lit, turned over:
//
// - BACK DOWN: the air's Fresnel reflectance (light meeting water from the air) mirrors what lies
//   under it, the game's planar mirror in the floor's plane (render/water.hpp's WaterViews, `below`
//   at the floor) or else marched in screen space, the room's walls where neither finds anything.
// - UP THROUGH IT: the eye's ray bent into the water by Snell's law, carried up to the top, bent out
//   through the top's own waves there, and the world over the pool read along it (the game's view
//   up, else marched, else the sky's model or the room). Light that came in through the bottom
//   always leaves through a level top (it is inside the critical angle), so there is no window's
//   rim to see through a flat slab; where the top's waves tilt it past the critical angle the top
//   mirrors the water's inside back down instead, as total internal reflection does. The two
//   interfaces' n^2 radiance factors cancel.
// - THE WATER ON THE WAY: what is seen through it is dimmed by exp(-c L) over the path L and the
//   water's own light (its deep colour under the room's light) fills the rest.
#include "render/wgsl/water_surface.wgsl"

const kEta = 1.334;  // water's refractive index (546 nm)
// The view up's reach (render/water_below.wgsl's): the tangent, sine and cosine of 72 degrees.
const kUpRim = 3.0776835;
const kUpRimSin = 0.9510565;
const kUpRimCos = 0.3090170;

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
}

// Light leaving water into air at `cos_i` to the normal: the exact Fresnel reflectance of the two
// polarisations, averaged; 1 past the critical angle.
fn fresnel_out(cos_i: f32) -> f32 {
  let s2 = kEta * kEta * max(1.0 - cos_i * cos_i, 0.0);
  if (s2 >= 1.0) {
    return 1.0;
  }
  let ct = sqrt(1.0 - s2);
  let rs = (kEta * cos_i - ct) / (kEta * cos_i + ct);
  let rp = (kEta * ct - cos_i) / (kEta * ct + cos_i);
  return 0.5 * (rs * rs + rp * rp);
}

@fragment
fn fs_underside(in: VertexOut) -> FragmentOut {
  let foot = max(length(dpdx(in.xz)), length(dpdy(in.xz)));
  let dist = length(in.rel);
  let d = in.rel / dist;  // the eye's ray, up toward the underside
  let uv = in.position.xy * frame.size.zw;
  // THE OPENING: the plane is laid past the top's box (a pool's bottom may be wider than its top),
  // and wherever the map stands at or in front of the floor's plane (the ceiling round the hole,
  // flush with the water's bottom) the map is kept: the undisturbed plane's distance along the ray
  // against the opaque scene's, so the waves never lift the water over the ceiling's edge.
  let flat = (body.sun.z - frame.eye.y) / max(d.y, 1e-4);
  let behind = scene_rel(vec2i(in.position.xy), uv);
  if (behind.w > 0.5 && length(behind.xyz) < flat + 0.05 + 0.002 * flat) {
    discard;
  }
  let t = frame.time.x;
  let sl = surface_slope(in.xz, t, foot);
  let up = normalize(vec3f(-sl.x, 1.0, -sl.y));  // the waves' normal, up into the water
  let room = vec3f(white_luminance() * body.emission.w);
  let walls = room * kRoomAlbedo;
  let open = body.rough.w;
  let refl = fresnel(dot(d, up));

  // BACK DOWN: the underside's mirror of what lies under it.
  let r = reflect(d, up);
  let down = normalize(vec3f(r.x, min(r.y, -0.01), r.z));
  var seen = walls;
  if (body.view.x > 1.5) {
    var leg = 0.0;
    let m = mirror_along(in.rel, down, uv, dist, &leg);
    seen = mix(walls, m.rgb, max(m.a, 0.0));
  } else if (body.view.x > -0.5) {
    var hit = 0.0;
    let m = march_scene(in.rel, down, max(0.02 * dist, 0.25), &hit);
    seen = mix(walls, m.rgb, m.a);
  }

  // UP THROUGH IT: into the water, up to the top, out through the top's waves.
  var tin = refract(d, -up, 1.0 / kEta);
  if (dot(tin, tin) < 1e-6) {
    tin = d;
  }
  let rise = max(body.info.x - (in.rel.y + frame.eye.y), 0.01);
  let leg_in = rise / max(tin.y, 0.05);
  let exit_rel = in.rel + tin * leg_in;
  let top_sl = surface_slope(exit_rel.xz + frame.eye.xz, t, foot);
  let top = normalize(vec3f(-top_sl.x, 1.0, -top_sl.y));
  let refl_top = fresnel_out(clamp(dot(tin, top), 0.0, 1.0));
  // The water's own light: its deep colour under the room's light.
  let own = room * body.deep.rgb + body.emission.rgb;
  var above = vec3f(0.0);
  if (refl_top < 1.0) {
    var tout = refract(tin, -top, kEta);
    if (dot(tout, tout) < 1e-6) {
      tout = normalize(tin - top * dot(tin, top) + top * 0.02);
    }
    let sky = mix(walls, sky_luminance(normalize(vec3f(tout.x, max(tout.y, 0.01), tout.z))), open);
    if (body.view.y > 0.5) {
      // The game's view up, its camera on the top straight over the eye (water_below.wgsl's read).
      var up_ray = tout;
      let across = length(tout.xz);
      if (across > kUpRim * max(tout.y, 0.0) && across > 1e-5) {
        up_ray = vec3f(tout.x / across * kUpRimSin, kUpRimCos, tout.z / across * kUpRimSin);
      }
      let c = body.up_view_proj *
              vec4f(exit_rel + vec3f(0.0, frame.eye.y - body.info.x, 0.0) + up_ray * 6.0, 1.0);
      let st = clamp(vec2f(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5), vec2f(0.0), vec2f(1.0));
      let u = textureSampleLevel(up_view, linear_clamp, st, 0.0);
      above = mix(sky, u.rgb, u.a);
    } else {
      var hit = 0.0;
      let m = march_scene(exit_rel, tout, 0.25, &hit);
      above = mix(mix(walls, sky, smoothstep(0.05, 0.3, tout.y)), m.rgb, m.a);
    }
  }
  let through = exp(-body.c.rgb * leg_in);
  let inside = (above * (1.0 - refl_top) + own * refl_top) * through + own * (1.0 - through);
  let c = seen * refl + inside * (1.0 - refl);
  var out: FragmentOut;
  out.colour = vec4f(haze(c, uv, dist), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}
