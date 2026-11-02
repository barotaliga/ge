// THE SHARE OF THE FALL THAT REACHES A POINT under a shelter map (render/precip.hpp's
// `PrecipShelter`): an orthographic depth map looking along the fall, holding for each texel the
// surface nearest the sky. One function for every reader, so the faces a renderer wets and snows
// on, the flakes and drops drawn and the splashes agree on where the weather reaches.
//
// The drops and the flakes do not come along one line: the gusts veer them and the eddies toss
// them, so they arrive on a cone of ways round the fall's (`spread`, the cone's tangent: wider for
// snow than for rain). For each of `rays` ways over that cone the ray back up toward the sky is
// MARCHED through the map: at each step the map says whether a surface stands between that point
// and the sky along the fall. A way is open only when every step is; one covered step (the air of
// a roofed room, the lee under an eave, the wall beside a window) closes it. The share of open
// ways is what reaches `p`, so a window's patch is densest where the line through it lands and
// thins out, scattered, round it, and a room with a roof stays dry however wide the cone.
//
// The march looks only at the air the ray crosses: a point under a roof is covered at its first
// step, whatever lies open beside the building. (A soft shadow's taps round the point, compared
// against the point's own depth, see the open ground next to the room as open and let the weather
// in through the roof; that is why this marches instead.)
//
// ge:summary WGSL library: the share of the fall that reaches a point, a cone of ways marched
// ge:summary through the fall's shelter map
// ge:keywords shelter fall_share precip rain snow roof indoor window cone march occlusion wgsl
// ge:keywords library weather
// ge:skill render
// ge:test tests/render_precip_test.cpp

// The steps of a way's march, metres back up it from the point: close together near the point
// (a sill, the lee of a low wall) and further apart higher up (a roof, a window across a hall).
const kFallSteps = array<f32, 7>(0.12, 0.35, 0.8, 1.6, 3.0, 5.5, 10.0);

// The map's camera: `o` (world m) with axes `x`, `y` and `z` (z along the fall, toward the
// ground), `half` m across each side, `depth` m deep each way; `layer` of `maps`.
struct FallMap {
  o: vec3f,
  x: vec3f,
  y: vec3f,
  z: vec3f,
  half: f32,
  depth: f32,
  layer: i32,
};

// Whether the world point `q` sees the sky along the fall (1) or not (0), 2 x 2 compared: off the
// map's edge it does. `bias_m` (m along the fall) keeps a surface from covering itself.
fn fall_open_at(maps: texture_depth_2d_array, cmp: sampler_comparison, m: FallMap, q: vec3f,
                bias_m: f32) -> f32 {
  let r = q - m.o;
  let uv = vec2f(0.5 + 0.5 * dot(r, m.x) / m.half, 0.5 - 0.5 * dot(r, m.y) / m.half);
  if (any(uv < vec2f(0.0)) || any(uv > vec2f(1.0))) {
    return 1.0;
  }
  let z = (dot(r, m.z) - bias_m + m.depth) / (2.0 * m.depth);
  return textureSampleCompareLevel(maps, cmp, uv, m.layer, z);
}

// The share of the fall that reaches `p` (0 .. 1) over `rays` ways (1 .. 16) on the cone of
// tangent `spread`, the spiral turned by `turn` (radians: a hash of the place scatters the edge).
fn fall_share(maps: texture_depth_2d_array, cmp: sampler_comparison, m: FallMap, p: vec3f,
              spread: f32, rays: i32, turn: f32) -> f32 {
  // The bias keeps a surface from covering itself: a floor seen along a slanted fall changes
  // depth across a texel by the slant's run over its drop, and the comparison spans two texels.
  let texel = 2.0 * m.half / f32(textureDimensions(maps).x);
  let run = sqrt(max(1.0 - m.z.y * m.z.y, 0.0)) / max(abs(m.z.y), 0.05);
  let bias_m = 0.03 + 1.5 * texel * (1.0 + run);
  // Covered right where it stands: under a roof every way is shut.
  if (fall_open_at(maps, cmp, m, p, bias_m) < 0.5) {
    return 0.0;
  }
  let c = cos(turn);
  let s = sin(turn);
  var open = 0.0;
  for (var k = 0; k < rays; k++) {
    // A golden-angle spiral over the cone's disc; the first way the fall's own.
    let r = select(sqrt(f32(k) / f32(rays)), 0.0, k == 0);
    let a = f32(k) * 2.3999632;
    let d = vec2f(cos(a), sin(a)) * r * spread;
    let t = vec2f(d.x * c - d.y * s, d.x * s + d.y * c);
    let up = -(m.z + m.x * t.x + m.y * t.y);
    var seen = 1.0;
    for (var j = 0; j < 7; j++) {
      seen = min(seen, fall_open_at(maps, cmp, m, p + up * kFallSteps[j], bias_m));
      if (seen <= 0.0) {
        break;
      }
    }
    open += seen;
  }
  return open / f32(max(rays, 1));
}
