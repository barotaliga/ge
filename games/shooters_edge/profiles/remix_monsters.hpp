// CLASSIC REMIX'S MONSTERS (`cr_monsters`, docs/goals/M18u.md): each campaign monster of the
// classic game paired with Quake II's that plays its part, by role first (a rifleman beside a
// guard, a leaper beside a leaper, a swimmer beside a swimmer) and then by strength. A map's
// monsters are made the chosen game's by this table, whole: the played game's rules spawn the
// chosen game's monster with its own health, attacks, model and sounds where those rules can run
// it (`kRunsMonsters`), else the played game's partner.
//
// - **By role** (`monster_as`): the first row whose other column names a monster gives the chosen
//   game's; a monster no row knows is kept as it is.
// - **Many to one**: Quake II has three guards and the classic game one rifleman; the extra rows
//   follow the first, so going back the first row's monster is taken.
// - **Bosses** (each game's end-of-episode fights, which a map's own triggers drive) are not rows:
//   they stay the map's own.
//
// ge:summary Classic remix's monsters: the classic game's and Quake II's paired by role
// ge:keywords remix classic-remix cr_monsters monster-swap monster_as monster-match campaign
// ge:keywords remix_monsters runs-monsters single-player
// ge:skill shooter
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace shooters_edge::profiles {

// The games the table's columns are, by content profile id.
inline constexpr std::array<std::string_view, 2> kMonsterGames = {"classic", "q2"};

struct MonsterMatch {
  std::string_view classic;
  std::string_view q2;
  std::string_view what;  // the part both play, for the lab and the docs
};

inline constexpr MonsterMatch kMonsterMatches[] = {
    {"monster_army", "monster_soldier", "rifleman and guard (shotgun)"},
    {"monster_dog", "monster_soldier_light", "the weakest: hound and light guard"},
    {"monster_enforcer", "monster_infantry", "trooper and enforcer (rapid fire)"},
    {"monster_knight", "monster_berserk", "swordsman and berserker (blades)"},
    {"monster_ogre", "monster_gunner", "brute and gunner (grenades)"},
    {"monster_ogre_marksman", "monster_gunner", "the brute's marksman"},
    {"monster_hell_knight", "monster_gladiator", "death knight and gladiator"},
    {"monster_wizard", "monster_flyer", "flyers"},
    {"monster_zombie", "monster_parasite", "the slow ones that wear you down"},
    {"monster_demon1", "monster_mutant", "leapers with claws"},
    {"monster_shalrath", "monster_chick", "homing and rocket throwers"},
    {"monster_shambler", "monster_tank", "the heaviest walkers"},
    {"monster_fish", "monster_flipper", "swimmers"},
    {"monster_tarbaby", "monster_floater", "the odd ones"},
    {"monster_enforcer", "monster_soldier_ss", "heavy guard (machinegun)"},
    {"monster_hell_knight", "monster_medic", "medic"},
    {"monster_shalrath", "monster_brain", "brains"},
    {"monster_wizard", "monster_hover", "hover"},
    {"monster_shambler", "monster_tank_commander", "tank commander"},
};

// The other game's monsters each game's rules can run whole, by the played game's profile id: the
// classic campaign runs Quake II's monsters of the table (rules/campaign_q2_kinds.cpp), the
// Quake II's rules the classic game's (rows of server/q2_monster_kinds.hpp), each by its own
// game's numbers, models and sounds.
inline constexpr std::string_view kClassicRunsQ2[] = {
    "monster_soldier_light", "monster_soldier",       "monster_soldier_ss", "monster_infantry",
    "monster_gunner",        "monster_berserk",       "monster_gladiator",  "monster_flyer",
    "monster_parasite",      "monster_mutant",        "monster_chick",      "monster_tank",
    "monster_flipper",       "monster_floater",       "monster_medic",      "monster_brain",
    "monster_hover",         "monster_tank_commander"};
inline constexpr std::string_view kQ2RunsClassic[] = {
    "monster_army",   "monster_dog",           "monster_enforcer",    "monster_knight",
    "monster_ogre",   "monster_ogre_marksman", "monster_hell_knight", "monster_wizard",
    "monster_zombie", "monster_demon1",        "monster_shalrath",    "monster_shambler",
    "monster_fish",   "monster_tarbaby"};
struct RunsMonsters {
  std::string_view played;
  std::span<const std::string_view> classnames;
};
inline constexpr RunsMonsters kRunsMonsters[] = {
    {"classic", kClassicRunsQ2},
    {"q2", kQ2RunsClassic},
};

namespace remix_monsters_detail {
constexpr int column(std::string_view game) {
  for (size_t i = 0; i < kMonsterGames.size(); i++)
    if (kMonsterGames[i] == game) return static_cast<int>(i);
  return -1;
}
}  // namespace remix_monsters_detail

// Whether `classname` is a monster of the table (either game's).
constexpr bool is_matched_monster(std::string_view classname) {
  for (const MonsterMatch& m : kMonsterMatches)
    if (m.classic == classname || m.q2 == classname) return true;
  return false;
}

// The monster `classname` (either game's) as game `game`'s: that game's by the first row whose
// other column names it, else `classname`.
constexpr std::string_view monster_as(std::string_view classname, std::string_view game) {
  const int to = remix_monsters_detail::column(game);
  if (to < 0) return classname;
  for (const MonsterMatch& m : kMonsterMatches) {
    const std::string_view cols[2] = {m.classic, m.q2};
    for (int c = 0; c < 2; c++)
      if (c != to && cols[c] == classname) return cols[to];
  }
  return classname;
}

// Whether `played`'s rules run the monster `classname`: one of its own (its column), or one of
// the other game's it has learnt (`kRunsMonsters`).
constexpr bool runs_monster(std::string_view played, std::string_view classname) {
  const int mine = remix_monsters_detail::column(played);
  for (const MonsterMatch& m : kMonsterMatches)
    if ((mine == 0 && m.classic == classname) || (mine == 1 && m.q2 == classname)) return true;
  for (const RunsMonsters& r : kRunsMonsters)
    if (r.played == played)
      for (const std::string_view c : r.classnames)
        if (c == classname) return true;
  return false;
}

}  // namespace shooters_edge::profiles
