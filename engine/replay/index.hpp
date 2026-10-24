// THE REPLAY INDEX: the replays in a directory, one entry a file, for a menu's replay browser to
// list (docs/replays.md § The replay file). An entry is what a list row shows without playing
// anything: the game and its version, the seed, the length, how many moments were marked and of
// which kinds, the clip, and when it was recorded (the app's text). A file that does not read is
// a problem line naming the file and why, never a missing row with no word.
//
// ge:summary The replay index: a directory's replay files as list rows (game, seed, length, marks)
// ge:keywords replay index browser list scan directory entry entries kinds marks seconds
// ge:skill replay
// ge:test tests/replay_file_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "replay/file.hpp"

namespace ge::replay {

struct IndexEntry {
  std::string path;
  std::string game;
  int32_t game_version = 0;
  double seed = 0;
  int64_t steps = 0;
  double seconds = 0;
  size_t marks = 0;
  std::vector<std::string> kinds;  // the marks' kinds, each once, in the order first marked
  int64_t clip_in = 0, clip_out = 0;
  std::string recorded;
};

// The row for `file`, saved at `path`.
IndexEntry entry_of(const ReplayFile& file, const std::string& path);

// Every `kReplayExtension` file directly in `dir`, by path. A file that does not read adds a line
// to `problems` (when given) and no row; a directory that does not exist is one problem.
std::vector<IndexEntry> scan_replays(const std::string& dir,
                                     std::vector<std::string>* problems = nullptr);

}  // namespace ge::replay
