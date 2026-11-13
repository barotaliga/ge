// THE CLASSIC GAMES' INSTALLS ON THIS MACHINE: what SELECT GAME shows beside each game the content
// profiles know (classicenhance/profile.hpp): the install of it the player loaded last, and which
// edition that install is.
//
// - **Kept by game** (`remember_install`, `remembered_installs`): `<prefs>/installs.txt`, a line a
//   game, `<profile id> <folder>`; written whenever a game's install opens, beside the single game
//   loaded last (game_folder.hpp's `remember_game`), so picking a game a second time opens its
//   install at once, with no folder to find.
// - **Which edition** (`edition_of`): the packs read through the search path, the profile's
//   full-game file found in them or not (`full_game`: a picture only the full game's packs hold),
//   so an install reads as the shareware game or the registered one the moment it is picked.
//
// ge:summary Each classic game's install kept by game, and whether it is the shareware or the full
// ge:keywords installs select-game remember_install remembered_installs edition shareware
// ge:keywords registered full-game installs-txt
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#pragma once

#include <map>
#include <string>
#include <string_view>

#include "classicenhance/profile.hpp"

namespace shooters_edge::client {

// Keeps `basedir` as the install of the game `id` (a content profile's id); false (nothing kept)
// when `prefs` is empty or unwritable.
bool remember_install(const std::string& prefs, std::string_view id, const std::string& basedir);
// Every game's install kept, by profile id; empty when none (or `prefs` is empty).
std::map<std::string, std::string, std::less<>> remembered_installs(const std::string& prefs);

enum class Edition : uint8_t { unknown, shareware, full };

// Which edition the install at `basedir` of `content` is: unknown when its packs do not open.
Edition edition_of(const std::string& basedir, const ge::classicenhance::Profile& content);

// The edition in the game's words for the player ("Shareware", "Registered"; the sequel's "Demo",
// "Full game").
std::string_view edition_words(Edition e, const ge::classicenhance::Profile& content);

}  // namespace shooters_edge::client
