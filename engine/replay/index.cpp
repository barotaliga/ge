// The replay index's definitions (index.hpp): a file's row and a directory's scan.
//
// ge:summary The replay index's definitions: entry_of and scan_replays over a directory
// ge:keywords replay index entry_of scan_replays directory
// ge:skill replay
// ge:test tests/replay_file_test.cpp
#include "replay/index.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace ge::replay {

IndexEntry entry_of(const ReplayFile& file, const std::string& path) {
  IndexEntry e;
  e.path = path;
  e.game = file.log.game;
  e.game_version = file.game_version;
  e.seed = file.log.seed;
  e.steps = file.log.steps();
  e.seconds = file.log.hz > 0 ? static_cast<double>(e.steps) / file.log.hz : 0;
  e.marks = file.marks.size();
  for (const Mark& m : file.marks) {
    if (std::find(e.kinds.begin(), e.kinds.end(), m.kind) == e.kinds.end()) {
      e.kinds.push_back(m.kind);
    }
  }
  e.clip_in = file.in();
  e.clip_out = file.out();
  e.recorded = file.recorded;
  return e;
}

std::vector<IndexEntry> scan_replays(const std::string& dir, std::vector<std::string>* problems) {
  namespace fsys = std::filesystem;
  std::vector<IndexEntry> out;
  std::error_code ec;
  fsys::directory_iterator it(dir, ec);
  if (ec) {
    if (problems) problems->push_back(dir + ": cannot list it (" + ec.message() + ")");
    return out;
  }
  std::vector<std::string> paths;
  for (const fsys::directory_entry& f : it) {
    if (f.is_regular_file(ec) && f.path().extension() == kReplayExtension) {
      paths.push_back(f.path().string());
    }
  }
  std::sort(paths.begin(), paths.end());
  for (const std::string& p : paths) {
    std::string error;
    const std::optional<ReplayFile> file = load_replay(p, error);
    if (!file) {
      if (problems) problems->push_back(error);
      continue;
    }
    out.push_back(entry_of(*file, p));
  }
  return out;
}

}  // namespace ge::replay
