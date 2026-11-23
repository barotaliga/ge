// QUAKE II'S RULES AS A RUNNER: the second program form behind `RulesRunner`
// (rules_runner.hpp), a C++ server for Quake II's maps (version 38, brushes). It keeps the parts
// every runner keeps (`RulesCommon`) and its own: the map's brushes and their trace, the entities
// the map's text spawns, and each player's packed move state (plugins/classic/q2_move.hpp).
//
// - **Time**: the rules run a frame every 100 ms of level time, the frame number × 0.1. The host
//   calls `frame` at its own tick (`sv.set.mintic` of host time a call); a rules frame runs each
//   time the host's clock reaches the next 100 ms. Commands run at once, as they arrive: a
//   packet's newest command, and the commands before it that a lost packet left unrun (up to the
//   two it carries).
// - **Players** spawn 9 above a spawn point and 1 higher (deathmatch: an `info_player_deathmatch`
//   that is not one of the two nearest to a living player, all allowed with two or fewer; else
//   the first `info_player_start`), facing its yaw through the delta angles, with health 100. A
//   deathmatch respawn holds the player still for 14 × 8 ms. `kill` is refused within 5 s of
//   spawning; a dead player may respawn 1 s after dying by pressing attack.
// - **The world** (rules_world.hpp) is filled at the start and after every call: entity slot 0
//   the world, slots 1..max clients the players, then the map's entities; the model list is the
//   map's brush models (`*1` ...) and the player's model.
//
// - **Items** (q2_items.hpp): spawned from their class names (an entity flagged 0x800 is not
//   spawned in deathmatch; outside it 0x100, 0x200 and 0x400 leave it out at skill 0, 1 and 2-3),
//   dropped up to 128 units to the floor (one that starts in a solid is removed), taken when a
//   living player's box touches theirs (−15..15), hidden in deathmatch until their respawn time and
//   removed otherwise. A mega health counts its taker down a point a second above his maximum,
//   from 5 s after, and shows again 20 s after he is back to it.
//
// - **Weapons and damage** (q2_combat.hpp, q2_fight.cpp): each player holds a weapon whose
//   frames step once a rules frame; a number key (impulse 1..10) or `use <name>` chooses one, and
//   a newly owned weapon is taken up at once when the blaster is in hand (always outside
//   deathmatch). Bullets, pellets and the rail are lines; bolts, grenades, rockets and the BFG's
//   ball fly, moved each rules frame. A blow pushes (the rocket jump), is shared with the armour
//   and takes the rest from health; at 0 the player dies and the obituary scores (+1 the killer,
//   −1 a suicide or the world's kill) and is printed to everyone.
//
// - **The rest of the fight**: power armour (the screen guards hits in front, a third of a blow at
//   a cell a point; the shield all round, two thirds at two points a cell), the powerups used by
//   `use <name>` (or at once with `dmflags` 16): quad damage ×4, invulnerability, the silencer,
//   the rebreather and the suit; drowning after 12 s under, lava and slime each frame, falling
//   from the change of the fall speed; `fraglimit` and `timelimit` end the level.
//
// - **The hand grenade, drops and the level's end**: the hand grenade is lit at frame 11, cooked
//   while attack is held and thrown faster the longer it was held (exploding in the hand at the
//   fuse's end); at death in deathmatch the weapon in hand and (`dmflags` 16384) the quad are
//   dropped, and `drop <name>` throws a weapon, ammo or a powerup, the dropper's again only after
//   1 s, gone after 30 s in deathmatch; a body under −40 is thrown apart; a level's end starts the
//   intermission (everyone frozen at its spot, a button after 5 s leaves for the next map); the
//   `dmflags` bits 1, 2, 2048 and 8192 leave items out, 4 keeps weapons, 32 repeats the level, 512
//   spawns farthest, 1024 respawns at once.
// - **Teams, bodies and scores**: with `dmflags` 64 (by skin) or 128 (by model), or in `coop`,
//   teammates' blows do nothing with 256 but push, and a team kill scores −1; `fov` is held to
//   1..160 (90 with 32768); a respawn leaves the body, up to 8, thrown apart by a blast under −40;
//   `score` lists the scores, which the intermission prints to everyone.
// - **The map's movers and triggers**: q2_map.hpp. Monsters are the next row (M18v7d).
//
// ge:summary Quake II's rules as a second runner: its maps' brushes, spawns and the 10 Hz frame
// ge:keywords q2 q2-server q2-runner runner-calls program-form spawn-point respawn
// ge:keywords deathmatch level-time frame-100ms brush-map q2_runner q2_of midair
// ge:keywords pm_speedjump cl_speedjump easy-jump jump-up-move
// ge:skill shooter
// ge:test tests/shooters_edge_q2_server_test.cpp
#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "classic/q2_move.hpp"
#include "classic/q2_view.hpp"
#include "core/prng.hpp"
#include "shooter/midair.hpp"
#include "shooter/pmove.hpp"
#include "shooter/world.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_poses.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace ge::brush {
struct Map;
class BrushTracer;
struct LightProbe;
}  // namespace ge::brush

