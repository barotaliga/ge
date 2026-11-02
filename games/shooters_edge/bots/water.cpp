// The fighter in the water (water.hpp): the air clock, the dive kept or given up, the swim's way.
//
// ge:summary The fighter in the water: the air clock, the dive's give-up, the way to swim at
// ge:keywords water_way dive give-up air swim_steps swim_next dive_now
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/bots/water.hpp"

#include <cmath>

#include "shooter/swim.hpp"

namespace shooters_edge::bots {

namespace {

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

}  // namespace

bool water_way(Fighter& f, const server::QcServer& s, int me, double t, int msec, ge::ai::Why* why,
               sh::Vec3f& way) {
  const sh::Edict& body = s.sv.edicts[static_cast<size_t>(me)];
  // Its air: the head under this long, it takes whichever way out of the water is shorter, the
  // dive's on or the way back, so it never drowns on a swim.
  if (body.waterlevel >= 3) {
    if (f.under_since < 0) f.under_since = t;
  } else {
    f.under_since = -1;
  }
  const bool short_of_air = f.under_since >= 0 && t - f.under_since > kAir_s;
  const ItemSpot* dive_for = f.diving >= 0 ? &f.items[static_cast<size_t>(f.diving)] : nullptr;
  bool on_dive = dive_for && dive_for->dive;
  // The dive kept, or given up late or short of air (water.hpp).
  if (on_dive && body.waterlevel >= 1) {
    if (f.dive_wet_since < 0) f.dive_wet_since = t;
    f.stats.dive_wet_s += msec / 1000.0;
    const int32_t on = sh::swim_steps(*dive_for->dive, body.origin);
    const int32_t back = f.swim ? sh::swim_steps(*f.swim, body.origin) : -1;
    const bool late = t - f.dive_wet_since > dive_for->dive_s + kDiveLate_s;
    const bool turn = short_of_air && back >= 0 && (on < 0 || back < on);
    if (why) {
      why->decide("dive");
      why->fact("wet_s", t - f.dive_wet_since);
      why->fact("planned_s", dive_for->dive_s);
      why->fact("steps_on", on);
      why->fact("steps_back", back);
      why->choose(late || turn ? "give_up" : "swim_on",
                  late   ? "in the water longer than its swim's time and the margin"
                  : turn ? "short of air, and the way back is shorter than the way on"
                         : "within its swim's time");
    }
    if (late || turn) {
      f.stats.dives_given_up++;
      f.dive_off = f.diving;
      f.dive_off_until = t + kDiveRetry_s;
      f.diving = f.last_dive = -1;
      f.dive_wet_since = -1;
      f.swimming = f.swim && back >= 0;
      dive_for = nullptr;
      on_dive = false;
    }
  } else {
    f.dive_wet_since = -1;
  }
  // A walk along a pool's floor turns up and out by the field at kAir_s.
  if (short_of_air && f.swim && !on_dive && sh::swim_steps(*f.swim, body.origin) >= 0)
    f.swimming = true;
  // Diving: in the water, or at the dive's node (arrived, or within a step of it).
  const bool dive_now =
      dive_for && dive_for->dive &&
      (body.waterlevel >= 1 ||
       (f.goal_node == dive_for->dive_node &&
        (f.bot.pilot.pilot.mode == sh::PilotMode::arrived ||
         dist(body.origin,
              f.bot.pilot.pilot.g->nodes[static_cast<size_t>(dive_for->dive_node)].origin) <
             48))) &&
      (body.waterlevel >= 1
           ? sh::swim_next(*dive_for->dive, *f.bot.pilot.pilot.g, body.origin, way)
           : dive_for->dive_entry >= 0 &&
                 (way =
                      f.bot.pilot.pilot.g->nodes[static_cast<size_t>(dive_for->dive_entry)].origin,
                  true));
  return dive_now ||
         (f.swimming && f.swim && sh::swim_next(*f.swim, *f.bot.pilot.pilot.g, body.origin, way));
}

}  // namespace shooters_edge::bots
