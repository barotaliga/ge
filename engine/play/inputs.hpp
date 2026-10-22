// THE INPUT LOG: a run written down as text, so it can be replayed, compared and edited by hand.
// A run is a pure function of its game, its setup and its inputs, so this file IS the run:
// `ge replay` plays it back to the digest it records, and `ge diverge` steps two of them side by
// side. It is plain text on purpose: an agent reads it, changes one line and asks what changed.
//
//   ge-inputs 1
//   game fall_line
//   seed 38
//   option rivals 3
//   hz 120
//   axes steer tuck brake jump reset
//   steps 5400
//   digest 8c1f0e5a2b7d9634
//   120 0 1 0 0 0
//   36 -0.5039370078740157 1 0 0 0
//
// After the header, each line is a RUN of steps: how many, then one value per axis, held for
// all of them. The values are written exactly (shortest round-trip text), and snapped onto their
// axis's grid again when read, so a hand-typed 0.5 is fine. `digest` is the run's after `steps`
// steps (absent while a log is being recorded); `#` starts a comment.
//
// ge:summary The input log: a run as editable text (header, then runs of held inputs); its parser
// ge:keywords inputs input-log replay tape record parse rle run-length text format digest
// ge:keywords inputlog inputrecorder parse_inputs
// ge:skill agent-interface
// ge:test tests/play_inputs_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "play/game.hpp"

namespace ge::play {

inline constexpr int kInputsVersion = 1;

struct InputRun {
  int64_t count = 0;
  std::vector<double> values;  // one per axis
};

struct InputLog {
  std::string game;
  double seed = 0;
  std::vector<std::pair<std::string, std::string>> options;  // in the order written
  double hz = 0;
  std::vector<std::string> axes;
  std::optional<uint64_t> digest;  // after every step
  std::vector<InputRun> runs;

  int64_t steps() const;
  // The values held at step `i` (0-based); empty past the end.
  std::span<const double> at(int64_t i) const;
};

// Appends each step's inputs to a log, merging a step equal to the last into its run.
class InputRecorder {
 public:
  explicit InputRecorder(InputLog log) : log_(std::move(log)) {}
  void record(std::span<const double> values);
  const InputLog& log() const { return log_; }

 private:
  InputLog log_;
};

// A fresh log for `spec` and `setup`, with no steps.
InputLog new_log(const GameSpec& spec, const Setup& setup);

// The log as text (the format above), ending with a newline.
std::string to_text(const InputLog& log);

// The text read back; on failure, `error` says what and on which line.
std::optional<InputLog> parse_inputs(std::string_view text, std::string& error);

// Whether `log` was written for `spec`: the same game and axes; `error` says what differs.
bool fits(const InputLog& log, const GameSpec& spec, std::string& error);

// Sixteen lower-case hex digits.
std::string hex64(uint64_t v);
std::optional<uint64_t> parse_hex64(std::string_view s);

}  // namespace ge::play
