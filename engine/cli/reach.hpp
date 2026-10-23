// What a change reaches, read off git and the build's include graph: the files changed since a
// base (the merge base of it and HEAD, untracked files included), the .cpp files that compile one
// of them (Ninja's header dependencies, `ninja -t deps`), and the targets those reach. A target is
// reached when one of its objects includes a changed file, or includes a header in a directory a
// reached library compiles sources in (so a change in a plugin reaches the game library that uses
// its headers, and the tests using the game's); every target links every library here, so the
// link graph alone would reach everything. `ge lint` lints the sources; `ge test --affected` runs
// the tests whose targets are reached. The deps are the last build's, so a header a file has only
// just started including is seen through the file's own change.
//
// ge:summary What a change reaches: the changed files, the sources compiling them, the targets
// ge:keywords cli change-reach affected-tests ninja-deps include-graph merge-base
// ge:keywords change_reach reach_from_deps pick_affected
// ge:skill agent-interface
// ge:test tests/cli_reach_test.cpp
#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "cli/build.hpp"

namespace ge::cli {

struct Reach {
  std::set<std::string> changed;  // repository-relative paths changed since the base
  std::set<std::string> sources;  // repository-relative .cpp files that compile a changed file
  std::set<std::string> targets;  // the build targets reached (CMake's names: ge_core, labs, ...)
  // The .cpp files whose own object is reached: it includes a changed file, or a header in a
  // directory a reached library compiles in. Finer than `targets` where one executable holds many
  // subjects (each lab of the labs executable is reached on its own).
  std::set<std::string> objects;
};

// The change since `base` in `repo`, its sources and objects off `build`'s deps. False when git
// finds no merge base (the reach is then unknown, and the caller takes everything).
bool change_reach(const fs::path& repo, const fs::path& build, const std::string& base,
                  const fs::path& log, Reach& r);
// `ninja -t deps` output ("<dir>/CMakeFiles/<target>.dir/<file>.o: #deps ..." then one indented
// path a line, the source first) folded into `r`: the sources compiling a changed file, and the
// targets reached from them through the headers of reached libraries' directories. Only a target
// in `libraries` owns the directories it compiles in (nothing links an executable); with none
// named, every target but a test (`*_test`, all of them in tests/) does.
void reach_from_deps(const fs::path& repo, std::string_view deps,
                     const std::set<std::string>& libraries, Reach& r);
// The library targets in `ninja -t targets all` output (`libge_core.a: <rule>` is ge_core).
std::set<std::string> libraries_of(std::string_view targets);
// Whether a path is the build's own (a CMakeLists.txt, a .cmake file, CMakePresets.json): a
// change to one reaches every test.
bool build_file(std::string_view path);
// The tests (CTest's names) a change picks: each whose target is in `targets`, the labs' smoke
// slices when the labs target is or the digests changed, the test of a changed golden
// (`tests/golden/<name>.txt` is <name>_test), and every test when a build file changed.
std::vector<std::string> pick_affected(const std::vector<std::string>& tests,
                                       const std::set<std::string>& targets,
                                       const std::set<std::string>& changed);

}  // namespace ge::cli
