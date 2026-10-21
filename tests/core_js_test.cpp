// engine/core/js.hpp against Node (tests/golden/core_js.txt, tools/parity/golden-js.mjs).
#include "core/js.hpp"

#include <cmath>
#include <cstdint>

#include "golden.hpp"

using ge::test::as_double;

TEST(js_semantics_match_node) {
  namespace js = ge::core::js;
  const auto rows = ge::test::golden_rows("tests/golden/core_js.txt");
  CHECK(rows.size() == 4096);
  for (const auto& row : rows) {
    const double x = as_double(row[0]);
    const double y = as_double(row[1]);
    CHECK_JS_BITS(js::round(x), as_double(row[2]));
    CHECK(static_cast<uint32_t>(js::to_int32(x)) == row[3]);
    CHECK(js::to_uint32(x) == row[4]);
    CHECK_JS_BITS(js::sign(x), as_double(row[5]));
    CHECK_JS_BITS(js::max(x, y), as_double(row[6]));
    CHECK_JS_BITS(js::min(x, y), as_double(row[7]));
    CHECK_JS_BITS(js::mod(x, y), as_double(row[8]));
    CHECK(static_cast<uint32_t>(js::imul(js::to_uint32(x), js::to_uint32(y))) == row[9]);
  }
}

TEST(js_max_min_fold_left_to_right) {
  namespace js = ge::core::js;
  CHECK(js::max(1.0, 3.0, 2.0) == 3.0);
  CHECK(js::min(1.0, -0.0, 0.0, 2.0) == 0.0);
  CHECK(std::signbit(js::min(1.0, -0.0, 0.0, 2.0)));
  CHECK(js::max(1.0, 2.0, std::nan("")) != js::max(1.0, 2.0, std::nan("")));
}
