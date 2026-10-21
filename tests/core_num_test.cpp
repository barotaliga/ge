// engine/core/num.hpp and num_text.hpp against tests/golden/core_num.txt and core_num_text.txt.
//
// ge:summary Golden test: core/num and core/num_text, bit for bit
// ge:keywords test golden num to-fixed to-string
// ge:skill testing
#include "core/num.hpp"
#include "core/num_text.hpp"

#include <cmath>
#include <cstdint>
#include <string>

#include "golden.hpp"

using ge::test::as_double;

TEST(num_semantics_match_golden) {
  namespace num = ge::core::num;
  const auto rows = ge::test::golden_rows("tests/golden/core_num.txt");
  CHECK(rows.size() == 4096);
  for (const auto& row : rows) {
    const double x = as_double(row[0]);
    const double y = as_double(row[1]);
    CHECK_BITS_ANY_NAN(num::round(x), as_double(row[2]));
    CHECK(static_cast<uint32_t>(num::to_int32(x)) == row[3]);
    CHECK(num::to_uint32(x) == row[4]);
    CHECK_BITS_ANY_NAN(num::sign(x), as_double(row[5]));
    CHECK_BITS_ANY_NAN(num::max(x, y), as_double(row[6]));
    CHECK_BITS_ANY_NAN(num::min(x, y), as_double(row[7]));
    CHECK_BITS_ANY_NAN(num::mod(x, y), as_double(row[8]));
    CHECK(static_cast<uint32_t>(num::imul(num::to_uint32(x), num::to_uint32(y))) == row[9]);
  }
}

TEST(js_max_min_fold_left_to_right) {
  namespace num = ge::core::num;
  CHECK(num::max(1.0, 3.0, 2.0) == 3.0);
  CHECK(num::min(1.0, -0.0, 0.0, 2.0) == 0.0);
  CHECK(std::signbit(num::min(1.0, -0.0, 0.0, 2.0)));
  CHECK(num::max(1.0, 2.0, std::nan("")) != num::max(1.0, 2.0, std::nan("")));
}

// engine/core/num_text.hpp against tests/golden/core_num_text.txt: to_string(x) and
// to_fixed(x, d) for d = 0, 1, 2, 3, 20.
TEST(num_text_matches_golden) {
  namespace num = ge::core::num;
  const auto rows = ge::test::golden_words("tests/golden/core_num_text.txt");
  CHECK(rows.size() == 2048);
  constexpr int kDigits[] = {0, 1, 2, 3, 20};
  for (const auto& row : rows) {
    const double x = as_double(std::stoull(row[0], nullptr, 16));
    if (num::to_string(x) != row[1]) {
      ge::test::fail(
          __FILE__, __LINE__,
          "to_string(" + row[0] + ") is " + row[1] + ", num::to_string gave " + num::to_string(x));
    }
    for (size_t d = 0; d < 5; d++) {
      const std::string got = num::to_fixed(x, kDigits[d]);
      if (got != row[2 + d]) {
        ge::test::fail(__FILE__, __LINE__,
                       "to_fixed(" + row[1] + ", " + std::to_string(kDigits[d]) + ") is " +
                           row[2 + d] + ", num::to_fixed gave " + got);
      }
    }
  }
}
