// THE COLOUR OF HOT THINGS on the GPU (render/blackbody.hpp's table): a caller keeps the table's
// kBlackbodyRows rows wherever it likes (a uniform array, a texture), finds the fractional row of
// a temperature with `blackbody_coord`, takes row u32(coord) and the next (at most the last) and
// lerps them with `blackbody_mix`. Nothing here is bound, so any pass may include it.

const kBlackbodyLoK = 700.0;
const kBlackbodyHiK = 3200.0;
const kBlackbodyRows = 64u;

fn blackbody_coord(kelvin: f32) -> f32 {
  let x = (kelvin - kBlackbodyLoK) / (kBlackbodyHiK - kBlackbodyLoK);
  return clamp(x, 0.0, 1.0) * f32(kBlackbodyRows - 1u);
}

// The colour between rows `lo` and `hi`, `f` of the way, at its luminance over `hottest` (a row's
// w), clamped to 0..1.
fn blackbody_mix(lo: vec4f, hi: vec4f, f: f32, hottest: f32) -> vec3f {
  let row = mix(lo, hi, f);
  return row.rgb * clamp(row.w / max(hottest, 1e-12), 0.0, 1.0);
}