namespace shooters_edge::server {

// One of the map's entities, as its text placed it.
struct Q2Entity {
  // Times (level time; 0 none): an item's respawn, a mover's next action, the last touch that
  // counted (a field's once a second), a running effect's end and its next tick
  // (q2_targets.hpp).
  double respawn_at = 0;
  int64_t back_frame = -1;  // the rules frame an item showed again (its entity event); -1 none
  double think_at = 0;
  double touched_at = -100;
  double until = 0, tick_at = 0;
  std::string classname, targetname, target, model;
  // The map's movers and triggers (q2_map.hpp).
  std::string message, killtarget, team, pathtarget, noise, map;
  int modelindex = 0;  // `*n` brush models; an item's model in the model list
  int spawnflags = 0;
  // An item (q2_items.hpp's row, -1 none): dropped to the floor at the start, taken by a
  // player's touch, hidden until its respawn in deathmatch, gone otherwise.
  int item = -1;
  int block = -1;  // its block in the map's entity text; -1 one the rules made (a door's field)
  int owner = -1;  // a field's or a trigger's mover
  float speed = 0, accel = 0, decel = 0, wait = 0, lip = 0, delay = 0, random = 0;
  int dmg = 0, health = 0, max_health = 0, sounds = 0;
  int state = 0;         // a mover's: 0 at rest closed, 1 opening, 2 open, 3 closing
  int team_master = -1;  // the first of its team; -1 none
  int think = 0;         // which action (q2_map.cpp)
  int activator = -1;    // the slot that set it going (a train's, a delayed use's)
  int frames = 0;        // the plain move's whole frames left
  float rest = 0;        // the plain move's remainder; the accelerated move's distance left
  float now_speed = 0, next_speed = 0, top = 0, decel_span = 0;  // per frame
  // Past 10 frames a second (`sv_fps`): the tenth's move and turn still to make, spread over its
  // frames (q2_map.cpp's split_push); `split` once they are taken for this tenth.
  sh::Vec3f left, left_turn;
  bool split = false;
  int arrive = 0;  // what arriving does (q2_map.cpp)
  // The single player's other entities (q2_targets.hpp): a count (a counter's, a string's
  // place), the frame shown, the item a key lock needs, the entity a laser aims at.
  int count = 0, frame = 0;
  int needs = -1, aim = -1;
  sh::Vec3f origin, angles;
  sh::Vec3f mins, maxs;    // its box about its origin (a brush model's bounds, a field's box)
  sh::Vec3f dir;           // a mover's or a push's way
  sh::Vec3f closed, open;  // a door's or button's two places (a plat's top and bottom)
  // The move in progress: its velocity (units/s), the place it is going to.
  sh::Vec3f velocity, dest;
  // A turning mover (a rotating door, a rotator): its turn in progress (degrees/s), the angles it
  // is turning to, a door's two angles, and the axis it turns about (a unit vector of angles).
  sh::Vec3f avelocity, adest, closed_angles, open_angles, axis;
  bool hidden = false, gone = false;
  bool coop_taken = false;  // a staying weapon some coop player has taken (coop_pickup_weapons)
  bool solid = false;       // a brush that stops moves and shots (a mover, a wall present)
  bool trigger = false;     // touched when a living player's box overlaps it
  // Drawn as a teleporter's disc though a trigger or a point: 1 the pad (skin 1, its rising
  // specks), 2 its exit (skin 0); 0 neither.
  int pad = 0;
  // The loop it makes while it runs (a mover moving, a speaker on, a pad always): the sound
  // list's index, set as the map starts (q2_map.cpp's `map_loops`); 0 none.
  int loop = 0;
  bool moved = false;  // a plat: it has had a move (its use is spent)
  bool last = false;   // the next push is the move's last
  bool accelerated = false;
  bool turns = false;   // a turning mover
  bool box = false;     // a box that stops what is traced against monsters (a living monster)
  bool on = false;      // a laser's, a conveyor's or a looping speaker's on
  bool corpse = false;  // its box a dead monster's: shots meet it, moves pass it
};

// A monster of the single player (q2_monsters.hpp): its map entity and its mind.
struct Q2Monster {  // NOLINT(clang-analyzer-optin.performance.Padding): few, by name
  int entity = -1;
  int kind = 0;    // its row in the roster
  int enemy = -1;  // the slot it hunts, another monster as -2 - its entity (infighting); -1 none
  float ideal_yaw = 0;
  sh::Vec3f velocity;  // its fall, a blow's push
  bool on_ground = false;
  bool start_solid = false;  // placed inside solid (said a frame after the map's load)
  sh::Vec3f last_sight;      // where it last saw (or heard) its enemy
  double seen_at = -100;
  double attack_finished = 0;  // the next missile attack's earliest time
  double busy_until = 0;       // an attack's wind-up, shots and recovery
  double fire_at = 0, burst_until = 0;
  int attack = 0;  // the attack in progress: 0 and 1 the blows, 2.. the ranged ones
  int burst_frame = 0;
  double pain_at = -100, pain_hold = 0;
  float gravity = 1;        // its share of the world's gravity (a gravity trigger sets it)
  int corner = -1;          // the `path_corner` it walks to
  int combat = -1;          // the `point_combat` it runs to on finding an enemy
  std::string lost_combat;  // a `combattarget` naming nothing, said on finding an enemy
  bool hold = false;        // holds its ground (at its combat point)
  bool ambush = false;      // wakes only by seeing or being hurt
  bool waiting = false;     // hidden until used
  bool dead = false;
  std::string item;  // dropped at death
  // The full game's (q2-monsters-full.md):
  int loops = 0;       // the attack's loops after its first
  bool aimed = false;  // a rail's aim taken, at `aim_at`
  sh::Vec3f aim_at;
  bool leaping = false;  // in the air on a leap; `struck` once it has hurt
  bool struck = false;
  int screen = 0;           // a power screen's points left
  double screen_flash = 0;  // its plate shown till then (level time): it took a blow
  double died_at = 0;       // level time
  double end_at = 0;        // a boss's wreck, a crashed flier: when it ends
  int boom_step = -1;       // a boss's blasts so far
  int patient = -1;         // a medic's corpse (a monster row) it is raising
  int old_enemy = -1;       // and the enemy it had
  double revive_at = 0;     // the cable's moment
  bool claimed = false;     // a corpse a medic is raising
  bool spare = false;       // waits to come out at a boss's death (not yet counted)
  double used_at = -1;      // a figure's use (the commander's body)
  SqMonsterPose pose;       // the frame it shows (q2_poses.hpp)
  // A dodge's duck (q2_monsters.cpp, q2-monsters-full.md § Dodging and ducking): the frame
  // of the duck it plays (-1 none), whether that frame is new this think, the soldiers' duck that
  // shoots, its box down by 32, the hold's end (level time) and the model frame it shows.
  int duck_frame = -1;
  bool duck_fresh = false;
  bool duck_shoot = false;
  bool ducked = false;
  double duck_hold = 0;
  int duck_pose = -1;
  // In liquid (q2_monsters.cpp's `liquids`): the level 0..3 and the liquid's contents, whether it
  // is in, its air's end (level time; a swimmer's out of the water) and the next lava or slime
  // hurt.
  int water_level = 0;
  int water_type = 0;
  bool in_water = false;
  double air_until = 12;
  double liquid_hurt_at = 0;
  double corner_wait = 0;  // held at its path_corner until (level time)
  // A noise heard (no enemy yet): it goes to where it was, stands within 64 and forgets it 5 s on.
  bool sound_target = false;
  sh::Vec3f sound_at;
  double sound_time = 0;
  // Its enemy out of sight: the point it goes to (the last sighting, then trail marks), whether
  // it has lost him, the time of the newest look it has had (a sighting or a mark taken) and the
  // search's start (level time; 0 none): 20 s past it, it goes once straight at him.
  bool lost = false;
  bool on_trail = false;
  bool side_step = false;  // `goal` a step round an obstacle, `real_goal` the point beyond
  sh::Vec3f goal, real_goal;
  double trail_time = 0;
  double search_time = 0;
  double idle_at = 0;  // its idle or search call's next time (level time); 0 not yet set
  // A soldier's shot on the run at first sight: the step of its run (0 to 13), -1 none.
  int run_shoot = -1;
  bool head_shot = false;  // its death blow landed within 4 of its eye: the head-shot death
  int death_run = -1;      // the death run its kind's deaths chose (-1: its poses pick)
  int stomp = -1;          // a tank stamping on its dead enemy: the step of its run, -1 none
  // A flier sliding sideways round its enemy instead of closing (`slide_left` its side).
  bool slide = false, slide_left = false;
  double flies_at = 0;  // flies on its corpse from then for 60 s (level time); 0 not yet, -1 none
};

struct Q2Player {  // NOLINT(clang-analyzer-optin.performance.Padding): few, by name
  ge::classic::Q2State move;
  // How far the movers carried him in the last rules frame (a lift's rise): the window eases its
  // view over the frame instead of showing the jump. Not part of the match's state.
  sh::Vec3f pushed;
  uint32_t last_sequence = 0;
  // `sv_speedcheck` (Quake II's `sv_enforcetime`): the milliseconds of moves he may still run,
  // set to kTimeBudgetMs every kTimeBudgetFrames rules frames; each move takes its msec, and with
  // the check on a move that takes it below 0 is not run.
  int32_t time_budget_ms = 0;
  bool alive = false;
  float health = 0;
  float frags = 0;
  double spawned_at = 0, died_at = 0;  // level time, s
  // The time `kill` (and a change to spectating) counts 5 s from: his last respawn's, 0 (the map's
  // start) after a first placing.
  double respawn_time = 0;
  uint8_t buttons = 0;  // the last command's
  uint8_t pressed = 0;  // the buttons pressed afresh since he died (a respawn wants a new press)
  // Spectating (q2_spectate.hpp): his `spectator` wish, the slot his chase camera follows (-1
  // none), and whether his last command held the up move (a jump's press is its first).
  bool want_spectator = false;
  int chase = -1;
  bool jump_held = false;
  int16_t cmd_angles[3] = {};  // the last command's view, 65536ths of a turn
  bool changed = false;        // placed by the rules since its last move (a spawn)
  // The view the rules turned him to (a spawn), 65536ths of a turn, until his client sends it back
  // as its own (with `Q2ServerOptions::clients_take_view`): the delta angles that turned him
  // are then spent (zeroed), so the turn is not added twice.
  bool forced = false;
  int16_t forced_angles[3] = {};
  sh::Vec3f view_angles;
  SqBody body;             // health, armour, ammo and what he carries (q2_items.hpp)
  int mega = -1;           // the mega health's entity he took, while it counts down; -1 none
  double drained_at = 0;   // level time the mega's count-down last took a point
  std::vector<int> taken;  // the item rows he took, in order (the HUD's pickups, the tests)
  sh::Vec3f mins, maxs;
  float view_height = 0;
  int water_level = 0;
  int32_t water_type = 0;  // the liquid he is in (contents bits)
  bool on_ground = false;
  // The weapon in hand (q2_combat.hpp), stepped once a rules frame: by the first command of the
  // frame that presses attack, else at the frame's end (`stepped` says which).
  SqHand hand;
  bool stepped = false;
  int killer = -1;  // the slot that killed him last; his own for a suicide, -1 the world
  // The powerups in use (level time each ends; 0 none), the silencer's shots left, power armour
  // switched on, and the body's state the frame reads: his air, the next drowning blow and its
  // size, and his fall speed at the last frame's end.
  double quad_until = 0, invulnerable_until = 0, breather_until = 0, suit_until = 0;
  int silencer_shots = 0;
  bool power_on = false;
  double power_flash = 0;  // the screen's plate or the shield's shell shown till then: a blow taken
  double air_until = 0, drown_at = 0;
  int drown_damage = 2;
  float old_fall = 0;
  bool landed = false;  // a landing sounded this frame (no running step with it)
  // His body's sounds: the blows this frame (with what armour took) for its pain cry, the last
  // cry's time, the water level at the last frame's end, the rebreather's breath in turn.
  int frame_damage = 0;
  int killing_blow = 0;  // the points of the blow that killed him (his gibs' throw)
  double pain_sound_at = -100;
  int old_water_level = 0;
  bool breath_second = false;
  bool gibbed = false;        // died under −40: his body is gone (no model)
  int ground = -1;            // the world row he stands on (0 the world, a mover's row), -1 none
  double windfly_at = -10;    // the last push's sound
  int64_t noise_frame = -10;  // the rules frame of his last noise a monster may hear (his weapon)
  sh::Vec3f noise_at;
  double hostile_until = 0;
  sh::Vec3f trail_last;  // where he stood at the last monster frame (a trail mark's place)    //
                         // fired lately: a monster near him sees him behind it till then
  int64_t impact_frame = -10;  // and of his shots' last impact (a wall struck, a burst)
  sh::Vec3f impact_at;
  // The campaign's (q2_campaign.hpp): the help news seen and its beeps, the screen open; the
  // body carried to the next level and the one he entered this level with.
  int help_seen = 0, help_unread = 0;
  // The screen up over his bar (q2_screens.hpp: none, the scores, the help computer) and the
  // last layout program sent him, which his client keeps until the next.
  uint8_t screen = 0;
  std::string layout;
  // The inventory open (drawn while he lives) and its counts as he opened it, by item number.
  bool inven_open = false;
  std::vector<int16_t> inventory;
  bool carried = false, has_entered = false;
  ge::shooter::MidairTally midair;  // his counted midairs this level (plugins/shooter/midair.hpp)
  SqBody carry, entered;
  SqPose pose;  // the frame his body shows (q2_poses.hpp)
  // The status bar's (q2_bar.hpp): the item last picked up (its number, 0 none) and when, and
  // the blows taken in rules frame `hit_frame` (bit 1 to his health, bit 2 to his armour).
  int picked = 0;
  double picked_at = -100;
  int64_t hit_frame = -1;
  int hit_bits = 0;
  // The screen's tint (q2_blend.hpp): this frame's blows by what took them (blood, armour, power
  // armour), the damage flash and its colour, the pickup flash, the pickups counted so far, and
  // the tint sent (0..1 each).
  int blow_parts[3] = {};
  float damage_alpha = 0, bonus_alpha = 0;
  float damage_rgb[3] = {};
  size_t taken_seen = 0;
  float blend[4] = {};
  float blend_apart[4] = {}, damage_blend[4] = {};  // the tint without the flash, the flash alone
  // His view (plugins/classic/q2_view.hpp): its state, this frame's offset and kick, and the
  // angles the dead view lies at.
  ge::classic::Q2ViewState view;
  ge::classic::Q2View seen;
  sh::Vec3f dead_angles;
  // The player's commands (q2_commands.hpp): the cheats on him, the weapon held before this one
  // (`weaplast`) and the one held at the last frame's end, his chat's last ten times and the level
  // time his flood lock ends.
  bool god = false, notarget = false, noclip = false;
  bool skull = false;  // thrown apart, his head shows as the skull (else his own head)
  int last_weapon = -1, held_weapon = -1;
  double said_at[10] = {};
  int said_next = 0;
  double quiet_until = 0;
  double load_asked_at = -1;  // single player's death: when a button asked for the load menu
};

// A shot in flight (a bolt, a grenade, a rocket, the BFG's ball), moved each rules frame.
// The loops a shot in flight or a player carries (q2_fight.cpp): a blaster bolt's hiss, a
// rocket's roar, a hand grenade's fuse (thrown or held), the BFG's ball, the railgun's and the
// BFG's hum while held, the hyperblaster's and the chaingun's spin while firing, a body in lava or
// slime frying.
enum class SqLoop : uint8_t { bolt, rocket, fuse, bfg_ball, rail_hum, bfg_hum, hyper, chain, fry };
inline constexpr int kSqLoops = 9;

struct Q2Missile {
  bool live = false;
  SqShot kind = SqShot::bolt;
  int owner = -1;  // the shooter's slot
  int weapon = 0;  // the weapon that fired it (the obituary's words)
  sh::Vec3f origin, velocity;
  double dies_at = 0;  // level time it goes (a grenade's fuse explodes it)
  int damage = 0;      // a direct hit's
  int radius_damage = 0;
  float radius = 0;
  bool resting = false;  // a grenade come to rest on a floor
  int from = -1;         // the map entity that fired it (a monster); -1 a player
  int burst = 0;         // a BFG ball's final burst's strength (0: its direct hit's)
  float reach = 1000;    // and its reach
  float fired_z = 0;     // a rocket's: the height its shooter stood at when he fired (midair)
  int effects = -1;      // its effect bits on the wire (its trail and light); -1: by its kind
};
inline constexpr int kQ2Missiles = 64;  // the rows the world keeps for shots in flight

// An item a player dropped (by `drop`, or at death): thrown, falling to rest, taken on touch.
struct Q2Drop {
  bool live = false;
  int item = -1;   // q2_items.hpp's row
  int count = 0;   // ammo's: how much it gives (0 the item's own amount)
  int owner = -1;  // the dropper's slot, who may take it back only after `takeable_at`
  sh::Vec3f origin, velocity;
  double takeable_at = 0;
  double dies_at = 0;     // level time it vanishes; 0 never
  double power_left = 0;  // a dropped timed powerup's time left, s
  int shots = 0;          // a dropped silencer's shots left
  bool resting = false;   // come to rest on a floor
  bool bare = false;      // a weapon thrown by `drop`: taking it gives no ammunition
};
inline constexpr int kQ2Drops = 32;  // the rows the world keeps for dropped items

// A body left where a player died, kept when he respawns: up to 8 stay, the oldest reused. A blow
// that takes it under −40 throws it apart.
struct Q2Body {
  bool live = false;
  sh::Vec3f origin;
  float yaw = 0;
  float health = 0;
  int frame = 0;  // the death's last, which it keeps
};
inline constexpr int kQ2Bodies = 8;

// A piece of a body thrown apart (q2_gibs.hpp): flying, spinning, then lying until it goes.
struct Q2Gib {
  bool live = false;
  int model = 0, skin = 0;  // the model list's index
  sh::Vec3f origin, velocity;
  sh::Vec3f angles, spin;  // degrees, degrees a second
  double dies_at = 0;      // level time it goes
  bool metal = false;      // bounces (flesh stops where it lands)
  bool resting = false;
  bool debris = false;  // a broken thing's chunk: no blood trail
};
inline constexpr int kQ2Gibs = 64;      // the rows the world keeps for pieces, the oldest reused
inline constexpr int kSqGibModels = 7;  // q2_gibs.cpp's pieces: meat, bone, chest, metal, ...

struct Q2ServerOptions {
  uint32_t seed = 1;
  // The clients take the server's view as their own when it turns them (the QuakeWorld protocol's
  // clients: the window's listen server and the QuakeWorld endpoint), so a turn's delta angles are
  // spent once a client sends the turned view back (`Q2Player::forced`). False: Quake II's
  // own model, where a client keeps its angles and the delta stays.
  bool clients_take_view = false;
  // pm_speedjump's default: the easy jump's share (0 off). The runner keeps pm_speedjump in
  // `sv.set.move.speedjump` and a client's `cl_speedjump 1` ask in `sv.clients[slot].speedjump`, as
  // the classic runners do; it reads a command's jump button as the up move
  // (`ge::classic::q2_command`).
  float speedjump = 0;
};

// The player's model, last in the model list.
inline constexpr const char* kQ2PlayerModel = "players/male/tris.md2";

// The time budget `sv_speedcheck` holds a player to (Q2Player::time_budget_ms).
inline constexpr int32_t kTimeBudgetMs = 1800;
inline constexpr int64_t kTimeBudgetFrames = 16;

// What a change to or from spectating is checked against, the network server's (q2_spectate.hpp):
// the players' password, the spectators' password ("" or `none`: none) and the spectators' limit.
struct Q2Gate {
  std::string password, spectator_password;
  int max_spectators = 4;
};

struct Q2Server : RulesCommon {
  Q2ServerOptions opt;
  Q2Gate gate;
  int64_t moves_held = 0;  // moves past a budget not run (`sv_speedcheck`)
  std::shared_ptr<const ge::brush::Map> map;
  std::unique_ptr<ge::brush::BrushTracer> tracer;
  // The map's baked light by the floor under a point: how dark a player stands (a monster
  // cannot spot one whose light is 5 or less, 150 times the brightest channel).
  std::unique_ptr<ge::brush::LightProbe> light;
  // The map's world as the pool's traces read it (`sv.brushes`, set as the world is mirrored).
  sh::Solid world_solid;
  std::vector<Q2Entity> entities;
  Q2Player players[sh::kMaxClients];
  ge::core::Rng rng = ge::core::make_rng(1);
  int64_t frame_number = 0;  // the tenths of a second the level has run (the 10 Hz world's frames)
  int pain_turn = 0, death_turn = 0;  // the players' pains and deaths, in turn (q2_poses.hpp)
  double level_time = 0;              // frame_number × 0.1 + sub × step, s
  // `sv_fps` as the level started: the rules' frames a second, 10 to 60 by tens. Every frame steps
  // the movers, the shots, the drops and the pieces by `step`; the frame that ends a tenth
  // (`tenth`) also steps the 10 Hz world (monsters, items, the players' hands, bodies and views,
  // the scoreboards), so at 10 every frame is a tenth and nothing differs.
  int fps = 10;
  int sub = 0;              // the frames run since the last tenth
  float step = 0.1f;        // a frame, s
  bool tenth = true;        // this frame ends a tenth
  int64_t fast_number = 0;  // every frame the level has run (frame_number at 10)
  double host_time = 0;     // the host's clock, s
  int next_userid = 1;
  // LAG COMPENSATION (the network server's `sv_antilag`, net/qw_antilag.hpp): while a
  // player's hitscan is traced (`lag_shooter` his slot), and with `lag_missiles` while his
  // missiles fly, each other player is met where `lag_place` says the shooter saw him, in the box
  // he had then; none set, or false: where he is.
  bool (*lag_place)(void* ctx, int shooter, int target, sh::Vec3f& at, sh::Vec3f& mins,
                    sh::Vec3f& maxs) = nullptr;
  void* lag_ctx = nullptr;
  int lag_shooter = -1;
  bool lag_missiles = false;
  Q2Missile missiles[kQ2Missiles];
  int weapon_models[kSqWeaponCount] = {};   // each weapon's view model in the model list
  int carried_models[kSqWeaponCount] = {};  // each weapon as another player sees it carried
  int missile_models[kSqShotKinds] = {};    // by `SqShot`
  int loop_sounds[kSqLoops] = {};           // the loops a shot or a player carries, by `SqLoop`
  std::vector<std::string> obituaries;      // every death's line, in order (the tests, the lab)
  std::vector<sh::Packet> rj_packets;       // a frame's packets as the rocket jump helper left them
  Q2Drop drops[kQ2Drops];
  Q2Body bodies[kQ2Bodies];
  Q2Gib gibs[kQ2Gibs];
  int next_gib = 0;                   // the gib slot the next piece takes
  int gib_models[kSqGibModels] = {};  // each piece's model in the model list
  int next_body = 0;                  // the body slot the next respawn fills
  std::vector<int> item_models;       // each item row's world model in the model list (the drops')
  // Classic remix (`sv_wear`): the other game's things the rules wear, renamed as the players are
  // told them (profiles/remix_arsenal.hpp's `Look`), and each model's look by its index (null: its
  // own). Read as a map starts.
  std::span<const profiles::Look> looks;
  std::vector<const profiles::Look*> model_looks;
  // The players' bodies' physics (`sv_physics`): Quake II's own, or Quake's (classic
  // remix's `cr_physics`): its ground, air, landing and water, and no harm from a fall. Read as a
  // map starts.
  bool q1_physics = false;
  // The intermission: level time it began (0 none) and the map it leads to; everyone stands frozen
  // at its spot, and after 5 s any button leaves.
  double intermission_at = 0;
  std::string next_map;
  // What the window shows before the next map (a unit's movie, the game's last picture: `+`
  // joined, client/cinematic.hpp), the exit's and then the one handed over with `pending_map`,
  // which the window takes; `game_over`: the last exit was used and no map follows.
  std::string next_cinematic, cinematic;
  bool game_over = false;
  // A use put off by its `delay`: the entity whose targets fire, at a level time, by a slot.
  struct Delayed {
    int entity;
    double at;
    int activator;
    bool burst = false;  // a target_explosion's own blast put off (its targets fire with it)
  };
  std::vector<Delayed> delayed;
  int secrets = 0, secrets_found = 0, goals = 0, goals_found = 0;
  // The game across its levels (q2_campaign.hpp): the help computer's two messages and how
  // often they changed, the spawn point this level was entered by and the one the exit names, the
  // cross-level bits, whether the exit ends a unit.
  std::string help[2];
  int help_changed = 0;
  std::string spawn_name, next_spawn;
  uint32_t cross_bits = 0;
  bool next_unit = false;
  bool exit_now = false;  // an exit was used: the intermission starts at the frame's end
  // The area portals by number, open or shut (a door opens those its target names), and the map's
  // areas flooded over the open ones, redone when they change (q2_map.hpp's map_areas_connected).
  std::vector<uint8_t> portals;
  mutable std::vector<uint8_t> flooded_portals;
  mutable std::vector<int32_t> area_groups;
  mutable const ge::brush::Map* flooded_map = nullptr;
  // The monsters (q2_monsters.hpp), the level's count, and who saw a player last: the map
  // entity and its frame, and the player the monsters look at this frame.
  std::vector<Q2Monster> monsters;
  int monsters_total = 0, monsters_killed = 0;
  int sight_entity = -1;
  int64_t sight_frame = -10;
  int sight_client = -1;
  // The players' trail (single player and coop): 8 marks in a ring, one dropped where a player
  // stood last frame once he can no longer see the newest; `trail_head` the oldest. A monster that
  // lost sight of him follows the marks newer than its own last look (q2_monsters.cpp).
  sh::Vec3f trail_at[8];
  double trail_time[8] = {};
  float trail_yaw[8] = {};
  int trail_head = 0;
  // Midair (`k_midair`): the rocket whose blow is being dealt (its shooter's height when he fired;
  // NaN none), and the frags and rise of the counted hit its kill scores.
  float rocket_from_z = std::numeric_limits<float>::quiet_NaN();
  // How the blow being dealt landed (`kSqHow*`, q2_fight.hpp): an obituary's words tell a rocket's
  // direct hit from its blast.
  int blow_how = 0;
  int midair_frags = 0;
  float midair_rise = 0;
  // The C++ mods' `self` and `other` while a hook runs (q2_mods.hpp's rows; 0 none).
  int32_t mod_self = 0, mod_other = 0;
  // The levels of this unit left and kept (q2_save.hpp): each map's name and its state's
  // bytes, read back over the map when it is entered again.
  std::vector<std::pair<std::string, std::vector<uint8_t>>> kept;
  // A saved game being loaded (q2_save.hpp): its level's bytes (empty: the map's kept state, or
  // a fresh start) and no 10 s run before the players come in.
  bool loading = false, load_over_game = false;
  std::vector<uint8_t> load_level;
  std::vector<uint8_t> load_file;  // the save a load asked for, read as the next map starts
  std::string load_name;
  // The slots a load put back in their bodies (by bit): the player connecting to one takes it over.
  uint32_t restored = 0;
  // The players already told entered on this map (at its load): their `begin` on it says it
  // no more, so each arrival is said once.
  uint32_t entered_told = 0;
  // The autosave the rules made as this level was entered (empty: none), for the host to keep.
  std::vector<uint8_t> autosave;

