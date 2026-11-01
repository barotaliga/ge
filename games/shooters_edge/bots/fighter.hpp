// THE FIGHTER: a bot (plugins/shooter/bot.hpp) that plays the baseline deathmatch mod
// (games/shooters_edge/qc): it chooses where to go, what to hold and when to shoot, on `ai/choose`,
// from what a player's client has and nothing else.
//
// WHAT IT KNOWS:
// - its own stats, as the server sends a client its own (health, armour and its share, the items
//   carried, the weapon held, the four ammunitions: `read_stats`);
// - the map's items, off the map's entity text (a client loads the map file, and its entities with
//   it): each one's classname, place and spawnflags, matched at the start to the entity the server
//   sends for it;
// - whether each item is there: an item taken is no longer sent (its model is gone), and it is back
//   its respawn time later (health, armour and ammunition 20 s, a weapon 30 s), so an item seen to
//   go is timed;
// - the others through its senses: sight in its view's cone, hearing, memory (senses.hpp).
//
// WHAT IT CHOOSES, every `kThink_s` (each a decision on `ai::Why`):
// - its MODE (`ai::enter`): `roam` (no one known: go for the item worth most), `fight` (a contact
//   in sight: keep to the item route and shoot), `chase` (lost from sight a moment ago: go where it
//   was last seen), `retreat` (weak with someone about: go for the nearest health or armour away
//   from them, and back to fighting once strong again);
// - its GOAL (`ai::pick_best`): every item scored by what it gives this player now (health it
//   lacks, protection over what it wears, a weapon it does not carry, ammunition it is short of)
//   over the time to have it (the run there at full speed, or the wait for its return, whichever is
//   longer);
// - its WEAPON (`ai::pick_best`): every weapon carried with the ammunition to fire, scored by the
//   range to the target (the double scattergun close, the rockets past their own blast, the arc
//   within its beam), sent as the weapon's impulse;
// and every command, whether to FIRE: a contact in sight, the view within the weapon's tolerance
// of the aim (the target's width at its range, a blast's wider), and in range. A rocket and a
// grenade are aimed at the feet, a projectile led by its flight time. Dead, it presses attack to
// come back once the mod allows.
//
// Deterministic: a pure function of what it is sent and its hands' seeds.
//
// ge:summary The fighter: a bot on the baseline mod choosing its mode, goal, weapon and trigger
// ge:keywords fighter deathmatch mode roam fight chase retreat item-timing respawn weapon-choice
// ge:keywords fire trigger tolerance stats items pick_best enter baseline-mod
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ai/path.hpp"
#include "shooter/bot.hpp"
#include "shooter/swim.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::bots {

namespace sh = ge::shooter;

constexpr double kThink_s = 0.1;  // how often the mode, the goal and the weapon are chosen

// What a client is sent of its own player.
struct Stats {
  float health = 0, armor = 0, armortype = 0;
  int32_t items = 0;   // the W_ and IT_ bits
  int32_t weapon = 0;  // the W_ bit held
  float shells = 0, nails = 0, rockets = 0, cells = 0;
};
Stats read_stats(const server::QcServer& s, int ent);

enum class ItemKind : uint8_t {
  health,
  mega,
  armor1,
  armor2,
  armor3,
  shells,
  nails,
  rockets,
  cells,
  weapon,
  quad,
  pent,  // the shield (no damage taken)
};

// How well a player knows the map (knowledge.hpp): where its items are and when they come back.
enum class MapSense : uint8_t { novice, average, expert };

struct ItemSpot {
  std::string name;  // its classname (a health box by its size)
  // In a pocket only the water reaches (no route leads in): dived for from `dive_node` on the
  // field that leads through the water to the pocket (`dive`); none when no route is needed.
  const ge::shooter::SwimField* dive = nullptr;
  // What this player believes of it (knowledge.hpp), not what the server holds.
  double back_at = 0;       // when it is expected back (while believed gone)
  double last_seen = -1e9;  // when it was last seen there
  sh::Vec3f at;             // where it stands (the middle of its box on the floor)
  // Behind a door a button opens (the map's text: a button whose target is the door's name): the
  // button touched first while the door stands shut.
  sh::Vec3f door_rest;   // the door's origin while shut
  sh::Vec3f button_at;   // the button's middle
  int ent = -1;          // the server's entity for it, -1 when none matched
  int32_t weapon = 0;    // a weapon's W_ bit
  float amount = 0;      // health given, or ammunition (doubled by spawnflags 1)
  float respawn_s = 20;  // gone this long once taken
  int32_t node = -1;     // the nav node nearest it
  int door_ent = -1;     // the door's entity (-1: none)
  int32_t button_node = -1;
  int button_ent = -1;                      // the button's entity
  int32_t dive_node = -1, dive_entry = -1;  // and the node in the water it drops or steps to
  float dive_s = 0;                         // the swim's time, s (its steps at a swimmer's pace)
  int32_t sightings = 0;                    // separate times it was seen (kSightingGap_s apart)
  ItemKind kind = ItemKind::health;
  bool button_shoot = false;  // opened by a shot (it has health), not a touch
  bool present = true;        // believed there
  bool known = true;          // where it is, recalled from anywhere (a novice learns it by sight)
  bool timed = false;         // `back_at` is its real time back (seen taken by one who times)
  bool confirmed = true;      // seen there since it was last gone (or the match's start)
  bool in_sight = false;      // its spot in sight at the last think
  bool watched = false;       // and the item there with it
};

