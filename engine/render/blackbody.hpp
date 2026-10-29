// THE COLOUR OF HOT THINGS (docs/rendering.md § Explosions): the light a body gives off at a
// temperature, for anything that glows by its heat (a fireball, a flame, a spark, an ember).
//
// - THE SPECTRUM is Planck's law, integrated from 380 to 780 nm in 1 nm steps against the CIE
//   1931 colour matching functions (their multi-lobe Gaussian fit), turned to linear sRGB (Rec.
//   709 primaries, D65 white) and scaled to luminance 1. Below about 1900 K the blue falls under
//   zero; it is clamped there and the colour scaled back to luminance 1.
// - THE BRIGHTNESS: `luminance` is the body's luminance relative to one at 1000 K, and
//   `blackbody_cd` the absolute luminance of a perfect radiator, cd/m^2 (683 lm/W times the
//   photopic integral), so a renderer can put a glow in the same units as the sun.
// - THE TABLE (`blackbody_table`) is what a shader reads: kBlackbodyRows rows evenly from
//   kBlackbodyLoK to kBlackbodyHiK, each the linear rgb at luminance 1 in xyz and in w the
//   luminance relative to the hottest row. wgsl/blackbody.wgsl lerps two rows a caller fetched.
//
// Pure: the same temperature gives the same bits (ge::core::libm).
//
// ge:summary Blackbody colour and luminance by temperature, and the table shaders lerp
// ge:keywords render blackbody planck temperature kelvin colour-temperature glow fire flame
// ge:keywords ember spark cie colour-matching luminance blackbody_table blackbody_cd
// ge:skill render
// ge:test tests/render_blackbody_test.cpp
#pragma once

#include <cstdint>

namespace ge::render {

struct Blackbody {
  float rgb[3];      // linear sRGB, its luminance 1
  double luminance;  // relative to a blackbody at 1000 K
};

// The colour and relative luminance of a blackbody at `kelvin` (clamped to 200..40000 K).
Blackbody blackbody(double kelvin);

// A perfect radiator's luminance at `kelvin`, cd/m^2.
double blackbody_cd(double kelvin);

inline constexpr uint32_t kBlackbodyRows = 64;
inline constexpr double kBlackbodyLoK = 700;   // row 0
inline constexpr double kBlackbodyHiK = 3200;  // the last row

// The shaders' table: row i is the blackbody at kBlackbodyLoK + i * (kBlackbodyHiK -
// kBlackbodyLoK) / (kBlackbodyRows - 1): rgb at luminance 1, w its luminance over the last row's.
void blackbody_table(float out[kBlackbodyRows][4]);

}  // namespace ge::render
