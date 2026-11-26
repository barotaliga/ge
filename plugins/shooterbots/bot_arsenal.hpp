// A GAME'S WEAPONS AS A BOT SEES THEM: one row a weapon (the bit the server's numbers carry, how it
// is asked for, the ammunition it spends, how it is aimed and fired, how good it is at a range,
// what it is worth to pick up) and the game's own few facts round them (how many ammunitions, which
// item gives which, the key weapons a bot plans round: the launcher it rocket-jumps with, the lob,
// the beam). The fighter (fighter.hpp) reads a game's arsenal and names no weapon itself, so the
// first game's bots and Quake II's are one bot over two tables (M18v8b).
//
// - `first_arsenal()`: Quake's eight weapons and four ammunitions (shells, nails, rockets,
//   cells), held bit-exact to the fighter's play before the seam (the bots' lab digests).
// - `q2_arsenal()`: Quake II's ten weapons and six ammunitions, off its rules' own weapon
//   table (server/q2_combat.hpp); a weapon's bit is 1 << its index there, and the impulse that
//   asks for it is the index + 1, as Quake II's rules take it.
//
// ge:summary A game's weapons as the bots see them: rows, ammunitions, key weapons; both games
// ge:keywords bot-arsenal weapon-row ammunition launcher lob beam weapon-curve q2-arsenal
// ge:keywords first-arsenal impulse stats-reader item-ammo
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_q2_nav_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "shooter/rules_world.hpp"
#include "shooterbots/item_tables.hpp"
#include "shooterbots/map_book.hpp"
#include "shooterbots/team_talk.hpp"

namespace ge::shooterbots {

inline constexpr int kMaxAmmo = 12;  // the most ammunitions a game's arsenal has

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

// The highest number a weapon of any game's arsenal answers to (Quake II's ten run 1..10): a
// table by weapon number has kMostWeaponNumber + 1 rows.
constexpr int kMostWeaponNumber = 10;

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
  // The aim trait it is held by (0 the close guns, 1 the spikers, 2 the lob, 3 the launcher, 4 the
  // beam: the fighter's `aim_slot`); -1 by its number, as Quake's eight are (1..3, 4..5, 6, 7, 8).
  int aim_slot = -1;
};

struct BotArsenal;

// What a client is sent of its own player, and the arsenal that read it (`read_stats`).
struct Stats {
  const BotArsenal* arsenal = nullptr;
  float health = 0, armor = 0, armortype = 0;
  int32_t items = 0;          // the weapon bits carried, and Quake's IT_ bits
  int32_t weapon = 0;         // the weapon bit held
  float ammo[kMaxAmmo] = {};  // by the arsenal's ammunition index
};

// A weapon's fire as a listener tells it: the sample, the weapon's number (1..), and how long after
// the sound the weapon is surely still in hand (its refire; a stream's end after its last sound).
struct BotFireSound {
  std::string_view sample;
  int32_t weapon = 0;
  double held_s = 0;
};

struct BotArsenal {
  std::span<const BotWeapon> weapons;
  int ammo_count = 4;
  // The ammunition an item of `kind` gives (an index of `Stats::ammo`), or -1: not ammunition.
  int (*item_ammo)(ge::shooterbots::ItemKind kind) = nullptr;
  // The key weapons: the launcher a bot rocket-jumps and climbs out of a pit with, the lob it
  // grenade-jumps with, the beam; 0 when the game has none.
  int32_t launcher = 0, lob = 0, beam = 0;
  int32_t heavy = 0;              // the heavy spray (a map's lesser key weapon)
  int launcher_ammo = -1;         // the launcher's ammunition (stocked before the launcher is had)
  float ammo_max[kMaxAmmo] = {};  // the most of each a player carries (a backpack is no use then)
  // The map's items by class name (shooterbots/item_tables.hpp): what a bot learns of a map's
  // entities.
  const ge::shooterbots::ItemTables* tables = nullptr;
  // The words and samples a bot hears and says (the call and heard words, the sounds' meanings).
  const ge::shooterbots::ItemTables* heard = nullptr;
  // The fire sounds it tells an enemy's weapon by (the packs it guesses, shooterbots/packs.hpp).
  std::span<const BotFireSound> fire_sounds;
  // An ammunition below this is short (a backpack worth going for).
  float short_of[kMaxAmmo] = {};
  // The game's numbers for a player read into stats.
  void (*read)(const ge::shooter::PlayerNumbers& n, Stats& st) = nullptr;
  // What a model a client is sent is to a companion (shooterbots/companion.hpp): 0 no monster, 1 a
  // monster any weapon puts down, 2 one only a blast keeps down, 3 one no weapon kills.
  int (*monster)(std::string_view model) = nullptr;
  // How a player keeps to a monster of a model in a fight (none: as to a player): the distance he
  // backs off to while it is nearer (one that bites or leaps: a dog, a fiend, a knight), units.
  float (*keep_off)(std::string_view model) = nullptr;
  // Whether a monster of a model is put down for good only by a blast (Quake's zombie: anything
  // less knocks it down to get up again); none: every monster falls to any weapon.
  bool (*blast_only)(std::string_view model) = nullptr;
  // Whether a thing lying on the map of a model is a weapon that blasts (Quake's grenade and
  // rocket launchers): a campaign with blast-only monsters fetches one first; none: never fetched.
  bool (*blast_pickup)(std::string_view model) = nullptr;
  // The game's maps as its bots' book: people's tricks, routes and item rules by map
  // (shooterbots/map_book.hpp; none: no map has any).
  const ge::shooterbots::MapBook* maps = nullptr;
  // The game's team words (shooterbots/team_talk.hpp): a team message written and read.
  const ge::shooterbots::TeamWords* talk = nullptr;
};

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
// Carrying a weapon that blasts (the launcher or the lob) with its ammunition.
bool can_blast(const Stats& st);
// The impulse that asks for the launcher (0 when the game has none).
int launcher_number(const BotArsenal& a);

}  // namespace ge::shooterbots