enum class FightMode : uint8_t { roam, fight, chase, retreat };
const char* fight_mode_name(FightMode m);

// A fighter's part in its team's plan (team.hpp); `free` when it plays alone.
enum class Role : uint8_t { free, hold, rush, carry };
const char* role_name(Role r);
struct Team;

struct FighterStats {
  int64_t commands = 0, shots = 0, switches = 0, pickups = 0, respawns = 0;
  int64_t taken = 0;                // items seen to go (by anyone)
  int64_t modes = 0;                // mode changes
  int64_t mode_commands[4] = {};    // commands spent in each mode
  int64_t weapon_commands[9] = {};  // commands holding each weapon, by its number (1..8)
  int64_t packs = 0;                // backpacks gone for
  int64_t held_fire = 0;            // shots held for a teammate in the line of fire
};

struct Fighter {
  // Ordered by size, largest first, so the struct packs tight.
  sh::Bot bot;
  std::vector<ItemSpot> items;
  Mind mind;             // what it believes of its team, its claim, its calls (mind.hpp)
  std::string name;      // its player name, as its teammates see it in their team lines
  ge::ai::Search reach;  // every node it can get to from where it stood at `reach_at`
  FighterStats stats;
  // Its team messages (team.hpp's `team_say`), sent as `say_team` by whoever drives its client.
  std::vector<std::string> outbox;
  double held = 0;          // in the mode this long
  double next_think = 0;    // server time of the next choice
  double impulse_at = -1;   // when the last impulse went
  double reach_at = -1;     // when `reach` was worked out (-1: never)
  double said_at = -1e9;    // its last team message
  double called_at = -1e9;  // its last enemy callout
  double under_since = -1;  // when its head last went under the water (-1: not under)
  ge::ai::Why* why = nullptr;
  Team* team = nullptr;                          // its side (team.hpp); null in a free-for-all
  const ge::shooter::Locs* locs = nullptr;       // the map's place names (none: no names)
  const ge::shooter::SwimField* swim = nullptr;  // the map's way out of its water (none: no water)
  int64_t buttons = 0;                           // buttons gone to
  int64_t dives = 0;                             // dives begun
  int64_t lost = 0;                              // thinks spent off the graph
  int32_t goal_item = -1;
  int32_t goal_node = -1;
  int32_t want_weapon = 0;  // the number (1..8) asked for; 0 none pending
  int slot = -1;
  int32_t pack_ent = -1;      // a backpack gone for
  int32_t lost_to = -1;       // off the graph or in a pool it cannot leave by its links: the node
                              // walked (or swum) straight at to get back (-1: none)
  int32_t reach_n = 0;        // how many nodes `reach` holds
  int32_t diving = -1;        // the item it dives for (in the water on `dive`), -1 none
  int32_t last_dive = -1;     // the item the last think dived for
  int32_t explore_node = -1;  // a novice with nothing known walks the map to learn it
  uint32_t rng = 1;           // its own draws (where it explores), seeded off the bot's seed
  FightMode mode = FightMode::roam;
  Role role = Role::free;
  MapSense sense = MapSense::expert;  // how well it knows the map (off its skill)
  bool was_dead = false;
  bool unlocking = false;  // on the way to the goal item's button
  bool swimming = false;   // in water no route leaves: following the swim field
};

// The fighter's strength: health and armour's protection, a launcher counted on top.
float fighter_strength(const Stats& st);

// A fighter on graph `g` with `skill`'s aim, the map's items off `entities` (the map's text)
// matched to what the server `s` sends client `slot`'s player now. Call after the slot connects.
void fighter_init(Fighter& f, const sh::NavGraph& g, const server::QcServer& s, int slot,
                  const char* entities, sh::AimSkill skill, double hz, uint32_t seed);

// The fighter's next command, `msec` long, on server `s` as it stands after the last frame.
sh::MoveCmd fighter_command(Fighter& f, const server::QcServer& s, int slot, int msec);

}  // namespace shooters_edge::bots
