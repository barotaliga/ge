// A GAME'S WEAPONS AS A BOT SEES THEM: one row a weapon (the bit the server's numbers carry, how it
// is asked for, the ammunition it spends, how it is aimed and fired, how good it is at a range,
// what it is worth to pick up) and the game's own few facts round them (how many ammunitions, which
// item gives which, the key weapons a bot plans round: the launcher it rocket-jumps with, the lob,
// the beam). The fighter (fighter.hpp) reads a game's arsenal and names no weapon itself, so the
// first game's bots and the sequel's are one bot over two tables (M18v8b).
//
// - `first_arsenal()`: the first game's eight weapons and four ammunitions (shells, nails, rockets,
//   cells), held bit-exact to the fighter's play before the seam (the bots' lab digests).
// - `sequel_arsenal()`: the sequel's ten weapons and six ammunitions, off its rules' own weapon
//   table (server/sequel_combat.hpp); a weapon's bit is 1 << its index there, and the impulse that
//   asks for it is the index + 1, as the sequel's rules take it.
//
// ge:summary A game's weapons as the bots see them: rows, ammunitions, key weapons; both games
// ge:keywords bot-arsenal weapon-row ammunition launcher lob beam weapon-curve sequel-arsenal
// ge:keywords first-arsenal impulse stats-reader item-ammo
// ge:skill bots shooter
// ge:test tests/shooters_edge_sequel_nav_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "shooters_edge/profiles/items.hpp"
#include "shooters_edge/server/rules_world.hpp"

namespace shooters_edge::bots {

inline constexpr int kMaxAmmo = 8;  // the most ammunitions a game's arsenal has

// The part a weapon plays in a bot's plans (the map's key items, the team's talk, the jumps): the
// launcher, the lob, the beam, the heavy spray; `none` for the rest.
enum class WeaponRole : uint8_t { none, launcher, lob, beam, heavy };

// How a weapon's worth at a range is read (`weapon_score`): its shape of curve.
enum class WeaponCurve : uint8_t {
  blade,      // only at arm's length
  scatter,    // flat, weak
  double_,    // close, its spread wider than a man past a few hundred units
  spiker,     // flat
  heavy,      // better near
  lob,        // a bouncing blast at a room's range (its own splash too near)
  rocket,     // a blast past its own splash
  beam,       // near and middle
  hitscan,    // a gun that is as good at any range (the machine guns)
  rail,       // a slug: best far, still good near
  big_blast,  // a huge blast: only far from itself
};

struct BotWeapon {
  int32_t bit = 0;  // the bit the numbers carry it as (held and carried)
  int number = 0;   // the impulse that asks for it
  const char* name = "";
  int ammo = -1;          // the ammunition it spends (an index of `Stats::ammo`); -1 none
  float cost = 0;         // a shot's
  float shot_speed = 0;   // 0 a hitscan
  float aim_dz = 0;       // aimed over the middle
  float width = 24;       // the target's half width the trigger allows at range, units
  float range = 1000;     // fired within this, units
  float refire_s = 0.5f;  // from one shot to the next, s
  bool stream = false;    // fired as a stream: the finger holds it down
  bool blast = false;     // its shot blasts (kept off a companion's friends)
  WeaponCurve curve = WeaponCurve::scatter;
  float worth = 30;  // picked up when not carried
  float arms = 1;    // how armed it makes its holder feel, loaded (temper.hpp)
};

struct BotArsenal;
const BotArsenal& first_arsenal();

// What a client is sent of its own player, and the arsenal that read it (`read_stats`).
struct Stats {
  const BotArsenal* arsenal = &first_arsenal();
  float health = 0, armor = 0, armortype = 0;
  int32_t items = 0;          // the weapon bits carried, and the first game's IT_ bits
  int32_t weapon = 0;         // the weapon bit held
  float ammo[kMaxAmmo] = {};  // by the arsenal's ammunition index
};

struct BotArsenal {
  std::span<const BotWeapon> weapons;
  int ammo_count = 4;
  // The ammunition an item of `kind` gives (an index of `Stats::ammo`), or -1: not ammunition.
  int (*item_ammo)(shooters_edge::profiles::ItemKind kind) = nullptr;
  // The key weapons: the launcher a bot rocket-jumps and climbs out of a pit with, the lob it
  // grenade-jumps with, the beam; 0 when the game has none.
  int32_t launcher = 0, lob = 0, beam = 0;
  int32_t heavy = 0;              // the heavy spray (a map's lesser key weapon)
  int launcher_ammo = -1;         // the launcher's ammunition (stocked before the launcher is had)
  float ammo_max[kMaxAmmo] = {};  // the most of each a player carries (a backpack is no use then)
  // The map's items by class name (profiles/items.hpp): what a bot learns of a map's entities.
  const shooters_edge::profiles::ItemTables* tables = nullptr;
  // An ammunition below this is short (a backpack worth going for).
  float short_of[kMaxAmmo] = {};
  // The game's numbers for a player read into stats.
  void (*read)(const server::PlayerNumbers& n, Stats& st) = nullptr;
  // What a model a client is sent is to a companion (bots/companion.hpp): 0 no monster, 1 a
  // monster any weapon puts down, 2 one only a blast keeps down, 3 one no weapon kills.
  int (*monster)(std::string_view model) = nullptr;
};

const BotArsenal& first_arsenal();
const BotArsenal& sequel_arsenal();

// The part weapon bit `bit` plays.
WeaponRole role_of(const BotArsenal& a, int32_t bit);
// The row of weapon bit `bit`, or null.
const BotWeapon* weapon_row(const BotArsenal& a, int32_t bit);
// A weapon's ammunition in hand (1 for one that spends none).
float ammo_of(const Stats& st, int ammo);
// Carried and with a shot's ammunition.
bool loaded(const Stats& st, const BotWeapon& w);
// How good the weapon is at range `d` (higher better).
double weapon_score(const BotWeapon& w, float d);
// The launcher's ammunition in hand (0 when the game has no launcher).
float launcher_rounds(const Stats& st);
// The launcher held.
bool holds_launcher(const Stats& st);
// Carrying the launcher.
bool has_launcher(const Stats& st);
// The impulse that asks for the launcher (0 when the game has none).
int launcher_number(const BotArsenal& a);

}  // namespace shooters_edge::bots
