// A REGULAR GRID OF HEIGHTS over the plan (x, z), read back bilinearly. Port of
// oss-game-framework `src/core/heightfield.ts`, bit for bit. A world compiler bakes one (the
// ground, a distance field, anything that is a function of the plan) once, and everything
// downstream (the physics' probes, collision, the terrain mesh, a lab's map) reads it here:
// a sample is two lerps instead of a stack of noise octaves at the physics rate.
//
// Samples outside the grid clamp to the nearest edge cell, so a probe past the world's edge
// never reads a hole. Storage is float (the oracle's Float32Array); every read widens to
// double exactly, as a JS typed-array read does.
// Parity: tests/core_heightfield_test.cpp against tests/golden/core_heightfield.txt.
//
// ge:summary A baked regular grid of heights over the plan, read back bilinearly
// ge:keywords heightfield grid bilinear terrain ground gradient slope sample bake createHeightfield
// ge:keywords fillField sampleField sampleFieldGradient fieldGradient fieldCell
// ge:oracle oss-game-framework/src/core/heightfield.ts
// ge:skill determinism
// ge:test tests/core_heightfield_test.cpp
#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace ge::core {

struct Heightfield {
  double origin_x;  // world x of column 0, m
  double origin_z;  // world z of row 0, m
  double cell;      // cell pitch, m, the same along both axes
  int cols;
  int rows;
  std::vector<float> data;  // row-major: index = row * cols + col
};

// A zeroed field of cols x rows samples from the origin.
inline Heightfield create_heightfield(double origin_x, double origin_z, double cell, int cols,
                                      int rows) {
  return Heightfield{
      origin_x, origin_z,
      cell,     cols,
      rows,     std::vector<float>(static_cast<size_t>(cols) * static_cast<size_t>(rows))};
}

// The four corners and the weights of the cell a point falls in, after the edge clamp. Both
// samplers below share it so their weights are the same bits. `ok` is false for a NaN point,
// where the oracle reads `data[NaN]` (undefined) and every result is NaN.
struct FieldCell {
  double a, b, c, e;  // (c0,r0) (c1,r0) (c0,r1) (c1,r1)
  double tx, tz;
  bool ok;
};

inline FieldCell field_cell(const Heightfield& f, double x, double z) {
  const double fx = (x - f.origin_x) / f.cell;
  const double fz = (z - f.origin_z) / f.cell;
  const double max_c = f.cols - 1;
  const double max_r = f.rows - 1;
  const double cx = fx <= 0 ? 0.0 : fx >= max_c ? max_c : fx;
  const double cz = fz <= 0 ? 0.0 : fz >= max_r ? max_r : fz;
  if (cx != cx || cz != cz) return FieldCell{0, 0, 0, 0, 0, 0, false};
  const double c0 = std::floor(cx);
  const double r0 = std::floor(cz);
  const size_t ic0 = static_cast<size_t>(c0);
  const size_t ir0 = static_cast<size_t>(r0);
  const size_t ic1 = c0 < max_c ? ic0 + 1 : ic0;
  const size_t ir1 = r0 < max_r ? ir0 + 1 : ir0;
  const size_t cols = static_cast<size_t>(f.cols);
  const float* d = f.data.data();
  return FieldCell{d[ir0 * cols + ic0],
                   d[ir0 * cols + ic1],
                   d[ir1 * cols + ic0],
                   d[ir1 * cols + ic1],
                   cx - c0,
                   cz - r0,
                   true};
}

// Bilinear sample at a world point, clamped to the grid's edge.
inline double sample_field(const Heightfield& f, double x, double z) {
  const FieldCell k = field_cell(f, x, z);
  if (!k.ok) return std::numeric_limits<double>::quiet_NaN();
  return (k.a + (k.b - k.a) * k.tx) * (1.0 - k.tz) + (k.c + (k.e - k.c) * k.tx) * k.tz;
}

// Bilinear sample AND its plan gradient off one set of weights: out = {value, d/dx, d/dz}.
// The gradient is the bilinear patch's own: constant across a cell, zero along a clamped axis.
inline void sample_field_gradient(const Heightfield& f, double x, double z, double out[3]) {
  const FieldCell k = field_cell(f, x, z);
  if (!k.ok) {
    out[0] = out[1] = out[2] = std::numeric_limits<double>::quiet_NaN();
    return;
  }
  const double inv = 1.0 / f.cell;
  out[0] = (k.a + (k.b - k.a) * k.tx) * (1.0 - k.tz) + (k.c + (k.e - k.c) * k.tx) * k.tz;
  out[1] = ((k.b - k.a) * (1.0 - k.tz) + (k.e - k.c) * k.tz) * inv;
  out[2] = ((k.c - k.a) * (1.0 - k.tx) + (k.e - k.b) * k.tx) * inv;
}

struct FieldSlope {
  double gx;  // dh/dx, m per m
  double gz;  // dh/dz, m per m
};

// Central-difference slope at a world point, half a cell either side.
inline FieldSlope field_gradient(const Heightfield& f, double x, double z) {
  const double h = f.cell * 0.5;
  return FieldSlope{(sample_field(f, x + h, z) - sample_field(f, x - h, z)) / (2.0 * h),
                    (sample_field(f, x, z + h) - sample_field(f, x, z - h)) / (2.0 * h)};
}

// Fill every sample from fn(x, z) of its world position (a build-time bake, not a step).
// The double -> float store rounds to nearest, as a Float32Array store does.
template <class Fn>
void fill_field(Heightfield& f, Fn&& fn) {
  for (int r = 0; r < f.rows; r++) {
    const double z = f.origin_z + r * f.cell;
    for (int c = 0; c < f.cols; c++) {
      f.data[static_cast<size_t>(r) * static_cast<size_t>(f.cols) + static_cast<size_t>(c)] =
          static_cast<float>(fn(f.origin_x + c * f.cell, z));
    }
  }
}

}  // namespace ge::core
