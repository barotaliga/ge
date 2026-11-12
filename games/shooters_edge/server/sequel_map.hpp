// THE SEQUEL'S MAP AT WORK: the movers and triggers of its maps, run by the sequel runner
// (sequel_server.hpp). The facts are the clean-room note's
// (porting/shooters-edge/sequel-movers.md); the words a player reads are our own.
//
// - **Movers** (doors, buttons, plats, trains) slide their brush model at a constant speed (the
//   plain move: whole frames of speed × 0.1, then the remainder) or, for a plat, speed up and slow
//   down (the accelerated move, in units per frame). Each frame's push is rounded to eighths; the
//   players riding a mover (standing on it) or in its way move with it. A player that would end in
//   a solid stays where he was when he fits there, else the push is undone and the mover is
//   blocked: it crushes (`dmg`) and a door or plat turns back.
// - **func_door** opens along its angle by its size less `lip`, waits `wait` and closes; it opens
//   by a trigger field round its team (60 units wider in x and y), by a use, or by being shot
//   (with `health`). **func_button** moves in, fires its targets, and comes back after `wait`.
//   **func_plat** rests at the bottom and rises when a player steps into the field above it, waits
//   3 s at the top (held up while someone stands there) and goes down. **func_train** follows its
//   `path_corner`s, its lowest corner on each. **func_wall** toggles, **func_timer** fires on its
//   own.
// - **Triggers**: `trigger_once` and `trigger_multiple` fire their targets when a living player
//   enters them (facing their angle, when they have one); `trigger_relay`, `trigger_always`;
//   `trigger_push` sets a player's velocity; `trigger_hurt` hurts each frame; `misc_teleporter`
//   moves a player to its destination, killing whoever stands there.
// - **Targets**: a use waits `delay`, prints `message` to its activator, removes `killtarget` and
//   uses every entity its `target` names, in map order. `target_changelevel` ends the level (in
//   deathmatch only with `dmflags` 4096), `target_speaker`, `target_explosion`, `target_secret`
//   and `target_goal`.
//
// - **Turning movers**: `func_door_rotating` turns `distance` degrees (90) about its origin by the
//   angular plain move (whole frames, then what is left, so it ends on its angles), on its yaw,
//   roll (64) or pitch (128); `func_rotating` spins at `speed` while on, carrying its riders round
//   and turning their view, crushing what blocks it. A turned brush is traced in its own frame.
// - **Area portals** (`func_areaportal`, by `style`) open while the door that targets them is open;
//   a named `light` with a style of 32 or more switches it on and off by a use.
//
// ge:summary The sequel's movers and triggers: doors, buttons, plats, trains, triggers, targets
// ge:keywords sequel func-door func-button func-plat func-train trigger-multiple trigger-push
// ge:keywords trigger-hurt teleporter target-changelevel use-targets plain-move accelerated-move
// ge:keywords mover-push riders crush killtarget func-door-rotating func-rotating angular-move
// ge:keywords area-portal light-switch light-style
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_map_test.cpp
#pragma once

#include <cstdint>

#include "shooter/sequel_move.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

struct SequelServer;

// Sets up every map entity the runner keeps (movers, triggers, targets): their keys read, their
// places and fields made. After the items; adds the door fields and plat triggers as entities.
void map_spawn(SequelServer& s);

// One rules frame of the map: the delayed uses, the timers, every mover's push and step.
void map_frame(SequelServer& s);

// The triggers and pads player `slot`'s box overlaps after his move, and the door brushes he
// leans on (a door's message).
void map_touch(SequelServer& s, int slot);

// Uses entity `e`'s targets (its delay, message, killtarget and targets), `activator` a player's
// slot or -1.
void map_use_targets(SequelServer& s, int e, int activator);

// A blow of `damage` from `attacker` (a slot or -1) to entity `e`: a door or button with health is
// opened or pressed when it reaches 0. False when the entity takes no damage.
bool map_damage(SequelServer& s, int e, int attacker, int damage);

// A box swept from `start` to `end` through the map and every solid mover (`mask` the contents that
// stop it). The hit entity is the world row (0 the world, a mover's row) or -1 for none.
sh::SequelTrace map_trace(const SequelServer& s, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs,
                          sh::Vec3f end, int32_t mask);

// The world a player's move collides through: the map and the movers.
struct MapWorld {
  const SequelServer* s = nullptr;
  int32_t mask = 0;
  sh::SequelWorld world();
};

// The world row of entity `e` (the map's entities follow the world and the players).
int map_row(int e);
// The entity of world row `row`, or -1 when it is no map entity.
int map_entity_of_row(const SequelServer& s, int row);

// The movers' world rows: their places and whether they are shown.
void map_rows(SequelServer& s);

}  // namespace shooters_edge::server
