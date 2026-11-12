// THE PILOT: a route on the nav graph (nav.hpp) played as a player plays it, one command at a time,
// through the same move. It reads only what a client knows of its own player (where it stands,
// how fast it moves, whether it is on the ground) and of the movers it can see (their boxes now),
// and writes only a command: the view's yaw, the forward and side axes, the jump. So it can do
// nothing a player cannot, and whatever the move does to a person it does to the pilot.
//
// Each command (`pilot_step`), the link it is on (from `route[k]` to `route[k + 1]`) decides:
//   walk, drop  pure pursuit: the view turned at the point `look` units along the route (carried
//               through the level walks that follow, so no stop at each node), steered from where
//               the player will be `lead_s` ahead (late hands); forward pressed by how much the
//               view already faces the way, and slowed into the goal. A node is passed within
//               `arrive` units, or within `corner` of the next link's line on its height.
//   jump        a jump link is proven from its start moving toward the target at its own `speed`.
//               The walk into the start runs at the start on the jump's line, slowing to that
//               speed before the run-up; the jump is pressed at the start, on the line, moving
//               0.9 to 1.15 of it toward the target. Arriving off the line or past the start
//               without the speed, it goes back to the RUN-UP point (`run_up` units behind the
//               start), stands there turned down the jump, and runs again. In the air the view
//               turns at the target with forward full, as the proof did.
//   ride        stand still on the mover's top until it carries the player within 2 units of the
//               top node's height (wherever on the top it stands), then on.
//   a mover     a link onto a ridden mover waits until the mover is back at rest; a link a door's
//               box now meets waits. The pilot waits stepped clear of the mover's box and of its
//               travel, out of its nearer side (a plat stopped on a player stands still). A wait
//               at a door longer than `wait_s` bars that door and plans again round it.
//   strafe      a stretch of walks on level ground in a near-straight line longer than
//               `strafe_min` units is run as strafe jumps: the jump pressed on every landing, in
//               the air the view held along the velocity and the side axis pressed to the full
//               (the air's 30 unit/s cap adds the most speed with the wish at right angles to the
//               velocity), its sign the side the target is on; left `strafe_end` units before the
//               stretch ends.
//   hop         with `hop` set (an expert) the strafe gives way to the BUNNY HOP, as a recorded
//               pro moves (docs/movement.md § The bunny hop): any stretch of walks and drops with
//               no mover or jump, `hop_min` units or more, curves and stairs too. In the air the
//               side key alone and the view led on the velocity by the move's most turn (the
//               most gain); the side the route `hop_look_s` ahead lies on, switched past
//               `hop_hold` after `hop_side_s` (at once past `hop_swing`); the back key when the
//               aim is `hop_brake` off. The jump is let go in the air and pressed on the
//               landing's command (no friction taken). With the map's solids in the sense, a
//               side whose arc meets a wall within `hop_see_s` gives way to the other. Left
//               `hop_end` units before the stretch ends, or landing under `hop_slow` x maxspeed.
//               `hop_simple` (an average player) holds forward and the side the route turns to.
// STUCK: no nearer the target for `stuck_s` on the ground plans again from the node nearest where
// it stands; LOST (more than `lost` units from the link, on the ground) the same. Every decision
// is recorded on the `ai::Why` handed in (`link`: the mode chosen, with its facts), and the intent
// before the hands (`hands`: yaw, forward, side).
//
// THE HANDS (`ai::Hands`): yaw, forward and side pass through them, the jump untouched.
// `none` presses what the pilot wants; `human` reacts about 0.15 s late (too late to hop);
// `practised` (an expert's) no reaction or settle; `steady` (an average's) a little late.
//
// Allocates only when it plans; deterministic to the bit (angles through core's libm). Drawn by
// the `se-pilot` and `se-hop` labs.
//
// ge:summary The pilot: a nav route played one command at a time through the player move
// ge:keywords pilot bot route follow usercmd command jump run-up ride plat door wait strafe-jump
// ge:keywords bunny-hop air-strafe stuck replan lost hands why pilot_step pilot_go pilot_source
// ge:keywords rocket-jump flip back-rocket-jump strafe-turn flip_keys headroom lintel last-jump
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "ai/hands.hpp"
#include "ai/path.hpp"
#include "ai/why.hpp"
#include "core/prng.hpp"
#include "shooter/circle_jump.hpp"
#include "shooter/cmd_source.hpp"
#include "shooter/look_ahead.hpp"
#include "shooter/nav.hpp"

