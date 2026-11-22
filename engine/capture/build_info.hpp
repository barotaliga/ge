// THE BUILD'S OWN STAMP: which source a binary was built from, for a report a player or an agent
// pastes (`gpudebug`'s notes, capture/debug_shot.hpp): the commit, whether the tree had changes not
// committed, the branch, the build type, the compiler and the system.
//
// `build_info` is defined in a file cmake/BuildInfo.cmake writes on every build (replaced only when
// it changes), in the library `ge_build_info`; only an executable that reports it (a game's window)
// links it, so a new commit relinks that alone. A tree with no git says "unknown".
//
// ge:summary The build's stamp: commit, uncommitted changes, branch, build type, compiler, system
// ge:keywords build-info commit git-hash dirty version stamp branch compiler build-type
// ge:keywords build_info build_info_line ge_build_info
// ge:skill build
// ge:test tests/capture_build_info_test.cpp
#pragma once

#include <string>
#include <string_view>

namespace ge::capture {

struct BuildInfo {
  std::string_view commit;       // the commit's short hash (12), or "unknown"
  std::string_view branch;       // "HEAD" when detached
  std::string_view commit_date;  // ISO 8601, the commit's
  int dirty_files = 0;           // tracked files changed since the commit (0: clean)
  std::string_view dirty_hash;   // the first 8 of the diff's SHA-256 ("" when clean)
  std::string_view build_type;   // Debug, Release, ...
  std::string_view compiler;     // "AppleClang 17.0.0"
  std::string_view system;       // "Darwin arm64"
};

// This binary's stamp (linked from `ge_build_info`).
const BuildInfo& build_info();

// The stamp in one line: "0708d6639a1c +3 files changed (1a2b3c4d) on main, 2026-11-22; Release,
// AppleClang 17.0.0, Darwin arm64".
inline std::string build_info_line(const BuildInfo& b) {
  std::string s(b.commit);
  if (b.dirty_files > 0)
    s += " +" + std::to_string(b.dirty_files) + " file" + (b.dirty_files == 1 ? "" : "s") +
         " changed (" + std::string(b.dirty_hash) + ")";
  s += " on " + std::string(b.branch) + ", " + std::string(b.commit_date.substr(0, 10)) + "; " +
       std::string(b.build_type) + ", " + std::string(b.compiler) + ", " + std::string(b.system);
  return s;
}

}  // namespace ge::capture
