// engine/core/libm against the oracle's runtime: tests/golden/libm_<fn>.txt, written by
// tools/parity/golden-libm.mjs under game5's pinned Node (make golden-libm). Bits, not closeness.
//
// ge:summary Parity test: core/libm against Node 24
// ge:keywords test parity golden libm
// ge:skill testing
#include "core/libm/libm.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#include "golden.hpp"

namespace {

using ge::test::as_double;
using ge::test::bits;

// JS has one NaN and V8 does not keep payloads, so any NaN matches any NaN.
bool js_same(double got, double want) {
  return (got != got && want != want) || bits(got) == bits(want);
}

void mismatch(const char* fn, const std::string& args, double got, double want) {
  char m[320];
  std::snprintf(m, sizeof m,
                "libm::%s(%s) = %016llx, Node 24 gives %016llx. The vendored routine has "
                "drifted from V8/glibc: diff engine/core/libm against its upstream "
                "(engine/core/libm/libm.hpp says which) and check the target links ge_fp.",
                fn, args.c_str(), static_cast<unsigned long long>(bits(got)),
                static_cast<unsigned long long>(bits(want)));
  ge::test::fail(__FILE__, __LINE__, m);
}

std::string hex(double v) {
  char b[24];
  std::snprintf(b, sizeof b, "0x%016llx", static_cast<unsigned long long>(bits(v)));
  return b;
}

constexpr size_t kRows = 2048;

void unary(const char* fn, double (*f)(double)) {
  const auto rows =
      ge::test::golden_rows(("tests/golden/libm_" + std::string(fn) + ".txt").c_str());
  CHECK(rows.size() == kRows);
  for (const auto& row : rows) {
    const double x = as_double(row[0]);
    const double got = f(x);
    if (!js_same(got, as_double(row[1]))) mismatch(fn, hex(x), got, as_double(row[1]));
  }
}

void binary(const char* fn, double (*f)(double, double)) {
  const auto rows =
      ge::test::golden_rows(("tests/golden/libm_" + std::string(fn) + ".txt").c_str());
  CHECK(rows.size() == kRows);
  for (const auto& row : rows) {
    const double a = as_double(row[0]);
    const double b = as_double(row[1]);
    const double got = f(a, b);
    if (!js_same(got, as_double(row[2])))
      mismatch(fn, hex(a) + ", " + hex(b), got, as_double(row[2]));
  }
}

}  // namespace

namespace libm = ge::core::libm;

TEST(sin_matches_node24) { unary("sin", libm::sin); }
TEST(cos_matches_node24) { unary("cos", libm::cos); }
TEST(tan_matches_node24) { unary("tan", libm::tan); }
TEST(asin_matches_node24) { unary("asin", libm::asin); }
TEST(acos_matches_node24) { unary("acos", libm::acos); }
TEST(atan_matches_node24) { unary("atan", libm::atan); }
TEST(atan2_matches_node24) { binary("atan2", libm::atan2); }
TEST(exp_matches_node24) { unary("exp", libm::exp); }
TEST(expm1_matches_node24) { unary("expm1", libm::expm1); }
TEST(log_matches_node24) { unary("log", libm::log); }
TEST(log2_matches_node24) { unary("log2", libm::log2); }
TEST(cbrt_matches_node24) { unary("cbrt", libm::cbrt); }
TEST(tanh_matches_node24) { unary("tanh", libm::tanh); }
TEST(pow_matches_node24) { binary("pow", libm::pow); }

// JS-only answers of Math.pow that IEEE 754 pow does not give.
TEST(pow_js_special_cases) {
  constexpr double inf = std::numeric_limits<double>::infinity();
  constexpr double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(std::isnan(libm::pow(1.0, inf)));    // IEEE says 1
  CHECK(std::isnan(libm::pow(-1.0, -inf)));  // IEEE says 1
  CHECK(std::isnan(libm::pow(1.0, nan)));    // IEEE says 1
  CHECK(libm::pow(nan, 0.0) == 1.0);
  CHECK(bits(libm::pow(-0.0, 0.5)) == bits(0.0));  // the sqrt shortcut's +0
  CHECK(libm::pow(-inf, 0.5) == inf);
}
