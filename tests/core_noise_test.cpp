// engine/core/noise.hpp against the TypeScript oracle (tests/golden/core_noise.txt, written by
// tools/parity/golden-core.mjs). Row: ix iz seed hash2 | x z scale vseed valueNoise.
//
// ge:summary Parity test: core/noise against the oracle
// ge:keywords test parity golden noise
// ge:skill testing
#include "core/noise.hpp"

#include "golden.hpp"

using ge::test::as_double;

TEST(hash2_matches_oracle_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/core_noise.txt");
  CHECK(rows.size() == 4000);
  for (const auto& row : rows) {
    CHECK_BITS(ge::core::hash2(as_double(row[0]), as_double(row[1]), as_double(row[2])),
               as_double(row[3]));
  }
}

TEST(value_noise_matches_oracle_bit_for_bit) {
  for (const auto& row : ge::test::golden_rows("tests/golden/core_noise.txt")) {
    CHECK_BITS(ge::core::value_noise(as_double(row[4]), as_double(row[5]), as_double(row[6]),
                                     as_double(row[7])),
               as_double(row[8]));
  }
}

// The rows come in runs of 40 sharing a scale and seed and walking in small steps: one
// NoiseField per run, read in order, must give valueNoise's bits whether its cache hits or not.
TEST(noise_field_is_value_noise_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/core_noise.txt");
  ge::core::NoiseField field = ge::core::noise_field(1.0, 0.0);
  for (size_t i = 0; i < rows.size(); i++) {
    const auto& row = rows[i];
    if (i % 40 == 0) field = ge::core::noise_field(as_double(row[6]), as_double(row[7]));
    CHECK_BITS(ge::core::sample_noise(field, as_double(row[4]), as_double(row[5])),
               as_double(row[8]));
  }
}
