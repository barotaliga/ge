// AN AGENT AT THE CONTROLS: one run of Fall Line driven by text, one JSON object a line each way,
// so an agent can playtest without a screen (the `play` lab puts it on stdio). The session is a
// `Driver` (simulate.hpp), so an agent's run is taped, hashed and reported exactly as the bot's
// is, and its tape played back (`replay_tape`, with the same rivals) is the same run.
//
// THE PROTOCOL, stated here and in docs/labs.md § An agent at the controls:
// - The session opens with a `hello` line (`hello()`): the seed, the field, the course, the
//   step's length and the ops.
// - `{"op":"observe"}` answers the OBSERVATION: the clock, where the skier is and how fast, the
//   gate owed (its bearing off his heading, + right, and its distance), where he stands on the
//   piste and how it bends ahead, his place, and the events since the last answer (his own, and
//   any shoulder he was in).
// - `{"op":"act","input":{"steer":s,"tuck":t,"brake":b,"jump":j,"reset":r},"steps":n}` holds the
//   input for n steps (1..`kMostSteps`, default 1) and answers the observation after them. Every
//   input key is optional (absent is 0 or false); `reset` is taken on the first step only. Once
//   every racer is home there is nothing to act on: the answer is an error.
// - `{"op":"tape"}` answers the tape so far: its steps and one base64 stream per axis.
// - `{"op":"end"}` answers the run's report and its digest, and ends the session.
// - Anything else answers `{"error":...,"next":...}`: what was wrong and what to send instead.
// Every answer is one line and carries the `op` it answers. Numbers are fixed-point text,
// three decimals; the digest is sixteen hex digits.
//
// ge:summary An agent at the controls: a run driven by JSON lines (observe, act, tape, end)
// ge:keywords play agent stdio protocol json observe act tape end playtest session hello
// ge:keywords observation play-session
// ge:skill sim agent-interface
// ge:test tests/fall_line_play_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "fall_line/sim/simulate.hpp"
#include "fall_line/sim/state.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

// The most steps one `act` may hold its input for (ten seconds).
inline constexpr int32_t kMostSteps = 1200;
// The most events one answer carries; older ones are dropped and counted.
inline constexpr size_t kMostEvents = 256;
// How far down the piste the observation's `piste_bend` looks, m.
inline constexpr double kBendAhead = 30;

class PlaySession {
 public:
  PlaySession(std::shared_ptr<const ge::mountain::mapgen::Level> level, double seed,
              int32_t rivals);
  // The session's first line.
  std::string hello() const;
  // One line in, one line out (neither with its newline).
  std::string handle(std::string_view line);
  bool ended() const { return ended_; }
  const Driver& driver() const { return driver_; }

 private:
  std::string observation(std::string_view op);
  void collect();

  Driver driver_;
  double seed_;
  std::vector<GameEvent> backlog_;
  int32_t dropped_ = 0;
  bool ended_ = false;
};

}  // namespace fall_line::sim
