// The console's expressions: what `if (...) then`, `eval` and `set_eval` read (docs/console.md
// § Scripting). A small language over numbers and text, so a player's config can test and
// compute without a chain of set_calc lines:
//
//   numbers      7  3.5  -2          (an integer stays one through + - * and stays exact)
//   text         "two words"  'one'  a bare word that is no number and no operator
//   cvars        %name               the cvar's value ($name is expanded before, like any line)
//   arithmetic   + - * /             (+ joins when either side is text; / is always a fraction)
//   comparisons  = == != <> < <= > >=   (numbers when both sides are, else text)
//                isin !isin          a is a substring of b
//                =~ !~               b is a regular expression (ECMAScript) searched for in a
//   logic        and && or ||        (true: a number not 0, or text not empty; the result 1 or 0)
//   grouping     ( )  and a unary -
//
// Precedence, loosest first: or, and, the comparisons, + -, * /, unary -. A comparison or a
// logic operator gives the integer 1 or 0. A fault (an unclosed bracket, division by zero, a bad
// regular expression) returns false with the reason in `error`, worded for the player.
//
// ge:summary The console's expressions: numbers, text, cvars, arithmetic, comparisons, regex
// ge:keywords console expression expr eval set_eval if-expression arithmetic compare regex isin
// ge:keywords and or precedence integer double string truthy
// ge:skill console
// ge:test tests/console_expr_test.cpp
#pragma once

#include <string>
#include <string_view>

namespace ge::console {

class Cvars;

struct ExprValue {
  enum class Kind : unsigned char { integer, number, text };
  Kind kind = Kind::integer;
  double num = 0;    // integer and number
  std::string text;  // text
};

// `src` evaluated into `out`; false with `error` set when it cannot be. `cvars` answers `%name`
// (null: every %name is empty).
bool eval_expr(std::string_view src, const Cvars* cvars, ExprValue& out, std::string& error);

// True: a number other than 0, or text that is not empty.
bool expr_truthy(const ExprValue& v);

// The value as a cvar holds it ("3", "3.5", "abc").
std::string expr_text(const ExprValue& v);

// What `eval` prints: `3 (integer)`, `3.5 (number)`, `"abc" (text)`.
std::string expr_describe(const ExprValue& v);

}  // namespace ge::console
