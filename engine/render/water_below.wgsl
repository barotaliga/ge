// THE WATER'S SURFACE FROM BELOW ("water.below", render/water.hpp; docs/water.md § Drawn):
// `fs_below` over the shared half (render/wgsl/water_surface.wgsl): the same surface seen from
// under it, the view refracted out through the surface within Snell's window, the world above read
// along the bent ray (the game's view up, else marched in screen space, else the sky's model),
// total internal reflection past it mirroring the water below (the mirror under the surface, else
// marched), weighed by the exact Fresnel terms for light leaving water; what the reflection finds
// is fogged over its own way through the water, and the share it lacks (nothing to mirror, the
// water's own light) goes out beside it for the view under water to fill with its veil.
//
// THE RIM: the Fresnel terms are taken per colour (blue bends most, so its window is the narrowest
// and the rim is fringed) and averaged over the normals the pixel cannot resolve (the slope
// variance its waves lose to the footprint), so a far or choppy rim fades into the mirror over a
// few degrees as it does on real water, while a near calm one stays sharp; the waves the pixel
// does resolve (the chop, the wakes) break it up. The light through the window is n^2 times the
// air's radiance: the whole hemisphere is gathered into its cone.
#include "render/wgsl/water_surface.wgsl"

// The view up's reach: the tangent, sine and cosine of 72 degrees off straight up, inside the 75
// degrees its square reaches along its sides (brush/app/bsp_views.hpp's kUpFov).
const kUpRim = 3.0776835;
const kUpRimSin = 0.9510565;
const kUpRimCos = 0.3090170;

struct BelowOut {
  @location(0) colour: vec4f,  // the surface's light, its distance from the eye (m)
  @location(1) veil: vec4f,    // the share of it the water's own light fills
}

@fragment
fn fs_below(in: VertexOut) -> BelowOut {
  let foot = max(length(dpdx(in.xz)), length(dpdy(in.xz)));
  if (any(in.xz < body.box.xy) || any(in.xz > body.box.zw)) {
    discard;
  }
  let cells = vec2f(textureDimensions(shore));
  let on_box = (in.xz - body.box.xy) / max(body.box.zw - body.box.xy, vec2f(1e-3));
  if (textureLoad(shore, vec2i(clamp(on_box * cells, vec2f(0.0), cells - 1.0)), 0).r < 0.5) {
    discard;
  }
  let t = frame.time.x;
  let sl = surface_slope(in.xz, t, foot);
  let n = normalize(vec3f(-sl.x, 1.0, -sl.y));  // up, out of the water
  let dist = length(in.rel);
  let d = in.rel / dist;  // the eye's ray, up toward the surface
  let uv = in.position.xy * frame.size.zw;
  let cos_i = clamp(dot(d, n), 0.0, 1.0);
  // Half the slope variance lost to the footprint tilts the normal in the plane of incidence.
  let refl3 = fresnel_exit_rough(cos_i, sqrt(0.5 * sl.z));
  let refl = refl3.g;
  let room = vec3f(white_luminance() * body.emission.w);
  let open = body.rough.w;

  // OUT THROUGH THE WINDOW: the ray bent away from the normal by Snell's law; past the critical
  // angle (where only the rough rim's tilted normals let light through) the ray out grazes the
  // surface along the eye's bearing.
  var above = vec3f(0.0);
  if (refl3.r < 1.0) {
    var tr = refract(d, -n, kWaterEta.g);
    if (dot(tr, tr) < 1e-6) {
      tr = normalize(d - n * dot(d, n) + n * 0.02);
    }
    let sky = mix(room * kRoomAlbedo, sky_luminance(normalize(vec3f(tr.x, max(tr.y, 0.01), tr.z))),
                  open);
    if (body.view.y > 0.5) {
      // The game's view up, its camera on the surface straight over the eye: the point a few
      // metres along the bent ray. A ray bent out nearer level than the view reaches is read at
      // its rim along the same bearing (the walls at the waterline are much like those just over
      // them), so the window's last ring is never a hole.
      var up_ray = tr;
      let across = length(tr.xz);
      if (across > kUpRim * max(tr.y, 0.0) && across > 1e-5) {
        up_ray = vec3f(tr.x / across * kUpRimSin, kUpRimCos, tr.z / across * kUpRimSin);
      }
      let c = body.up_view_proj *
              vec4f(in.rel + vec3f(0.0, frame.eye.y - body.info.x, 0.0) + up_ray * 6.0, 1.0);
      let st = clamp(vec2f(c.x / c.w * 0.5 + 0.5, 0.5 - c.y / c.w * 0.5), vec2f(0.0), vec2f(1.0));
      let u = textureSampleLevel(up_view, linear_clamp, st, 0.0);
      above = mix(sky, u.rgb, u.a);
    } else {
      var hit = 0.0;
      let m = march_scene(in.rel, tr, max(0.02 * dist, 0.25), &hit);
      // Nearly level and found on no screen: the walls round the water, lit as the room is.
      let low = mix(room * kRoomAlbedo, sky, open * smoothstep(0.05, 0.3, tr.y));
      above = mix(low, m.rgb, m.a);
    }
  }

  // BACK DOWN: what the surface mirrors of the water below, fogged over its way through it.
  var below = vec3f(0.0);
  var veil = refl;
  let r = reflect(d, -n);
  if (body.view.x > 1.5) {
    var leg = 0.0;
    let m = mirror_along(in.rel, normalize(vec3f(r.x, min(r.y, -0.01), r.z)), uv, dist, &leg);
    let found = max(m.a, 0.0);
    let through = exp(-body.c.rgb * leg);
    below = m.rgb * found * through;
    veil = refl * (1.0 - found * dot(through, vec3f(0.2126, 0.7152, 0.0722)));
  } else {
    var hit = 0.0;
    let m = march_scene(in.rel, r, max(0.02 * dist, 0.25), &hit);
    if (m.a > 0.0) {
      let through = exp(-body.c.rgb * hit);
      below = m.rgb * m.a * through;
      veil = refl * (1.0 - m.a * dot(through, vec3f(0.2126, 0.7152, 0.0722)));
    }
  }
  var out: BelowOut;
  out.colour = vec4f(above * (1.0 - refl3) * (kWaterEta.g * kWaterEta.g) + below * refl3, dist);
  out.veil = vec4f(veil, 0.0, 0.0, 0.0);
  return out;
}
