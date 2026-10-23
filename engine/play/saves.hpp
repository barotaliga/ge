// A RUN SAVED AND LOADED (docs/saves.md § The run saved): the play contract's half of saves. The
// engine writes the meta (the game, the setup as an input log's header, the title) and its own
// `PLAY` chunk (the step the run had reached); the game writes its own chunks (`Run::save`).
// Loading reads the setup back out of the meta, opens a run on it the way a replay does, and
// hands it the file (`Run::load`).
//
// THE CHECK every game is held to (`check_saves`, the `save-run` lab, `ge save check`): a run
// played off an input log, saved at a step, loaded into a fresh run and stepped on along the same
// log, has at every later step the digest of the run that was never saved, and at the end saves
// to the same bytes. A field a step writes and the game's describe leaves out shows up as the
// first step where the two part.
//
// ge:summary The run saved and loaded through the play contract, and the check against the run
// ge:summary never saved
// ge:keywords play save load save_run load_run check_saves saved-run digest setup meta step
// ge:keywords mismatch round-trip save-command play-save write diff
// ge:skill save agent-interface
// ge:test tests/play_saves_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "play/game.hpp"
#include "play/inputs.hpp"
#include "save/file.hpp"

namespace ge::play {

inline constexpr uint32_t kPlayTag = save::tag("PLAY");
inline constexpr uint16_t kPlayVersion = 1;

// `run`, opened on `setup` of `spec` and stepped `step` times, saved into `out` with `meta`'s
// title, place and date (its game, setup and play time are filled here). False with `error`.
bool save_run(const GameSpec& spec, const Setup& setup, const Run& run, int64_t step,
              save::Meta meta, std::vector<uint8_t>& out, std::string& error);

// A run opened on the save's setup and loaded from it, with `step` the step it was saved at; null
// with `error` naming the file's problem and the next action. `name` names the file in errors.
std::unique_ptr<Run> load_run(const GameSpec& spec, std::span<const uint8_t> bytes, int64_t& step,
                              std::string& error, std::string_view name = "save");

struct SavedAt {
  int64_t step = 0;    // saved after this many steps
  size_t bytes = 0;    // the file's size
  double save_ms = 0;  // `save_run` (measured)
  double load_ms = 0;  // `load_run`, the setup's rebuild included (measured)
  double open_ms = 0;  // of which opening the run on the setup (measured)
  // The first step whose digest differs from the run never saved, or the log's last step when
  // only the two runs' saves at the end differ; -1: none.
  int64_t parted = -1;
  std::vector<std::pair<std::string, size_t>> chunks;  // each chunk's tag#instance and bytes
};

// `log` played straight, and saved and loaded at each of `at` and played on to the log's end
// beside it. Empty with `error` when the log does not open or the game keeps no saves.
std::vector<SavedAt> check_saves(const GameSpec& spec, const InputLog& log,
                                 std::span<const int64_t> at, std::string& error);

// `play save <write|load|check|diff> ...` (save_cmd.cpp): `words` the command's words after
// `save`'s own (the subcommand first), `flags` its --flags; prints one JSON line, returns the exit.
int save_command(const GameSpec& spec, const std::vector<std::string>& words,
                 const std::map<std::string, std::string>& flags, std::ostream& out);

}  // namespace ge::play
