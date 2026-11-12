// THE WATER'S UNDERSIDE FROM THE AIR UNDER IT ("water.underside", drawn in the "water" pass;
// render/water.hpp's WaterDraw::open_below; docs/water.md § Drawn): a body whose water hangs over
// open air (a classic map's pool with no floor, which a player drops through) shows its bottom as
// a surface of its own, the shared half's geometry at the body's floor (`sun.z`) under the same
// waves as its top, laid out past the top's box and kept only where the opaque map does not stand
// at or in front of the floor's plane (the opening in the ceiling; drawn with no depth test).
//
// WATER UPSIDE DOWN, SEEN AS WATER: the physics of a flat slab hung over air is no help to the eye
// (light that came in through the bottom always leaves through a level top, so the room over it
// just shows through, dimmed, and the bottom mirrors a few per cent), so `fs_underside` draws two
// surfaces a player knows instead, each as it is seen in a pool the right way up:
//
// - THE BOTTOM, AS WATER SEEN FROM OVER IT (its "over" is the eye's side, the hall): the hall
//   mirrored in its waves (the game's planar mirror in the floor's plane, else marched in screen
//   space, else the room's walls) by the Fresnel reflectance off water, never under `kSheen` (as a
//   pool seen from its side at a low angle mirrors), so its waves show; the hall's lamps glint
//   on it; and what is seen into it is bent by its waves.
// - THE TOP, AS WATER SEEN FROM UNDER IT: the eye's ray goes on into the water at its own angle
//   (as if the eye were in it), bent only by the bottom's waves (`kBend` times their refraction),
//   up to the top, where Snell's law takes it out through the top's own waves inside the window
//   (48.6 degrees off straight up, the room over the pool read along the bent ray: the game's
//   view up, else marched, else the sky's model or the room) and the exact Fresnel terms for
//   light leaving water mirror the inside back down past it (total internal reflection: the pool's
//   own walls marched from the top along the mirrored ray, fogged over the way, else the water's
//   own light). The window's light is n^2 the air's (the
//   hemisphere gathered into its cone) and flickers as the top's waves gather the light over it
//   (their caustics). So the room over the pool is warped twice, by each surface's waves.
// - THE WATER ON THE WAY: what is seen through it is dimmed by exp(-c L) over the path L and the
//   water's own light (its deep colour under the room's light) fills the rest.
//
// The bottom's slopes and the top's are shown `kShowWaves` times steeper than the true waves (the
// look only: the geometry and what a float rides keep the true ones). The cost, paid only by the
// underside's pixels: one caustic sum over the waves, the lamps' glint loop over the pixel's
// cluster, a three-tap Fresnel at the top, and a screen-space march only where the top reflects
// totally or the view up does not reach (render/water_below.wgsl marches every pixel).
#include "render/wgsl/water_surface.wgsl"

// The view up's reach (render/water_below.wgsl's): the tangent, sine and cosine of 72 degrees.
const kUpRim = 3.0776835;
const kUpRimSin = 0.9510565;
const kUpRimCos = 0.3090170;
// The bottom's least reflectance: a pool seen from its side at a low angle mirrors about this much,
// which is what shows its waves.
const kSheen = 0.3;
// How strongly the bottom's waves bend the ray into the water: their refraction, made as plain as
// the top's are from under it (a slab's thin bottom bends the eye's ray little).
const kBend = 3.0;
// The waves' slopes as the underside shows them, both surfaces': a hall's pool lies nearly still
// (its rms slope a few hundredths), which from under it reads as glass; this many times steeper,
// it reads as water. The geometry and what a float rides keep the true waves.
const kShowWaves = 4.0;

struct FragmentOut {
  @location(0) colour: vec4f,
  @location(1) motion: vec4f,
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
  let sl = surface_slope(in.xz, t, foot) * vec3f(kShowWaves, kShowWaves, kShowWaves * kShowWaves);
  let up = normalize(vec3f(-sl.x, 1.0, -sl.y));  // the waves' normal, up into the water
  let face = -up;                                 // the bottom's face, toward the eye
  let v = -d;                                     // toward the eye
  let cos_v = max(dot(face, v), 1e-3);
  let room = vec3f(white_luminance() * body.emission.w);
  let walls = room * kRoomAlbedo;
  let open = body.rough.w;
  let refl = max(fresnel(cos_v), kSheen);
  let mss = max(sl.z, 2e-4);

