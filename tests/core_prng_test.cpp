// engine/core/prng.hpp against tests/golden/core_prng.txt.
//
// ge:summary Golden test: core/prng, bit for bit
// ge:keywords test golden prng rng
// ge:skill testing
#include "core/prng.hpp"

#include "golden.hpp"

using ge::test::as_double;

TEST(prng_matches_golden_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/core_prng.txt");
  CHECK(rows.size() == 8);
  for (const auto& row : rows) {
    CHECK(row.size() == 1 + 64 + 16 + 16);
    ge::core::Rng r = ge::core::make_rng(static_cast<uint32_t>(row[0]));
    size_t k = 1;
    for (int i = 0; i < 64; i++) CHECK_BITS(ge::core::next(r), as_double(row[k++]));
    for (int i = 0; i < 16; i++)
      CHECK_BITS(double(ge::core::int_in(r, -5, 17)), as_double(row[k++]));
    for (int i = 0; i < 16; i++) CHECK_BITS(ge::core::range(r, -3, 9), as_double(row[k++]));
  }
}

TEST(prng_stream_is_a_value) {
  ge::core::Rng a = ge::core::make_rng(38);
  ge::core::Rng b = a;
  CHECK_BITS(ge::core::next(a), ge::core::next(b));
}
