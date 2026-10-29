// Planck's law against the colour matching functions' fit, sRGB, and the table.
//
// ge:summary Blackbody definitions: the spectrum integrated to XYZ, sRGB, and the table's rows
// ge:keywords render blackbody planck cie xyz srgb blackbody_table blackbody_cd
// ge:skill render
// ge:test tests/render_blackbody_test.cpp
#include "render/blackbody.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"

namespace ge::render {

namespace {

namespace lm = ge::core::libm;

// The CIE 1931 2-degree observer as sums of piecewise Gaussians, nm.
double lobe(double nm, double mu, double below, double above) {
  const double t = (nm - mu) / (nm < mu ? below : above);
  return lm::exp(-0.5 * t * t);
}
double cie_x(double nm) {
  return 1.056 * lobe(nm, 599.8, 37.9, 31.0) + 0.362 * lobe(nm, 442.0, 16.0, 26.7) -
         0.065 * lobe(nm, 501.1, 20.4, 26.2);
}
double cie_y(double nm) {
  return 0.821 * lobe(nm, 568.8, 46.9, 40.5) + 0.286 * lobe(nm, 530.9, 16.3, 31.1);
}
double cie_z(double nm) {
  return 1.217 * lobe(nm, 437.0, 11.8, 36.0) + 0.681 * lobe(nm, 459.0, 26.0, 13.8);
}

// Spectral radiance, W / (sr m^2 m), at wavelength `nm` and temperature `k`.
double planck(double nm, double k) {
  constexpr double kH = 6.62607015e-34, kC = 2.99792458e8, kB = 1.380649e-23;
  const double l = nm * 1e-9;
  return 2 * kH * kC * kC / (l * l * l * l * l) / lm::expm1(kH * kC / (l * kB * k));
}

struct Xyz {
  double x, y, z;
};

// The tristimulus values of a blackbody at `k`: W/(sr m^2) weighted by each function.
Xyz tristimulus(double k) {
  k = std::clamp(k, 200.0, 40000.0);
  Xyz s{0, 0, 0};
  for (int nm = 380; nm <= 780; ++nm) {
    const double b = planck(nm, k) * 1e-9;  // per nm
    s.x += b * cie_x(nm);
    s.y += b * cie_y(nm);
    s.z += b * cie_z(nm);
  }
  return s;
}

constexpr double kLuma[3] = {0.2126, 0.7152, 0.0722};

}  // namespace

Blackbody blackbody(double kelvin) {
  const Xyz c = tristimulus(kelvin);
  static const double y1000 = tristimulus(1000).y;
  double rgb[3] = {3.2406 * c.x - 1.5372 * c.y - 0.4986 * c.z,
                   -0.9689 * c.x + 1.8758 * c.y + 0.0415 * c.z,
                   0.0557 * c.x - 0.2040 * c.y + 1.0570 * c.z};
  double l = 0;
  for (int i = 0; i < 3; ++i) {
    rgb[i] = std::max(rgb[i], 0.0);
    l += kLuma[i] * rgb[i];
  }
  Blackbody out{};
  for (int i = 0; i < 3; ++i) out.rgb[i] = static_cast<float>(l > 0 ? rgb[i] / l : 0);
  out.luminance = c.y / y1000;
  return out;
}

double blackbody_cd(double kelvin) { return 683.0 * tristimulus(kelvin).y; }

void blackbody_table(float out[kBlackbodyRows][4]) {
  const double top = tristimulus(kBlackbodyHiK).y;
  for (uint32_t i = 0; i < kBlackbodyRows; ++i) {
    const double k = kBlackbodyLoK + (kBlackbodyHiK - kBlackbodyLoK) * i / (kBlackbodyRows - 1);
    const Blackbody b = blackbody(k);
    for (int c = 0; c < 3; ++c) out[i][c] = b.rgb[c];
    out[i][3] = static_cast<float>(tristimulus(k).y / top);
  }
}

}  // namespace ge::render
