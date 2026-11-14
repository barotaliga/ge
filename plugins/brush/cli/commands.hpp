// THE MAP COMMANDS IN A GAME'S PLAYER CLI (engine/gamecli): what any game on the classic brush
// maps adds to its `<game>_cli` so its players and modders build maps without the engine's tools:
//
// - `map build <plan|svg>`: a map plan (compile/plan.hpp) or an SVG floor plan (compile/svg.hpp)
//   compiled into a version 29 map (compile/compile.hpp). `--out=<file.bsp>` (default: beside the
//   source, its name with .bsp); `--textures=<folder>` takes the pictures from the maps in that
//   folder's packs and loose maps (default: the game's install when the CLI finds it; patterns
//   otherwise); `--no-light` skips the light; `--leak=<file>` writes a leak's way out as points,
//   one `x y z` a line.
// - `map plan <svg>`: the plan an SVG floor plan reads as, printed (to check a drawing).
//
// A game appends `commands()` to its Spec's `commands` and `command_flags()` to its
// `command_flags`.
//
// ge:summary The map commands in a game's player CLI: build a BSP from a plan or an SVG drawing
// ge:keywords gamecli player-cli map build compile plan svg floor-plan bsp modder leak textures
// ge:skill brush agent-interface
// ge:test tests/gamecli_cli_test.cpp
#pragma once

#include <vector>

#include "gamecli/gamecli.hpp"

namespace ge::brush::cli {

std::vector<gamecli::Command> commands();
std::vector<gamecli::CommandFlags> command_flags();

}  // namespace ge::brush::cli
