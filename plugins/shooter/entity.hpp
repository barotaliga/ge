// THE ENTITIES OF THE CLASSIC LOOP: every thing in a match is one record of fields (`Edict`), kept
// in one contiguous pool the server reserves when it is created (`Server::edicts`, server.hpp) and
// addressed by number, never by pointer: entity 0 is the world, 1..maxclients the players, the
// rest dynamic. The fields are the ones the loop reads and writes (the game program's own fields
// are the program's: a native program keeps parallel arrays by entity number, a bytecode program
// its field memory). Positions and velocities are 32-bit floats in the map format's axes (x and y
// across, z up); `angles` and `avelocity` are degrees and degrees/s.
//
// The numbers of the movetypes, solids and flags are the program's (a program sets them as
// numbers), so they are plain `int` constants rather than enums.
//
// ge:summary The classic loop's entity record and pool: fields, movetypes, solids, flags
// ge:keywords edict entity entities field fields movetype solid flags onground item monster
// ge:keywords toss bounce flymissile push noclip trigger bbox slidebox bsp nextthink think touch
// ge:keywords blocked ltime groundentity owner absmin absmax
// ge:skill shooter
// ge:test tests/shooter_physics_test.cpp
#pragma once

#include <cstdint>

#include "shooter/vec.hpp"

namespace ge::shooter {

// Movetypes: what moves an entity (gameloop's § Running an entity).
constexpr int kMovetypeNone = 0;        // nothing; only thinks
constexpr int kMovetypeWalk = 3;        // the player move (players only)
constexpr int kMovetypeStep = 4;        // falls when off the ground, else only thinks
constexpr int kMovetypeFly = 5;         // the toss rules without gravity
constexpr int kMovetypeToss = 6;        // the toss rules
constexpr int kMovetypePush = 7;        // the pusher rules (doors, plats, trains)
constexpr int kMovetypeNoclip = 8;      // straight-line motion through everything
constexpr int kMovetypeFlyMissile = 9;  // fly, with a missile's larger clip against monsters
constexpr int kMovetypeBounce = 10;     // toss with a 1.5 rebound
constexpr int kMovetypeLock = 12;       // nothing; the server holds the player

// Solids: how an entity meets others.
constexpr int kSolidNot = 0;       // no interaction (unlinked from the area tree)
constexpr int kSolidTrigger = 1;   // touched by things moving into it, does not block
constexpr int kSolidBbox = 2;      // blocks, as its box
constexpr int kSolidSlidebox = 3;  // blocks, as its box (a monster's or a player's)
constexpr int kSolidBsp = 4;       // blocks through its brush model's hulls (movetype push)

// Flags the loop reads and writes.
constexpr int kFlagFly = 1;
constexpr int kFlagSwim = 2;
constexpr int kFlagMonster = 32;    // a missile clips against it with a 30-unit cube
constexpr int kFlagItem = 256;      // a pick-up: its trigger box grown 15 units sideways
constexpr int kFlagOnGround = 512;  // resting on `groundentity`
constexpr int kFlagWaterJump = 2048;
constexpr int kFlagJumpReleased = 4096;  // the jump button let go since the last jump

// A function of the game program, by number; 0 is none. The loop only passes the number back to
// the program (`Program::call`, program.hpp).
using Func = int32_t;
constexpr Func kNoFunc = 0;

struct Edict {
  // The pool's own bookkeeping (not program fields).
  bool free = true;
  float freetime = 0;      // the server time it was freed, s (a slot is reused 0.5 s later)
  double last_run = -1;    // the server time it last ran in a physics frame (run once a frame)
  int32_t area_node = -1;  // the area tree node it is linked in; -1 when unlinked
  int32_t link_prev = -1, link_next = -1;  // its neighbours in that node's list
  bool in_triggers = false;                // linked in the trigger list (else the solid list)

  // The fields the loop uses.
  Vec3f origin, velocity;   // units, units/s
  Vec3f angles, avelocity;  // degrees, degrees/s
  Vec3f mins, maxs, size;   // the box about the origin; size = maxs - mins
  Vec3f absmin, absmax;     // the box in the world, grown (link_edict)
  int movetype = kMovetypeNone;
  int solid = kSolidNot;
  int flags = 0;
  int groundentity = 0;  // what it rests on (with kFlagOnGround)
  int owner = 0;         // traces skip an entity's owner and what it owns
  float nextthink = 0;   // server time of the next think; 0 none
  Func think = kNoFunc, touch = kNoFunc, blocked = kNoFunc;
  float ltime = 0;  // a pusher's own clock, s
  int modelindex = 0;
  int waterlevel = 0;
  int watertype = 0;        // 0 until the water check first runs; then a contents number
  float teleport_time = 0;  // the player's water jump time left, s
  Vec3f v_angle;            // the player's view, degrees
  bool fixangle = false;    // the program forces the view
  float health = 0;
  float button0 = 0, button1 = 0, button2 = 0;  // attack, use, jump
  float impulse = 0;
  float gravity = 0;   // a factor; 0 means 1
  float maxspeed = 0;  // a player's own cap; 0 means sv_maxspeed
  Vec3f movement;      // the command's forward, side and up moves, written before each command
  // The original move's (framemove.hpp).
  Vec3f oldorigin;   // the last spot the box was free at (unstick goes back to it)
  Vec3f view_ofs;    // the eye above the origin
  Vec3f movedir;     // a water jump's horizontal push
  Vec3f punchangle;  // the view's recoil kick, degrees (decays; never moves the aim)
};

}  // namespace ge::shooter
