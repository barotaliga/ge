// THE FIGHTER: a bot (plugins/shooterbots/bot.hpp) that plays the baseline deathmatch mod
// (games/shooters_edge/rules): it chooses where to go, what to hold and when to shoot, on
// `ai/choose`, from what a player's client has and nothing else.
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
// grenade are aimed at the feet, a projectile led by its flight time. In water with one in sight,
// the view stays on him and the swim's way is pressed with the keys under it. Dead, it presses
// attack to come back once the mod allows.
//
// Deterministic: a pure function of what it is sent and its hands' seeds.
//
// ge:summary The fighter: a bot on the baseline mod choosing its mode, goal, weapon and trigger
// ge:keywords fighter deathmatch mode roam fight chase retreat item-timing respawn weapon-choice
// ge:keywords fire trigger tolerance stats items pick_best enter baseline-mod danger danger-spots
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_fight_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ai/keys.hpp"
#include "ai/path.hpp"
#include "core/prng.hpp"
#include "shooterbots/bot.hpp"
#include "shooterbots/bot_arsenal.hpp"
#include "shooterbots/bot_view.hpp"
#include "shooterbots/companion.hpp"
#include "shooterbots/dodge.hpp"
#include "shooterbots/features.hpp"
#include "shooterbots/hearing.hpp"
#include "shooterbots/mind.hpp"
#include "shooterbots/nav_field.hpp"
#include "shooterbots/swim.hpp"
#include "shooterbots/temper.hpp"
#include "shooterbots/traits.hpp"

