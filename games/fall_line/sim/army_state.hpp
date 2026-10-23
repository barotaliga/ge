// THE ARMY ON FOOT AS A RUN HOLDS IT (`GameState::army`): every soldier out, what he is doing,
// and the counters that send more. Its own header so state.hpp can carry it without the rules
// that move it (army.hpp).
//
// A run whose hunt never sent a soldier leaves it inactive, hashes none of it and moves no digest.
//
// ge:summary The army as a run holds it: ArmyState, Soldier, ArmySled, their modes and changes
// ge:keywords army army-state soldier soldier-mode soldier-from squad wave lift-squad troops
// ge:keywords army-change army-sled army-sled-mode army-sled-change
// ge:skill sim
// ge:test tests/fall_line_army_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "snowmobile/sled.hpp"

namespace fall_line::sim {

// Where a soldier came from: a squad up the lifts, a ground wave up from the valley, or off one of
// the army's snowmobiles.
enum class SoldierFrom : uint8_t { lift, wave, sled };

// What he is doing:
// - `walk` up the lane to the lift's load line; `wait` there for a carrier; `ride` it up;
// - `climb` with his wave up the way from the valley;
// - `chase` after the man (or to where he was last seen);
// - `hold` with him in sight and in range, where the army shoots to kill;
// - `arrest` kneeling over him, where it does not;
// - `gone` sent home: taken off the map at the end of the step.
enum class SoldierMode : uint8_t { walk, wait, ride, climb, chase, hold, arrest, gone };

// What an army event says (`EventKind::army`: its `gate` the change, its `value` how many men): a
// lift's squad put down, a wave formed up, soldiers let off at a lift's top, the arrest, men sent
// home.
enum class ArmyChange : uint8_t { squad, wave, off, arrest, home };

struct Soldier {
  int32_t id = 0;  // the army's own, in the order sent; his pursuer id is `army::kPursuerBase` + it
  SoldierFrom from = SoldierFrom::lift;
  SoldierMode mode = SoldierMode::walk;
  int32_t squad = 0;           // the lift squad's or the wave's number
  double t = 0;                // s in this mode
  double x = 0, y = 0, z = 0;  // his feet, world frame, m
  double heading = 0;          // rad
  double speed = 0;            // m/s along `heading`
  bool climbing = false;       // working his way: skating, the herringbone, the traverse
  bool sees = false;           // whether he saw the man at his last look
  double tx = 0, tz = 0;       // where he is going, when told: his place in the lift's line
  // On a lift: which, the carrier that has him, and his seat on it.
  int32_t lift = -1;
  int32_t carrier = -1;
  int32_t seat = 0;
  // With his wave: the way he climbs (`GameState::ways`), the arc he climbs to next, the arc he
  // leaves the column at, and his file, m right of the way's line.
  int32_t way = -1;
  double s = 0;
  double s_end = 0;
  double lane = 0;
  // THE WATCH ON HIS PROGRESS (army_troop.hpp): where he was at the window's start and when, how
  // many windows in a row he barely moved, the side he steps off to and until when.
  double wx = 0, wz = 0, wt = 0;
  int32_t slow = 0;
  int32_t side = 1;
  double until = 0;
};

// What one of the army's snowmobiles is doing (army_sled.hpp):
// - `due` called, its crew mounting up in the village;
// - `drive` after him, or to where he was last seen;
// - `halt` stopped side-on to him, the gunner on him;
// - `leave` the hunt over, driving home;
// - `empty` left standing, its crew off it after him on foot;
// - `gone` home or let go: taken off the map at the end of the step.
enum class ArmySledMode : uint8_t { due, drive, halt, leave, empty, gone };

// What an army snowmobile's event says (`EventKind::army_sled`: its `gate` the change, its `value`
// the machine's id): called, out of the village, halted, its crew off it to take him in, dug out,
// stood back on its belt, home.
enum class ArmySledChange : uint8_t { called, out, halt, dismount, dug, righted, gone };

struct ArmySled {
  int32_t id =
      0;  // the army's own, in the order called; its pursuer id `army_sled::kPursuerBase` + it
  ArmySledMode mode = ArmySledMode::due;
  double due = 0;  // the clock it leaves the village at, s
  double t = 0;    // s in this mode
  ge::snowmobile::SledState body;
  double home_x = 0, home_z = 0;  // where it set off from, m
  double way = 0;                 // the heading it steers, rad
  double pace = 0;                // the pace it is driven at, m/s
  bool blocked = false;           // he is up a face it cannot climb: it waits
  int32_t on_way = -1;            // the way it climbs up to him (`GameState::ways`), or -1
  double way_end = 0;             // ...to the arc nearest him, m
  double next = 0;                // the clock before which it does not halt again, s
  double saw = -1e9;              // the clock its crew last saw him, s
  // NOT GETTING STUCK: where it was at the window's start and when, how many windows in a row it
  // barely moved, the side it swings off to and until when, and how often it was dug out.
  double wx = 0, wz = 0, wt = 0;
  int32_t slow = 0;
  int32_t side = 1;
  double until = 0;
  int32_t dug = 0;
};

struct ArmyState {
  bool active = false;  // a soldier was sent this run: only then is any of it hashed
  // THE SOLDIERS OUT, reserved for `army::kMostSoldiers` at create, so a step never allocates.
  std::vector<Soldier> soldiers;
  int32_t next_id = 0;
  // THE LIFTS' SQUADS: whether one is called, the clock it is due at, and how many were sent.
  bool lift_called = false;
  double lift_due = 0;
  int32_t lift_squads = 0;
  // THE GROUND WAVES: how many were sent this run (each one's number, and the row it is read at),
  // the stars they are being counted at and how many of those have gone, and their men sent.
  int32_t waves = 0;
  int32_t wave_star = 0;
  int32_t wave_at = 0;
  int32_t marched = 0;
  int32_t arrested_by = -1;  // the soldier who took him, or -1
  // THE SNOWMOBILES OUT, reserved for `army_sled::kMostSleds` at create; the next one's id, how
  // many were called this run (each one's place beside the spot), how many a wave keeps out on
  // top of the roster's, and where they set off from (beside the free ride's snowmobile spot).
  std::vector<ArmySled> sleds;
  int32_t next_sled = 0;
  int32_t sleds_called = 0;
  int32_t wave_sleds = 0;
  double pad_x = 0, pad_z = 0, pad_heading = 0;
};

}  // namespace fall_line::sim
