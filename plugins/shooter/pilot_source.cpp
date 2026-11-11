// THE PILOT AS A SERVER SLOT'S COMMAND SOURCE (pilot.hpp `PilotBot`, `pilot_source`): each
// command its sense read off the server's world as the player's client holds it (its own place,
// the movers where they stand, the other players near), its next goal taken when it arrives, and
// one `pilot_step`.
//
// ge:summary The pilot as a server slot's command source: its sense off the world, goals in turn
// ge:keywords pilot_source pilot-bot command-source sense movers other-players goals cmd-source
// ge:skill shooter bots
// ge:test tests/shooter_pilot_test.cpp
#include <cstddef>
#include <span>

#include "shooter/pilot.hpp"
#include "shooter/server.hpp"

namespace ge::shooter {

namespace {

constexpr float kOthersReach = 400;  // the players within this of the feet go in the walk's flight

MoveCmd pilot_next(void* ctx, const Server& sv, int slot, int msec) {
  PilotBot& b = *static_cast<PilotBot*>(ctx);
  Pilot& p = b.pilot;
  const Edict& me = sv.edicts[static_cast<size_t>(slot) + 1];
  PilotSense s;
  s.origin = me.origin;
  s.velocity = me.velocity;
  s.onground = (me.flags & kFlagOnGround) != 0;
  s.waterlevel = me.waterlevel;
  s.maxspeed = me.maxspeed > 0 ? me.maxspeed : 320;
  b.now.resize(p.g->movers.size());
  for (size_t i = 0; i < b.now.size(); i++) {
    const NavMover& m = p.g->movers[i];
    const Edict& e = sv.edicts[static_cast<size_t>(m.ent)];
    b.now[i] = m;
    // Where the mover stands now; one the player's client was not sent this frame (outside its
    // visibility set: a free slot in a client's world) is taken as the graph planned it.
    if (!e.free) {
      b.now[i].absmin = e.absmin;
      b.now[i].absmax = e.absmax;
    }
  }
  s.movers = b.now;
  s.world = b.world;
  // The other players alive within reach of a walk's flight, as many as there is room for (a free
  // slot is one the client was not sent this frame).
  size_t others = 0;
  if (p.set.ahead && p.set.ahead_bodies) {
    for (int i = 1; i <= sv.set.maxclients && i < static_cast<int>(sv.edicts.size()); i++) {
      const Edict& e = sv.edicts[static_cast<size_t>(i)];
      if (i == slot + 1 || e.free || e.health <= 0) continue;
      const float dx = e.origin.x - me.origin.x, dy = e.origin.y - me.origin.y;
      const float dz = e.origin.z - me.origin.z;
      if (dx * dx + dy * dy > kOthersReach * kOthersReach || dz > 96 || dz < -96) continue;
      if (others < b.others.size()) b.others[others++] = {e.origin, e.velocity};
    }
  }
  s.others = std::span<const OtherBox>(b.others.data(), others);
  s.grenade_ready = b.grenade_ready;
  s.rocket_ready = b.rocket_ready;
  if ((p.mode == PilotMode::idle || p.mode == PilotMode::arrived) && !b.goals.empty()) {
    if (p.mode == PilotMode::arrived) b.arrivals++;
    for (int32_t& d : p.barred) d = -1;
    p.cut = p.failed;
    for (size_t tries = 0; tries < b.goals.size(); tries++) {
      const int32_t goal = b.goals[b.next_goal];
      b.next_goal = (b.next_goal + 1) % b.goals.size();
      if (pilot_go(p, goal, s)) break;
    }
  }
  ai::Why* why = b.why ? b.why->follow(slot, b.command) : nullptr;
  b.command++;
  return pilot_step(p, s, msec, why);
}

}  // namespace

CmdSource pilot_source(PilotBot& b) { return {&b, pilot_next}; }

}  // namespace ge::shooter
