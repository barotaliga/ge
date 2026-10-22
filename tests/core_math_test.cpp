// engine/core/math.hpp against tests/golden/core_math.txt.
//
// ge:summary Golden test: core/math, bit for bit
// ge:keywords test golden math hypot
// ge:skill testing
#include "core/math.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

#include "golden.hpp"

using ge::test::as_double;

TEST(hypot_family_matches_golden_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/core_math.txt");
  CHECK(rows.size() == 4000);
  for (const auto& row : rows) {
    const double a = as_double(row[0]), b = as_double(row[1]);
    const double c = as_double(row[2]), d = as_double(row[3]);
    CHECK_BITS(ge::core::hypot(a, b), as_double(row[4]));
    CHECK_BITS(ge::core::hypot3(a, b, c), as_double(row[5]));
    CHECK_BITS(ge::core::hypot4(a, b, c, d), as_double(row[6]));
    CHECK_BITS(ge::core::smoothstep(-std::fabs(c), std::fabs(d), a), as_double(row[7]));
    CHECK_BITS(ge::core::lerp(a, b, c), as_double(row[8]));
    CHECK_BITS(ge::core::approach(a, b, std::fabs(c)), as_double(row[9]));
  }
}

// angle_diff(a, b) rows: a b want (tests/golden/core_math_angle.txt).
TEST(angle_diff_matches_golden_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/core_math_angle.txt");
  CHECK(rows.size() == 2100);
  for (const auto& row : rows) {
    CHECK_BITS_ANY_NAN(ge::core::angle_diff(as_double(row[0]), as_double(row[1])),
                       as_double(row[2]));
  }
}

// One division, not two: the scale-by-max recipe's bits, written out the long way.
TEST(hypot_divides_once_for_the_same_bits) {
  const auto long_way = [](double a, double b) {
    const double x = std::fabs(a);
    const double y = std::fabs(b);
    const double m = x > y ? x : y;
    if (m == 0.0) return 0.0;
    const double p = x / m;
    const double q = y / m;
    return std::sqrt(p * p + q * q) * m;
  };
  uint64_t h = 0x2545f4914f6cdd1dull;
  for (int i = 0; i < 20000; i++) {
    h = h * 6364136223846793005ull + 1442695040888963407ull;
    const double a = (double(h >> 11) / 9007199254740992.0 - 0.5) * std::ldexp(1.0, i % 40 - 10);
    h = h * 6364136223846793005ull + 1442695040888963407ull;
    const double b = (double(h >> 11) / 9007199254740992.0 - 0.5) * std::ldexp(1.0, i % 37 - 10);
    CHECK_BITS(ge::core::hypot(a, b), long_way(a, b));
    CHECK_BITS(ge::core::hypot(a, a), long_way(a, a));
    CHECK_BITS(ge::core::hypot(0.0, b), long_way(0.0, b));
  }
}

TEST(hypot_special_values) {
  constexpr double inf = std::numeric_limits<double>::infinity();
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(ge::core::hypot(inf, nan) == inf);  // an infinity wins over NaN
  CHECK(ge::core::hypot(0.0, -0.0) == 0.0);
  CHECK(ge::core::hypot(3.0, nan) != ge::core::hypot(3.0, nan));
}
