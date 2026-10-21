// Colour encodings shared by every pass. Passes work in linear light; the one place that
// encodes for an 8-bit display target is the pass that writes it (the post pass from M7).

// Linear light to sRGB-encoded, per channel (IEC 61966-2-1's piecewise curve).
fn linear_to_srgb(c: vec3f) -> vec3f {
  let lo = c * 12.92;
  let hi = 1.055 * pow(c, vec3f(1.0 / 2.4)) - 0.055;
  return select(hi, lo, c <= vec3f(0.0031308));
}
