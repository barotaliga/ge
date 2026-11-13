// THE LOOKS A SERVER OFFERS (`listskins`): the skins and the player figures in the server's game
// folder, so a player can pick one a client that lacks it will fetch at its next signon.
//
// - A SKIN is a picture under `skins/` (`.pcx`, `.png` or `.tga`), named without its extension.
// - A FIGURE is a folder `players/<model>/` with a `tris.md2` or `tris.mdl`, its skins the pictures
//   beside it (not an icon `<skin>_i` nor `weapon`).
// - The answer reads like a game's: the skins in a line, each figure with its skins in brackets,
//   and a word when downloads are off (a player gets only what they already have).
//
// ge:summary The skins and player figures a server offers, listed for `listskins`
// ge:keywords listskins looks-offered server-skins server-models player-figures downloads
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_looks_test.cpp
#pragma once

#include <span>
#include <string>

namespace shooters_edge::net {

// The answer to `listskins` over the server's file names (lower case, '/' between).
std::string looks_offered(std::span<const std::string> names, bool downloads);

}  // namespace shooters_edge::net
