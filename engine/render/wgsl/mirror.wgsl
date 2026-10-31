// THE SCENE IN A MIRROR (render/mirror.hpp): `scene_reflection(rel, r, rough)`, what the
// last frame drew along the reflected ray `r` from the camera-relative point `rel`: rgb the
// colour there, blurred by `rough` (0 a mirror .. 1 a rough film), and a the share it counts for
// (0 where the ray meets nothing in the picture, leaves it, or none is kept: the caller's sky
// stands there). The ray is marched in the last frame's view, 24 steps growing with the distance
// out to 120 m, then halved five times down onto what it meets; a point behind the kept surface
// by more than its own thickness (half a metre, more far off) passes behind it.
//
// ge:summary WGSL library: the scene along a reflected ray, marched through the last frame kept
// ge:keywords mirror reflection ssr scene_reflection mirror_last screen-space wgsl library
// ge:skill render
// ge:test tests/render_mirror_test.cpp

@group(0) @binding(15) var mirror_last: texture_2d<f32>;

// Where the camera-relative point `rel` was in the last frame's picture: uv, then its distance
// from the last frame's eye (m); w 0 where it was behind that eye.
fn mirror_at(rel: vec3f) -> vec4f {
  let q = rel + frame.eye_delta.xyz;
  let clip = frame.prev_view_proj * vec4f(q, 1.0);
  if (clip.w <= 1e-4) {
    return vec4f(0.0);
  }
  return vec4f(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5, length(q), 1.0);
}

fn scene_reflection(rel: vec3f, r: vec3f, rough: f32) -> vec4f {
  let dims = vec2f(textureDimensions(mirror_last));
  if (dims.x < 2.0) {
    return vec4f(0.0);
  }
  let reach = 120.0;
  var last = 0.0;
  for (var i = 1; i <= 24; i++) {
    let f = f32(i) / 24.0;
    let s = reach * f * f;
    let m = mirror_at(rel + r * s);
    if (m.w == 0.0 || any(m.xy < vec2f(0.0)) || any(m.xy > vec2f(1.0))) {
      return vec4f(0.0);
    }
    let seen = textureLoad(mirror_last, vec2i(m.xy * dims), 0).a;
    if (seen > 0.0 && m.z > seen) {
      if (m.z - seen < max(0.5, 0.15 * s)) {
        // Halve down onto the surface between the last step in front and this one.
        var a = last;
        var b = s;
        for (var k = 0; k < 5; k++) {
          let h = 0.5 * (a + b);
          let mh = mirror_at(rel + r * h);
          let sh = textureLoad(mirror_last, vec2i(clamp(mh.xy, vec2f(0.0), vec2f(0.9999)) * dims),
                               0).a;
          if (sh > 0.0 && mh.z > sh) {
            b = h;
          } else {
            a = h;
          }
        }
        let uv = clamp(mirror_at(rel + r * b).xy, vec2f(0.0), vec2f(1.0));
        // A rough surface spreads its mirror: four taps round the hit, wider the rougher.
        let spread = rough * rough * 0.03;
        var c = vec3f(0.0);
        for (var t = 0; t < 4; t++) {
          let a4 = (f32(t) + 0.5) * 1.5707963;
          let o = vec2f(cos(a4), sin(a4)) * spread * vec2f(dims.y / dims.x, 1.0);
          c += textureSampleLevel(mirror_last, linear_clamp, uv + o, 0.0).rgb;
        }
        let edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
        let share = smoothstep(0.0, 0.08, edge) * (1.0 - smoothstep(0.6, 1.0, b / reach));
        return vec4f(c * 0.25, share);
      }
    }
    last = s;
  }
  return vec4f(0.0);
}
