// engine/core/heightfield.hpp against tests/golden/core_heightfield.txt. Per field: a header row
//   origin_x origin_z cell cols rows n_samples
// the data row as fill_field stored it, then n_samples rows of
//   x z sample_field  sample_field_gradient[0..2]  field_gradient.gx .gz
//
// ge:summary Golden test: core/heightfield, bit for bit
// ge:keywords test golden heightfield
// ge:skill testing
#include "core/heightfield.hpp"

#include <cstdint>
#include <limits>

#include "core/noise.hpp"
#include "golden.hpp"

using ge::test::as_double;

namespace {

// The fill function the golden's fields were filled with (its generator states the same one).
double field_fn(double x, double z) {
  return ge::core::value_noise(x, z, 37.0, 5.0) * 100.0 + x * 0.013 - z * 0.007 +
         1e-3 / (1.0 + x * x);
}

}  // namespace

TEST(heightfield_matches_golden_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/core_heightfield.txt");
  size_t at = 0;
  int fields = 0;
  while (at < rows.size()) {
    const auto& head = rows[at++];
    const int cols = static_cast<int>(as_double(head[3]));
    const int nrows = static_cast<int>(as_double(head[4]));
    const size_t samples = static_cast<size_t>(as_double(head[5]));
    ge::core::Heightfield f = ge::core::create_heightfield(as_double(head[0]), as_double(head[1]),
                                                           as_double(head[2]), cols, nrows);
    ge::core::fill_field(f, field_fn);
    const auto& data = rows[at++];
    CHECK(data.size() == f.data.size());
    for (size_t i = 0; i < data.size() && i < f.data.size(); i++) {
      CHECK_BITS(static_cast<double>(f.data[i]), as_double(data[i]));
    }
    for (size_t s = 0; s < samples; s++) {
      const auto& row = rows[at++];
      const double x = as_double(row[0]), z = as_double(row[1]);
      CHECK_BITS_ANY_NAN(ge::core::sample_field(f, x, z), as_double(row[2]));
      double g[3];
      ge::core::sample_field_gradient(f, x, z, g);
      CHECK_BITS_ANY_NAN(g[0], as_double(row[3]));
      CHECK_BITS_ANY_NAN(g[1], as_double(row[4]));
      CHECK_BITS_ANY_NAN(g[2], as_double(row[5]));
      const ge::core::FieldSlope slope = ge::core::field_gradient(f, x, z);
      CHECK_BITS_ANY_NAN(slope.gx, as_double(row[6]));
      CHECK_BITS_ANY_NAN(slope.gz, as_double(row[7]));
    }
    fields++;
  }
  CHECK(fields == 5);
}

// A power-of-two pitch multiplies by its exact inverse; any other divides. Either way a sample
// is the division's bits.
TEST(pitch_inverse_is_exact_or_absent) {
  CHECK(ge::core::exact_inverse(2) == 0.5);
  CHECK(ge::core::exact_inverse(0.25) == 4);
  CHECK(ge::core::exact_inverse(1) == 1);
  CHECK(ge::core::exact_inverse(10) == 0);
  CHECK(ge::core::exact_inverse(3) == 0);
  CHECK(ge::core::exact_inverse(0) == 0);
  CHECK(ge::core::exact_inverse(-2) == 0);
  CHECK(ge::core::exact_inverse(std::numeric_limits<double>::infinity()) == 0);
  uint64_t h = 0x853c49e6748fea9bull;
  for (int i = 0; i < 4000; i++) {
    h = h * 6364136223846793005ull + 1442695040888963407ull;
    const double v = (double(h >> 11) / 9007199254740992.0 - 0.3) * 5000;
    for (const double cell : {0.5, 2.0, 4.0, 16.0}) {
      CHECK_BITS(v * ge::core::exact_inverse(cell), v / cell);
    }
  }
}
