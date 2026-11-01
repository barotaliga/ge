// A CLASSIC GAME'S GAME HALF: what Shooter's Edge does with a classic game's content, one row of
// data a game beside its content row (plugins/classicenhance/profile.hpp: where the files are
// and what kind each is). The window, the menu, the server and the labs read the row instead of
// writing a file name or a map into their code, so a second classic game is a second row here
// and a second there (docs/shooters-edge.md § Profiles).
//
// - **Programs**: each mode's program files, looked for in order through the game's search path
//   (client/files.hpp: a mod's folder over the base), the first whose form (the header's layout
//   CRC, plugins/progs/layout.hpp) is the mode's being run. The multiplayer form's own name comes
//   first, then the one name the single-player form also uses, so a multiplayer mod that ships
//   either name runs; a file of the other form is passed over, never run in the wrong mode.
// - **The fallback map**: where the window starts when the content holds no start map (the
//   content row's `start_map`): the built-in arena.
// - **The full game** is the content row's `full_game` file being in the packs.
//
// ge:summary A classic game's game half as data: each mode's program files, the fallback map
// ge:keywords game-profile profile classic program progs qwprogs multiplayer single-player
// ge:keywords mod-program fallback-map game-half find_game
// ge:skill shooter
// ge:test tests/shooters_edge_game_profile_test.cpp
#pragma once

#include <span>
#include <string_view>

#include "classicenhance/profile.hpp"

namespace shooters_edge::profiles {

struct GameProfile {
  std::string_view id;  // the content row's id (classicenhance/profile.hpp)
  std::span<const std::string_view> multiplayer_programs;    // looked for in order
  std::span<const std::string_view> single_player_programs;  // looked for in order
  std::string_view fallback_map;                             // with no start map in the content
};

inline constexpr std::string_view kClassicMultiplayer[] = {"qwprogs.dat", "progs.dat"};
inline constexpr std::string_view kClassicSinglePlayer[] = {"progs.dat"};

inline constexpr GameProfile kGameProfiles[] = {
    {.id = "classic",
     .multiplayer_programs = kClassicMultiplayer,
     .single_player_programs = kClassicSinglePlayer,
     .fallback_map = "arena"},
};

// The first row: the classic game, the one every reader uses until a choice of game is made.
inline const GameProfile& classic_game() { return kGameProfiles[0]; }

// The row of `id`, or nullptr.
inline const GameProfile* find_game(std::string_view id) {
  for (const GameProfile& g : kGameProfiles) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

// The game row's content row (every game row has one: the test holds it).
inline const ge::classicenhance::Profile& content_of(const GameProfile& g) {
  const ge::classicenhance::Profile* p = ge::classicenhance::find_profile(g.id);
  return p != nullptr ? *p : ge::classicenhance::classic_profile();
}

}  // namespace shooters_edge::profiles