namespace ge::shooterbots {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;

constexpr double kThink_s = 0.1;  // how often the mode, the goal and the weapon are chosen

// What a client is sent of its own player (`Stats`, bot_arsenal.hpp), read by the game's arsenal.
Stats read_stats(const ge::shooterbots::BotView& s, int ent, const BotArsenal& arsenal);

// What an item is, the shooter's vocabulary (classic/item_tables.hpp).
using ItemKind = ge::shooterbots::ItemKind;

// How well a player knows the map (knowledge.hpp): where its items are and when they come back.
enum class MapSense : uint8_t { novice, average, expert };

struct ItemSpot {
  std::string name;   // its classname (a health box by its size)
  std::string label;  // its name and spot ("item_armorInv@2480,-1992"): one option in a record
  // In a pocket only the water reaches (no route leads in): dived for from `dive_node` on the
  // field that leads through the water to the pocket (`dive`); none when no route is needed.
  const ge::shooterbots::SwimField* dive = nullptr;
  // What this player believes of it (knowledge.hpp), not what the server holds.
  double back_at = 0;       // when it is expected back (while believed gone)
  double last_seen = -1e9;  // when it was last seen there
  double mine_at = -1e9;    // when its own bar said it took it (the view sent lags behind)
  sh::Vec3f at;             // where it stands (the middle of its box on the floor)
  // Behind a door a button opens (the map's text: a button whose target is the door's name): the
  // button touched first while the door stands shut.
  sh::Vec3f door_rest;                 // the door's origin while shut
  sh::Vec3f button_at;                 // the button's middle
  int ent = -1;                        // the server's entity for it, -1 when none matched
  int32_t weapon = 0;                  // a weapon's bit (the arsenal's)
  WeaponRole role = WeaponRole::none;  // the part that weapon plays (bot_arsenal.hpp)
  float amount = 0;                    // health given, or ammunition (doubled by spawnflags 1)
  float weight = 1;      // how much it matters on this map, as the player rates it (knowledge.hpp)
  float respawn_s = 20;  // gone this long once taken
  // Its words in the game's tables (BotArsenal::heard), read when it is learned: what a teammate
  // calls it ("ra", "rl") and the word its pickup sound is heard as.
  const char* call = "";
  const char* heard = "";
  int32_t node = -1;  // the nav node nearest it
  int door_ent = -1;  // the door's entity (-1: none)
  int32_t button_node = -1;
  int button_ent = -1;  // the button's entity
  int32_t after =
      -1;  // an item rule (shooterbots/map_book.hpp): no goal while item `after` is up or due
  float after_due_s = 0;                    // within this
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
  // (`itemglance`, knowledge.hpp) a pass by its spot under way, and whether it is looked at in it
  bool glance_pass = false, glance_want = false;
  int32_t stand_node = -2;  // (`holdpoint`) where a holder stands off it (-1 none, -2 not sought)
  double glance_line_at = -1e9;  // the spot last in a clear line, s
};

// A button on the map (buttons.hpp).
struct ButtonSpot {
  sh::Vec3f at;                // its middle
  std::vector<int> doors;      // the door entities it opens
  std::vector<int32_t> opens;  // the items behind them (indices into the fighter's items)
  int ent = -1;
  int32_t node = -1;   // where it is pressed (or shot) from
  bool shoot = false;  // opened by a shot (health over 0), not a touch
  // The way a touch presses it in, on the floor plan (its `angle`: the way it moves when pressed),
  // a unit vector; zero for one pressed up or down. Its face is on the other side.
  sh::Vec3f push{0, 0, 0};
  float open_s =
      0;  // how long its doors stay open once fired (their shortest wait), s; 0: for good
  std::vector<int> way_doors;  // (`fields`) of `doors`, those that are a way once fired
  // A shot trigger's inline model ("*N"): the rules keep no model on a trigger's edict, so the
  // plan finds its entity text by this. Empty for a button or a wall.
  std::string model;
  // (`complete`) A breakable that switches off a current (`way_shot_currents`): the box of the
  // push it removes, whose nodes are shut to him till it is shot. Empty (lo == hi) for the rest.
  sh::Vec3f calm_lo{0, 0, 0}, calm_hi{0, 0, 0};
  // (`complete`) A breakable lid over a way (`way_shot_currents`): the links that climb through it
  // are shut to him till it is shot.
  bool lid = false;
  // How it looks at rest (its world row's frame and effects, first seen; -1 not yet): one that
  // does not move when pressed shows it by its texture only (`button_looks_pressed`).
  int rest_frame = -1, rest_effects = -1;
};

// A floor over lava or slime a touch button slides away (floor_trap.hpp, `floortrap`): the button
// (an index into `Fighter::button_spots`), its floor doors (indices into `Fighter::bridges`), the
// floor's box at rest (its linked floor doors too), the button's box, and the teleporters landing
// on the floor.
struct FloorTrap {
  int32_t button = -1;
  std::vector<int32_t> floors;
  sh::Vec3f lo, hi;
  sh::Vec3f blo, bhi;  // the button's box
  // The teleporters landing on its floor: shut while the floor is away (dm2's two into the big
  // room drop whoever takes them into the lava then).
  std::vector<sb::NavExtraLink> teleports;
  // (`trapbutton`) The links whose way passes through its button: shut unless it presses it.
  std::vector<sb::NavExtraLink> touches;
  bool away = false;  // its floor away when last looked (the opens counted on the change)
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

// The wet commands' causes, FighterStats::wet_why's columns.
inline constexpr const char* kWetWhy[12] = {"roam",       "fight",    "chase", "retreat",
                                            "aiming",     "goal-wet", "still", "pilot-swim",
                                            "pilot-walk", "under",    "hold",  "route-dry"};

// Why the finger did or did not press with the launcher up (FighterStats::finger): `down` the
// trigger's own press, `other` an attack another writer pressed (a spam shot, a rocket jump's),
// then the hold's first reason: nobody known, the one known out of sight, not aiming, still
// reacting, out of the weapon's range, the view off him, a mate in the line or by the burst, the
// burst too near (`blastsafe`), the finger between its clicks, the hand still turning to a view.
// With the launcher carried and not up: another weapon held with rockets in hand, the one aimed at
// in sight (`weapon-fight`) or not (`weapon`), or no rockets left.
enum class FingerWhy : uint8_t {
  down,
  other,
  none,
  unseen,
  not_aiming,
  reacting,
  far,
  off,
  mate,
  burst,
  rhythm,
  turning,
  weapon_fight,
  weapon,
  empty
};
inline constexpr int kFingerWhys = 15;
inline constexpr float kFingerBands[2] = {192, 400};  // the ledger's range bands, units
inline constexpr const char* kFingerWhyNames[kFingerWhys] = {
    "down", "other", "none",   "unseen",  "not-aiming",   "reacting", "far",  "off",
    "mate", "burst", "rhythm", "turning", "weapon-fight", "weapon",   "empty"};

// The key items `FighterStats::passed` counts, and why one there near was not taken on the way.
inline constexpr const char* kShowGates[11] = {
    "not roaming", "aiming",   "launcher not up", "rockets short", "no plan area",    "news",
    "too soon",    "in sight", "no spot ahead",   "dead or wet",   "turned to a spot"};
// (`rlgang`) a free or holding fighter's thinks by the first thing that kept it from calling the
// team onto an enemy launcher's holder (the last two: called, then said; another role apart)
inline constexpr int kGangGates = 13;
inline constexpr const char* kGangGateNames[kGangGates] = {
    "push under way", "novice",  "not strong",  "too soon",       "none believed",
    "not placed",     "no area", "no way",      "no gather spot", "switch off",
    "not said",       "called",  "another role"};
inline constexpr const char* kPassItems[5] = {"ra", "ya", "mh", "quad", "rl"};
inline constexpr const char* kPassReasons[21] = {"not known there",
                                                 "its goal near",
                                                 "a scripted way held",
                                                 "escaping",
                                                 "diving",
                                                 "the map's way",
                                                 "retreating",
                                                 "fetching",
                                                 "off",
                                                 "the goal",
                                                 "its route lost",
                                                 "not reached",
                                                 "another floor",
                                                 "its door shut",
                                                 "not wanted",
                                                 "worth nothing now",
                                                 "a mate's",
                                                 "given up",
                                                 "behind its run",
                                                 "farther than another",
                                                 "taken on the way"};

struct FighterStats {
  int64_t commands = 0, shots = 0, switches = 0, pickups = 0, respawns = 0;
  int64_t goal_changes = 0;  // its item goal changed (the commitment's measure)
  int64_t shot_waits = 0;    // rocket jumps whose shot waited for the jump key (`shotjump`)
  int64_t packs_called = 0, packs_fetched = 0, bored = 0;  // launchers in packs (packs.hpp)
  // Every death's pack it came to know of (`packinfer`), by how its place was known (`PackFrom`),
  // its guesses (the weapon believed against the truth where the runner knows it) and its trips.
  int64_t packs_known[5] = {};
  int64_t pack_guesses = 0, pack_guess_right = 0, pack_guess_rl = 0, pack_truth_rl = 0;
  int64_t pack_trips = 0, pack_reached = 0, pack_seen_gone = 0;
  int64_t surfaced = 0;        // times up for air (water.hpp)
  int64_t surface_failed = 0;  // and rises given up short of the surface
  // The trigger, with one in sight aimed at: looked at, out of the weapon's range, the view off.
  int64_t trigger_checks = 0, trigger_far = 0, trigger_off = 0;
  // (M18r.md 18b) Every live command carrying the launcher, by what the finger did (`FingerWhy`):
  // pressed by the trigger or by another writer (a spam shot, a rocket jump), or why not.
  int64_t finger[kFingerWhys] = {};
  // And with the one aimed at in sight, by his range (kFingerBands): pressed (`down`, `rhythm`),
  // the burst too near, another weapon up.
  int64_t finger_band[3][3] = {};
  int64_t after_shots = 0;  // (`afterfire`) rockets at one gone from sight a moment ago
  int64_t ear_shots = 0;    // (`earfire`) rockets at one heard and not seen
  int64_t lob_shots = 0;    // (`earlob`) grenades thrown round a corner
  int64_t aim_cleared = 0;  // (`aimclear`) commands the height aimed at was raised off a burst
  int64_t held_looks = 0;   // (`holdra`) commands a holder kept its view on its armour
  // The burst held past the near band by what the crosshair met: the world with his line blocked
  // too, a player, the world with his line clear and the view still off the aim's point (the hand
  // on its way), the same with the view on the point (the point itself runs into the world).
  int64_t burst_by[4] = {};
  int64_t on_the_way = 0;    // short goals taken up on the way
  int64_t trips_kept = 0;    // (`tripkeep`) thinks a scripted way's goal was held
  int64_t hides = 0;         // (`hide`) the launcher put away for the scattergun
  int64_t stood = 0;         // (`fightway`) a fight's back key let go: stood and dodged
  int64_t chained = 0;       // item goals planned on to the next one (`chains`)
  int64_t turned_round = 0;  // commands a turn back was pressed across the run (`uturn`)
  int64_t took[ge::shooterbots::kItemKinds] = {};  // its own pickups by the item's kind (ItemKind)
  int64_t swim_fights = 0;                         // commands swum under the aim, an enemy in sight
  int64_t wet = 0;  // commands alive with the waist in water (swimming or wading)
  // And the wet commands by what it was doing (`kWetWhy`): its mode, aiming, its goal in the
  // water, standing still, the pilot swimming or walking a pool's floor, the head under.
  int64_t wet_why[12] = {};
  // Wet and aiming, swimming (the pilot's swim), walking (any other pilot mode): the speed across
  // summed, the commands, those under 50 units/s.
  double wet_speed[3] = {};
  int64_t wet_n[3] = {}, wet_still[3] = {};
  int64_t taken = 0;              // items seen to go (by anyone)
  int64_t modes = 0;              // mode changes
  int64_t mode_commands[4] = {};  // commands spent in each mode
  int64_t weapon_commands[kMostWeaponNumber + 1] =
      {};                     // commands holding each weapon, by its number
  int64_t packs = 0;          // backpacks gone for
  int64_t held_fire = 0;      // shots held for a teammate in the line of fire
  int64_t heard_taken = 0;    // items heard taken (ears.hpp)
  int64_t heard_spawns = 0;   // players heard spawning on a point it knows
  int64_t spam_shots = 0;     // rockets fired at a spot enemies come in by
  int64_t pokes = 0;          // buttons a novice tried on a whim
  int64_t door_errands = 0;   // buttons pressed for a shut door on the way (`locked`)
  int64_t field_errands = 0;  // of those, begun off the goal's field before the door (`fields`)
  // how the door errands ended: the door open, its time up with the door still shut, no way to the
  // button but through the door; and shut doors on the route gone round instead (`errandworth`)
  int64_t errand_open = 0, errand_timeup = 0, errand_noway = 0, errand_round = 0;
  // the same by button (an index into `button_spots`, the first kErrandButtons): begun, open, time
  // up, no way, gone round
  static constexpr size_t kErrandButtons = 16;
  int64_t errand_by[kErrandButtons][5] = {};
  // (the decision log's `item_passed`) thinks with a key item there within kOnTheWay, by the item
  // (kPassItems) and what kept it from being taken on the way (kPassReasons; the last: taken)
  int64_t passed[5][21] = {};
  // (`spamshow`) commands carrying the launcher, by the first thing that kept a show-up rocket off
  // (kShowGates; the last: turned to a spot)
  int64_t show_gate[11] = {};
  int64_t rl_hunts = 0;                // (`rlhunt`) thinks going for an enemy launcher's holder
  int64_t gang_gate[kGangGates] = {};  // (`rlgang`) thinks by what held the call off
  int64_t noroute = 0;                 // goals the pilot found no route to (`noroute`)
  int64_t item_first = 0;  // thinks a key item of its own came before a fight (`itemfirst`)
  int64_t chase_held = 0;  // thinks one lost from sight was not followed (`chasekill`)
  // (`goalfeet`) fight commands whose feet kept the route: across the line, toward him, and
  // those with the goal behind the line (the fight's feet kept)
  int64_t goal_across = 0, goal_ahead = 0, goal_behind = 0;
  int64_t bridge_errands = 0;     // and of those, a bridge's button gone to first
  int64_t trap_presses = 0;       // a floor trap's button gone for, an enemy on it (`floortrap`)
  int64_t trap_fired = 0;         // and its floor gone while it pressed
  int64_t trap_opens = 0;         // a floor trap's floor seen going away, whoever pressed
  int64_t trap_kept = 0;          // commands its keys were kept off a trap's button
  int64_t trap_flees = 0;         // runs off a trap's floor as it went (`trapflee`)
  int64_t bridges_seen_gone = 0;  // a bridge believed out seen gone, not sent (`bridgeseen`)
  int64_t dives_given_up = 0;     // dives turned back (late, or short of air)
  double dive_wet_s = 0;          // time in the water on a dive
  int64_t outmatched = 0;         // commands spent turned from a fight it believed it loses
  int64_t out_reach = 0;          // and of them, a blade's reach short of a foe kept in sight
  int64_t sound_chases = 0;       // commands spent going at an enemy it only heard
  int64_t kept_off = 0;        // commands backing off a monster that bites (BotArsenal::keep_off)
  int64_t ledge_kept = 0;      // a fight's keys given up for the route's at a ledge (`ledge_guard`)
  int64_t climbs_yielded = 0;  // climbs out of the water given up for the aim a while
  int64_t dodging = 0;         // commands spent stepping across an enemy's line (dodge.hpp)
  int64_t brink = 0;           // commands whose keys the brink changed (shooterbots/brink.hpp)
  int64_t dodge_turns = 0;     // and the times it turned the step back
  // The press (`press`, footwork.hpp): commands it was weighed in a fight, those pressed on at him
  // and back, and those held off by his nearness or a step not clear.
  int64_t press_weighed = 0, press_on = 0, press_back = 0, press_near = 0, press_blocked = 0;
  int64_t watching = 0;  // commands standing with the view on a way in (watch.hpp)
  int64_t landings = 0;  // commands walking onto an exit node just climbed out to
  double speed_sum = 0;  // its speed across the floor summed over its commands alive
  double mode_speed[sb::kPilotModes + 1] = {};  // and by the pilot's mode at the command
  int64_t mode_n[sb::kPilotModes + 1] = {};
};

// What one player was heard fire (by entity; packs.hpp): when each weapon (by its number 1..8)
// was last heard, and the last weapon of all.
struct ShotsHeard {
  double at[9] = {-1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9, -1e9};
  int32_t last = 0;  // the number of the last weapon heard; 0 none
  double last_t = -1e9;
};

// A backpack this player believes lies on the map (packs.hpp): where, since when, whose, and
// what it believes is in it.
struct KnownPack {
  double t = -1e9;  // when it fell (or was first seen lying)
  ge::shooter::Vec3f at;
  int32_t ent = -1;      // the pack's entity, once seen
  int32_t victim = -1;   // whose (his entity); -1 a pack found lying
  int32_t weapon = 0;    // the weapon believed in it, by number (0: ammunition alone believed)
  int32_t truth = -1;    // the weapon really in it (the lab's, filled by the runner; -1 unknown)
  uint8_t how = 0;       // how its place is known (`PackFrom`)
  bool sure = false;     // the weapon known for certain (an expert's: inside its refire)
  bool rockets = false;  // he fired rockets in the fight: the pack is worth much more
  bool wanted = true;    // this player bothers with it (drawn once: most often yes)
};

// What a fighter believes of one enemy (temper.hpp): his body and arms as last heard.
struct FoeRead {
  float body = 100, arms = 1;
  double t = -1e9;           // when it was last heard of him
  double quad_until = -1e9;  // his quad heard taken, believed held until then
  double seen_t = -1e9;      // (`ledgerforget`) when he was last in its sight
  double hit_t = -1e9;       // (`ledgerhits`) when its own rocket was last counted on him
  // (`rlknown`) when he was last known to carry the launcher (heard firing it, named a rocket's
  // killer in the kill feed); kept through the fade, cleared by his death
  double key_t = -1e9;
  // What it last learned of him (`FoeNews`, for the ledger's read by kind; decides nothing).
  uint8_t news = 0;
};
// What a fighter last learned of an enemy's body: nothing yet, a death or a spawn heard (100), a
// pickup heard, a cry, its own rocket's burst.
enum class FoeNews : uint8_t { none, spawn, pickup, cry, hit };
inline constexpr int kFoeNews = 5;

// (`ledgerhits`) One of its own rockets in flight: the entity, where it was last seen.
struct OwnRocket {
  int ent = -1;
  ge::shooter::Vec3f at;
};

struct MapPlan;

// The hand on the keys (feature `keys`, keyboard.hpp): the keyboard, the key set wanted last (-1
// none), whether feathering has the keys on, and the steering's tally (the way wished less the
// way pressed, summed over time, in sets times seconds).
struct KeyHand {
  ge::ai::Keyboard board;
  int set = -1;
  bool feather = true;
  double steer = 0;
  float wish_forward = 0, wish_side = 0;  // the last wish it pressed (for a lab's trace)
  double clock = 0;                       // its own time, s
  double jump_until = -1;                 // the jump key wanted until then (a jump asked for)
};

struct Fighter {  // NOLINT(clang-analyzer-optin.performance.Padding): eight a match, read by name
  // Ordered by size, largest first, so the struct packs tight.
  sb::Bot bot;
  // The game's weapons as it reads them (bot_arsenal.hpp), set by `fighter_init`.
  const BotArsenal* arsenal = nullptr;
  std::vector<ItemSpot> items;
  std::vector<sh::Vec3f> spawns;                                // the map's spawn points (ears.hpp)
  std::vector<ShotsHeard> fired = std::vector<ShotsHeard>(33);  // by entity: weapons heard fired
  std::vector<sh::Vec3f> cried_at = std::vector<sh::Vec3f>(33);  // by entity: a pain or death cry
  std::vector<double> cried_t = std::vector<double>(33, -1e9);   // heard there then
  std::vector<KnownPack> known_packs;  // the packs it believes lie on the map (packs.hpp)
  std::vector<double> last_seen_t = std::vector<double>(33, -1e9);   // and when last in sight
  std::vector<sh::Vec3f> last_seen_at = std::vector<sh::Vec3f>(33);  // and where (packs.hpp)
  std::vector<sb::HeardSound> heard;                                 // this frame's sounds taken in
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
    bool in = true;         // its floor is not there: its nodes shut
    bool floor = false;     // a floor door (map_nav.hpp NavBridge): there at rest, not when away
    float wait = 0;         // a bridge's time out before it goes back, s (<= 0: stays out)
    double out_since = -1;  // when it last came all the way out, server time; -1: not out
    int32_t button = -2;    // (`bridgebutton`) the button that runs it out (-1 none, -2 unread)
    bool down = true;       // all the way in, where it stood when the map began (its button wanted)
    sh::Vec3f out_at;       // its origin all the way out (map_nav.hpp NavBridge::out_at)
    // (a floor door's, under `bridgeseen`: when it was last seen away)
  };
  std::vector<Bridge> bridges;
  bool bridges_seen = false;
  std::vector<FloorTrap> floor_traps;  // the map's floor traps (floor_trap.hpp)
  int32_t trap_press = -1;             // the trap whose button it presses now, or -1
  int32_t trap_shut_for = -2;          // (`trapbutton`) the press the shut links were made for
  double trap_until = -1e9;            // and until when
  double trap_told = -1e9;             // when a mate's `trap` call was read (floor_trap.hpp)
  int32_t trap_stay = -1;              // the trap it fired: held at its button while it is away
  int32_t trap_flee = -1;  // the trap whose floor it stands on as it goes (`trapflee`), or -1
  int32_t trap_exit = -1;  // and the node off it it runs to
  Mind mind;               // what it believes of its team, its claim, its calls (mind.hpp)
  Escort escort;           // a co-op companion's own state (companion.hpp; leader -1: none)
  KeyHand keys;            // the hand on the keys (feature `keys`, keyboard.hpp)
  std::string name;        // its player name, as its teammates see it in their team lines
  std::string pack_call_item = "rl";  // what the pending pack call says is in it ("rl", "lg")
  ge::ai::Search reach;               // every node it can get to from where it stood at `reach_at`
  ge::ai::Search rush_search;         // (`rushrun`) a mate's route to the quad
  FighterStats stats;
  sb::HearStats hear_stats;
  sb::HearSkill ears;  // what it takes in of the sounds it is sent (hearing.hpp)
  // Its team messages (team.hpp's `team_say`), sent as `say_team` by whoever drives its client.
  std::vector<std::string> outbox;
  double held = 0;               // in the mode this long (worked out when it changes)
  double mode_at = 0;            // server time it entered the mode
  double heard_t = -1;           // the server time whose sounds were last taken in
  double quad_glow_t = -1e9;     // (`quadseen`) when the quad's glow was last seen or called
  double spam_fired = -1e9;      // its last rocket at a spot
  double poke_until = -1e9;      // a novice on a button until then (buttons.hpp)
  double poke_seen_t = -1e9;     // (`pokelook`) the button last in sight
  bool poke_looked = false;      // and the poke's look set this command
  double unlock_seen_t = -1e9;   // (`unlocklook`) the goal item's shot button last in sight
  bool unlock_looked = false;    // and its look set this command
  double next_poke = 0;          // and his next whim
  double next_think = 0;         // server time of the next choice
  double ready_at = 0;           // the held weapon ready again (fighter_command: its rhythm)
  double impulse_at = -1;        // when the last impulse went
  double reach_at = -1;          // when `reach` was worked out (-1: never)
  double climb_since = -1;       // when `climb_out` began its try at the edge (-1: none)
  double climb_rest_until = -1;  // and the view the aim's again till then (the edge held)
  int32_t reach_from = -1;       // and the node it was worked out from (where it stood)
  double said_at = -1e9;         // its last team message
  double called_at = -1e9;       // its last enemy callout
  double under_since = -1;       // when its head last went under the water (-1: not under)
  double dive_wet_since = -1;    // when the dive under way went into the water (-1: none)
  double breathe_until = -1;     // up for air (water.hpp): at the surface until then (-1: rising)
  double bore_until = -1e9;      // its own rocket at its feet for the quad's carrier (packs.hpp)
  double bored_at = -1e9;
  double pack_call_until = -1e9;  // a pack's call pending until then (packs.hpp)
  // Its own last shot (packs.hpp): when, and when the blast was heard nearest its line after it
  // (`shot_from`, `shot_dir`, `blast_at` below).
  double shot_t = -1e9, blast_t = -1e9;
  double pack_going = -1e9;    // the known pack gone for (its time as its name)
  double surface_by = 0;       // out of the water by then, or the rise is given up
  double surface_retry = 0;    // no rise for air before then
  double dive_off_until = -1;  // until when the dive given up (`dive_off`) is not tried again
  double door_until = -1e9, door_avoid_until = -1e9;  // a door's errand and avoidance (below)
  ge::ai::Why* why = nullptr;
  Team* team = nullptr;                     // its side (team.hpp); null in a free-for-all
  const ge::shooter::Locs* locs = nullptr;  // the map's place names (none: no names)
  const MapPlan* plan = nullptr;  // the map's team plan (map_plan.hpp; none: the items alone)
  // The map's traps (map_nav.hpp: nodes no way on foot leads back from), never a goal; null none.
  const std::vector<uint8_t>* trap = nullptr;
  // The map's wet nodes (map_nav.hpp `wet`): a fight's place to stand or a foe's spot in the water
  // is no goal while he stands dry (a person does not wade in to fight); null none.
  const std::vector<uint8_t>* wet = nullptr;
  // A dry pocket (a trap he stands in with no way out under water) is walked out of straight at
  // the nearest node outside it; false: a map's plan leaves it by its doors and buttons
  // (way_through.hpp: base2's start is such a pocket until its door opens).
  bool dry_pockets = true;
  const ge::shooterbots::SwimField* swim =
      nullptr;          // the map's way out of its water (none: no water)
  int64_t buttons = 0;  // buttons gone to
  int64_t dives = 0;    // dives begun
  int64_t lost = 0;     // thinks spent off the graph
  int32_t goal_item = -1;
  int32_t goal_was = -1;    // the goal item held since `goal_since`
  int32_t tired_item = -1;  // an item gone for too long in vain, worth less until `tired_until`
  double goal_since = 0, tired_until = 0;
  bool hid = false;       // (`hide`) the scattergun taken up over the launcher, nobody in sight
  int32_t spam_for = -1;  // the claim `spam_from` was found for (-1: none yet)
  sh::Vec3f news_spot;    // spam at news (spam.hpp): the way in fired at
  double news_t = -1e9;   // the news it is fired at for (when it came), and the rockets spent
  int32_t news_shots = 0;
  bool news_on = false;  // a way in turned to this command
  // (`spamshow`) a show-up spot turned to with no news (spam.hpp), and the last one fired at
  bool show_on = false;
  sh::Vec3f show_last;
  double show_last_t = -1e9;
  double show_raise_t = -1e9;  // when the launcher was raised for one
  int64_t show_shots = 0;      // rockets at show-up spots
  int32_t spam_node = -1;      // where it fires at a spot from now (-1: not out)
  int32_t spam_spot = -1;      // and at which
  int32_t poke = -1;           // the button a novice is on (or a door's errand's, below)
  // A door on the way that only a button opens, resting shut (`locked`): its button (an index into
  // `button_spots`) gone to and pressed or shot first, and the door (until `door_until`); a door
  // whose button no way reaches but through it (dm2's t11, pressed from the far side): no route
  // through it until `door_avoid_until` (`door_avoid`, below).
  int32_t door_button = -1, door_ent = -1;
  // (`noroute`) An item the pilot found no route to, set aside as a goal until `noroute_until`; a
  // bridge in the way's button gone to first (an index into `button_spots`, the bridge an index
  // into `bridges`) until `bridge_until`.
  double noroute_until = -1e9, bridge_until = -1e9;
  int32_t noroute_item = -1, bridge_button = -1, bridge_k = -1;
  int32_t objective = -1;   // a goal set from outside (way_through.hpp): taken before any item
  int32_t mega_watch = -1;  // the mega it took, its timer waiting on its health (knowledge.hpp)
  int32_t goal_node = -1;
  int32_t chain_node = -1;   // the goal chained on past it (`chains`, pilot.hpp `pilot_chain`)
  int32_t chain_item = -1;   // and its item, when an item's
  int32_t want_weapon = 0;   // the number (1..8) asked for; 0 none pending
  int32_t asked_weapon = 0;  // the number the last impulse asked for (`rlback`)
  int32_t after_left = 0;    // (`afterfire`) rockets left for the one last lost from sight
  double ear_fired = -1;     // (`earfire`) the sound last fired at: its time heard
  OwnRocket own_rockets[4];  // (`ledgerhits`) its rockets in flight, for the ledger
  // (`earlob`, launcher_fire.hpp) The throw under way: where at, under which view, until when; the
  // sound or sighting it is for (its time), the lob asked for, when the next is looked for.
  bool lob_on = false, lob_asked = false;
  ge::shooter::Vec3f lob_spot, lob_view;
  double lob_until = 0, lob_heard = -1, lob_next = 0;
  int slot = -1;
  int32_t pack_ent = -1;    // a backpack gone for
  double pack_seen_t = -1;  // a backpack in the view within reach of a pickup last (knowledge.cpp)
  // The weapon's rhythm (fighter_command): the weapon and its ammunition last command (`ready_at`
  // with the doubles: when the weapon is ready again after the last shot seen leave it).
  int32_t shot_weapon = 0;
  float shot_ammo = 0;
  int32_t lost_to = -1;  // off the graph or in a pool it cannot leave by its links: the node
                         // walked (or swum) straight at to get back (-1: none)
  int32_t reach_n = 0;   // how many nodes `reach` holds
  // (`fields`) The ways to every item from everywhere, the match's (shooterbots/nav_field.hpp;
  // null: none), and the node it stood nearest at its last think (-1: none yet).
  ge::shooterbots::NavFields* fields = nullptr;
  int32_t field_node = -1;
  int32_t diving = -1;        // the item it dives for (in the water on `dive`), -1 none
  int32_t last_dive = -1;     // the item the last think dived for
  int32_t explore_node = -1;  // a novice with nothing known walks the map to learn it
  int32_t dive_off = -1;      // the item whose dive it last gave up (-1: none)
  int32_t swim_exit = -1;  // the exit node it is climbing out onto (shooterbots/swim.hpp), -1 none
  double swim_exit_until = 0;
  int32_t short_item = -1;  // a short goal on the way (fighter_items.cpp on_the_way), -1 none
  int32_t door_avoid = -1;  // the door a button's errand cannot reach round (above), -1 none
  // (`errandworth`) a shut door gone round to the goal node `door_round_goal`: kept off the routes
  // while that goal holds, until `door_round_until`
  int door_round = -1;
  int32_t door_round_goal = -1;
  double door_round_until = -1e9;
  double short_until = 0;
  bool swim_view_on = false;  // the swimmer's view turned at a hand's pace (fighter.cpp)
  float view_sent[2] = {};    // the view (pitch, yaw) its last command carried
  int32_t landing = -1;       // the exit node it climbed out to, walked onto first (-1: none)
  // Which behaviours it has (shooterbots/features.hpp): set before `fighter_init`, which reads it.
  sb::Features features = sb::features_all();
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
  sh::Vec3f pack_call_at;     // where the pending pack call says the pack lies
  // Its own last shot: from where, along which line; the blast heard near it (where a rocket
  // into the water landed).
  sh::Vec3f shot_from, shot_dir, blast_at;
  int32_t shot_number = 0;  // and its weapon
  sb::Dodge dodge;          // its side steps in a fight (shooterbots/dodge.hpp)
  // The forward key in a fight (`press`, footwork.hpp): +1 on at him, -1 back, 0 the route's own,
  // held until `press_until`, drawn off its own stream; and for the trace alone what the feet did
  // with the aim this command ("on", "back", "route") or what kept the press out ("errand", "wet",
  // "feet", "unseen", "retreat", "near", "blocked").
  double press_until = -1e9;
  const char* press_note = "-";
  // For the trace alone: the last piece of the command that changed the move's keys (forward,
  // side, jump): "bot" (the pilot and the aim's own), "item", "button", "dodge", "swim", "landing",
  // "lost", "spam", "packs", "poke", "trap", "escort", "uturn", "brink"; "-" none pressed (the
  // keyboard after them presses what they wanted).
  const char* keys_by = "-";
  // (`shotjump`) A rocket jump's shot held for the jump key until this (`t`), on this view.
  double shot_wait_until = -1e9;
  sh::Vec3f shot_view;
  // For the trace alone: what the brink guard did this command (-1 it did not run: not near a
  // harm node, or the feet not free; else the ways round into a pit, +10 the run heading in,
  // +20 instead its flight landing in (`brinkfall`), +100 the keys changed).
  int32_t brink_note = -1;
  // The last command's finger with the launcher carried (FingerWhy), -1 without it: what a
  // runner samples beside its own view of the match (net/qw_team.cpp, 18b).
  int32_t finger_last = -1;
  int32_t finger_now = -1;  // every live command's finger by its reason (FingerWhy), any weapon
  int32_t held_number = 0;  // the weapon up on the last command, by its number (0: none)
  int32_t press = 0;
  ge::core::Rng press_rng{1};
  ge::core::Rng glance_rng{1};  // (`itemglance`) a pass's draw whether it looks at the spot
  // The ways in it watches from where it stands (watch.hpp): their spots, the node they were
  // worked out for, the one watched and until when.
  std::vector<sh::Vec3f> watch;
  ge::ai::Search watch_search;
  int32_t watch_node = -1, watch_pick = -1;
  double watch_until = 0, watch_at = -1e9;
  sb::DodgeSkill dodge_k;
  FightMode mode = FightMode::roam;
  Role role = Role::free;
  sb::Temper temper = sb::Temper::balanced;  // when it takes a fight (shooterbots/temper.hpp)
  sb::AimSkill skill = sb::AimSkill::expert;
  bool outmatched = false;   // turned from the fight in sight: armour, a weapon or the team first
  double unreached_at = -1;  // (`outreach`) since when a foe in sight lay past a blade's reach
  bool item_first =
      false;  // (`itemfirst`) a key item of its own due: the fight's feet keep the route
  sb::Traits traits = sb::traits_of(sb::AimSkill::expert, sb::Temper::balanced);  // traits.hpp
  // Its traits read in steps (shooterbots/traits.hpp `tier_of`):
  MapSense sense = MapSense::expert;     // what it knows of the map: items, buttons, their worth
  MapSense timing = MapSense::expert;    // which items it times, and how well
  MapSense comms = MapSense::expert;     // what it says, and reads of its team's lines
  MapSense teamwork = MapSense::expert;  // what it does for the team: areas, help, the quad's rush
  MapSense hearing = MapSense::expert;   // what the sounds it hears tell it
  MapSense camp = MapSense::expert;      // the spots an enemy comes in weak, worked
  float edge = 1;                        // its aim's worth in a fight (shooterbots/temper.hpp)
  bool rash = false;                     // goes after every sound (a novice's discipline)
  int aim_slot = -1;                     // the aim trait its hands hold now (0 the scattergun)
  // A sound is gone after this long after it was heard (s): its reaction to a sighting and the
  // thought of where a sound puts him on top (100 ms for a quick player, 200 for a slow one).
  float hear_react_s = 0;
  bool was_dead = false;
  // Its status bar at the last think (knowledge.hpp: a gain there is its own pickup).
  Stats bar;
  bool unlocking = false;  // on the way to the goal item's button
  // A fight's own keys (the dodge, the press, the back-off) never walk him off a ledge: a
  // campaign's leader, whose fall into a pit with no way out ends his map (`ledge_ahead`).
  bool ledge_guard = false;
  bool swimming = false;   // in water no route leaves: following the swim field
  bool surfacing = false;  // short of air under open water: up for a breath (water.hpp)
  int8_t side_was = 0;     // the side key's sign last sent (footwork.hpp `fighter_keys`)
  int8_t side_gap = 0;     // commands still held with no side key
  bool hunts = false;      // every monster a foe (companion.hpp `hunt_init`)
};

// `gapbuild`: a hopper plans a gap trick slower than a gap's speed when the route reaches the
// takeoff this long after setting off, s (a hop gains about 100 units/s a second from a run's 320).
constexpr float kGapBuild_s = 0.3f;

// `sidebrink`: a walk or drop along a lip over lava or slime costs a route this share of its time
// more (shooterbots/nav.hpp `NavJumper::lip_share`): up a ramp over a pit the route keeps to its
// middle.
constexpr float kLipShare = 1.0f;
// `midroute`: a walk into a node at its floor's edge costs a route this share of its time more
// (shooterbots/nav.hpp `NavJumper::wall_share`; half that a grid step in).
constexpr float kWallShare = 0.8f;
// (`quadseen`) The quad holder's glow among the effects the rules show everyone (its blue).
constexpr int kGlowQuad = 64;

// The pilot's settings and hands for a fighter with these behaviours and moves (`traits`' moves
// tier: an expert hops on practised hands, an average player the simple hop on steady ones, a
// novice walks on a person's); `fighter_init` uses them, and a lab flies a route with the same.
sb::PilotSettings fighter_pilot(const sb::Features& features, const sb::Traits& traits,
                                sb::AimSkill skill, sb::PilotHands& hands);

// The fighter's strength: health and armour's protection, a launcher counted on top.
float fighter_strength(const Stats& st);

// (`fields`) WHERE IT CAN GET TO, read off the match's fields (shooterbots/nav_field.hpp) for its
// jumper from the node it last stood at, when the switch is on and the match has fields; else off
// its reach. `node_reached`: a way there (no reach worked out yet: taken as one, as before);
// `node_run_s`: the run there, s (infinity: none).
bool fields_on(const Fighter& f);
bool node_reached(const Fighter& f, int32_t node);
double node_run_s(const Fighter& f, int32_t node);

// A fighter on graph `g` with the personality `traits` (shooterbots/traits.hpp; `skill` perfect
// passes its aim's hands untouched, the measuring stick) and the behaviours `f.features`
// (shooterbots/features.hpp), the map's items off `entities` (the map's text) matched to what the
// server `s` sends client `slot`'s player now, read by the game's `arsenal` (bot_arsenal.hpp).
// Call after the slot connects.
void fighter_init(Fighter& f, const sb::NavGraph& g, const ge::shooterbots::BotView& s, int slot,
                  const char* entities, sb::AimSkill skill, const sb::Traits& traits, double hz,
                  uint32_t seed, const BotArsenal& arsenal);

// Sends the fighter to nav node `node` (kept when it is on its way there or standing on it; none
// when no way leads there from where it stands).
void fighter_go(Fighter& f, int32_t node);

// The fighter's next command, `msec` long, on server `s` as it stands after the last frame.
// (`blastsafe`) No blast bursts nearer its shooter than this, units.
constexpr float kBlastNear = 160;
// (`blastsafe`) Whether a rocket fired from player `me` along `angles` would burst (its first wall
// or player, from the eye and from where the shot leaves, within `reach`) nearer it than
// kBlastNear; false with the switch off.
// Whether enemy `ent` is believed to carry a key weapon (the launcher, the arc): heard firing one
// lately (`FoeRead`).
bool foe_has_key(const Fighter& f, int ent, double t);
// A living teammate within `reach` of `at` (a blast's aim: its splash would hurt him).
bool mate_near(const Fighter& f, const ge::shooterbots::BotView& s, ge::shooter::Vec3f at,
               float reach);
constexpr float kBlastClear = 140;  // a rocket or grenade is held with a mate this near its aim
bool blast_bursts_near(const Fighter& f, const ge::shooterbots::BotView& s, int me,
                       sh::Vec3f angles, float reach);
sh::MoveCmd fighter_command(Fighter& f, const ge::shooterbots::BotView& s, int slot, int msec);

// A climb out of the water he fights in, held at the edge (a hover sat on hangar1's lip over the
// spot, out of the shot's line while the view is on the edge, lived through ten minutes of climbs),
// is tried kClimbOutTry_s, then the view is the aim's for kClimbOutRest_s.
constexpr double kClimbOutTry_s = 3;
constexpr double kClimbOutRest_s = 1.5;
// Whether the climb has the view at `t`, on its clock: `since` the try began (-1: none) and
// `rest_until` the aim's turn ends; a try run out starts the rest and counts in `yielded`.
bool climb_turn(double& since, double& rest_until, double t, int64_t& yielded);

}  // namespace ge::shooterbots
