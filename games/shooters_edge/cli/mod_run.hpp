// `secli mod run <package>` (docs/mods.md § The SDK): a C++ mod package played in Shooter's Edge's
// mod scenario, the scripted match the `mod` lab plays (sim/match.hpp), once without the mod and
// twice with it. The answer says whether the two modded runs agree on every frame's hash (the
// replay holds), whether the mod changed the match, how many hooks it ran and what they cost: the
// modded match's time over the plain one's, a hook call. `ge mod check --run` asks it, so a mod's
// author checks the package in the game it is for without the engine's tree.
//
// The package is loaded as the game loads it (engine/mod/loader.hpp), so a refused one is refused
// here with the loader's reason. A run that does not replay is a refusal (exit 1) naming the first
// frame the two runs part at; a mod that changes nothing is answered, and `changed` says so.
//
// ge:summary secli mod run: a mod package played in the game's mod scenario twice and replayed
// ge:keywords secli mod-run mod check-run replay scenario hook-cost hook-calls package match
// ge:skill mod shooter agent-interface
// ge:test tests/shooters_edge_cli_mod_run_test.cpp
#pragma once

#include <vector>

#include "gamecli/gamecli.hpp"

namespace shooters_edge::cli {

// The command's row and its flags, for the Spec.
ge::gamecli::Command mod_run_command();
ge::gamecli::CommandFlags mod_run_flags();

}  // namespace shooters_edge::cli
