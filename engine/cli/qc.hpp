// `ge qc` (engine/toolkit/qc.hpp: the rows and the answer, shared with every game's CLI): ge's way
// of running the qc tool, by building the game repo's `progs` executable (the progs plugin's tool:
// the engine's command line never links a plugin) and handing it the command's words, its output
// kept in a log under the build. The map group's rows (engine/toolkit/map.hpp) run the brush
// plugin's `brush` executable the same way.
//
// ge:summary ge's runner of the qc tool: the repo's progs executable built and handed the words
// ge:keywords cli ge qc progs brush map tool-runner hand-off build-log run_tool
// ge:skill agent-interface progs
// ge:test tests/toolkit_qc_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/build.hpp"
#include "toolkit/call.hpp"

namespace ge::cli {

// The tool `name` ("qc", "brush") built in the repo and run with `args`; `code` -1 with findings
// when it could not be built or has no executable.
toolkit::ToolRun run_tool(const BuildOptions& o, std::string_view name,
                          const std::vector<std::string>& args);

}  // namespace ge::cli
