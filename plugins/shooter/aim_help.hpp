// THE AIM HELP: a player's hitscan shot (a bullet, a pellet, a beam, a rail, a blade) that would
// pass just beside, above or below another player is bent onto him, for a player the server has
// granted it (engine/server/aids.hpp: a player with a disability, or one on a phone's touch
// controls). A hitscan shot's line is bent where it passes; a missile's launch is LED (`aim_lead`):
// its flight is foreseen, and so is each player's run or fall, and it is sent where he will be.
//
// THE LEEWAY is two numbers in units, measured at the target as the shot would pass him: ACROSS
// (`sv_aimhelp_x`, sideways in the level's plan) and UP AND DOWN (`sv_aimhelp_y`). A line whose
// miss past a target's box is no more than the leeway on each axis is bent so it passes through
// the box (`kAimInset` inside its edge); a leeway of 0 on an axis bends nothing on that axis, so
// `sv_aimhelp_x 0` is the vertical help alone. A line that already passes through a target's box
// is never bent; of several targets in reach, the one the smallest turn reaches is taken.
//
// THE BEND is worked out once from the view (the eye and the view's forward) and turns every line
// of the shot by the same amount (`aim_bend_dir`), so a scattergun's spread keeps its shape round
// the new aim. The game checks the bent line reaches the target (no wall between) before it
// takes it.
//
// Float math on the view's own numbers, a pure function of them: the same shot bends the same on
// every machine (`tests/shooter_aim_help_test.cpp`).
//
// ge:summary The aim help: a hitscan shot just missing a player bent onto him within a leeway
// ge:keywords aim-help aimhelp aim-assist vertical-aim horizontal-aim leeway hitscan bend
// ge:keywords sv_aimhelp_x sv_aimhelp_y accessibility touch-controls phone aim_help aim_bend_dir
// ge:keywords aimleeway aimtarget aimbend aim_lead aim_help_missile lead predict projectile
// ge:keywords rocket grenade nail missile-watch
// ge:skill shooter
// ge:test tests/shooter_aim_help_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "shooter/vec.hpp"

namespace ge::shooter {

constexpr float kAimInset = 4;  // a bent line passes this far inside the box's edge, units

struct AimLeeway {
  float across = 0;  // sv_aimhelp_x: sideways, units at the target
  float updown = 0;  // sv_aimhelp_y: up and down, units at the target
};

// A player who may be aimed at: his entity and his box in the world.
struct AimTarget {
  int ent = 0;
  Vec3f absmin, absmax;
};

struct AimBend {
  bool bent = false;
  int target = -1;      // the entity aimed at (-1 none)
  Vec3f aim;            // the point the bent view passes through
  Vec3f forward;        // the bent view's forward, unit length
  float across = 0;     // the miss corrected sideways, units (signed: + to the right)
  float updown = 0;     // the miss corrected up and down, units (+ up)
  bool direct = false;  // the line already passes through a target: nothing to bend
};

// From `eye` looking along unit `forward` out to `range`: the target the leeway bends onto.
AimBend aim_help(Vec3f eye, Vec3f forward, float range, std::span<const AimTarget> targets,
                 AimLeeway leeway);

// A shot's line direction `dir` (unit) turned by the bend from `forward` to `bent`, unit length.
Vec3f aim_bend_dir(Vec3f dir, Vec3f forward, Vec3f bent);

// THE LEAD, for a missile (a rocket, a grenade, a nail): its flight from `origin` at `velocity`
// (falling by `gravity` when it is thrown; 0 when it flies straight) is followed out to
// `max_time`, and each target's box is moved along his own motion (straight on at his velocity on
// the ground; falling by `target_gravity` in the air). Where the missile passes a target's depth
// along the view, a miss no wider than the leeway past his box at that moment is bent onto him:
// a straight missile's direction is turned (its speed kept) to meet his box's middle where he will
// be when it gets there (a falling player stops at the ground under him); a thrown one is given the
// velocity whose arc, flown for the same time, lands there. Nothing is bent when the flight already
// meets a box.
struct AimMover {
  int ent = 0;
  Vec3f absmin{}, absmax{};
  Vec3f velocity{};
  bool airborne = false;
  float floor = -1e30f;  // the lowest his box's bottom falls to (the ground under him)
  float fade = 0;        // s: his run across fades by this time constant (0: kept); aim_path.hpp
  float turn = 0;   // rad/s: his run across turns at this rate, anticlockwise from above (0: none)
  Vec3f accel{};    // units/s^2 across: his run across speeds up or bends by this (aim_path.hpp)
  float top = 0;    // units/s: with `accel`, the fastest across he averages (0: no cap)
  float reach = 0;  // s: his run across is foreseen this far ahead and no further (0: no end)
};
// The mover the lead reads from a player's box, velocity and footing, by the rule recorded matches
// chose (the se-aimlead lab, docs/servers.md § Player aids): his velocity as it is, his jump's arc
// falling by the gravity to `floor` (the bottom of his box where the ground under him stops it),
// no run across below `kLeadRun` (a player stepping about is not going anywhere), and his run
// across foreseen at most `kLeadReach` ahead (on a long flight he turns before it gets there).
constexpr float kLeadRun = 150;     // units/s across: slower than this is led as standing
constexpr float kLeadReach = 0.6f;  // s: how far ahead his run across is foreseen
AimMover lead_mover(int ent, Vec3f absmin, Vec3f absmax, Vec3f velocity, bool airborne,
                    float floor);
struct LeadShot {
  Vec3f origin, velocity;
  float gravity = 0;   // units/s^2 the missile falls by; 0 straight
  float max_time = 3;  // s of flight followed
};
struct LeadBend {
  bool bent = false;
  int target = -1;
  Vec3f velocity;  // the missile's new velocity
  Vec3f at;        // where it meets him
  float time = 0;  // s of flight to there
};
constexpr float kLeadStep = 1.0f / 128;  // s between the flight's samples
LeadBend aim_lead(const LeadShot& shot, Vec3f view_forward, std::span<const AimMover> targets,
                  AimLeeway leeway, float target_gravity);

struct Server;

// THE LOOP'S HALF, for a program that shoots by tracing a line (a classic mod's `traceline`):
// whether the line `start`..`end` traced for player entity `shooter` is his shot (it starts within
// `kShotReach` of his eye and runs within `kShotCone` of his view's forward), and if so `end` bent
// by the leeway onto the nearest other living player (not one whose slot bit is in `friends`) the
// bent view reaches with no wall between. The target's entity, or -1 when nothing was bent.
constexpr float kShotReach = 40;    // a shot's line starts this near the eye, units
constexpr float kShotCone = 0.94f;  // and runs at least this cosine from the view's forward
int aim_help_line(Server& sv, int shooter, Vec3f start, Vec3f& end, AimLeeway leeway,
                  uint32_t friends);
// The missile `missile` a player just launched (the loop's new missile, its owner the shooter):
// its velocity bent by the lead (above) onto the nearest other living player (not one in
// `friends`) a clear straight line reaches. The target's entity, or -1 when nothing was bent.
int aim_help_missile(Server& sv, int missile, AimLeeway leeway, uint32_t friends);

}  // namespace ge::shooter
