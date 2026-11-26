// CLASSIC REMIX'S SOUNDS (`cr_sounds`, docs/goals/M18u.md): the sounds a match is heard with,
// chosen apart from its items. Each row pairs a multiplayer sound of Quake with the
// Quake II's that plays the same part: a weapon's shot, a blast, a bounce, a ricochet, a pickup, an
// item coming back, a teleport, a player's jump, landing, pain, death, gibbing and splash. The
// campaign's sounds (monsters, machines, voices of the story) are left out: they keep their own.
//
// - **Heard as one game** (`heard_as`): a sound named by either game is heard as the chosen game's
//   by its row: the first row whose other column names it gives the chosen game's name. A name
//   no row knows is heard as it is.
// - **Silence**: an empty name in a column is a sound that game does not make (Quake
//   has no footsteps), so hearing Quake II as Quake leaves them out.
// - **Many to one**: Quake II's machinegun picks among five shots, its hand grenade bounces two
//   ways; the extra rows follow the first, so going back the first row's name is taken.
// - **Names the games share** (`items/damage.wav`, `misc/talk.wav`, `player/gasp1.wav`) are not
//   rows: the name is the same, and the chosen game's own copy of it is read, not the played
//   game's over it (client/demo_sound.hpp's `hear_as`).
// - **A player's own sounds** (Quake II's `*jump1.wav`, kept per figure): their row names the
//   base voice, `player/male/<file>`, which a `*` name is looked up as.
//
// The client hears every sound by this table while classic remix is on (client/remix.hpp's
// `heard_game`), but a weapon's own sounds (`SoundMatch::weapon`: its shots, blasts, bounces and
// hits) are never swapped: a weapon `cr_items` swaps in is its game's whole, its sounds too, and
// a weapon of the played game keeps its own.
//
// ge:summary Classic remix's sounds: Quake's and Quake II's multiplayer sounds paired
// ge:keywords remix classic-remix cr_sounds sound-swap heard_as sound-match multiplayer-sounds
// ge:keywords weapon-sound pickup-sound pain-sound footsteps remix_sounds
// ge:skill shooter audio
// ge:test tests/shooters_edge_remix_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace shooters_edge::profiles {

// The games the table's columns are, by content profile id.
inline constexpr std::array<std::string_view, 2> kSoundGames = {"q1", "q2"};

struct SoundMatch {
  std::string_view classic;  // "": Quake makes no such sound
  std::string_view q2;
  std::string_view what;  // the part both play, for the lab and the docs
  // A weapon's own sound (its shot, its blast, its bounce, its hit): it stays the weapon's,
  // whatever `cr_sounds` names, since a weapon swapped in fires as its own game's whole.
  bool weapon = false;
};

