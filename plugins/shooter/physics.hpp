// THE PHYSICS FRAME: every entity that is not a player runs once a frame by its movetype; players
// are moved by their commands (client.hpp), or on the single-player clock with the original move
// in their turn here (`run_player_frame`).
//
// THE FRAME (`physics_frame`): `frametime` is the run clock's step (frame.hpp; the map's first
// physics frame runs the longest step, sv_maxtic). The program's frame start runs with `time` the
// server's time; then every entity in number order, the world first, skipping free ones: when
// `force_retouch` is set it is relinked with its trigger touches; players are then skipped (or run
// by `run_player_frame` on the single-player clock with the original move); every
// other runs by its movetype (`run_entity`), followed by the new missile check. `force_retouch`
// counts down by one. An entity never runs twice at one server time (`Edict::last_run`).
//
// THINKING (`run_think`): an entity whose nextthink is above 0 and NO LATER THAN `time + dt`
// thinks now, before it moves; the program sees `time` = max(nextthink, the server's time), self
// the entity, other the world, and nextthink is cleared before the call. A removed entity ends
// the run. A think that sets a nextthink still due by the same rule and LATER than the time just
// used thinks again (a chain of short thinks catches up within the frame); one that sets the same
// time or earlier waits for the next frame.
//
// BY MOVETYPE (`run_entity`): push: push.hpp; none and lock: think only; noclip: think, then
// `angles += avelocity dt`, `origin += velocity dt`, relinked without touches; step: falls while
// off the ground (fly-move, a landing sound after a fast fall), then thinks and checks water; toss,
// bounce, fly and flymissile: THE TOSS (`physics_toss`):
//  1. think (a removed entity stops); 2. rising clears kFlagOnGround, and resting on the ground
//     stops; 3. the velocity bounded (a non-number component 0, longer than sv_maxvelocity scaled
//     to it); 4. gravity, but for fly and flymissile; 5. angles turned; 6. moved: velocity times dt
//     (dt rounded to float), traced (a missile sweep for flymissile, nomonsters for a trigger or
//     `not` solid), moved to the trace's end, relinked with trigger touches, both touch on a hit;
//  7. a full run, or a removed entity, stops; 8. the velocity clipped on the plane hit (1.5 for
//     bounce, 1 otherwise); 9. a floor (normal z above 0.7) stops it (bounce: only below 60 up)
//     with onground, groundentity, no velocity and no turning; 10. the water check.
// GRAVITY: `velocity.z -= factor x sv_gravity x dt`, the factor (the entity's gravity, 0 meaning
// 1) times sv_gravity in float, times dt in double, the difference in double rounded to float.
// THE WATER CHECK: the world's contents at the origin; an entity with no water type yet takes it
// with level 1; entering water or worse from empty, or leaving it, plays the splash; in water the
// type is the contents and the level 1; out of it the type empty and the level the contents
// number (a quirk a program may read).
//
// NEW MISSILES (`run_new_missile`): after a player's post-think and after each entity's run, an
// entity the program named in `newmis` runs once by its movetype with dt 0.05 s, so a missile
// leaves the barrel instead of appearing inside it; `newmis` is cleared first.
//
// Held to the measured flights by tests/shooter_toss_recording_test.cpp and to its own bits and
// the thinking examples by tests/shooter_physics_test.cpp.
//
// ge:summary The physics frame: thinking, toss and bounce, fly, step, noclip, water, new missiles
// ge:keywords physics physics_frame run_entity run_think think nextthink toss bounce fly
// ge:keywords flymissile step fly_move noclip gravity bound-velocity maxvelocity water-transition
// ge:keywords splash newmis new-missile force_retouch frametime last-run
// ge:skill shooter
// ge:test tests/shooter_physics_test.cpp tests/shooter_toss_recording_test.cpp
#pragma once

#include "shooter/server.hpp"

namespace ge::shooter {

constexpr double kNewMissileDt = 0.05;  // s a new missile runs ahead
constexpr const char* kSoundSplash = "misc/h2ohit1.wav";
constexpr const char* kSoundLand = "demon/dland2.wav";

// Thinks as due within `dt`; false when the think removed the entity.
bool run_think(Server& sv, int e, double dt);

// One entity by its movetype, once a server time.
void run_entity(Server& sv, int e, double dt);

// The toss rules for one entity (toss, bounce, fly, flymissile); `last` (optional) gets the move's
// trace, or a full-length trace with no entity when the entity did not move.
void physics_toss(Server& sv, int e, double dt, HullTrace* last = nullptr);

// The step rules (falling monsters and resting things).
void physics_step(Server& sv, int e, double dt);

// The entity's slide along what it meets for `time` seconds: 1 a floor was hit, 2 a wall, 3
// trapped in a solid, 7 out of planes.
int fly_move(Server& sv, int e, float time, HullTrace* wall = nullptr);
// `wall` (optional) gets the last wall the slide met (a plane whose normal z is exactly 0).

// Moves the entity by `push` as far as it goes, relinks it with its trigger touches and, on a
// hit, runs both touches.
HullTrace push_entity(Server& sv, int e, Vec3f push);

void check_water_transition(Server& sv, int e);

// The velocity with non-number components zeroed and its length capped.
void bound_velocity(Server& sv, int e);

// Runs and clears the program's `newmis`, if any.
void run_new_missile(Server& sv);

// One physics frame of `frametime` seconds.
void physics_frame(Server& sv, double frametime);

}  // namespace ge::shooter
