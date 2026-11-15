// THE PROGS PLUGIN IN A GAME'S PLAYER CLI (engine/gamecli): the `qc` tool run in the CLI's own
// process, so a game that links the plugin offers the toolkit's `qc` rows (engine/toolkit/qc.hpp:
// build, disasm, run, trace, profile) to a mod's author who has the game and not the engine.
// `ge qc` runs the same rows by building the repo's `progs` executable instead.
//
// A game sets its Spec's `tools` to `tools()` and its `tool` to `run_tool`.
//
// ge:summary The progs plugin's qc tool run inside a game's player CLI, for the toolkit's qc rows
// ge:keywords progs qc gamecli player-cli toolkit tool-runner in-process modder qc_main
// ge:skill progs agent-interface
// ge:test tests/gamecli_cli_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "toolkit/call.hpp"

namespace ge::progs::cli {

// The tools this plugin runs: "qc".
std::vector<std::string> tools();
// `qc_main` with `args` (the command, then its words and flags); its output captured.
toolkit::ToolRun run_tool(std::string_view program, std::string_view name,
                          const std::vector<std::string>& args);

}  // namespace ge::progs::cli
