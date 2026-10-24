// THE REPLAY FILE: a run kept to be watched again (docs/replays.md § The replay file). A run is
// its setup and its inputs (play/inputs.hpp), so the file is the input log with what a player
// keeps beside it: the MARKS (the moments the game filed as they happened: a flight, a crash, a
// pass, the finish, each with a weight a director ranks by), a CLIP (an in and an out: a clip is
// cut by marking, never by copying the run), the game's behaviour version and when it was
// recorded. It is plain text, like the log it carries:
//
//   ge-replay 1
//   game_version 1
//   recorded 2026-10-23T11:00:00Z
//   clip 1200 3600
//   mark 1410 takeoff 1
//   mark 2875 wipeout 3
//   inputs
//   ge-inputs 1
//   game fall_line
//   ...
//
// Everything after the `inputs` line is the input log as `play::to_text` writes it. `recorded`
// is the app's text (the engine reads no clock) and may be absent; `clip` is absent for the whole
// run. A mark's kind is the game's word (`[a-z0-9_]+`); the engine only lists and ranks them.
//
// THE VERSION. A change of a game's behaviour changes every digest (G1), so a replay recorded by
// another version of the game would play a different run. `plays_on` refuses it, naming both
// versions: the game's `GameSpec::version` is the one to raise when a change moves what a log
// plays to, and a game that wants old replays to keep playing pins the old behaviour as a trait.
//
// ge:summary The replay file: the input log with marks, a clip, the game's version; text and I/O
// ge:keywords replay replay-file file mark marks clip in out game_version version recorded
// ge:keywords save load parse_replay to_text plays_on refuse ge-replay
// ge:skill replay
// ge:test tests/replay_file_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "play/game.hpp"
#include "play/inputs.hpp"

namespace ge::replay {

inline constexpr int kReplayVersion = 1;
// The extension a replay file is saved under (the index lists only these).
inline constexpr std::string_view kReplayExtension = ".ge-replay";

// A moment the game filed: at `step` (the steps taken when it happened, back-dated where the
// game only knows it later), of `kind`, worth `weight` (the game's scale; a director ranks by it).
struct Mark {
  int64_t step = 0;
  std::string kind;
  double weight = 1;
};

struct ReplayFile {
  play::InputLog log;
  int32_t game_version = 1;
  std::vector<Mark> marks;  // by step, oldest first
  int64_t clip_in = 0;
  int64_t clip_out = -1;  // -1: the log's end
  std::string recorded;   // the app's text, "" when none: the engine reads no clock

  // The clip in steps, held inside 0..the log's steps (`out` at least `in`).
  int64_t in() const;
  int64_t out() const;
};

// Whether `kind` can name a mark: one or more of a-z, 0-9 and _.
bool mark_kind_ok(std::string_view kind);

// The file as text (the format above), ending with a newline.
std::string to_text(const ReplayFile& file);
// The text read back; on failure `error` says what and on which line.
std::optional<ReplayFile> parse_replay(std::string_view text, std::string& error);

// Whether `file` plays on `spec`: the log fits (its game and axes) and was recorded by the same
// version of the game. `error` names what differs, both versions for a version.
bool plays_on(const ReplayFile& file, const play::GameSpec& spec, std::string& error);

// Written to `path` (any directory must exist) or read from it; `error` says why not.
bool save_replay(const std::string& path, const ReplayFile& file, std::string& error);
std::optional<ReplayFile> load_replay(const std::string& path, std::string& error);

}  // namespace ge::replay