inline constexpr SoundMatch kSoundMatches[] = {
    // The weapons' shots, slot by slot as the arsenals pair them (remix_arsenal.hpp).
    {"weapons/ax1.wav", "weapons/blastf1a.wav", "first slot's swing or shot", true},
    {"weapons/guncock.wav", "weapons/shotgf1b.wav", "shotgun", true},
    {"weapons/shotgn2.wav", "weapons/sshotf1b.wav", "double-barrelled shotgun", true},
    {"weapons/rocket1i.wav", "weapons/machgf1b.wav", "nailgun or machinegun", true},
    {"weapons/spike2.wav", "weapons/machgf2b.wav", "super nailgun or chaingun", true},
    {"weapons/grenade.wav", "weapons/grenlf1a.wav", "grenade launcher", true},
    {"weapons/sgun1.wav", "weapons/rocklf1a.wav", "rocket launcher", true},
    {"weapons/lhit.wav", "weapons/railgf1a.wav", "lightning gun or railgun", true},
    {"weapons/lstart.wav", "weapons/railgf1a.wav", "lightning gun's start", true},
    {"weapons/lhit.wav", "weapons/hyprbf1a.wav", "hyperblaster", true},
    {"weapons/lhit.wav", "weapons/bfg__f1y.wav", "BFG", true},
    {"weapons/grenade.wav", "weapons/hgrent1a.wav", "hand grenade's throw", true},
    {"weapons/rocket1i.wav", "weapons/machgf3b.wav", "machinegun, third shot", true},
    {"weapons/rocket1i.wav", "weapons/machgf4b.wav", "machinegun, fourth shot", true},
    {"weapons/rocket1i.wav", "weapons/machgf5b.wav", "machinegun, fifth shot", true},
    // What the shots do.
    {"weapons/r_exp3.wav", "weapons/rocklx1a.wav", "rocket's blast", true},
    {"weapons/r_exp3.wav", "weapons/grenlx1a.wav", "grenade's blast", true},
    {"weapons/bounce.wav", "weapons/grenlb1b.wav", "grenade's bounce", true},
    {"weapons/bounce.wav", "weapons/hgrenb1a.wav", "hand grenade's bounce", true},
    {"weapons/bounce.wav", "weapons/hgrenb2a.wav", "hand grenade's second bounce", true},
    {"weapons/ric1.wav", "world/ric1.wav", "ricochet", true},
    {"weapons/ric2.wav", "world/ric2.wav", "ricochet", true},
    {"weapons/ric3.wav", "world/ric3.wav", "ricochet", true},
    {"weapons/tink1.wav", "world/ric1.wav", "nail on a wall", true},
    // The items.
    {"weapons/pkup.wav", "misc/w_pkup.wav", "weapon picked up"},
    {"weapons/lock4.wav", "misc/am_pkup.wav", "ammunition picked up"},
    {"items/armor1.wav", "misc/ar1_pkup.wav", "armour picked up"},
    {"items/armor1.wav", "misc/ar2_pkup.wav", "heavy armour picked up"},
    {"items/armor1.wav", "misc/ar3_pkup.wav", "armour shard picked up"},
    {"items/r_item1.wav", "items/n_health.wav", "small health"},
    {"items/r_item1.wav", "items/s_health.wav", "stimpack"},
    {"items/health1.wav", "items/l_health.wav", "large health"},
    {"items/r_item2.wav", "items/m_health.wav", "mega health"},
    {"items/itembk2.wav", "items/respawn1.wav", "an item comes back"},
    {"misc/r_tele1.wav", "misc/tele1.wav", "teleport"},
    {"misc/r_tele2.wav", "misc/tele1.wav", "teleport"},
    {"misc/r_tele3.wav", "misc/tele1.wav", "teleport"},
    {"misc/r_tele4.wav", "misc/tele1.wav", "teleport"},
    {"misc/r_tele5.wav", "misc/tele1.wav", "teleport"},
    // The players.
    {"player/plyrjmp8.wav", "player/male/jump1.wav", "jump"},
    {"player/land.wav", "player/land1.wav", "landing"},
    {"player/land2.wav", "player/male/fall1.wav", "hard landing"},
    {"player/land2.wav", "player/male/fall2.wav", "landing that hurts"},
    {"", "player/step1.wav", "footstep"},
    {"", "player/step2.wav", "footstep"},
    {"", "player/step3.wav", "footstep"},
    {"", "player/step4.wav", "footstep"},
    {"player/pain1.wav", "player/male/pain25_1.wav", "pain"},
    {"player/pain2.wav", "player/male/pain25_2.wav", "pain"},
    {"player/pain3.wav", "player/male/pain50_1.wav", "pain"},
    {"player/pain4.wav", "player/male/pain50_2.wav", "pain"},
    {"player/pain5.wav", "player/male/pain75_1.wav", "pain"},
    {"player/pain6.wav", "player/male/pain75_2.wav", "pain"},
    {"player/pain5.wav", "player/male/pain100_1.wav", "pain at low health"},
    {"player/pain6.wav", "player/male/pain100_2.wav", "pain at low health"},
    {"player/death1.wav", "player/male/death1.wav", "death"},
    {"player/death2.wav", "player/male/death2.wav", "death"},
    {"player/death3.wav", "player/male/death3.wav", "death"},
    {"player/death4.wav", "player/male/death4.wav", "death"},
    {"player/death5.wav", "player/male/death4.wav", "death"},
    {"player/udeath.wav", "misc/udeath.wav", "gibbed"},
    {"player/gib.wav", "misc/udeath.wav", "gibbed"},
    {"player/drown1.wav", "player/male/gurp1.wav", "choking under water"},
    {"player/drown2.wav", "player/male/gurp2.wav", "choking under water"},
    {"player/h2odeath.wav", "player/male/drown1.wav", "drowned"},
    {"player/teledth1.wav", "misc/udeath.wav", "telefragged"},
    {"player/inh2o.wav", "player/watr_in.wav", "into the water"},
    {"player/h2ojump.wav", "player/watr_in.wav", "jumped into the water"},
    {"misc/outwater.wav", "player/watr_out.wav", "out of the water"},
    {"", "player/watr_un.wav", "head under the water"},
    {"misc/water1.wav", "player/wade1.wav", "swimming"},
    {"misc/water2.wav", "player/wade2.wav", "swimming"},
    {"misc/water1.wav", "player/wade3.wav", "swimming"},
    {"player/inlava.wav", "player/lava_in.wav", "into lava"},
    {"player/lburn1.wav", "player/lava1.wav", "burnt by lava"},
    {"player/lburn2.wav", "player/lava2.wav", "burnt by lava"},
    {"player/slimbrn2.wav", "player/burn1.wav", "burnt by slime"},
};

namespace remix_sounds_detail {
constexpr bool same_name(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    const char x = a[i] >= 'A' && a[i] <= 'Z' ? char(a[i] + 32) : a[i];
    const char y = b[i] >= 'A' && b[i] <= 'Z' ? char(b[i] + 32) : b[i];
    if (x != y) return false;
  }
  return true;
}
}  // namespace remix_sounds_detail

// Whether `name` is a weapon's own sound (a weapon row's, either game's, or one under `weapons/`):
// it is heard as its weapon's game names it, never as another's copy.
constexpr bool weapon_sound(std::string_view name) {
  if (name.size() >= 8 && remix_sounds_detail::same_name(name.substr(0, 8), "weapons/"))
    return true;
  for (const SoundMatch& m : kSoundMatches)
    if (m.weapon && (remix_sounds_detail::same_name(m.classic, name) ||
                     remix_sounds_detail::same_name(m.q2, name)))
      return true;
  return false;
}

// The column of game `game` in the table, or -1 when the table has none.
constexpr int sound_column(std::string_view game) {
  for (size_t i = 0; i < kSoundGames.size(); i++)
    if (kSoundGames[i] == game) return static_cast<int>(i);
  return -1;
}

// The sound `name` (as either game names it, any case) heard as `game`'s: that game's name by the
// first row whose other column names it, "" when that game makes no such sound, else `name`. A
// weapon's own sound is heard as it is named: the weapon that fired it is its game's whole.
constexpr std::string_view heard_as(std::string_view name, std::string_view game) {
  const int to = sound_column(game);
  if (to < 0 || name.empty()) return name;
  for (const SoundMatch& m : kSoundMatches) {
    if (m.weapon) continue;
    const std::string_view cols[2] = {m.classic, m.q2};
    for (int c = 0; c < 2; c++) {
      if (c == to || cols[c].empty()) continue;
      if (remix_sounds_detail::same_name(cols[c], name)) return cols[to];
    }
  }
  return name;
}

}  // namespace shooters_edge::profiles
