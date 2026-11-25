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
// ge:keywords registered full-game installs-txt game_rows mission-pack packs_of pack-row
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_game_folder_test.cpp
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "classicenhance/profile.hpp"
#include "shooters_edge/menu/games_page.hpp"

namespace shooters_edge::client {

// Keeps `basedir` as the install of the game `id` (a content profile's id); false (nothing kept)
// when `prefs` is empty or unwritable.
bool remember_install(const std::string& prefs, std::string_view id, const std::string& basedir);
// Every game's install kept, by profile id; empty when none (or `prefs` is empty).
std::map<std::string, std::string, std::less<>> remembered_installs(const std::string& prefs);

enum class Edition : uint8_t { unknown, shareware, full };

// Which edition the install at `basedir` of `content` is: unknown when its packs do not open.
Edition edition_of(const std::string& basedir, const ge::classicenhance::Profile& content);

// The edition in the game's words for the player ("Shareware", "Registered"; Quake II's "Demo",
// "Full game").
std::string_view edition_words(Edition e, const ge::classicenhance::Profile& content);

// SELECT GAME's rows: each classic game added on this computer (its install kept here, the folder
// still holding it), in the profiles' order; the game `playing` (a profile id, "" none) is one of
// them, from `playing_install`; `remixed` the one classic remix plays it with ("" none). Each with
// its edition. A game never added is not listed: ADD GAME adds it. Under a game's row, a row for
// each of its mission packs that install holds (`<install>/<pack folder>/` with its first pack),
// the one playing when `playing_folder` (the game folder playing over the install) is the pack's.
std::vector<shooters_edge::menu::GameRow> game_rows(const std::string& prefs,
                                                    std::string_view playing,
                                                    const std::string& playing_install,
                                                    std::string_view remixed = {},
                                                    std::string_view playing_folder = {});

// The game folder to play over the install at `basedir`: the mission pack `*pack` names when it is
// one of `content`'s, else the game's own (`game_of`); `*pack` is cleared either way.
std::string folder_over(const std::string& basedir, const ge::classicenhance::Profile& content,
                        std::string* pack);

// The mission packs of `content` the install at `basedir` holds, in the profile's order.
std::vector<const ge::classicenhance::MissionPack*> packs_of(
    const std::string& basedir, const ge::classicenhance::Profile& content);

}  // namespace shooters_edge::client
