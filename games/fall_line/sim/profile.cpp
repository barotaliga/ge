// The profile taken from a run (its career summed from the run's report) and carried into another.
//
// ge:summary profile_of and carry_in: the skier's body and career out of one run and into another
// ge:keywords profile career body carry report sum
// ge:skill save sim
// ge:test tests/fall_line_profile_test.cpp
#include "fall_line/sim/profile.hpp"

#include <algorithm>

namespace fall_line::sim {

bool profile_of(const Driver& d, Profile& out, std::string& error) {
  const GameState& s = d.state();
  const ge::skier::BodyState& body = s.racers[0].body;
  if (body.mortal >= 0) {
    error = "the skier was killed in this run, so there is no one to carry to another mountain";
    return false;
  }
  out.body = body;
  out.body.impact = {};  // this run's blow in progress and its owed load end with it
  out.body.owed = {};
  const RunReport r = d.report(0);
  Career c = d.carried();
  c.runs += 1;
  c.finishes += r.finished ? 1 : 0;
  if (r.finished && s.mode == Mode::race && (c.best_time == 0 || r.time < c.best_time))
    c.best_time = r.time;
  c.seconds += s.t;
  c.distance += s.mode == Mode::free ? r.distance : r.finished ? s.level->track.length : 0;
  c.top_speed = std::max(c.top_speed, r.top_speed);
  c.max_landing_g = std::max(c.max_landing_g, r.max_landing_g);
  c.flights += r.flights;
  c.wipeouts += r.wipeouts;
  c.resets += r.resets;
  c.missed_gates += r.missed;
  c.lifts += r.lifts;
  out.career = c;
  return true;
}

bool carry_in(Driver& d, const Profile& p, std::string& error) {
  if (d.state().steps != 0) {
    error = "the run has already been played " + std::to_string(d.state().steps) +
            " steps; carry a profile into a run just opened";
    return false;
  }
  d.carry(p.career, p.body);
  return true;
}

}  // namespace fall_line::sim
