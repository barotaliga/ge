// SHOOTER'S EDGE'S PLAYER CLI (`shooters_edge_cli`, docs/gamecli.md): the engine's generic
// commands (engine/gamecli) over this game's Spec, classic enhance's group (the install's packs),
// and the rows only this game has:
//
// - THE PLACES are the window's own: the install it opens (`--basedir`, else the classic game
//   loaded last, kept in the platform's per-user folder, client/game_folder.hpp), the game folder
//   over it (`--game`, else the multiplayer folder when the install has one), the player's
//   settings in `<install>/<game folder>/settings/` (client/settings.hpp) and the masters the
//   install's server lists name.
// - THE PROTOCOL is the classic one (net/classic_browse.hpp): the status query and the masters
//   for the browser, the connectionless remote console.
// - THE WINDOW joins with `+connect <address>` and hosts as HOST GAME's START does: `+sv_port`,
//   the server's lines (`+sv deathmatch 1`, `+sv maxclients <n>`) and `+map`; the dedicated server
//   is `shooters_edge_server` (net/classic_game.hpp's ServerGame).
// - ITS OWN ROWS: `demos` (the recordings in the game folder) and `demos play <name>`.
//
// ge:summary Shooter's Edge's player CLI: its Spec, places, protocol, launch words and demos
// ge:keywords shooters_edge_cli gamecli player-cli spec locate basedir game-folder join host
// ge:keywords demos playdemo recordings mvd
// ge:skill shooter agent-interface
// ge:test tests/gamecli_cli_test.cpp
#pragma once

#include "gamecli/gamecli.hpp"

namespace shooters_edge::cli {

// The Spec `shooters_edge_cli` runs.
const ge::gamecli::Spec& spec();

}  // namespace shooters_edge::cli
