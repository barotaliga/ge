// LEVEL ANALYSIS — the generator's scoreboard, and its gate: re-check a FINISHED map against
// the rule book, rule by rule, reading only what the `Level` publishes. The single piste's
// checks so far (the resort's, R25–R28, and the terrain park's, R20, arrive with
// docs/goals.md M4b–M4c; the `stats` block arrives with the analyze CLI that prints it).
//
// Every message's text is fixed, to the character: `generate_level` writes the rejected
// attempts' findings into its debug line, and the golden table holds that line.
// Bits: tests/mountain_generate_test.cpp (`findings`, and the `log` of seeds 20 and 50).
//
// ge:summary Level analysis: analyze_level, the rule book re-checked on a finished map
// ge:keywords analysis finding findings severity rule-check
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/num_text.hpp"
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

// How a check files a finding.
using Add = std::function<void(const char* rule, Severity severity, std::string message)>;

// The messages' numbers: fixed to `digits` decimals, the shortest round-trip, and a band as
// "<min>–<max><unit>".
inline std::string fmt(double v, int digits = 1) { return ge::core::num::to_fixed(v, digits); }
inline std::string str(double v) { return ge::core::num::to_string(v); }
inline std::string band_text(mapgen::Band b, const char* unit = "") {
  return str(b.min) + "\u2013" + str(b.max) + unit;
}

// Re-check a finished single-piste level against the rule book.
LevelAnalysis analyze_level(const mapgen::Level& level);

}  // namespace ge::mountain::analysis
