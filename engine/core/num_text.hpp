// NUMBERS AS TEXT: fixed-point and shortest round-trip formatting, for diagnostic lines (a
// generator's refusal, an analysis finding). Nothing that feeds a result is text, so these are
// never on a hot path; they exist so a log line is the same text on every platform, and a
// golden of those lines can hold it.
// Bits: tests/core_num_test.cpp against tests/golden/core_num_text.txt.
//
// ge:summary Numbers as text: fixed-point (to_fixed) and shortest round-trip (to_string)
// ge:keywords num to-fixed to-string number-format text
// ge:skill determinism agent-interface
// ge:test tests/core_num_test.cpp
#pragma once

#include <string>

namespace ge::core::num {

// x with `digits` decimals (0…100): the exact decimal value rounded half away from zero,
// "-" kept on a negative (but not on -0), to_string(x) at or past 1e21.
std::string to_fixed(double x, int digits);

// The shortest digits that read back as x, in fixed notation on [1e-6, 1e21) and the
// exponent form "1e+21", "1.5e-7" outside it; "NaN", "Infinity", "-Infinity", "0".
std::string to_string(double x);

}  // namespace ge::core::num
