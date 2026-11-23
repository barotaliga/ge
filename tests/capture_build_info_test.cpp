// engine/capture/build_info.hpp: the build's line names the commit, the changes not committed
// (how many files and the diff's hash) only when there are some, the branch and the commit's day,
// then the build type, the compiler and the system.
//
// ge:summary Tests for the build's stamp in one line
// ge:keywords test build-info commit dirty build_info_line
// ge:skill testing build
#include <string>

#include "capture/build_info.hpp"
#include "check.hpp"

TEST(a_clean_build_names_its_commit_branch_and_day) {
  ge::capture::BuildInfo b{
      "0708d6639a1c",      "main",        "2026-11-22T14:03:12+01:00", 0, "", "Release",
      "AppleClang 17.0.0", "Darwin arm64"};
  CHECK(ge::capture::build_info_line(b) ==
        "0708d6639a1c on main, 2026-11-22; Release, AppleClang 17.0.0, Darwin arm64");
}

TEST(a_dirty_build_says_how_many_files_and_the_diffs_hash) {
  ge::capture::BuildInfo b{
      "0708d6639a1c", "feature",     "2026-11-22T14:03:12+01:00", 3, "1a2b3c4d", "Debug",
      "GNU 13.2.0",   "Linux x86_64"};
  CHECK(ge::capture::build_info_line(b) ==
        "0708d6639a1c +3 files changed (1a2b3c4d) on feature, 2026-11-22; Debug, GNU 13.2.0, "
        "Linux x86_64");
  b.dirty_files = 1;
  CHECK(ge::capture::build_info_line(b).find("+1 file changed") != std::string::npos);
}
