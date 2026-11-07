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
// ge:keywords fire trigger tolerance stats items pick_best enter baseline-mod danger danger-spots
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ai/path.hpp"
#include "core/prng.hpp"
#include "shooter/bot.hpp"
#include "shooter/dodge.hpp"
#include "shooter/hearing.hpp"
#include "shooter/swim.hpp"
#include "shooter/temper.hpp"
#include "shooter/traits.hpp"
#include "shooters_edge/bots/companion.hpp"
#include "shooters_edge/bots/mind.hpp"
#include "shooters_edge/profiles/items.hpp"
#include "shooters_edge/server/rules_runner.hpp"

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
Stats read_stats(const server::RulesCommon& s, int ent);

// What an item is, the game profile's vocabulary (profiles/items.hpp).
using ItemKind = shooters_edge::profiles::ItemKind;

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
  float weight = 1;      // how much it matters on this map, as the player rates it (knowledge.hpp)
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

// A button on the map (buttons.hpp).
struct ButtonSpot {
  sh::Vec3f at;                // its middle
  std::vector<int> doors;      // the door entities it opens
  std::vector<int32_t> opens;  // the items behind them (indices into the fighter's items)
  int ent = -1;
  int32_t node = -1;   // where it is pressed (or shot) from
  bool shoot = false;  // opened by a shot (health over 0), not a touch
};

// A map's exit (`trigger_changelevel`), as a player who keeps off it knows it: its box, the nodes
// where a standing player's box would touch it, and a node beside it to stand on and show it.
struct ExitSpot {
  sh::Vec3f lo, hi;             // its box
  std::vector<int32_t> inside;  // the nodes whose player box touches it (grown by a margin)
  int32_t stand = -1;           // the nearest node clear of it, or -1
};

enum class FightMode : uint8_t { roam, fight, chase, retreat };
const char* fight_mode_name(FightMode m);

// A fighter's part in its team's plan (team.hpp); `free` when it plays alone.
enum class Role : uint8_t { free, hold, rush, carry, push };
constexpr int kRoles = 5;
const char* role_name(Role r);
struct Team;

struct FighterStats {
  int64_t commands = 0, shots = 0, switches = 0, pickups = 0, respawns = 0;
  int64_t goal_changes = 0;    // its item goal changed (the commitment's measure)
  int64_t surfaced = 0;        // times up for air (water.hpp)
  int64_t surface_failed = 0;  // and rises given up short of the surface
  // The trigger, with one in sight aimed at: looked at, out of the weapon's range, the view off.
  int64_t trigger_checks = 0, trigger_far = 0, trigger_off = 0;
  int64_t taken = 0;                // items seen to go (by anyone)
  int64_t modes = 0;                // mode changes
  int64_t mode_commands[4] = {};    // commands spent in each mode
  int64_t weapon_commands[9] = {};  // commands holding each weapon, by its number (1..8)
  int64_t packs = 0;                // backpacks gone for
  int64_t held_fire = 0;            // shots held for a teammate in the line of fire
  int64_t heard_taken = 0;          // items heard taken (ears.hpp)
  int64_t heard_spawns = 0;         // players heard spawning on a point it knows
  int64_t spam_shots = 0;           // rockets fired at a spot enemies come in by
  int64_t pokes = 0;                // buttons a novice tried on a whim
  int64_t dives_given_up = 0;       // dives turned back (late, or short of air)
  double dive_wet_s = 0;            // time in the water on a dive
  int64_t outmatched = 0;           // commands spent turned from a fight it believed it loses
  int64_t sound_chases = 0;         // commands spent going at an enemy it only heard
  int64_t dodging = 0;              // commands spent stepping across an enemy's line (dodge.hpp)
  int64_t dodge_turns = 0;          // and the times it turned the step back
  int64_t watching = 0;             // commands standing with the view on a way in (watch.hpp)
  int64_t landings = 0;             // commands walking onto an exit node just climbed out to
  double speed_sum = 0;             // its speed across the floor summed over its commands alive
  double mode_speed[sh::kPilotModes + 1] = {};  // and by the pilot's mode at the command
  int64_t mode_n[sh::kPilotModes + 1] = {};
};

// What a fighter believes of one enemy (temper.hpp): his body and arms as last heard.
struct FoeRead {
  float body = 100, arms = 1;
  double t = -1e9;           // when it was last heard of him
  double quad_until = -1e9;  // his quad heard taken, believed held until then
};

struct MapPlan;