namespace ge::shooter {

struct PilotSettings {
  float arrive = 12;       // a node is reached within this across, units
  float corner = 24;       // a walk turns at the next node's target within this of its own
  float stuck_s = 1;       // no nearer the target for this long on the ground: plan again
  float lost = 96;         // this far off the link on the ground: plan again
  float wait_s = 5;        // a mover in the way this long is barred and the way planned round
  bool strafe = true;      // run long straight stretches as strafe jumps
  float strafe_min = 448;  // the shortest stretch strafed, units
  float strafe_end = 160;  // strafing stops this far before the stretch's end
  float line = 20;         // a stretch's nodes lie within this of its straight line, units
  float look = 64;         // a walk aims this far along the route, units
  float lead_s = 0;        // and steers from where it will be this long ahead (late hands), s
  // THE HOP (an expert's bunny hop, off by default; replaces the strafe when on):
  bool hop = false;     // hop the route's walks and drops, round its curves too
  float hop_min = 256;  // the shortest stretch hopped, along the route, units
  // STAIRS WALKED (`stairs`): a hop's stretch ends at a walk that climbs `hop_climb` units or more
  // (a stair, a steep ramp): a jump up a stair loses speed; people walk up and hop from the top.
  // STAIR CORNERS (`slopes`): a walk link up or down a stair no steeper than 45 degrees is level
  // to the walk (its nodes passed and looked past as on the flat), not only one within a step
  // (a stair of 20 a tread between nodes 32 apart was reached node by node, overshot at a turn).
  bool slopes = false;
  // THE RUN-UP STEPPED BACK (`backstep`): a jump's run-up point is gone to with the view kept down
  // the jump (the back and side keys, as a person steps back into line), and a run-up already
  // moving down the jump's line behind its start runs on from there, not from a stop.
  bool backstep = false;
  // `runupkeys`: the stepped-back run-up's keys pressed for the view as it will be when they land,
  // and a run-up or a jump's run fallen well under its start (or into water) planned again.
  bool runup_keys = false;
  // `gapbuild`: a hop's first jump into a gap trick over water is a circle jump aimed across at
  // its landing, taken as soon as a jump from there lands across; short of the lip a landing runs
  // on along the ground (straight, curving or side to side, rolled through the hands:
  // pilot_gaps.hpp `gap_run`) to where a jump does, and far out it runs on rather than gives up.
  bool gap_circle = false;
  bool hop_stairs_walked = false;
  float hop_climb = 10;
  // THE BRINK (`brink`): a hop's or a strafe's stretch ends at a link along a lip over lava or
  // slime (`NavLink::brink`): an arc in the air there comes down in it.
  bool brink = false;
  float hop_end = 96;       // hopping stops this far along the route before the stretch's end
  float hop_look_s = 0.3f;  // the hop aims at the route this long ahead at its speed, s
  float hop_hold = 12;      // the side holds until the aim is this far off the other way, deg
  float hop_side_s = 0.2f;  // and for at least this long (a hand's rhythm), unless the aim is
  float hop_swing = 45;     // this far off: then the side it is on at once, deg
  float hop_brake = 70;     // the aim this far off the velocity: the back key pressed too, deg
  float hop_slow = 0.75f;   // landed under this share of maxspeed: the hop ends (run instead)
  float hop_see_s = 0.12f;  // the arc ahead checked for a wall this long, s (with a world)
  // The SIMPLE hop (an average player's): forward held with the side key of the way the route
  // turns, the view kept 45 degrees outside the velocity so the two keys' wish lies across it;
  // no side switch to weave, no look ahead for walls, no brake.
  bool hop_simple = false;
  // THE HEADROOM (`headroom`): a hop's jump not pressed when its flight would meet a ceiling over
  // the route ahead (a lintel over a stair down): run on under it along the route, jump past it;
  // and the stretch's last jump one whose flight lands before the stretch ends (a climb, a lip).
  bool hop_headroom = false;
  // THE LIPS HOPPED (`liphop`): a hop's stretch runs on along a lip over lava or slime (a `brink`
  // link) instead of ending there: the brink guard holds the feet off it in the air as on the
  // ground, so the hop needs no stop of its own (hops along dm2's pits: share 9-11 % -> 14-15 %;
  // head to head ~50 frags a set: kept for the speed, which passes for a person more than frags).
  bool hop_lips = false;
  // THE CIRCLE JUMP (`circlejump`, circle_jump.hpp): a hop's first jump taken out of a curve on
  // the ground, forward and a side key, the view on the velocity, as people's are (their standing
  // jumps take off at ~430): until `circle.target` speed, `circle.most_s` or past the route's line.
  bool hop_circle = false;
  CircleSettings circle;
  // ROCKET JUMPS (nav.hpp `NavKind::rocket`): planned only while `jumper.rockets` is set (the
  // caller's say: the launcher in hand, its rockets, the health for the blast), those of its
  // level and at its price (`NavJumper`); played as a jump, the view
  // `rocket_pitch` down (or the link's own, `NavLink::pitch`: the one its proof landed) and the
  // trigger pressed with the jump; a flip (`NavLink::back`) turned into from as long before its
  // start as the view's hand takes to turn round (its delay and its slew over 180 degrees), its
  // run-up that much longer, and `rocket_back_pitch` down behind.
  NavJumper jumper;
  float rocket_pitch = 80;
  float rocket_back_pitch = 55;
  // AIR STEERING (`air` below); off, a flight is flown with the view at its target and forward.
  bool air_steer = true;
  float swim_arrive = 24;  // a swim node is reached within this, units (any way, not across)
  // A POOL'S FLOOR SWUM: a swimmer (waist deep, off the ground) on a walk between a pool's floor
  // nodes is anywhere in the water over them, not on the floor: it reaches and passes them by
  // the distance across, up to `floor_swim_up` over them; and it plans again when no nearer or
  // off the way as a walker on the ground does (on a walk whose ends lie over `lost_under` above
  // it, it was knocked or fell into the water under the way). Off: the floor's nodes are reached
  // standing on them, and a swimmer on a walk never plans again by itself.
  bool floor_swim = true;
  float floor_swim_up = 96;
  float lost_under = 48;
  // THE LANE: a person does not walk the route's line. On a level walk with room either side (the
  // graph's `nav_room`), the walker keeps `lane` of that room off the line, to one side or the
  // other: a side and an amount drawn every `lane_min_s`..`lane_max_s` off its own stream, eased in
  // over `lane_ease_s`, back to the line before a jump, a drop, a mover, water or a narrow place.
  float lane = 0;  // 0: on the line
  float lane_min_s = 1.2f, lane_max_s = 3.0f, lane_ease_s = 0.8f;
  // THE WALK'S LOOK AHEAD (`ahead`, with a world): a proven link is clear, the line a walker runs
  // round a corner at 320 u/s is not. Two halves: the AIM pulled back along the route (the look
  // halved, its lane kept and then dropped) until the player's box reaches it straight; and THE
  // WALK FORESEEN: where the box swept along the velocity meets a wall within the hands' delay and
  // `ahead_s`, the keys are flown through the move itself (player_move over the map's solids: the
  // keys already pressed for the delay, then the pursuit's, the turn braked, the next node, or no
  // keys) and a way that does not bump is pressed, the nearest to the aim (a person knows how
  // their body meets the room). dm2, 24 ten-minute seeds, one side with it: its wall bumps 12.0 ->
  // 9.8 a bot-minute, its speed no lower, head to head even (+0.4 frags a match over both sides).
  bool ahead = false;
  float ahead_s = 0.25f;  // the walk's flights foreseen this long past the hands' delay, s
  // OTHER PLAYERS' BOXES in the walk foreseen (`bodies`, with `ahead`): the players the sense
  // hands over (`PilotSense::others`) flown as boxes along their velocities, so a way that runs
  // into a mate or an enemy is a bump, and one standing on the walk's line is walked round
  // (look_ahead.hpp `body_on_way`), or waited for where there is no room; off, the walk knows the
  // map's solids alone. dm2, 24 ten-minute seeds, one side with it: its player bumps 5.4 -> 4.4 a
  // bot-minute, its speed +6 u/s, head to head +1.6 frags a match over both sides; on both sides
  // player bumps 5.5 -> 4.1 and speed 174 -> 182.
  bool ahead_bodies = false;
};

// `swim`: on a swim link (nav.hpp), the view pitched at the next node, forward held, jump to rise;
// onto a dry node out of the water by the climb the nav proved (swim.hpp `swim_out`).
// `air`: in a flight (a jump's, a rocket jump's, a grenade jump's) STEERED as a player steers in
// the air: the aim its target, or near it a point a little past it toward the node after it when
// the route walks on (the momentum carried round the corner it turns there); off the velocity's
// line, the side key toward the turn (pressed by how far off) and the view led on the velocity by
// the move's turn, as the hop does (the air's 30 unit/s cap turns a fast flight only with the wish
// across it: forward does nothing past 30 units/s); on the line, the view along the velocity and no
// key; slow, or the aim behind, forward at it as the proof flew. A flip's view turns back to its
// target first (`flip_keys`). `grenade`: a GRENADE JUMP (nav.hpp `NavKind::grenade`), on the
// ground: to its start and still there, the view on the target and `NavLink::pitch` down, the
// grenade fired (only while the sense says the launcher is in hand), then to the link's `lead` past
// where it will lie (the nav's `grenade_at`, with the world), and jump as the fuse runs out (the
// jump key passes the hands at once); the flight is a jump's.
enum class PilotMode : uint8_t {
  idle,
  walk,
  run_up,
  jump,
  air,
  wait,
  ride,
  strafe,
  arrived,
  hop,
  swim,
  grenade
};
inline constexpr int kPilotModes = 12;  // PilotMode's count: a table indexed by mode is this long
const char* pilot_mode_name(PilotMode m);

enum class PilotHands : uint8_t { none, human, practised, steady };
// The hands' axes: yaw (degrees), forward and side (shares of maxspeed), jump.
std::vector<ai::AxisHands> pilot_hands(PilotHands profile);
// How far ahead a pilot with these hands steers from (their delay), s.
float pilot_lead_s(PilotHands profile);

// What the pilot reads before each command: its own player and the movers' boxes now.
struct PilotSense {
  Vec3f origin, velocity;
  bool onground = false;
  int waterlevel = 0;  // 0 dry .. 3 eyes under
  float maxspeed = 320;
  std::span<const NavMover> movers;  // each mover's entity and box as it stands now
  // The map's solids, when the pilot may look ahead through them (the hop checks its arc for a
  // wall, as a player sees the room); null: the hop flies blind.
  const MoveWorld* world = nullptr;
  bool grenade_ready = true;  // the grenade launcher in hand: a grenade jump fires only then
  bool rocket_ready = true;   // the rocket launcher in hand: a rocket out of a pit fires only then
  // The other players alive near, as the client was sent them (look_ahead.hpp `OtherBox`): the
  // walk foreseen flies them as boxes (`ahead_bodies`).
  std::span<const OtherBox> others;
};

struct PilotStats {
  int32_t plans = 0, stuck = 0, lost = 0, waits = 0, barred = 0, jumps = 0, run_ups = 0;
  int32_t rides = 0, strafes = 0;
  int32_t hops = 0, hop_jumps = 0;  // hop stretches entered, jumps pressed on a landing in them
  int32_t circles = 0;              // hops started by a circle jump
  float circle_speed = 0;           // their takeoff speeds, summed, units/s
  int32_t rocket_jumps = 0;         // rocket jumps fired
  int32_t flips = 0;                // of them, taken as the flip
  int32_t grenade_jumps = 0;        // grenades fired for a grenade jump
  int32_t escapes = 0;              // rockets fired out of a pit's liquid (escape.hpp)
  int32_t escapes_out = 0;          // and of them, landed on their ledge
  int32_t gap_jumps = 0;            // gap tricks jumped out of a hop (nav_gaps.hpp)
  int32_t gap_landed = 0;           // and of them, landed by their landing node
  int32_t gap_refused = 0;          // gaps given up at the lip: no jump from there lands across
  float gap_speed = 0;              // the gap jumps' takeoff speeds across, summed, units/s
  double time_s = 0;                // the commands' time since the last pilot_go
};

struct Pilot {
  const NavGraph* g = nullptr;
  PilotSettings set;
  std::vector<int32_t> route;  // the nodes; empty: none
  size_t k = 0;                // on the link route[k] -> route[k + 1]
  int32_t goal = -1;
  PilotMode mode = PilotMode::idle;
  float yaw = 0;                 // the view's yaw last pressed, degrees, unwrapped
  float best = 0, best_s = 0;    // the nearest the target has been and when (the stuck check)
  float wait_s = 0;              // waiting at a mover this long
  int32_t strafe_end_node = -1;  // the route index a strafe or hop stretch ends at
  int8_t strafe_side = 0;        // the side axis's sign in the air, -1 left, 1 right
  float side_s = 0;              // the hop's side held this long, s
  float circle_s = -1;           // on the circle jump's curve this long, s (-1: not on it)
  int8_t circle_side = 0;        // and the side key it curves on (0: not chosen yet)
  bool jump_pressed = false;     // the jump button in the last command
  bool airborne = false;         // left the ground on the current jump link
  float grenade_s = -1;          // a grenade jump's: since its shot, s (-1: not fired yet)
  Vec3f grenade_stand;           // and where the jumper stands for its blast
  float grenade_yaw = 0;         // and the view's yaw at the shot, held through the fuse
  int32_t barred[4] = {-1, -1, -1, -1};      // doors found shut: planned round
  std::vector<NavExtraLink> cut;             // links an open door's leaf stands across: round
  std::vector<NavExtraLink> short_jumps;     // jumps missed (short, or no nearer), one entry a time
  std::vector<NavExtraLink> failed;          // those missed twice: never planned again
  int32_t try_from = -1, try_to = -1;        // the jump being lined up for
  int tries = 0;                             // and the run-ups at it
  int32_t script_from = -1, script_to = -1;  // the scripted way being flown (nav_script.hpp)
  size_t script_step = 0, script_seen = 0;   // its step aimed at, and the furthest yet
  bool script_done = false;                  // its last step reached
  const char* because = "";                  // the last command's reason, for a trace
  // A rocket jump's shot ahead (this command's, read by the view's hand): the view it fires with
  // (pitch 0: none ahead), the time until it, s (0: this command), and whether this command fires
  // it. Under a hand that flies the view itself (`shot_hand`, set by its owner), the shot waits
  // until the owner says the view is there (`shot_ready`, set each command).
  float shot_pitch = 0, shot_yaw = 0, shot_in_s = 0;
  bool shot_now = false;
  bool shot_hand = false, shot_ready = true;
  // OUT OF A PIT (escape.hpp, `NavLink::escape`): the escape under way, how long it has backed
  // (-1: still swimming to its takeoff), and whether this command is one of it (the backing or
  // its flight: the view's owner keeps the swim's 3D wish off then).
  int32_t escape_on = -1;
  float escape_back_s = -1;
  bool escaping = false;
  bool plan_on_landing = false;  // a goal given in its flight: planned once it lands
  // A GAP TRICK in the hop's stretch (nav_gaps.hpp, `NavLink::gap`): the route index of its
  // start (-1: none), whether the flight under way is its jump, and whether the move said the
  // flight under way lands safe (then nothing else brakes it: `brink_free`).
  int32_t gap_k = -1;
  std::vector<NavExtraLink> gaps_off;  // gap tricks given up, off every route until `gaps_off_s`
  std::vector<double> gaps_off_s;      // (the pilot's own clock, `clock_s`)
  double clock_s = 0;                  // the commands' time, never reset
  // Each ridden mover's height at the last command and when it last moved (`clock_s`): a plat is
  // stepped onto only at its rest and silent, still a beat (pilot.cpp `mover_in_way`).
  struct MoverSeen {
    int32_t ent = -1;
    float z = 0;
    double moved_at = -1e9;
  };
  MoverSeen movers_seen[8];
  bool gap_flight = false;
  NavExtraLink gap_link;     // the gap jumped (`gap_flight`): judged where it comes down
  bool gap_aloft = false;    // and its flight has left the ground (a bot's keys may press late)
  double gap_pressed_s = 0;  // when its jump was pressed (`clock_s`)
  int32_t gap_way = -1;      // the run to a gap's lip under way (pilot_gaps.hpp `gap_run`; -1 none)
  int32_t gap_way_i = 0;     // and its commands so far
  bool safe_flight = false;
  bool brink_free = false;    // this command's keys are a flight the move proved: no brink guard
  std::vector<int32_t> shut;  // doors the caller knows are shut: never planned through
  std::vector<NavExtraLink> shut_links;           // and links it knows are (onto a bridge still in)
  std::vector<NavExtraLink> cut_all;              // cut and shut_links together, for the route
  double wanted[3] = {}, pressed[3] = {};         // the last command's yaw, forward, side
  float lane = 0, lane_want = 0, lane_until = 0;  // the lane now, wanted, and until when (s)
  float offset = 0;                               // units off the line now (signed, left +)
  core::Rng lane_rng{1};
  ai::Hands hands;
  ai::Search search;
  PilotStats stats;
};

// A pilot on `g` (kept alive and unmoved) with `hands` (from pilot_hands), its wander off `seed`.
void pilot_init(Pilot& p, const NavGraph& g, const PilotSettings& set,
                std::span<const ai::AxisHands> hands, double hz, uint32_t seed);
// The view turned to `yaw` (degrees) with the hands at rest there: a pilot taking over a player
// already moving (a run started from a recorded player's place and velocity).
void pilot_face(Pilot& p, float yaw);
// Plans the way to node `goal` from where the pilot stands; false when there is none.
bool pilot_go(Pilot& p, int32_t goal, const PilotSense& sense);
// The next command, `msec` long.
MoveCmd pilot_step(Pilot& p, const PilotSense& sense, int msec, ai::Why* why = nullptr);

// The line the pilot at `at` means to walk, to draw: the route ahead with its lane applied along
// the level walks (each node moved off the line by the offset it keeps now), the nodes as they
// are from the first link that is not one. `breaks`, when given, marks each point the line does
// not run to (a teleporter's: the player is there at once, nothing is walked between).
std::vector<Vec3f> pilot_path(const Pilot& p, Vec3f at, std::vector<uint8_t>* breaks = nullptr);

// A pilot filling a server's slot: it reads the slot's player and the movers' edicts. Kept alive
// and unmoved while the slot is filled. `goals` are visited in turn, round and round; a goal with
// no way to it is skipped, and the doors barred are forgotten at each new goal.
struct PilotBot {
  Pilot pilot;
  std::vector<int32_t> goals;
  size_t next_goal = 0;
  std::vector<NavMover> now;                   // the movers as they stand (reused)
  std::array<OtherBox, kWalkOthers> others{};  // the players near (`PilotSense::others`)
  MoveWorld* world = nullptr;  // the map's solids the hop looks ahead through (null: none)
  bool grenade_ready = true;   // the grenade launcher in hand (the caller's say, each command)
  bool rocket_ready = true;    // the rocket launcher in hand (the caller's say, each command)
  ai::Why* why = nullptr;
  int64_t command = 0;
  int32_t arrivals = 0;
};
CmdSource pilot_source(PilotBot& b);

}  // namespace ge::shooter
