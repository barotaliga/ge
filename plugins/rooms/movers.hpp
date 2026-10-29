// A ROOM LEVEL'S MOVING PARTS: the brush groups its entities name as doors, platforms, buttons and
// trains, the trigger volumes that set them off, and the world a body meets with each part where
// it has moved to. A pure function of the level, the bodies and the steps, so a server and every
// client open the same door on the same step.
//
// WHAT EACH ENTITY DOES (keys in metres, metres a second and seconds; `heading` in degrees
// clockwise from +z, -1 up, -2 down; the cloner writes a classic map's keys in these terms):
//   - `func_door`: slides along `heading` by its size that way less `lip` (default 0.25 m) at
//     `speed` (3.125 m/s), waits `wait` s (3; -1 stays open) and comes back. A door with no
//     `targetname` opens when a body touches it; one with a name waits to be fired. Doors that
//     touch each other open and close as one. A closing door a body blocks goes back open.
//   - `func_plat`: rests lowered by `height` (default its own height less 0.25 m) and rises at
//     `speed` (4.6875 m/s) when a body stands on it or touches it, waits 3 s at the top and goes
//     back down. A plat with a `targetname` rests at the top until it is fired.
//   - `func_button`: pressed in along `heading` by its size less `lip` (0.125 m) at `speed`
//     (1.25 m/s) when a body touches it, fires its `target` when in, waits `wait` s (1) and
//     comes back out.
//   - `func_train`: moves its lowest corner from `path_corner` to `path_corner` along their
//     `target` chain at `speed` (3.125 m/s), looping, waiting a corner's `wait` s there. A train
//     with a `targetname` waits at its first corner until it is fired.
//   - `trigger_multiple`, `trigger_once`: a body inside fires `target`, again after `wait` s
//     (0.2) for a multiple, never again for a once.
// Firing a name sets off every mover of that `targetname`: a door opens (or closes again when
// open and waiting), a plat rises, a train starts. Other classnames are left as they are.
//
// THE WORLD (`movers_world`): the level's still brushes and rooms, and each mover's brushes built
// alone where they stand, swept at their offset. A hit on a mover reports `solid` = its index + 1
// and `surface_vel` its velocity, so a body standing on a plat rides it (move/push.hpp `carry`,
// which `step_movers` calls for every body each time a part moves).
//
// ge:summary A room level's doors, plats, buttons, trains and triggers, stepped, and the world
// ge:keywords mover movers door plat platform lift button train path-corner trigger target
// ge:keywords targetname fire carry rider crush heading lip speed wait team linked-doors
// ge:skill rooms move
// ge:test tests/rooms_movers_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "move/mesh_world.hpp"
#include "move/spec.hpp"
#include "move/step.hpp"
#include "rooms/brush_build.hpp"
#include "rooms/level.hpp"

namespace ge::rooms {

enum class MoverKind : uint8_t { door, plat, button, train };
// Where a mover is in its cycle: at rest (`pos1`), on its way out, out (`pos2`), on its way back.
enum class Phase : uint8_t { rest, going, out, coming };

struct Mover {
  MoverKind kind = MoverKind::door;
  uint16_t group = 0;
  uint32_t entity = 0;                      // into Level::entities
  move::Vec3 lo{0, 0, 0}, hi{0, 0, 0};      // the group's bounds where it is built, m
  move::Vec3 pos1{0, 0, 0}, pos2{0, 0, 0};  // offsets at rest and out
  move::Vec3 offset{0, 0, 0};               // where it is now, from where it is built
  move::Vec3 vel{0, 0, 0};                  // m/s this step
  double speed = 0, wait = 0, timer = 0;
  Phase phase = Phase::rest;
  bool named = false;             // has a targetname: waits to be fired
  int32_t team = -1;              // the first door of a set that moves as one (-1 none)
  std::vector<move::Vec3> path;   // a train's corners as offsets
  std::vector<double> path_wait;  // the wait at each corner
  size_t leg = 0;                 // the corner a train is heading for
  move::MeshWorld mesh;           // its brushes alone
};

struct Trigger {
  uint32_t entity = 0;
  std::vector<HalfSpace> planes;  // its brushes' planes, `brush` the start of each
  std::vector<uint32_t> brush;    // into planes: brush k is planes[brush[k]..brush[k+1])
  double wait = 0.2, timer = 0;
  bool once = false, spent = false;
};

enum class MoverEventKind : uint8_t { started, stopped, fired };

struct MoverEvent {
  MoverEventKind kind = MoverEventKind::started;
  uint32_t entity = 0;  // the mover's or trigger's (fired: the one that fired)
};

struct Movers {
  std::vector<Mover> movers;
  std::vector<Trigger> triggers;
  std::vector<std::string> names;    // a mover's targetname (by index)
  std::vector<std::string> targets;  // a mover's or trigger's target: movers first, then triggers
  move::MeshWorld still;             // the rooms and every brush of no group
  std::vector<MoverEvent> events;    // this step's, cleared by each `step_movers`
  size_t skipped = 0;                // entities naming a group with a classname not moved here
};

// The movers of a linked level, their brushes built on `workers` threads. False, with what failed
// and what to do in `*error`, when a group an entity names has no closed brush, or a train's
// `target` chain names a corner the level does not have.
bool build_movers(const Level& level, size_t workers, Movers* out, std::string* error);

// The still world and every mover where it is now; `m` must outlive it.
move::World movers_world(const Movers& m);

// One step of `dt` s: each trigger tested against the bodies (their hulls from `spec`), touched
// doors, plats and buttons set off, every mover moved and the bodies it bears or pushes carried
// with it (a closing door or a falling plat a body blocks goes back). Fills `m.events`.
void step_movers(Movers& m, const move::MoveSpec& spec, std::span<move::MoveState> bodies,
                 double dt);

// Fires `name`: every mover with that targetname set off.
void fire(Movers& m, const std::string& name);

}  // namespace ge::rooms