  Q2Server();
  ~Q2Server();
};

std::unique_ptr<Q2Server> make_q2_server(const Q2ServerOptions& opt);

// Quake II's server as a runner, owning it (form "Quake II").
RulesRunner q2_runner(std::unique_ptr<Q2Server> s);
// Quake II's server behind a runner, or null for another form.
Q2Server* q2_of(const RulesRunner& r);
// A thrown or falling thing's speed held to `sv_maxvelocity` along each axis (before its gravity).
void q2_hold_speed(const Q2Server& s, sh::Vec3f& velocity);

// One rules frame `frames` times with every player out of the world (a level settling before
// anyone enters it).
void q2_frames_alone(Q2Server& s, int frames);
// The world's lists and rows built again from the rules' state (a level read back over its map).
void q2_rebuild(Q2Server& s);
// Player `slot` put in the world at the level's spawn point, as a map's start puts him.
void q2_spawn(Q2Server& s, int slot);
// The runner's calls on the server itself: a console line, a key of a player's user info, a
// player connecting.
bool q2_console(Q2Server& s, const std::string& line);
void q2_user_info(Q2Server& s, int slot, const std::string& key, const std::string& value);
void q2_connect(Q2Server& s, int slot, const std::string& name);

// The air acceleration the move runs with: `sv_airaccelerate` in deathmatch, 0 otherwise (a
// client's easy jump picks its air law by it: shooter/speedjump_assist.hpp's q2_jump_rules).
float q2_air_accelerate(const Q2Server& s);

// The feel every player's move runs on (profiles::MoveFeel): the stock ground and water with the
// air acceleration above, or Quake's (`sv_physics q1`). What our server tells a
// joined client that asks (net/q2_net_server.hpp, svc 122), so its prediction runs the same
// move; a rule that changes the move changes it here.
profiles::MoveFeel q2_move_feel(const Q2Server& s);
// The feel `slot`'s move runs on: the above, but the stock landing pause and surfacing lift for a
// client that is not told the feel (no bit 64 in his `*ge_ext`), unless `sv_landjump` or
// `sv_waterjump` is 2. A bot predicts nothing and takes the feel as it is.
profiles::MoveFeel q2_move_feel(const Q2Server& s, int slot);

// A player's state as a client's input aid reads it (the easy jump: shooter/speedjump_assist.hpp):
// the velocity, the ground, the water and walking (`normal`) while alive and playing, else `none`.
ge::shooter::MoveState q2_aid_state(const Q2Server& s, int slot);

// A world row as the other game's thing it wears draws it (`sv_wear`): its model's skin and the
// lift to where that game stands it, a brush model never turning. Nothing while the rules wear
// their own.
void sq_dress(const Q2Server& s, WorldEntity& e);

}  // namespace shooters_edge::server
