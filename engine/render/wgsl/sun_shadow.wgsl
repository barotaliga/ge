// THE SUN'S SHADOW of what stands on the ground (render/shadows.hpp): `SunShadow`, field for
// field `ShadowParams`, and `sun_shadow`, the share of the sun a point gets past the casters
// drawn into the maps: the two cascades (what stands still) and the figures' atlas tiles (what
// moves). Every map is an orthographic view along the sun with depth 1 toward the sun and 0 away
// from it, cleared to 0 (nothing casts); a point is lit where its depth is at least the map's.
// Positions are camera-relative: a map's matrix takes a point relative to the map's own centre,
// and `offset` (the eye minus that centre) moves a camera-relative point there.

struct SunShadow {
  cascade: array<mat4x4f, 2>,  // a point relative to the cascade's centre -> its clip
  offset: array<vec4f, 2>,     // the eye minus the cascade's centre, m; w: 1 when drawn
  atlas: mat4x4f,              // a point relative to a figure's centre -> its tile's clip
  tile: array<vec4f, 16>,      // the eye minus figure k's centre, m; w: 1 when drawn
  info: vec4f,                 // x: texels across a map; y: tiles across the atlas; z: the
                               // receiver's depth bias in a cascade (clip z); w: the figures
  texel: vec4f,                // a texel's size, m: cascade 0, cascade 1, the atlas; w: the
                               // receiver's depth bias in a tile (clip z)
}

const kShadowTaps = array<vec2f, 4>(vec2f(-0.94, -0.34), vec2f(0.34, -0.94), vec2f(0.94, 0.34),
                                    vec2f(-0.34, 0.94));

// Four rotated taps round `uv` (each a hardware 2x2 comparison) of `layer`, `radius` uv apart,
// held inside [lo, hi] (a tile's texels).
fn shadow_taps(maps: texture_depth_2d_array, cmp: sampler_comparison, layer: i32, uv: vec2f,
               depth: f32, radius: f32, angle: f32, lo: vec2f, hi: vec2f) -> f32 {
  let c = cos(angle);
  let s = sin(angle);
  var sum = 0.0;
  for (var k = 0; k < 4; k++) {
    let o = kShadowTaps[k];
    let at = clamp(uv + vec2f(c * o.x - s * o.y, s * o.x + c * o.y) * radius, lo, hi);
    sum += textureSampleCompareLevel(maps, cmp, at, layer, depth);
  }
  return sum * 0.25;
}

// The share of the sun the camera-relative point `rel` (normal `n`) gets past the casters.
// `pixel` turns the taps (interleaved gradient noise, stepped by `frame_no`; the TAA averages it).
fn sun_shadow(s: SunShadow, maps: texture_depth_2d_array, cmp: sampler_comparison, rel: vec3f,
              n: vec3f, pixel: vec2f, frame_no: f32) -> f32 {
  let p = pixel + 5.588238 * f32(u32(frame_no) % 64u);
  let angle = 6.2831853 * fract(52.9829189 * fract(dot(p, vec2f(0.06711056, 0.00583715))));
  let texel_uv = 1.0 / max(s.info.x, 1.0);
  var lit = 1.0;
  // The near cascade where it reaches, else the far one; past both, nothing stands in the way.
  for (var c = 0; c < 2; c++) {
    if (s.offset[c].w == 0.0) {
      continue;
    }
    let q = rel + s.offset[c].xyz + n * (1.5 * s.texel[c]);
    let clip = s.cascade[c] * vec4f(q, 1.0);
    if (max(abs(clip.x), abs(clip.y)) < 0.98 && clip.z > 0.0 && clip.z < 1.0) {
      let uv = vec2f(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
      lit = shadow_taps(maps, cmp, c, uv, clip.z + s.info.z, 1.5 * texel_uv, angle, vec2f(0.0),
                        vec2f(1.0));
      break;
    }
  }
  // Each figure's tile casts its figure (and whatever else stood in it) where it reaches.
  let tiles = max(s.info.y, 1.0);
  let figures = u32(s.info.w);
  for (var k = 0u; k < figures; k++) {
    let t = s.tile[k];
    if (t.w == 0.0) {
      continue;
    }
    let q = rel + t.xyz + n * (1.5 * s.texel.z);
    let clip = s.atlas * vec4f(q, 1.0);
    if (max(abs(clip.x), abs(clip.y)) >= 1.0 || clip.z <= 0.0 || clip.z >= 1.0) {
      continue;
    }
    let local = vec2f(clip.x * 0.5 + 0.5, 0.5 - clip.y * 0.5);
    let cell = vec2f(f32(k % u32(tiles)), f32(k / u32(tiles)));
    let half_texel = 0.5 * texel_uv;
    let lo = cell / tiles + vec2f(half_texel);
    let hi = (cell + vec2f(1.0)) / tiles - vec2f(half_texel);
    lit = min(lit, shadow_taps(maps, cmp, 2, (cell + local) / tiles, clip.z + s.texel.w,
                               1.5 * texel_uv, angle, lo, hi));
  }
  return lit;
}