struct Fighter {
  // Ordered by size, largest first, so the struct packs tight.
  sh::Bot bot;
  std::vector<ItemSpot> items;
  std::vector<sh::Vec3f> spawns;         // the map's spawn points (ears.hpp)
  std::vector<sh::HeardSound> heard;     // this frame's sounds taken in
  std::vector<sh::Vec3f> spam_spots;     // where enemies come in by (spam.hpp)
  std::vector<int32_t> spam_from;        // per spot, the node near its area it is fired from
  std::vector<FoeRead> foes;             // each enemy as believed, by entity (temper.hpp)
  std::vector<ButtonSpot> button_spots;  // the map's buttons (buttons.hpp)
  std::vector<ExitSpot> exits;           // the map's exits (buttons.hpp `exits_init`)
  // The doors only something else opens (a named door, a secret door), and where each rests:
  // shut to a route while at rest (buttons.hpp `exits_init`).
  // Where and when it should not heal lately: where it died, where a mate died, an area called lost
  // (mind.hpp `mind_danger`).
  std::vector<std::pair<sh::Vec3f, double>> danger;
  std::vector<int> locked;
  std::vector<sh::Vec3f> locked_rest;
  // The map's BRIDGES (a plate a button slides out over a pit: map_nav.hpp `NavBridge`), as the
  // fighter last saw each: one still where the map began (in) shuts every link onto its nodes
  // (the pilot's `shut_links`), so no route walks off into the pit under it.
  struct Bridge {
    int ent = -1;
    sh::Vec3f spawn;
    std::vector<int32_t> nodes;
    bool in = true;
  };
  std::vector<Bridge> bridges;
  bool bridges_seen = false;
  Mind mind;             // what it believes of its team, its claim, its calls (mind.hpp)
  Escort escort;         // a co-op companion's own state (companion.hpp; leader -1: none)
  std::string name;      // its player name, as its teammates see it in their team lines
  ge::ai::Search reach;  // every node it can get to from where it stood at `reach_at`
  FighterStats stats;
  sh::HearStats hear_stats;
  sh::HearSkill ears;  // what it takes in of the sounds it is sent (hearing.hpp)
  // Its team messages (team.hpp's `team_say`), sent as `say_team` by whoever drives its client.
  std::vector<std::string> outbox;
  double held = 0;             // in the mode this long
  double heard_t = -1;         // the server time whose sounds were last taken in
  double spam_fired = -1e9;    // its last rocket at a spot
  double poke_until = -1e9;    // a novice on a button until then (buttons.hpp)
  double next_poke = 0;        // and his next whim
  double next_think = 0;       // server time of the next choice
  double impulse_at = -1;      // when the last impulse went
  double reach_at = -1;        // when `reach` was worked out (-1: never)
  double said_at = -1e9;       // its last team message
  double called_at = -1e9;     // its last enemy callout
  double under_since = -1;     // when its head last went under the water (-1: not under)
  double dive_wet_since = -1;  // when the dive under way went into the water (-1: none)
  double breathe_until = -1;   // up for air (water.hpp): at the surface until then (-1: rising)
  double surface_by = 0;       // out of the water by then, or the rise is given up
  double surface_retry = 0;    // no rise for air before then
  double dive_off_until = -1;  // until when the dive given up (`dive_off`) is not tried again
  ge::ai::Why* why = nullptr;
  Team* team = nullptr;                     // its side (team.hpp); null in a free-for-all
  const ge::shooter::Locs* locs = nullptr;  // the map's place names (none: no names)
  const MapPlan* plan = nullptr;  // the map's team plan (map_plan.hpp; none: the items alone)
  // The map's traps (map_nav.hpp: nodes no way on foot leads back from), never a goal; null none.
  const std::vector<uint8_t>* trap = nullptr;
  const ge::shooter::SwimField* swim = nullptr;  // the map's way out of its water (none: no water)
  int64_t buttons = 0;                           // buttons gone to
  int64_t dives = 0;                             // dives begun
  int64_t lost = 0;                              // thinks spent off the graph
  int32_t goal_item = -1;
  int32_t goal_was = -1;    // the goal item held since `goal_since`
  int32_t tired_item = -1;  // an item gone for too long in vain, worth less until `tired_until`
  double goal_since = 0, tired_until = 0;
  int32_t spam_for = -1;  // the claim `spam_from` was found for (-1: none yet)
  sh::Vec3f news_spot;    // spam at news (spam.hpp): the way in fired at
  double news_t = -1e9;   // the news it is fired at for (when it came), and the rockets spent
  int32_t news_shots = 0;
  bool news_on = false;     // a way in turned to this command
  int32_t spam_node = -1;   // where it fires at a spot from now (-1: not out)
  int32_t spam_spot = -1;   // and at which
  int32_t poke = -1;        // the button a novice is on
  int32_t objective = -1;   // a goal set from outside (way_through.hpp): taken before any item
  int32_t mega_watch = -1;  // the mega it took, its timer waiting on its health (knowledge.hpp)
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
  int32_t dive_off = -1;      // the item whose dive it last gave up (-1: none)
  int32_t swim_exit = -1;     // the exit node it is climbing out onto (shooter/swim.hpp), -1 none
  double swim_exit_until = 0;
  int32_t landing = -1;  // the exit node it climbed out to, walked onto first (-1: none)
  double landing_until = 0;
  // THE COMMITMENT (fighter.cpp `think`): its item goal is chosen again only on news (a new
  // sighting, an item seen or heard taken, a spawn heard, a team line read, a blow taken, a
  // pickup, a respawn), when the goal's item is gone, or after `kDecide_s` with none.
  int64_t decided_news = -1;  // the news count at the last choice
  double decided_at = -1e9;
  int64_t blows = 0;          // blows taken
  ge::core::Rng rng{1};       // its own draws (where it explores), seeded off the bot's seed
  ge::core::Rng keys_rng{1};  // the side key's changes (footwork.hpp `fighter_keys`)
  float clean_keys = 1;       // the share of side key changes made clean (1: every one)
  float news_health = 0;      // its health at the last think: a fall is a blow, news
  sh::Vec3f surface_at;       // the open water's top over its head it rises to (`surfacing`)
  int8_t side_was = 0;        // the side key's sign last sent
  int8_t side_gap = 0;        // commands still held with no side key
  sh::Dodge dodge;            // its side steps in a fight (shooter/dodge.hpp)
  // The ways in it watches from where it stands (watch.hpp): their spots, the node they were
  // worked out for, the one watched and until when.
  std::vector<sh::Vec3f> watch;
  ge::ai::Search watch_search;
  int32_t watch_node = -1, watch_pick = -1;
  double watch_until = 0, watch_at = -1e9;
  sh::DodgeSkill dodge_k;
  FightMode mode = FightMode::roam;
  Role role = Role::free;
  sh::Temper temper = sh::Temper::balanced;  // when it takes a fight (shooter/temper.hpp)
  sh::AimSkill skill = sh::AimSkill::expert;
  bool outmatched = false;  // turned from the fight in sight: armour, a weapon or the team first
  sh::Traits traits = sh::traits_of(sh::AimSkill::expert, sh::Temper::balanced);  // traits.hpp
  // Its traits read in steps (shooter/traits.hpp `tier_of`):
  MapSense sense = MapSense::expert;     // what it knows of the map: items, buttons, their worth
  MapSense timing = MapSense::expert;    // which items it times, and how well
  MapSense comms = MapSense::expert;     // what it says, and reads of its team's lines
  MapSense teamwork = MapSense::expert;  // what it does for the team: areas, help, the quad's rush
  MapSense hearing = MapSense::expert;   // what the sounds it hears tell it
  MapSense camp = MapSense::expert;      // the spots an enemy comes in weak, worked
  float edge = 1;                        // its aim's worth in a fight (shooter/temper.hpp)
  bool rash = false;                     // goes after every sound (a novice's discipline)
  int aim_slot = -1;                     // the aim trait its hands hold now (0 the scattergun)
  // A sound is gone after this long after it was heard (s): its reaction to a sighting and the
  // thought of where a sound puts him on top (100 ms for a quick player, 200 for a slow one).
  float hear_react_s = 0;
  bool was_dead = false;
  bool unlocking = false;  // on the way to the goal item's button
  bool swimming = false;   // in water no route leaves: following the swim field
  bool surfacing = false;  // short of air under open water: up for a breath (water.hpp)
  bool hunts = false;      // every monster a foe (companion.hpp `hunt_init`)
};

// The fighter's strength: health and armour's protection, a launcher counted on top.
float fighter_strength(const Stats& st);

// A fighter on graph `g` with the personality `traits` (shooter/traits.hpp; `skill` perfect
// passes its aim's hands untouched, the measuring stick), the map's items off `entities` (the
// map's text) matched to what the server `s` sends client `slot`'s player now. Call after the
// slot connects.
void fighter_init(Fighter& f, const sh::NavGraph& g, const server::RulesCommon& s, int slot,
                  const char* entities, sh::AimSkill skill, const sh::Traits& traits, double hz,
                  uint32_t seed);

// Sends the fighter to nav node `node` (kept when it is on its way there or standing on it; none
// when no way leads there from where it stands).
void fighter_go(Fighter& f, int32_t node);

// The fighter's next command, `msec` long, on server `s` as it stands after the last frame.
sh::MoveCmd fighter_command(Fighter& f, const server::RulesCommon& s, int slot, int msec);

}  // namespace shooters_edge::bots
