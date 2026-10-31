// THE VIEW FROM UNDER A BODY (render/liquid.hpp § Seen from below): one triangle over the screen
// when the eye is under a lava's or a slime's level, drawn after the surfaces. A slime is a fog
// of its own colour lit by the light that reaches the eye's depth, its surface from below a
// window onto the world over it and a mirror past it, its scum a shadow on the window; a lava is
// opaque, its melt's glow all round, cooler streaks rising through it.
#include "render/wgsl/frame.wgsl"
#include "render/wgsl/liquid_body.wgsl"

struct UnderOut {
  @builtin(position) position: vec4f,
}

@vertex
fn vs_under(@builtin(vertex_index) vid: u32) -> UnderOut {
  let p = vec2f(f32((vid << 1u) & 2u), f32(vid & 2u));
  var out: UnderOut;
  out.position = vec4f(p * 2.0 - 1.0, 0.0, 1.0);
  return out;
}

fn luma(c: vec3f) -> f32 {
  return dot(c, vec3f(0.2126, 0.7152, 0.0722));
}

// The camera-relative direction through the screen's point `uv`.
fn ray_at(uv: vec2f) -> vec3f {
  let ndc = vec2f(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  let p = frame.inv_view_proj * vec4f(ndc, 0.5, 1.0);
  return normalize(p.xyz / p.w);
}

// The scene copy where the camera-relative point `rel` lands on the screen (none off it).
fn scene_toward(rel: vec3f) -> vec4f {
  let clip = frame.view_proj * vec4f(rel, 1.0);
  if (clip.w <= 1e-4) {
    return vec4f(0.0);
  }
  let uv = vec2f(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
  if (any(uv < vec2f(0.0)) || any(uv >= vec2f(1.0))) {
    return vec4f(0.0);
  }
  let edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
  return vec4f(textureLoad(scene_colour, vec2i(uv * frame.size.xy), 0).rgb,
               smoothstep(0.0, 0.08, edge));
}

// The melt's luminance against a white face's, all round the eye.
const kUnderKey = 0.6;

@fragment
fn fs_under(in: UnderOut) -> FragmentOut {
  let t = frame.time.x;
  let uv0 = in.position.xy * frame.size.zw;
  var out: FragmentOut;
  out.motion = vec4f(0.0);
  let depth = max(body.info.x - frame.eye.y, 0.0);
  if (body.info.y < 0.5) {
    // LAVA: opaque, the melt's glow all round, cooler streaks rising through it, the eye keyed to
    // the melt below its white, so the tone curve's shoulder leaves it orange.
    let dir = ray_at(uv0);
    let q = vec2f(atan2(dir.x, dir.z) * 2.0, dir.y * 3.0);
    let swirl = fbm(q * 1.5 + vec2f(0.0, -t * 0.35));
    let streak = fbm(q * vec2f(2.5, 5.0) + vec2f(2.0 * swirl, -t * 0.8));
    let cool = smoothstep(0.35, 0.8, streak) * 180.0 + (1.0 - swirl) * 50.0;
    let core = body.lava0.x;
    let key = white_luminance() * kUnderKey / max(luma(glow_at(core)), 1e-6);
    let melt = glow_at(core - cool) * key;
    // What touches the eye shows through the first few centimetres of melt.
    let d = behind_distance(vec2i(in.position.xy), uv0);
    let through = exp(-d * 120.0);
    out.colour = vec4f(mix(melt, textureLoad(scene_colour, vec2i(in.position.xy), 0).rgb, through),
                       1.0);
    return out;
  }
  // SLIME: a slow wobble of the thick stuff before the eye.
  let uv = uv0 + 0.004 * vec2f(sin(t * 0.9 + uv0.y * 9.0), cos(t * 0.7 + uv0.x * 7.0));
  let pixel = vec2i(clamp(uv, vec2f(0.0), vec2f(0.9999)) * frame.size.xy);
  let dir = ray_at(uv);
  let d = behind_distance(pixel, uv);
  let K = body.absorb.rgb;
  let S = max(body.scatter.rgb, vec3f(1e-4));
  // A game's least view: no band thicker than 3 / reach (the reach's 5% left).
  var c = K + S;
  if (body.fx.w > 0.0) {
    c = min(c, vec3f(3.0 / body.fx.w));
  }
  let ks = K / S;
  let own = 1.0 + ks - sqrt(ks * ks + 2.0 * ks);
  let kd = sqrt(K * (K + 2.0 * S)) * (c / (K + S));
  // The light at the eye's depth: what falls on the surface, dimmed on its way down.
  let open_sky = body.flow.z;
  let room = vec3f(white_luminance() * body.flow.w);
  let sun = sun_light(body.info.x, frame.eye.xz) * step(0.0, frame.sun.y) * open_sky;
  let down = sun * max(frame.sun.y, 0.0) + sky_irradiance(vec3f(0.0, 1.0, 0.0)) * open_sky +
             room * PI;
  let veil = own * down * exp(-kd * depth) / PI + body.glow.rgb * frame.world.y / kSunLuxCd;
  // The path in the liquid: to the scene, or to the surface over the eye.
  let ts = select(1e9, depth / max(dir.y, 1e-5), dir.y > 1e-5);
  var beyond = textureLoad(scene_colour, pixel, 0).rgb;
  var path = d;
  if (ts < d) {
    path = ts;
    let hit = dir * ts;
    let out_dir = refract(dir, vec3f(0.0, -1.0, 0.0), 1.34);
    if (dot(out_dir, out_dir) < 0.25) {
      // Past the window: the surface mirrors the slime below.
      beyond = veil;
    } else {
      let across = scene_toward(hit + out_dir * max(d - ts, 1.0));
      let sky = mix(room, sky_luminance(out_dir), open_sky);
      let seen = mix(sky, across.rgb, across.a);
      let f = fresnel(dir.y, 0.02);
      beyond = mix(seen, veil, f);
      // The scum floating over the window: its underside, dark.
      let xz = hit.xz + frame.eye.xz;
      let drift = xz - body.flow.xy * t;
      let field = fbm(drift * 0.9) * 0.7 + fbm(drift * 4.0) * 0.3;
      let cover = body.slime0.z;
      let scum = step(1e-3, cover) *
                 smoothstep(1.0 - cover - 0.05, 1.0 - cover + 0.1, field);
      beyond = mix(beyond, veil * 0.25, clamp(scum, 0.0, 1.0));
    }
  }
  let through = exp(-c * path);
  out.colour = vec4f(beyond * through + veil * (1.0 - through), 1.0);
  return out;
}
