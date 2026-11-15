// The console's expressions (engine/console/expr.hpp): kinds, precedence, text, cvars and faults.
//
// ge:summary Tests the console's expressions: kinds, precedence, comparisons, regex, faults
// ge:keywords test console expression expr eval precedence regex isin integer text
// ge:skill console testing
#include <string>

#include "check.hpp"
#include "console/cvar.hpp"
#include "console/expr.hpp"

using namespace ge::console;

namespace {

std::string describe(const std::string& src, const Cvars* cvars = nullptr) {
  ExprValue v;
  std::string why;
  if (!eval_expr(src, cvars, v, why)) return "error: " + why;
  return expr_describe(v);
}

void expect(const std::string& src, const std::string& want, const Cvars* cvars = nullptr) {
  const std::string got = describe(src, cvars);
  CHECK_MSG(got == want,
            "'" + src + "' gave " + got + ", not " + want + ": read expr.cpp's parser");
}

}  // namespace

TEST(kinds_and_arithmetic) {
  expect("1+2", "3 (integer)");
  expect("7/2", "3.5 (number)");
  expect("8/2", "4 (number)");
  expect("2 + 3 * 4", "14 (integer)");
  expect("(2 + 3) * 4", "20 (integer)");
  expect("-(1.5 * 2)", "-3 (number)");
  expect("1 - -1", "2 (integer)");
}

TEST(text_and_joins) {
  expect("\"ab\" + 'cd'", "\"abcd\" (text)");
  expect("'x' + 1", "\"x1\" (text)");
  expect("hello", "\"hello\" (text)");
  expect("'5' + 1", "\"51\" (text)");
}

TEST(comparisons_and_logic) {
  expect("1+2 == 3 and \"a\" < \"b\"", "1 (integer)");
  expect("10 = 10.0", "1 (integer)");
  expect("2 <> 2 or 0", "0 (integer)");
  expect("ell isin hello", "1 (integer)");
  expect("ell !isin hello", "0 (integer)");
  expect("'dm6' =~ '^dm[0-9]$'", "1 (integer)");
  expect("'e1m1' !~ '^dm'", "1 (integer)");
  expect("1 && 0 || 1", "1 (integer)");
}

TEST(cvars_by_percent) {
  Cvars cvars(false);
  cvars.set("fov", "110", Source::local, true);
  expect("%fov - 10", "100 (integer)", &cvars);
  expect("%nothing + 'x'", "\"x\" (text)", &cvars);
}

TEST(faults_name_the_problem) {
  expect("1 / 0", "error: division by zero");
  expect("(1 + 2", "error: a '(' is not closed");
  expect("'open", "error: the text starting ' has no closing '");
  expect("a =~ '('", "error: '(' is not a regular expression");
  expect("1 2", "error: '2' after the end of the expression");
  expect("", "error: the expression is empty");
  expect("'a' * 2", "error: '*' needs numbers, not 'a'");
}
