// engine/core/math.hpp against the TypeScript oracle (tests/golden/core_math.txt).
#include "core/math.hpp"

#include <limits>

#include "golden.hpp"

using ge::test::as_double;

TEST(hypot_family_matches_oracle_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/core_math.txt");
  CHECK(rows.size() == 4000);
  for (const auto& row : rows) {
    const double a = as_double(row[0]), b = as_double(row[1]);
    const double c = as_double(row[2]), d = as_double(row[3]);
    CHECK_BITS(ge::core::hypot(a, b), as_double(row[4]));
    CHECK_BITS(ge::core::hypot3(a, b, c), as_double(row[5]));
    CHECK_BITS(ge::core::hypot4(a, b, c, d), as_double(row[6]));
    CHECK_BITS(ge::core::smoothstep(-std::fabs(c), std::fabs(d), a), as_double(row[7]));
  }
}

TEST(hypot_special_values) {
  constexpr double inf = std::numeric_limits<double>::infinity();
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(ge::core::hypot(inf, nan) == inf);  // JS: Infinity wins over NaN
  CHECK(ge::core::hypot(0.0, -0.0) == 0.0);
  CHECK(ge::core::hypot(3.0, nan) != ge::core::hypot(3.0, nan));
}
