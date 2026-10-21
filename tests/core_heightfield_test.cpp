// engine/core/heightfield.hpp against the TypeScript oracle (tests/golden/core_heightfield.txt,
// written by tools/parity/golden-core.mjs). Per field: a header row
//   originX originZ cell cols rows nSamples
// the data row as the oracle's fillField stored it, then nSamples rows of
//   x z sampleField  sampleFieldGradient[0..2]  fieldGradient.gx .gz
//
// ge:summary Parity test: core/heightfield against the oracle
// ge:keywords test parity golden heightfield
// ge:skill testing
#include "core/heightfield.hpp"

#include "core/noise.hpp"
#include "golden.hpp"

using ge::test::as_double;

namespace {

// The oracle's fill function, restated (tools/parity/golden-core.mjs `fieldFn`).
double field_fn(double x, double z) {
  return ge::core::value_noise(x, z, 37.0, 5.0) * 100.0 + x * 0.013 - z * 0.007 +
         1e-3 / (1.0 + x * x);
}

}  // namespace

TEST(heightfield_matches_oracle_bit_for_bit) {
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
      CHECK_JS_BITS(ge::core::sample_field(f, x, z), as_double(row[2]));
      double g[3];
      ge::core::sample_field_gradient(f, x, z, g);
      CHECK_JS_BITS(g[0], as_double(row[3]));
      CHECK_JS_BITS(g[1], as_double(row[4]));
      CHECK_JS_BITS(g[2], as_double(row[5]));
      const ge::core::FieldSlope slope = ge::core::field_gradient(f, x, z);
      CHECK_JS_BITS(slope.gx, as_double(row[6]));
      CHECK_JS_BITS(slope.gz, as_double(row[7]));
    }
    fields++;
  }
  CHECK(fields == 5);
}
