// What a change reaches (engine/cli/reach.hpp), on a made-up build: a change in a plugin's source
// reaches the plugin, the game library that includes its headers and the tests including the
// game's, never a test that only links them; an executable compiling in a library's directory
// claims no header there; the library targets read off Ninja's target list; and the tests a
// change picks, with the labs' smoke slices, a changed golden's test and a build file's
// everything.
//
// ge:summary Test: the change's reach through the include graph and the tests it picks
// ge:keywords test cli change-reach affected-tests reach_from_deps libraries_of pick_affected
// ge:skill testing agent-interface
#include <set>
#include <string>
#include <vector>

#include "check.hpp"
#include "cli/reach.hpp"

using namespace ge::cli;

namespace {

const fs::path kRepo = "/src/game";

// Four libraries, the labs executable compiling in the lab's directory, and three tests.
const std::string kDeps =
    "CMakeFiles/ge_core.dir/engine/core/math.cpp.o: #deps 2, deps mtime 1 (VALID)\n"
    "    /src/game/engine/core/math.cpp\n"
    "    /src/game/engine/core/math.hpp\n"
    "\n"
    "plugins/wing/CMakeFiles/ge_wing.dir/acro.cpp.o: #deps 3, deps mtime 1 (VALID)\n"
    "    /src/game/plugins/wing/acro.cpp\n"
    "    /src/game/plugins/wing/acro.hpp\n"
    "    /src/game/engine/core/math.hpp\n"
    "\n"
    "games/g/CMakeFiles/g_sim.dir/sim/outing.cpp.o: #deps 3, deps mtime 1 (VALID)\n"
    "    /src/game/games/g/sim/outing.cpp\n"
    "    /src/game/games/g/sim/outing.hpp\n"
    "    /src/game/plugins/wing/acro.hpp\n"
    "\n"
    "CMakeFiles/ge_lab.dir/engine/lab/raster.cpp.o: #deps 2, deps mtime 1 (VALID)\n"
    "    /src/game/engine/lab/raster.cpp\n"
    "    /src/game/engine/lab/raster.hpp\n"
    "\n"
    "CMakeFiles/labs.dir/engine/lab/main.cpp.o: #deps 3, deps mtime 1 (VALID)\n"
    "    /src/game/engine/lab/main.cpp\n"
    "    /src/game/engine/lab/raster.hpp\n"
    "    /src/game/games/g/sim/outing.hpp\n"
    "\n"
    "tests/CMakeFiles/g_outing_test.dir/g_outing_test.cpp.o: #deps 3, deps mtime 1 (VALID)\n"
    "    /src/game/tests/g_outing_test.cpp\n"
    "    /src/game/tests/check.hpp\n"
    "    /src/game/games/g/sim/outing.hpp\n"
    "\n"
    "tests/CMakeFiles/core_math_test.dir/core_math_test.cpp.o: #deps 3, deps mtime 1 (VALID)\n"
    "    /src/game/tests/core_math_test.cpp\n"
    "    /src/game/tests/check.hpp\n"
    "    /src/game/engine/core/math.hpp\n"
    "\n"
    "tests/CMakeFiles/lab_raster_test.dir/lab_raster_test.cpp.o: #deps 3, deps mtime 1 (VALID)\n"
    "    /src/game/tests/lab_raster_test.cpp\n"
    "    /src/game/tests/check.hpp\n"
    "    /src/game/engine/lab/raster.hpp\n";

const std::set<std::string> kLibraries{"ge_core", "ge_wing", "g_sim", "ge_lab"};

Reach reach_of(std::set<std::string> changed) {
  Reach r;
  r.changed = std::move(changed);
  reach_from_deps(kRepo, kDeps, kLibraries, r);
  return r;
}

}  // namespace

TEST(a_plugin_source_reaches_the_game_library_using_it_and_the_tests_including_that) {
  const Reach r = reach_of({"plugins/wing/acro.cpp"});
  CHECK((r.sources == std::set<std::string>{"plugins/wing/acro.cpp"}));
  CHECK(r.targets.count("ge_wing") && r.targets.count("g_sim"));
  CHECK(r.targets.count("g_outing_test"));
  CHECK(r.targets.count("labs"));  // it includes the game's headers
  CHECK(!r.targets.count("core_math_test") && !r.targets.count("ge_core"));
}

TEST(an_executable_claims_no_header_in_the_directory_it_compiles_in) {
  // labs compiles engine/lab/main.cpp; its change must not reach what includes engine/lab.
  const Reach r = reach_of({"engine/lab/main.cpp"});
  CHECK((r.targets == std::set<std::string>{"labs"}));
  // ge_lab, a library compiling there, does.
  const Reach lib = reach_of({"engine/lab/raster.cpp"});
  CHECK(lib.targets.count("lab_raster_test") && lib.targets.count("labs"));
}

TEST(a_shared_header_reaches_everything_including_it) {
  const Reach r = reach_of({"engine/core/math.hpp"});
  CHECK(r.targets.count("ge_core") && r.targets.count("ge_wing") && r.targets.count("g_sim"));
  CHECK(r.targets.count("core_math_test") && r.targets.count("g_outing_test"));
  CHECK(!r.targets.count("lab_raster_test"));
  CHECK(r.sources.count("engine/core/math.cpp") && r.sources.count("plugins/wing/acro.cpp"));
}

TEST(the_libraries_are_read_off_ninjas_target_list) {
  const std::set<std::string> libs = libraries_of(
      "libge_core.a: CXX_STATIC_LIBRARY_LINKER__ge_core_Debug\n"
      "plugins/wing/libge_wing.a: CXX_STATIC_LIBRARY_LINKER__ge_wing_Debug\n"
      "g_sim.lib: CXX_STATIC_LIBRARY_LINKER__g_sim_Debug\n"
      "tests/core_math_test: CXX_EXECUTABLE_LINKER__core_math_test_Debug\n"
      "all: phony\n");
  CHECK((libs == std::set<std::string>{"ge_core", "ge_wing", "g_sim"}));
}

TEST(the_tests_picked_take_the_smoke_slices_goldens_and_build_files) {
  const std::vector<std::string> tests{"core_math_test", "g_outing_test", "labs_smoke_1",
                                       "labs_smoke_2"};
  CHECK((pick_affected(tests, {"g_outing_test"}, {}) == std::vector<std::string>{"g_outing_test"}));
  CHECK(pick_affected(tests, {"labs"}, {}) ==
        (std::vector<std::string>{"labs_smoke_1", "labs_smoke_2"}));
  CHECK(pick_affected(tests, {}, {"tests/labs_digests.txt"}).size() == 2);
  CHECK((pick_affected(tests, {}, {"tests/golden/core_math.txt"}) ==
         std::vector<std::string>{"core_math_test"}));
  CHECK(pick_affected(tests, {}, {"plugins/wing/CMakeLists.txt"}) == tests);
  CHECK(build_file("cmake/Plugins.cmake") && build_file("CMakePresets.json"));
  CHECK(!build_file("docs/cmake.md"));
}
