// JS NUMBERS AS TEXT: `Number.prototype.toFixed` and `String(n)`, for the diagnostic lines a
// port writes in the oracle's words (a generator's refusal, an analysis finding). Nothing that
// feeds a result is text, so these are never on a hot path; they exist so a port's log line
// reads exactly as game5's does, and a golden of those lines can hold it.
// Parity: tests/core_js_test.cpp against tests/golden/core_js_text.txt
// (tools/parity/golden-js.mjs).
//
// ge:summary JS numbers as text: Number.prototype.toFixed and String(n)
// ge:keywords js toFixed toString number-format text
// ge:skill determinism agent-interface
// ge:test tests/core_js_test.cpp
#pragma once

#include <string>

namespace ge::core::js {

// `x.toFixed(digits)` (digits 0…100): the exact decimal value rounded half away from zero,
// "-" kept on a negative (but not on -0), `String(x)` at or past 1e21.
std::string to_fixed(double x, int digits);

// `String(x)`: the shortest digits that read back as x, in fixed notation on [1e-6, 1e21) and
// JS's exponent form ("1e+21", "1.5e-7") outside it; "NaN", "Infinity", "-Infinity", "0".
std::string to_string(double x);

}  // namespace ge::core::js
