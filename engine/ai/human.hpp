// HOW HUMAN A BOT LOOKS: metrics off what was pressed, step by step, and the bands a person's
// hands fall in. The same metrics are taken of a bot's run and of a person's (an input log is
// an input log), so "more human" is a number that moves, not a feeling.
//
// PER AXIS (`axis_humanness`, off one axis's values at every step, and optionally what was
// WANTED there: a pilot's intent before its hands, hands.hpp's `record_intent`):
// - `saturated`: the share of the steps the axis is in use (off its rest by more than 0.05) that
//   it is held at the end of its travel (within 0.02). A bot steers full lock and brakes full on.
// - `jumps`: the share of steps the axis moves more than half its travel in one step. A hand
//   cannot; a bot's bang-bang does it every switch.
// - `reversals_min`: changes of direction a minute (a move of 0.05 the other way counts). A bot's
//   dither is a high rate; a person corrects a few times a second at most.
// - `rate`: the mean speed of the axis, travel a second.
// - `hold_s`: how long the axis holds still pressed part-way (within 0.02 of where the hold
//   began, off its rest and its ends), as the hold a step picked at random is in; NaN when it is
//   never pressed part-way. A bot that corrects every step holds for a step; a perfectly still
//   one forever.
// - `lag_s`: with `wanted`, the delay at which what was pressed best follows what was wanted
//   (the least mean absolute difference over delays up to `kMostLag_s`). A bot's is 0; a wanted
//   that never moves has none (NaN).
//
// PER DECISION (`decision_humanness`, off a trace, why.hpp): `flips_min`, changes of mind a
// minute; `close_share`, the share of calls closer than a margin; `variety`, how evenly the
// options were taken (the choices' entropy over its most, 0..1).
//
// BANDS (`kHumanBands`): what a person on a gamepad's stick and triggers does, each with the
// assumption it stands on and the kind of control it holds for (`Feel`: a stick, a trigger, or
// both). They are stated, not yet measured: a reference corpus of logs played
// by people replaces them (docs/goals.md M16b). `out_of_band` lists every metric outside its band.
//
// ge:summary Humanness metrics off what was pressed (saturation, jumps, reversals, lag) and bands
// ge:keywords ai bot humanness human-like metrics saturation jumps reversals dither lag reaction
// ge:keywords hold bands out-of-band flips variety entropy close-call corpus robotic
// ge:skill bots
// ge:test tests/ai_human_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ai/why.hpp"

namespace ge::ai {

inline constexpr double kMostLag_s = 0.6;

struct AxisHumanness {
  double saturated = 0;      // share of steps in use, 0..1
  double jumps = 0;          // share of steps, 0..1
  double reversals_min = 0;  // a minute
  double rate = 0;           // travel a second
  double hold_s = 0;         // median, s
  double lag_s = 0;          // NaN without what was wanted, or when it never moves
  int64_t steps = 0;
};

// One axis's values at every step (`lo`..`hi` its travel, `rest` where it sits untouched), at `hz`
// steps a second; `wanted` (the same length, or empty) what was meant at each step.
AxisHumanness axis_humanness(std::span<const double> pressed, std::span<const double> wanted,
                             double lo, double hi, double rest, double hz);

struct DecisionHumanness {
  double flips_min = 0;    // a minute
  double close_share = 0;  // 0..1, of the calls with a margin
  double variety = 0;      // 0..1
  int64_t calls = 0;
};

// One agent's decision `name` over a trace `seconds` long; a call is close under `close`.
DecisionHumanness decision_humanness(const Why& why, int32_t agent, const char* name,
                                     double seconds, double close);

// The kind of control an axis is pressed with: the bands differ (a trigger held flat out is a
// person tucking; a stick held at its rim is not how a person steers).
enum class Feel : uint8_t { stick, trigger, any };

// What a person does, and why we think so.
struct HumanBand {
  const char* metric;  // "saturated", "jumps", "reversals_min", "hold_s", "lag_s", "flips_min"
  Feel feel;           // the controls it holds for
  double lo, hi;
  const char* because;
};
extern const std::span<const HumanBand> kHumanBands;

// A metric's value against its band: `axis` names the axis or decision it was taken of.
struct OutOfBand {
  std::string axis;
  const char* metric;
  double value, lo, hi;
  const char* because;
};
void check_axis(const std::string& axis, Feel feel, const AxisHumanness& h,
                std::vector<OutOfBand>& out);
void check_decision(const std::string& name, const DecisionHumanness& h,
                    std::vector<OutOfBand>& out);
// "steer saturated 0.62 (band 0..0.35): <because>"
std::string out_of_band_text(const OutOfBand& o);

}  // namespace ge::ai
