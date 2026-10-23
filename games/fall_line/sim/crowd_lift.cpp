// The crowd on the lifts (crowd.hpp, crowd_lift.hpp): the queue at a lift's foot, the carrier
// that takes the front row up, the let-go at its top and the skate down the ramp to the run his
// group chose.
//
// ge:summary The crowd on the lifts: the queue, the carriers, the top and the ramp to a run
// ge:keywords crowd lift queue ticket carrier seat ride let-go skate ramp join_queue queue_group
// ge:keywords step_crowd_lifts
// ge:skill sim
// ge:test tests/fall_line_crowd_test.cpp
#include "fall_line/sim/crowd_lift.hpp"

#include <cstddef>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "fall_line/sim/crowd_rules.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/query.hpp"

namespace fall_line::sim::crowd {

namespace ml = ge::mountain::lifts;
namespace mg = ge::mountain::mapgen;
namespace core = ge::core;
namespace lm = ge::core::libm;

namespace {

int32_t seats_of(const ml::LiftPlan& plan) { return kSeats[static_cast<size_t>(plan.kind)]; }

const ml::LiftPlan& plan_of(const GameState& state, int32_t lift) {
  return (*state.lifts)[static_cast<size_t>(lift)];
}

// His place in the queue: rows of a carrier's seats behind the load point, beside his seat on
// it, facing up the line.
ml::Place queue_spot(const ml::LiftPlan& plan, int32_t place) {
  const int32_t seats = seats_of(plan);
  const int32_t row = place / seats;
  const double back = row * kQueueRowM;
  const double across = (place % seats - (seats - 1) / 2.0) * kQueueSideM;
  ml::Place at = load_place(plan);
  at.x += -plan.dx * back + plan.dz * across;
  at.z += -plan.dz * back - plan.dx * across;
  return at;
}

// Skate toward (x, z) at `kSkate`; whether he is there.
bool skate_to(GameState& state, Amateur& a, double x, double z) {
  const double dx = x - a.x;
  const double dz = z - a.z;
  const double d = core::hypot(dx, dz);
  const double step = kSkate * kDt;
  if (d <= step) {
    a.x = x;
    a.z = z;
  } else {
    a.x += dx / d * step;
    a.z += dz / d * step;
    a.heading = lm::atan2(dx, dz);
  }
  a.y = state.level->ground_at(a.x, a.z);
  a.speed = d <= step ? 0 : kSkate;
  a.vx = a.speed * lm::sin(a.heading);
  a.vz = a.speed * lm::cos(a.heading);
  return d <= step;
}

// The run a group skis off a lift's top, weighed by its least skilled; -1 when the lift has none.
int32_t choose_run(CrowdState& crowd, const CrowdGroup& g, int32_t lift) {
  const auto& ramps = crowd.ramps[static_cast<size_t>(lift)];
  if (ramps.empty()) return -1;
  const double skill = group_skill(crowd, g);
  double total = 0;
  for (const CrowdRamp& r : ramps)
    total += run_weight(crowd.net[static_cast<size_t>(r.run)], skill, false);
  double at = core::next(crowd.rng) * total;
  for (const CrowdRamp& r : ramps) {
    at -= run_weight(crowd.net[static_cast<size_t>(r.run)], skill, false);
    if (at <= 0) return r.run;
  }
  return ramps.back().run;
}

// The ramp off lift `lift`'s top onto run `run`.
const CrowdRamp* ramp_to(const CrowdState& crowd, int32_t lift, int32_t run) {
  for (const CrowdRamp& r : crowd.ramps[static_cast<size_t>(lift)]) {
    if (r.run == run) return &r;
  }
  return nullptr;
}

// LET GO at the top where his seat is, onto the way to the run his group skis next: chosen
// afresh unless one of his group came off before him and is still on its way down to it.
void let_go(GameState& state, Amateur& a) {
  CrowdState& crowd = state.crowd;
  a.speed = 0;
  a.carrier = -1;
  CrowdGroup& g = crowd.groups[static_cast<size_t>(a.group)];
  if (a.rank == 0) g.stop_lift = -1;
  bool ahead = false;
  for (const int32_t m : g.members) {
    const Amateur& o = crowd.amateurs[static_cast<size_t>(m)];
    if (o.id == a.id) continue;
    ahead = ahead || (o.mode == AmateurMode::skate && o.lift == a.lift) ||
            (!on_lift(o) && o.run == g.next && o.s < kRegroupTop);
  }
  if (!ahead || g.next < 0 || ramp_to(crowd, a.lift, g.next) == nullptr) {
    g.next = choose_run(crowd, g, a.lift);
  }
  const CrowdRamp* r = g.next < 0 ? nullptr : ramp_to(crowd, a.lift, g.next);
  a.mode = AmateurMode::skate;
  a.leg = 0;
  a.run = g.next;
  a.tx = r ? r->fx : a.x;
  a.tz = r ? r->fz : a.z;
}

// SKATING off the top: to the ramp's head, down it to the run's snow, and onto the run there.
void skate_off(GameState& state, Amateur& a) {
  CrowdState& crowd = state.crowd;
  if (!skate_to(state, a, a.tx, a.tz)) return;
  const CrowdRamp* r = a.run < 0 ? nullptr : ramp_to(crowd, a.lift, a.run);
  if (r != nullptr && a.leg == 0) {
    a.leg = 1;
    a.tx = r->tx;
    a.tz = r->tz;
    return;
  }
  // From the ramp's foot onto the run's snow a few metres down it, where its line has him.
  if (r != nullptr && a.leg == 1) {
    const mg::Track& onto = (*state.runs)[static_cast<size_t>(r->run)];
    const Along at = sample_run(onto, (r->s > 0 ? r->s : 0) + kOntoM);
    const double half = at.width / 2 - 1;
    const double d = core::clamp(lateral_of(at, a.x, a.z), -half, half);
    a.leg = 2;
    a.tx = at.x + lm::cos(at.heading) * d;
    a.tz = at.z - lm::sin(at.heading) * d;
    return;
  }
  // Onto the run where he stands: its nearest point to him, so nothing jumps.
  const int32_t run = r != nullptr ? r->run : 0;
  const mg::Track& track = (*state.runs)[static_cast<size_t>(run)];
  const mg::TrackHit hit = mg::nearest_track_point(track, a.x, a.z);
  const double half = sample_run(track, hit.s).width / 2;
  a.mode = AmateurMode::ski;
  a.lift = -1;
  a.run = run;
  a.s = hit.s;
  a.d = hit.lateral;
  a.centre = core::clamp(a.d, -half + 1, half - 1);
  a.yaw = 0;
  a.speed = 0;
  a.timer = 0;
  place_on_run(state, a);
}

// ON HIS CARRIER up the line beside his seat; let go once it is past the let-go point.
void ride(GameState& state, Amateur& a) {
  const ml::LiftPlan& plan = plan_of(state, a.lift);
  const ml::LoopSpot spot = ml::carrier_at(plan, a.carrier, state.t + kDt);
  if (spot.side != 0 || spot.turn >= 0 || spot.u >= ml::off_at(plan)) {
    let_go(state, a);
    return;
  }
  const ml::Place at = ml::carrier_place(plan, spot);
  const double across = (a.seat - (seats_of(plan) - 1) / 2.0) * kQueueSideM;
  a.x = at.x + lm::cos(at.heading) * across;
  a.z = at.z - lm::sin(at.heading) * across;
  a.y = state.level->ground_at(a.x, a.z);
  a.heading = at.heading;
  a.speed = ml::carrier_speed_at(plan, spot.u, spot.side);
  a.vx = a.speed * lm::sin(a.heading);
  a.vz = a.speed * lm::cos(a.heading);
}

// THE FRONT ROW TAKEN UP: the carrier that passes the load point this step takes those stood in
// their places at the front, in their order, as many as it has seats.
void board(GameState& state, int32_t lift) {
  CrowdState& crowd = state.crowd;
  CrowdQueue& q = crowd.queues[static_cast<size_t>(lift)];
  if (q.served >= q.issued) return;
  const ml::LiftPlan& plan = plan_of(state, lift);
  const int32_t k = ml::carrier_passing(plan, ml::load_at(plan), state.t, kDt);
  if (k < 0) return;
  const int32_t seats = seats_of(plan);
  int32_t taken = 0;
  for (int32_t place = 0; place < seats; place++) {
    Amateur* next = nullptr;
    for (Amateur& a : crowd.amateurs) {
      if (a.mode == AmateurMode::queue && a.lift == lift && a.ticket == q.served + place) {
        next = &a;
        break;
      }
    }
    if (next == nullptr) break;
    const ml::Place at = queue_spot(plan, place);
    if (core::hypot(next->x - at.x, next->z - at.z) > kAtPlaceM) break;
    next->mode = AmateurMode::ride;
    next->carrier = k;
    next->seat = taken++;
  }
  q.served += taken;
}

}  // namespace

ml::Place load_place(const ml::LiftPlan& plan) {
  ml::LoopSpot spot;
  spot.u = ml::load_at(plan);
  spot.side = 0;
  spot.turn = -1;
  spot.out = true;
  return ml::carrier_place(plan, spot);
}

void join_queue(GameState& state, Amateur& a, int32_t lift) {
  CrowdQueue& q = state.crowd.queues[static_cast<size_t>(lift)];
  a.mode = AmateurMode::queue;
  a.lift = lift;
  a.ticket = q.issued++;
  a.yaw = 0;
}

void queue_group(GameState& state, CrowdGroup& g, int32_t lift) {
  const ml::LiftPlan& plan = plan_of(state, lift);
  for (const int32_t m : g.members) {
    Amateur& a = state.crowd.amateurs[static_cast<size_t>(m)];
    join_queue(state, a, lift);
    const CrowdQueue& q = state.crowd.queues[static_cast<size_t>(lift)];
    const ml::Place at = queue_spot(plan, a.ticket - q.served);
    a.x = at.x;
    a.z = at.z;
    a.y = state.level->ground_at(a.x, a.z);
    a.heading = at.heading;
    a.speed = 0;
  }
}

bool ride_group(GameState& state, CrowdGroup& g, int32_t lift) {
  CrowdState& crowd = state.crowd;
  const ml::LiftPlan& plan = plan_of(state, lift);
  const int32_t seats = seats_of(plan);
  const int32_t first = core::int_in(crowd.rng, 0, plan.carriers - 1);
  size_t seated = 0;
  for (int32_t n = 0; n < plan.carriers && seated < g.members.size(); n++) {
    const int32_t k = (first + n) % plan.carriers;
    const ml::LoopSpot spot = ml::carrier_at(plan, k, state.t);
    // A carrier out on the up rope, short of its top, and empty.
    if (spot.side != 0 || spot.turn >= 0 || spot.u >= ml::off_at(plan) * 0.95) continue;
    bool taken = false;
    for (const Amateur& o : crowd.amateurs) {
      taken = taken || (o.mode == AmateurMode::ride && o.lift == lift && o.carrier == k);
    }
    if (taken) continue;
    for (int32_t seat = 0; seat < seats && seated < g.members.size(); seat++) {
      Amateur& a = crowd.amateurs[static_cast<size_t>(g.members[seated++])];
      a.mode = AmateurMode::ride;
      a.lift = lift;
      a.carrier = k;
      a.seat = seat;
    }
  }
  if (seated == 0) return false;
  // Whoever did not find a seat waits at the foot for the next carriers.
  for (size_t m = seated; m < g.members.size(); m++) {
    Amateur& a = crowd.amateurs[static_cast<size_t>(g.members[m])];
    join_queue(state, a, lift);
    const ml::Place at =
        queue_spot(plan, a.ticket - crowd.queues[static_cast<size_t>(lift)].served);
    a.x = at.x;
    a.z = at.z;
    a.y = state.level->ground_at(a.x, a.z);
    a.heading = at.heading;
  }
  for (size_t m = 0; m < seated; m++)
    ride(state, crowd.amateurs[static_cast<size_t>(g.members[m])]);
  return true;
}

void step_crowd_lifts(GameState& state) {
  CrowdState& crowd = state.crowd;
  for (int32_t lift = 0; lift < static_cast<int32_t>(crowd.queues.size()); lift++) {
    board(state, lift);
  }
  for (Amateur& a : crowd.amateurs) {
    if (a.mode == AmateurMode::queue) {
      const ml::LiftPlan& plan = plan_of(state, a.lift);
      const CrowdQueue& q = crowd.queues[static_cast<size_t>(a.lift)];
      const ml::Place at = queue_spot(plan, a.ticket - q.served);
      if (skate_to(state, a, at.x, at.z)) a.heading = at.heading;
    } else if (a.mode == AmateurMode::ride) {
      ride(state, a);
    } else if (a.mode == AmateurMode::skate) {
      skate_off(state, a);
    }
  }
}

}  // namespace fall_line::sim::crowd
