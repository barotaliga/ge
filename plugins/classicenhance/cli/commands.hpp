// CLASSIC ENHANCE'S GROUP IN A GAME'S PLAYER CLI (engine/gamecli): the commands a game that plays
// the player's own classic packs adds to its `<game>_cli`, the same for every such game:
//
// - `content`: the packs in the install the CLI locates: which game they are, the release they
//   are verified as (by SHA-256) or that they are the player's own unverified copy, the later
//   packs (the full game), and what the local cache already holds for them (its stages).
//
// A game appends `commands()` to its Spec's `commands`.
//
// ge:summary Classic enhance's commands in a game's player CLI: the install's packs and cache
// ge:keywords classicenhance gamecli player-cli content packs release verified full-game cache
// ge:keywords stages sha256
// ge:skill classicenhance agent-interface
// ge:test tests/gamecli_cli_test.cpp
#pragma once

#include <vector>

#include "gamecli/gamecli.hpp"

namespace ge::classicenhance::cli {

std::vector<gamecli::Command> commands();

}  // namespace ge::classicenhance::cli
