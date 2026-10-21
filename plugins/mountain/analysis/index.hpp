// LEVEL ANALYSIS — the generator's scoreboard, and its gate: re-check a FINISHED map against
// the rule book, rule by rule, reading only what the `Level` publishes. Port of the single
// piste's half of game5 `engine/analysis/index.ts` (the resort's branches, R25–R28, and the
// terrain park's, R20, arrive with docs/goals.md M4b–M4c; the `stats` block arrives with the
// analyze CLI that prints it).
//
// Every message is the oracle's, to the character: `generate_level` writes the rejected
// attempts' findings into its debug line, and the golden table holds that line.
// Parity: tests/mountain_generate_test.cpp (`findings`, and the `log` of seeds 20 and 50).
//
// ge:summary Level analysis: analyzeLevel, the rule book re-checked on a finished map
// ge:keywords analysis analyzeLevel finding findings severity rule-check
// ge:oracle game5/engine/analysis/index.ts
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/js_text.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::analysis {

enum class Severity : uint8_t { error, warn };

struct Finding {
  std::string rule;  // "R1"…
  Severity severity;
  std::string message;
};

struct LevelAnalysis {
  bool ok = true;  // no errors (a warn is a smell, not a refusal)
  std::vector<Finding> findings;
};

// How a check files a finding (the oracle's `add`).
using Add = std::function<void(const char* rule, Severity severity, std::string message)>;

// The messages' numbers as the oracle writes them: `v.toFixed(digits)`, `${v}`, and a band as
// `${b.min}–${b.max}${unit}`.
inline std::string fmt(double v, int digits = 1) { return ge::core::js::to_fixed(v, digits); }
inline std::string num(double v) { return ge::core::js::to_string(v); }
inline std::string band_text(mapgen::Band b, const char* unit = "") {
  return num(b.min) + "\u2013" + num(b.max) + unit;
}

// Re-check a finished single-piste level against the rule book.
LevelAnalysis analyze_level(const mapgen::Level& level);

}  // namespace ge::mountain::analysis
