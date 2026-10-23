// WHY A BOT DID IT: an input log replayed with the run's decision trace on (game.hpp's
// `Run::trace`, ai/why.hpp), so every decision a bot made is read back without having been
// kept. `ge why` and `play why` print it.
//
// - THE PILOT IS ASKED AT EVERY STEP, before the step, with the trace on: what the game's own
//   bot would do there, as agent 0. On a log the pilot played (`ge sim`) that is what it did; on
//   a log a person or an agent played it is what the bot would have done instead, and `agrees`
//   says at which steps the two match (each axis snapped to its grid). The log's inputs are
//   what is stepped either way, so the run is the log's and its digest the log's.
// - The game's other bots (a field's rivals) record inside the step, each as its own agent.
// - THE TIMELINE (`why_timeline`): one agent's decisions as a series file, one row a step it
//   decided: `step`, `t_s`, then for each decision in the order first met its choice (text),
//   `<name>.choice` (the chosen option's index, NaN with no options), `<name>.margin` and every
//   fact as `<name>.<fact>`. `ge graph` charts it.
// - THE FLIPS (`why_flips`): the steps where a decision's choice changed from the step before,
//   and with `close` > 0 the close calls, where the margin fell under it: where a bot dithers or
//   chose oddly is in that list.
//
// ge:summary ge why's engine half: a log replayed with the decision trace on, timeline and flips
// ge:keywords why ge-why replay trace decisions timeline flips close-call agrees pilot explain
// ge:keywords why_replay why_timeline why_flips decision-timeline
// ge:skill bots agent-interface
// ge:test tests/play_why_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ai/why.hpp"
#include "lab/series.hpp"
#include "play/game.hpp"
#include "play/inputs.hpp"

namespace ge::play {

struct WhyRun {
  ai::Why why;
  bool asked_pilot = false;  // the game has a pilot and it was asked every step
  // Per step, with the pilot asked: 1 when its controls were the log's.
  std::vector<uint8_t> agrees;
  int64_t steps = 0;
  uint64_t digest = 0;  // the run's at its end: the log's own
};

// `log` played back with `agent`'s decisions recorded (ai::Why::kEvery: every agent's). False,
// with `error`, when the game's bots record none.
std::optional<WhyRun> why_replay(const GameSpec& spec, const InputLog& log, int32_t agent,
                                 std::string& error);

// The decisions made at steps `from`..`to` (inclusive), of the decision `name` ("" every one).
std::vector<const ai::Decision*> decisions_between(const ai::Why& why, int64_t from, int64_t to,
                                                   std::string_view name = "");

// One agent's decisions as a series file (x: `t_s`, at `hz` steps a second).
lab::Series why_timeline(const ai::Why& why, int32_t agent, double hz);

struct Flip {
  int64_t step = 0;
  const char* decision = "";
  const char* from = "";  // the choice the step before ("" when it is the first); == `to` when
  const char* to = "";    // the row is a close call, not a change
  double margin = 0;
};

// One agent's changes of choice, step by step, and (with `close` > 0) the close calls under it.
std::vector<Flip> why_flips(const ai::Why& why, int32_t agent, double close);

}  // namespace ge::play
