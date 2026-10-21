// engine/core/js.hpp and js_text.hpp against Node (tests/golden/core_js.txt, core_js_text.txt;
// tools/parity/golden-js.mjs).
//
// ge:summary Parity test: core/js and core/js_text against Node
// ge:keywords test parity golden js tofixed string
// ge:skill testing
#include "core/js.hpp"
#include "core/js_text.hpp"

#include <cmath>
#include <cstdint>
#include <string>

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

// engine/core/js_text.hpp against Node (tests/golden/core_js_text.txt): `String(x)` and
// `x.toFixed(d)` for d = 0, 1, 2, 3, 20.
TEST(js_text_matches_node) {
  namespace js = ge::core::js;
  const auto rows = ge::test::golden_words("tests/golden/core_js_text.txt");
  CHECK(rows.size() == 2048);
  constexpr int kDigits[] = {0, 1, 2, 3, 20};
  for (const auto& row : rows) {
    const double x = as_double(std::stoull(row[0], nullptr, 16));
    if (js::to_string(x) != row[1]) {
      ge::test::fail(
          __FILE__, __LINE__,
          "String(" + row[0] + ") is " + row[1] + ", js::to_string gave " + js::to_string(x));
    }
    for (size_t d = 0; d < 5; d++) {
      const std::string got = js::to_fixed(x, kDigits[d]);
      if (got != row[2 + d]) {
        ge::test::fail(__FILE__, __LINE__,
                       "(" + row[1] + ").toFixed(" + std::to_string(kDigits[d]) + ") is " +
                           row[2 + d] + ", js::to_fixed gave " + got);
      }
    }
  }
}