  // THE BOTTOM'S MIRROR: the hall under it, in its waves.
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
  // The hall's lamps glinting on it, over the slopes the pixel cannot resolve.
  var glint = vec3f(0.0);
  if (light_grid.info.x != 0u) {
    let cell = light_grid.cells[cluster_index(in.position.xyz)];
    for (var k = 0u; k < cell.y; k++) {
      var l: vec3f;
      let i = light_grid.refs[cell.x + k];
      let e = lamp_irradiance(i, in.rel, &l) * lamp_shadow(i, in.rel, face, in.position.xyz);
      let hl = normalize(l + v);
      let masked = smith_g1(cos_v, mss) * smith_g1(dot(face, l), mss);
      glint += e * (fresnel(dot(v, hl)) * beckmann(dot(face, hl), mss) / (4.0 * cos_v) * masked *
                    step(0.0, dot(face, l)));
    }
  }

  // INTO THE WATER at the eye's own angle, bent by the bottom's waves.
  let level_in = refract(d, vec3f(0.0, -1.0, 0.0), 1.0 / kWaterEta.g);
  var wavy_in = refract(d, face, 1.0 / kWaterEta.g);
  if (dot(wavy_in, wavy_in) < 1e-6) {
    wavy_in = level_in;
  }
  var tin = normalize(d + kBend * (wavy_in - level_in));
  tin = normalize(vec3f(tin.x, max(tin.y, 0.05), tin.z));
  let rise = max(body.info.x - (in.rel.y + frame.eye.y), 0.01);
  let leg_in = rise / tin.y;
  let exit_rel = in.rel + tin * leg_in;
  let exit_xz = exit_rel.xz + frame.eye.xz;
  let top_sl = surface_slope(exit_xz, t, foot * max(leg_in / dist + 1.0, 1.0)) *
               vec3f(kShowWaves, kShowWaves, kShowWaves * kShowWaves);
  let top = normalize(vec3f(-top_sl.x, 1.0, -top_sl.y));
  // OUT THROUGH THE TOP: Snell's window, total internal reflection past it, the rim softened by
  // the slopes the pixel cannot resolve.
  let refl_top = fresnel_exit_rough(clamp(dot(tin, top), 0.0, 1.0), sqrt(0.5 * top_sl.z));
  // The water's own light: its deep colour under the room's light.
  let own = room * body.deep.rgb + body.emission.rgb;
  var above = vec3f(0.0);
  if (refl_top.r < 1.0) {
    var tout = refract(tin, -top, kWaterEta.g);
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
      let raw = vec2f(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5);
      let st = clamp(raw, vec2f(0.0), vec2f(1.0));
      let u = textureSampleLevel(up_view, linear_clamp, st, 0.0);
      above = mix(sky, u.rgb, u.a);
      // A point the view up does not hold (an exit far from straight over the eye) is read off
      // the screen instead, where the hole in the ceiling shows the room over the pool, never
      // smeared from the view's edge.
      let edge = min(min(raw.x, 1.0 - raw.x), min(raw.y, 1.0 - raw.y));
      let held = select(0.0, smoothstep(0.0, 0.04, edge), c.w > 0.0);
      if (held < 1.0) {
        var hit = 0.0;
        let m = march_scene(exit_rel, tout, 0.25, &hit);
        above = mix(mix(mix(walls, sky, smoothstep(0.05, 0.3, tout.y)), m.rgb, m.a), above, held);
      }
    } else {
      var hit = 0.0;
      let m = march_scene(exit_rel, tout, 0.25, &hit);
      above = mix(mix(walls, sky, smoothstep(0.05, 0.3, tout.y)), m.rgb, m.a);
    }
    // The window flickers as the top's waves gather the light coming down through it.
    above *= caustic_at(exit_xz, rise * kShowWaves, vec3f(0.0, -1.0, 0.0), t, 1.0, foot);
  }
  // Past the window the top mirrors the inside: the pool's own walls, marched from the top along
  // the mirrored ray where the screen holds them, fogged over that way too; else the water's own
  // light (the hall far down through the bottom is lost in the water on the way).
  let through = exp(-body.c.rgb * leg_in);
  var back = own;
  if (refl_top.r > 0.02) {
    let rt = reflect(tin, -top);
    var hit = 0.0;
    let m = march_scene(exit_rel, normalize(vec3f(rt.x, min(rt.y, -0.05), rt.z)), 0.25, &hit);
    let fog = exp(-body.c.rgb * hit);
    back = mix(own, m.rgb * fog + own * (1.0 - fog), m.a);
  }
  let window = above * (1.0 - refl_top) * (kWaterEta.g * kWaterEta.g) + back * refl_top;
  let inside = window * through + own * (1.0 - through);
  let c = seen * refl + inside * (1.0 - refl) + glint;
  var out: FragmentOut;
  out.colour = vec4f(haze(c, uv, dist), 1.0);
  out.motion = vec4f(screen_motion(frame, vec4f(in.rel, 1.0), in.position.xy), 0.0, 0.0);
  return out;
}
