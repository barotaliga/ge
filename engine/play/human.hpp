// HOW HUMAN A RUN LOOKS, off its input log: ai/human.hpp's metrics taken of every axis the log
// holds and every decision its pilot made, flagged against a person's bands. `ge sim --human` and
// `play sim --human` print it beside each run; it reads a person's log the same way as a bot's.
//
// - THE AXES: each signed axis is taken as a stick and each lever as a trigger (ai::Feel); a
//   toggle or a press is a button and is left out. What was pressed is the log's.
// - WHAT WAS MEANT: the pilot's intent, off the trace's "hands" decisions (ai/hands.hpp's
//   `record_intent`, one fact an axis by the axis's name), when the game's pilot records one at
//   every step; the lag is measured against it. On a log the pilot played that is the bot's own
//   intent before its hands; on a person's log it is what the bot would have meant there.
// - THE DECISIONS: every decision agent 0 made but "hands", by name: changes of mind a minute,
//   close calls (a margin under `kCloseCall`), variety.
//
// ge:summary The humanness of a run off its input log and trace: every axis and decision, flagged
// ge:keywords humanness human ge-sim-human report axes decisions out-of-band intent lag stick
// ge:keywords trigger log
// ge:skill bots agent-interface
// ge:test tests/play_human_test.cpp
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "ai/human.hpp"
#include "ai/why.hpp"
#include "play/game.hpp"
#include "play/inputs.hpp"

namespace ge::play {

// A call closer than this margin is a close one.
inline constexpr double kCloseCall = 0.25;

struct HumanReport {
  std::vector<std::pair<std::string, ai::AxisHumanness>> axes;
  std::vector<std::pair<std::string, ai::DecisionHumanness>> decisions;
  std::vector<ai::OutOfBand> out;
};

// `log` (played under `spec`) and agent 0's decisions in `why` (a replay of it, why.hpp).
HumanReport human_report(const GameSpec& spec, const InputLog& log, const ai::Why& why);
// {"axes":[{"name",...metrics}],"decisions":[{"name","flips_min","close_share","variety",
// "calls"}],"out_of_band":["<line>",...]}; a metric that is not a number is null.
std::string human_json(const HumanReport& r);

}  // namespace ge::play
