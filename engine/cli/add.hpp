// GROWING A GAME REPO WITHOUT HAND-WIRING: `ge use <plugin>` and `ge add system|lab|test|water`.
//
//   ge use <plugin>          links ge::<plugin> into the game's simulation (CMakeLists.txt) and
//                            installs the plugin's consumer skill (`ge skills sync`)
//   ge add system <name>     sim/<name>.hpp, sim/<name>.cpp and tests/sim_<name>_test.cpp: a
//                            system's state and step, and the test that holds its bits
//   ge add lab <name>        labs/<name>_lab.cpp of one kind (sim, scenario, picture, bench),
//                            registered by GE_LAB and so in the smoke pass
//   ge add test <name>       tests/<name>_test.cpp on the repo's harness
//   ge add water [<name>]    sim/<name>.hpp, sim/<name>.cpp (a lake laid in one call, on the wind),
//                            labs/<name>_lab.cpp (its plan and its section, pictured) and
//                            tests/sim_<name>_test.cpp; links ge::water and syncs its skill. The
//                            name defaults to "water"
//
// The pieces come from templates/<kind>/ (compiled into ge, like the scaffold), each a small
// working example with its ge: tags filled, so the repo builds, tests and passes `ge doctor` the
// moment one lands. The placeholders a template may use, in its text and its path:
//
//   {{name}} {{ident}}   the game's name and identifier (read from CMakeLists.txt's project())
//   {{snake}}            the piece's name as an identifier ("wind_gust")
//   {{kebab}}            the same in kebab case, a lab's id ("wind-gust")
//   {{Type}}             the same in Pascal case, a type's name ("WindGust")
//
// Nothing is ever written over: a piece whose file exists is refused.
//
// ge:summary ge use and ge add: link an engine plugin; add a system, a lab, a test or water
// ge:keywords cli ge use add plugin system lab test template placeholders run_use run_add
// ge:keywords plugin_names game_ident add-water lake
// ge:skill agent-interface
// ge:test tests/cli_add_test.cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "cli/outcome.hpp"

namespace ge::cli {

// The engine's plugins this ge carries: each name and the plugins it links.
struct PluginInfo {
  std::string name;
  std::vector<std::string> plugins;
};
std::vector<PluginInfo> plugin_infos();

// The game's name as CMakeLists.txt's project() states it, or nothing.
std::optional<std::string> game_name(const fs::path& repo);

// Whether the repo's CMake links ge::<plugin>.
bool links_plugin(const fs::path& repo, const std::string& plugin);

Outcome run_use(const fs::path& repo, const std::string& plugin);

// `kind` is "system", "lab", "test" or "water"; `lab_kind` names the lab's kind (sim when empty).
// Water's name may be empty ("water").
Outcome run_add(const fs::path& repo, const std::string& kind, const std::string& name,
                const std::string& lab_kind, bool dry_run);

}  // namespace ge::cli
