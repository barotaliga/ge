// The crowd on the runs (crowd.hpp): the network read off the map, the deal, every amateur's
// decisions and his way down his run, the player meeting them, and the hash.
//
// ge:summary The crowd on the runs: the deal, the decisions, the way down, the player met, the hash
// ge:keywords crowd deal_crowd step_crowd meet_crowd crowd_hash decide move run-out launch
// ge:keywords group-launch knock bump cap turn-wave regroup
// ge:skill sim
// ge:test tests/fall_line_crowd_test.cpp
#include "fall_line/sim/crowd.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <optional>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"
#include "fall_line/sim/crowd_lift.hpp"
#include "fall_line/sim/free.hpp"
#include "mountain/lifts/lift_loop.hpp"
#include "mountain/mapgen/query.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace ml = ge::mountain::lifts;
namespace core = ge::core;
namespace lm = ge::core::libm;
using namespace crowd;

const char* amateur_mode_name(AmateurMode m) {
  constexpr const char* kNames[] = {"ski", "stop", "down", "queue", "ride", "skate", "unseen"};
  return kNames[static_cast<size_t>(m)];
}

namespace crowd {

const char* body_name(Body b) {
  constexpr const char* kNames[] = {"man",     "woman",     "teen",      "child",
                                    "old-man", "old-woman", "freerider", "retro"};
  return kNames[static_cast<size_t>(b)];
}

const char* kind_name(Kind k) {
  constexpr const char* kNames[] = {"cruiser", "carver", "bomber",     "beginner", "zigzag",
                                    "stopper", "drunk",  "instructor", "kid"};
  return kNames[static_cast<size_t>(k)];
}

const char* group_name(GroupKind g) {
  constexpr const char* kNames[] = {"solo", "couple", "family", "friends", "school", "drunks"};
  return kNames[static_cast<size_t>(g)];
}

Along sample_run(const mg::Track& run, double s) {
  const auto& pts = run.points;
  const auto n = static_cast<int64_t>(pts.size());
  const double spacing = run.length / static_cast<double>(n - 1);
  auto i = static_cast<int64_t>(std::floor(s / spacing));
  i = i < 0 ? 0 : i > n - 2 ? n - 2 : i;
  while (i > 0 && pts[static_cast<size_t>(i)].s > s) i--;
  while (i < n - 2 && pts[static_cast<size_t>(i + 1)].s < s) i++;
  const mg::TrackPoint& a = pts[static_cast<size_t>(i)];
  const mg::TrackPoint& b = pts[static_cast<size_t>(i + 1)];
  const double span = b.s - a.s > 1e-6 ? b.s - a.s : 1e-6;
  const double t = core::clamp((s - a.s) / span, 0, 1);
  const double turn = core::angle_diff(a.heading, b.heading);
  Along out;
  out.x = a.x + (b.x - a.x) * t;
  out.z = a.z + (b.z - a.z) * t;
  out.y = a.y + (b.y - a.y) * t;
  out.heading = a.heading + turn * t;
  out.width = a.width + (b.width - a.width) * t;
  out.pitch = (a.y - b.y) / span;
  out.bend = turn / span;
  return out;
}

double lateral_of(const Along& at, double x, double z) {
  return (x - at.x) * lm::cos(at.heading) - (z - at.z) * lm::sin(at.heading);
}

void place_on_run(const GameState& state, Amateur& a) {
  const Along at = sample_run((*state.runs)[static_cast<size_t>(a.run)], a.s);
  a.x = at.x + lm::cos(at.heading) * a.d;
  a.z = at.z - lm::sin(at.heading) * a.d;
  a.y = state.level->ground_at(a.x, a.z);
  a.heading = at.heading + a.yaw;
  a.vx = a.speed * lm::sin(a.heading);
  a.vz = a.speed * lm::cos(a.heading);
}

double run_weight(const CrowdRun& r, double skill, bool lengthy) {
  const auto g = static_cast<size_t>(r.grade);
  const double fits = skill >= kNeeds[g] ? 1 : kLost;
  return kShare[g] * fits * (lengthy ? r.length : std::sqrt(r.length));
}

double group_skill(const CrowdState& crowd, const CrowdGroup& g) {
  double skill = 1;
  for (const int32_t m : g.members) {
    const double k = crowd.amateurs[static_cast<size_t>(m)].knobs.skill;
    skill = k < skill ? k : skill;
  }
  return skill;
}

}  // namespace crowd

namespace {

// What the crowd's stream is seeded with beside the map's seed.
constexpr uint32_t kCrowdSalt = 0x0c40d5u;
constexpr double kG = 9.81;
// How fast one outside his run's edges is brought back onto its snow, m/s.
constexpr double kEaseIn = 2;

double band(core::Rng& rng, Band b) { return core::range(rng, b.lo, b.hi); }

template <typename T, size_t N>
T pick(core::Rng& rng, const std::array<T, N>& from) {
  return from[core::pick_index(rng, N)];
}

// THE NETWORK off the map: each run's colour, its merge and its foot, and each lift's ramps.
void read_net(GameState& state) {
  CrowdState& crowd = state.crowd;
  const auto& runs = *state.runs;
  crowd.net.assign(runs.size(), CrowdRun{});
  const auto& lifts = *state.lifts;
  crowd.ramps.assign(lifts.size(), {});
  crowd.queues.assign(lifts.size(), CrowdQueue{});
  const std::optional<mg::ResortMap>& has = state.level->resort;
  if (!has.has_value()) {
    if (!runs.empty()) crowd.net[0] = CrowdRun{1, runs[0].length, -1, 0, true, -1, {}};
    return;
  }
  const mg::ResortMap& resort = has.value();
  const mg::Level& level = *state.level;
  for (size_t i = 0; i < runs.size() && i < resort.runs.size(); i++) {
    const mg::Run& r = resort.runs[i];
    CrowdRun& c = crowd.net[i];
    c.grade = r.kind == mg::RunKind::road ? 4 : static_cast<int32_t>(r.grade);
    c.length = runs[i].length;
    c.top = !r.branch && !r.to;
    if (r.into) {
      c.into = run_index(level, r.into->run);
      c.into_s = r.into->s;
    }
    // The lifts it passes: each one's load point near its line, at the arc nearest it.
    for (const ml::LiftPlan& p : lifts) {
      if (resort.lifts[static_cast<size_t>(p.lift)].ramps.empty()) continue;
      const ml::Place at = load_place(p);
      const mg::TrackHit hit = mg::nearest_track_point(runs[i], at.x, at.z);
      if (hit.distance < kStopReach) c.stops.push_back(CrowdStop{p.lift, hit.s});
    }
    if (r.to) {
      for (size_t k = 0; k < resort.lifts.size(); k++) {
        if (resort.lifts[k].id == *r.to) c.foot = static_cast<int32_t>(k);
      }
    } else if (c.into < 0 && !runs[i].points.empty()) {
      // At its foot the lift whose load point is nearest its end, if he can skate to it.
      const mg::TrackPoint& end = runs[i].points.back();
      double best = kLiftReach;
      for (const ml::LiftPlan& p : lifts) {
        if (resort.lifts[static_cast<size_t>(p.lift)].ramps.empty()) continue;
        const ml::Place at = load_place(p);
        const double d = core::hypot(at.x - end.x, at.z - end.z);
        if (d < best) {
          best = d;
          c.foot = p.lift;
        }
      }
    }
  }
  for (size_t k = 0; k < resort.lifts.size() && k < lifts.size(); k++) {
    for (const mg::SummitRamp& r : resort.lifts[k].ramps) {
      const int32_t run = run_index(level, r.run);
      if (run < 0) continue;
      crowd.ramps[k].push_back(CrowdRamp{run, r.from.x, r.from.z, r.to.x, r.to.z, r.to.s});
    }
  }
}

int32_t pick_run(CrowdState& crowd, double skill, bool anywhere) {
  double total = 0;
  for (const CrowdRun& r : crowd.net)
    total += anywhere || r.top ? run_weight(r, skill, anywhere) : 0;
  if (total <= 0) return 0;
  double at = core::next(crowd.rng) * total;
  for (size_t i = 0; i < crowd.net.size(); i++) {
    const CrowdRun& r = crowd.net[i];
    if (!anywhere && !r.top) continue;
    at -= run_weight(r, skill, anywhere);
    if (at <= 0) return static_cast<int32_t>(i);
  }
  return 0;
}

// STAND A GROUP ON A RUN: the leader furthest down, the rest behind him at the group's gap, each
// on a line of his own about the piste (a snake in the leader's). Anywhere down any run at the
// deal; near the head of a run off a lift's top after a ride unseen.
void launch_group(GameState& state, CrowdGroup& g, bool anywhere) {
  CrowdState& crowd = state.crowd;
  core::Rng& rng = crowd.rng;
  const int32_t run = pick_run(crowd, group_skill(crowd, g), anywhere);
  const mg::Track& track = (*state.runs)[static_cast<size_t>(run)];
  const double span = g.gap * static_cast<double>(g.members.size() - 1);
  const double room = track.length - span - 20;
  const double s0 = anywhere ? core::range(rng, 0, room > 1 ? room : 1) : 2;
  const Along at = sample_run(track, s0 + span);
  const double half = at.width / 2;
  const double centre = core::range(rng, -0.4, 0.4) * half;
  for (size_t k = 0; k < g.members.size(); k++) {
    Amateur& a = crowd.amateurs[static_cast<size_t>(g.members[k])];
    const double side = g.track ? 0 : (k % 2 ? -2.5 : 2.5) * std::ceil(static_cast<double>(k) / 2);
    a.mode = AmateurMode::ski;
    a.run = run;
    a.lift = -1;
    a.s = s0 + span - static_cast<double>(k) * g.gap;
    a.centre = core::clamp(centre + side, -half + 1, half - 1);
    a.d = a.centre;
    a.speed = anywhere ? core::range(rng, 1, 4) : 0;
    a.yaw = 0;
    a.timer = 0;
    a.rising = false;
    place_on_run(state, a);
  }
}

Knobs deal_knobs(core::Rng& rng, Kind kind) {
  const KindDef& k = kKinds[static_cast<size_t>(kind)];
  Knobs o;
  o.skill = band(rng, k.skill);
  o.aggression = band(rng, k.aggression);
  o.width = band(rng, k.width);
  o.wobble = band(rng, k.wobble);
  o.stopper = band(rng, k.stopper);
  o.style = pick(rng, k.styles);
  const StyleDef& s = kStyleDefs[static_cast<size_t>(o.style)];
  o.turn = core::range(rng, s.len_lo, s.len_hi);
  return o;
}

GroupKind pick_group(core::Rng& rng) {
  double total = 0;
  for (const GroupDef& g : kGroups) total += g.weight;
  double at = core::next(rng) * total;
  for (size_t i = 0; i < kGroups.size(); i++) {
    at -= kGroups[i].weight;
    if (at <= 0) return static_cast<GroupKind>(i);
  }
  return GroupKind::solo;
}

// The pace he means: his skill's, as much of it as he dares, his style's, slow on a colour past
// him.
double cap_of(const Amateur& a, int32_t grade) {
  const Knobs& k = a.knobs;
  if (k.style == Style::slip) return kSlipSpeed;
  double cap = kBaseSpeed + kSpanSpeed * lm::pow(k.skill, 1.2) * (0.55 + 0.45 * k.aggression);
  if (k.style == Style::line) cap *= kLineSpeed;
  const double past = kPastBase + kPastSkill * k.skill;
  if (k.skill < kNeeds[static_cast<size_t>(grade)] && past < cap) cap = past;
  return cap;
}

// The lateral his turns sweep through now: his line, his style's wave and a drunk's sway.
double target_of(const Amateur& a, double half, double t) {
  const Knobs& k = a.knobs;
  const StyleDef& style = kStyleDefs[static_cast<size_t>(k.style)];
  const double amp = style.metres ? style.amp * (0.5 + k.width) : style.amp * k.width * half;
  const double wave = style.sharp ? (2 / core::kPi) * lm::asin(lm::sin(a.phase)) : lm::sin(a.phase);
  const double id = static_cast<double>(a.id);
  const double sway = k.wobble * (3 * lm::sin(t * 0.7 + id) + 1.5 * lm::sin(t * 1.9 + id * 3));
  return core::clamp(a.centre + amp * wave + sway, -(half - 0.8), half - 0.8);
}

void fall_down(core::Rng& rng, Amateur& a) {
  a.mode = AmateurMode::down;
  a.timer = band(rng, kLie);
  a.rising = false;
}

void stop_at(Amateur& a, double timer) {
  a.mode = AmateurMode::stop;
  a.timer = timer;
}

// WHAT HE DOES NEXT, every `kThinkS` seconds.
void decide(GameState& state, Amateur& a) {
  CrowdState& crowd = state.crowd;
  core::Rng& rng = crowd.rng;
  const CrowdRun& r = crowd.net[static_cast<size_t>(a.run)];
  const mg::Track& track = (*state.runs)[static_cast<size_t>(a.run)];
  const Along here = sample_run(track, a.s);
  const double half = here.width / 2;
  const Knobs& k = a.knobs;
  CrowdGroup& g = crowd.groups[static_cast<size_t>(a.group)];
  a.cap = cap_of(a, r.grade);
  if (a.mode == AmateurMode::down) return;

  // KEEPING UP WITH THE GROUP: a follower skis off his leader.
  const Amateur& leader = crowd.amateurs[static_cast<size_t>(g.members[0])];
  if (a.rank > 0 && !on_lift(leader) && leader.run == a.run) {
    const double behind = leader.s - a.s;
    const double want = a.rank * g.gap;
    const bool held = leader.mode == AmateurMode::stop || leader.mode == AmateurMode::down;
    if (held && behind < want + 8) {
      if (a.mode != AmateurMode::stop) stop_at(a, 1);
      a.timer = a.timer > 1 ? a.timer : 1;
      a.centre = core::clamp(leader.d + (a.rank % 2 ? -2 : 2), -half + 1, half - 1);
      return;
    }
    a.cap *= core::clamp(1 + (behind - want) / 30, 0.6, 1.35);
    if (g.track) {
      a.centre = leader.centre;
      a.phase = leader.phase - core::kPi * behind / k.turn;
    } else if (std::abs(a.centre - leader.centre) > half * 0.6) {
      a.centre += (leader.centre - a.centre) * 0.3;
    }
  }

  // THE LEADER WAITS for the last of his group to come down to him, and a parent or an instructor
  // skis at the slowest child's pace.
  if (a.rank == 0 && g.members.size() > 1) {
    double far = 0;
    for (const int32_t m : g.members) {
      const Amateur& o = crowd.amateurs[static_cast<size_t>(m)];
      if (o.id == a.id || o.mode == AmateurMode::unseen) continue;
      if (g.kind == GroupKind::family || g.kind == GroupKind::school) {
        const double slow = cap_of(o, r.grade) * 1.1;
        a.cap = slow < a.cap ? slow : a.cap;
      }
      // One still on the lift is waited for at its top, never from further down.
      if (on_lift(o)) {
        if (a.s < kRegroupTop) far = far > kRegroupFar + 1 ? far : kRegroupFar + 1;
        continue;
      }
      const double lag = o.run == a.run                ? a.s - o.s - o.rank * g.gap
                         : o.mode == AmateurMode::down ? 1e9
                                                       : 0;
      far = lag > far ? lag : far;
    }
    if (a.mode == AmateurMode::stop && far > kRegroupNear) {
      a.timer = a.timer > 1 ? a.timer : 1;
      return;
    }
    if (a.mode == AmateurMode::ski && far > kRegroupFar) {
      stop_at(a, 2);
      a.centre = (a.d >= 0 ? 1 : -1) * (half - 2.5);
      return;
    }
  }
  if (a.mode != AmateurMode::ski) return;

  // A FALL for nothing: the less skill, the steeper past it, the drunker.
  const double need = kNeeds[static_cast<size_t>(r.grade)];
  const double past = need - k.skill > 0 ? need - k.skill : 0;
  const double wild = a.speed > a.cap * 1.3 ? 2 : 1;
  const double rate = kFallRate * (1 - k.skill) * (1 - k.skill) * (1 + kFallSteep * past * 4) *
                      (1 + kFallWobble * k.wobble * 3) * wild;
  if (core::chance(rng, rate / 60 * kThinkS)) {
    fall_down(rng, a);
    return;
  }

  // A STOP, and the worse he is the likelier just below a crest.
  if (a.rank == 0 && k.stopper > 0) {
    const double back = a.s - kCrestBack > 0 ? a.s - kCrestBack : 0;
    const Along above = sample_run(track, back);
    const double crest = here.pitch - above.pitch > kCrestPitch ? kCrest : 1;
    if (core::chance(rng, kStopRate * k.stopper * crest / 60 * kThinkS)) {
      stop_at(a, band(rng, kStopHold));
      // The considerate ones pull over; the stopper stops where he is.
      if (k.stopper < 0.6) a.centre = (a.d >= 0 ? 1 : -1) * (half - 2.5);
      return;
    }
  }

  // ROOM: the one ahead on his run, or the player: a berth and, unless he is the kind that does
  // not give one, their pace. One level with him counts too for the one dealt later; in a school's
  // snake he drops back into his place instead.
  const double look = kRoomAhead + a.speed * kRoomTime;
  int32_t dodge = 0;
  double pace = 1e9;
  double held = 1e9;
  for (const int32_t m : crowd.on_run[static_cast<size_t>(a.run)]) {
    const Amateur& o = crowd.amateurs[static_cast<size_t>(m)];
    if (o.id == a.id) continue;
    const double ahead = o.s - a.s;
    const bool first = ahead > 0 || (ahead > -kLevel && o.id < a.id);
    if (!first || ahead > look || std::abs(o.d - a.d) > kBerth) continue;
    if (g.track && o.group == a.group) {
      const double v = o.speed * (ahead < kRoomAhead / 2 ? kSnakeBack : 1);
      held = v < held ? v : held;
      continue;
    }
    dodge = o.d >= a.d ? -1 : 1;
    pace = o.speed < pace ? o.speed : pace;
  }
  // Never below a crawl: a pace held off one stood still would stand him still too.
  const double floor = held > kCrawl ? held : kCrawl;
  a.cap = floor < a.cap ? floor : a.cap;
  if (!state.racers.empty()) {
    const ge::skier::SkierState& me = state.racers[0].skier;
    const double fx = lm::sin(a.heading);
    const double fz = lm::cos(a.heading);
    const double px = me.x - a.x;
    const double pz = me.z - a.z;
    const double p_ahead = px * fx + pz * fz;
    const double p_side = px * fz - pz * fx;
    if (p_ahead > 0 && p_ahead < look && std::abs(p_side) < kBerth + 0.6) {
      dodge = p_side >= 0 ? -1 : 1;
      pace = me.speed < pace ? me.speed : pace;
    }
  }
  if (dodge != 0) {
    a.centre = core::clamp(a.centre + dodge * (kBerth + 0.5), -half + 1, half - 1);
    const double slow = pace > kCrawl ? pace : kCrawl;
    if (k.aggression < 0.6 && slow < a.cap) a.cap = slow;
  }
}

// OFF THE END OF HIS RUN: on down the one it merges into, where he is across the snow kept; at
// its foot into the lift's queue there, or up one unseen.
void run_out(GameState& state, Amateur& a) {
  CrowdState& crowd = state.crowd;
  const CrowdRun& r = crowd.net[static_cast<size_t>(a.run)];
  if (r.into >= 0) {
    // Where he is on the run he merges into: its nearest point to him, so nothing jumps.
    const mg::Track& next = (*state.runs)[static_cast<size_t>(r.into)];
    const mg::TrackHit hit = mg::nearest_track_point(next, a.x, a.z);
    const Along at = sample_run(next, hit.s);
    a.run = r.into;
    a.s = hit.s;
    a.d = hit.lateral;
    a.yaw = core::angle_diff(at.heading, a.heading);
    a.centre = core::clamp(a.d, -at.width / 2 + 1, at.width / 2 - 1);
    return;
  }
  if (r.foot >= 0) {
    join_queue(state, a, r.foot);
    return;
  }
  a.mode = AmateurMode::unseen;
  CrowdGroup& g = crowd.groups[static_cast<size_t>(a.group)];
  bool all = true;
  for (const int32_t m : g.members) {
    all = all && crowd.amateurs[static_cast<size_t>(m)].mode == AmateurMode::unseen;
  }
  if (all) g.unseen = band(crowd.rng, kUnseenLift);
}

// DOWN IN THE SNOW: slid to a stop down the way he was going, lain there, got up.
void move_down(Amateur& a) {
  a.speed = a.speed - kSlideDecel * kDt > 0 ? a.speed - kSlideDecel * kDt : 0;
  a.s += a.speed * lm::cos(a.yaw) * kDt;
  a.d += a.speed * lm::sin(a.yaw) * kDt;
  if (a.speed <= 0) {
    a.timer -= kDt;
    if (a.timer <= 0 && !a.rising) {
      a.rising = true;
      a.timer = kRiseS;
    } else if (a.timer <= 0) {
      a.rising = false;
      a.mode = AmateurMode::ski;
      a.yaw = 0;
    }
  }
}

// THE LIFTS HIS RUN PASSES: a leader going by one takes it at `kTakeStop` (never the one at the
// run's foot, which he comes to anyway), and the rest of his group take it where he did. Whether
// he left the run for its queue.
bool pass_stops(GameState& state, Amateur& a, double s0) {
  CrowdState& crowd = state.crowd;
  const CrowdRun& r = crowd.net[static_cast<size_t>(a.run)];
  CrowdGroup& g = crowd.groups[static_cast<size_t>(a.group)];
  for (const CrowdStop& stop : r.stops) {
    if (stop.s <= s0 || stop.s > a.s || stop.lift == r.foot) continue;
    if (a.rank == 0 && a.mode == AmateurMode::ski && core::chance(crowd.rng, kTakeStop)) {
      g.stop_lift = stop.lift;
      g.stop_run = a.run;
      g.stop_s = stop.s;
    }
    if (g.stop_lift == stop.lift && g.stop_run == a.run && a.mode != AmateurMode::down) {
      join_queue(state, a, stop.lift);
      return true;
    }
  }
  return false;
}

// ONE AMATEUR'S STEP ON THE SNOW.
void move(GameState& state, Amateur& a) {
  const mg::Track& track = (*state.runs)[static_cast<size_t>(a.run)];
  const Along here = sample_run(track, a.s);
  const double half = here.width / 2;
  const Knobs& k = a.knobs;
  const StyleDef& style = kStyleDefs[static_cast<size_t>(k.style)];
  const double yaw0 = a.yaw;
  double v = a.speed;
  double braking = 0;
  if (a.mode == AmateurMode::down) {
    move_down(a);
  } else {
    // THE LINE: steer for the lateral his turns sweep through now.
    const bool stopping = a.mode == AmateurMode::stop;
    const double aim = stopping ? a.centre : target_of(a, half, state.t);
    const double ahead = v * 0.9 + 2 > 3 ? v * 0.9 + 2 : 3;
    const double want = core::clamp(lm::atan2(aim - a.d, ahead), -style.max_yaw, style.max_yaw);
    const double rate = kYawRate.lo + (kYawRate.hi - kYawRate.lo) * k.skill;
    const double jitter = k.wobble * 0.6 * lm::sin(state.t * 3.1 + static_cast<double>(a.id) * 1.7);
    a.yaw += core::clamp(want + jitter - a.yaw, -rate * kDt, rate * kDt);
    // THE SPEED: the fall along his line, less the snow, the air and the scrub.
    const double cap = stopping ? 0 : a.cap;
    double acc = kG * here.pitch * lm::cos(a.yaw) - kSnowDrag - kAirDrag * v * v -
                 kScrub * std::abs(a.yaw) * (1 - k.skill * 0.6);
    if (v > cap) {
      braking = stopping ? 1 : core::clamp((v - cap) / 2, 0, 1);
      acc -= kBrake * braking;
    }
    // Where the hill will not carry him to the pace he means he skates and poles for it.
    const double work = cap < kPushTop ? cap : kPushTop;
    if (!stopping && v < work && acc < kPush) {
      const double push = core::clamp((work - v) / kPushEase, 0, 1);
      acc = acc > kPush * push ? acc : kPush * push;
    }
    v = v + acc * kDt > 0 ? v + acc * kDt : 0;
    if (stopping) {
      // Below a crawl he sets his edges: the brake takes the rest off whatever the pitch, and the
      // stop's clock runs.
      if (v < kCrawl) v = v - kBrake * kDt > 0 ? v - kBrake * kDt : 0;
      if (v < kStand) v = 0;
      if (v < kCrawl) {
        a.timer -= kDt;
        if (a.timer <= 0) a.mode = AmateurMode::ski;
      }
    }
    a.speed = v;
    const double ds = v * lm::cos(a.yaw) * kDt;
    const double s0 = a.s;
    a.s += ds;
    if (pass_stops(state, a, s0)) return;
    a.d += v * lm::sin(a.yaw) * kDt;
    a.phase += core::kPi * ds / k.turn;
  }
  // Kept on the snow of his run, eased in from outside it (a merge onto a narrower one).
  const double room = half - 0.4;
  const double ease_in = kEaseIn * kDt;
  if (a.d > room) a.d = a.d - ease_in > room ? a.d - ease_in : room;
  if (a.d < -room) a.d = a.d + ease_in < -room ? a.d + ease_in : -room;
  if (a.s < 0) a.s = 0;
  if (a.s >= track.length) {
    run_out(state, a);
    if (on_lift(a)) return;
  }
  place_on_run(state, a);

  // THE FIGURE: leaned into the turn by its pull, low by his speed and his nerve, the wedge.
  const double omega = (a.yaw - yaw0) / kDt + here.bend * v;
  const double sway = k.wobble * 0.25 * lm::sin(state.t * 2.3 + static_cast<double>(a.id));
  const double lean =
      core::clamp(lm::atan2(v * omega, kG) * (0.45 + 0.55 * k.skill) + sway, -0.9, 0.9);
  const double ease = 1 - lm::exp(-kDt * 6);
  a.lean += (lean - a.lean) * ease;
  const double tuck = k.style == Style::line ? core::clamp(v / 16, 0, 1) * 0.9 : 0;
  const double nerve = 0.2 + 0.3 * k.aggression * core::clamp(v / 12, 0, 1) + 0.25 * (1 - k.skill);
  const double crouch = tuck > nerve ? tuck : nerve;
  a.crouch += (crouch - a.crouch) * ease;
  const bool wedge =
      k.style == Style::plough || (k.skill < 0.3 && (braking > 0 || a.mode == AmateurMode::stop));
  a.plough += ((wedge ? 0.9 : 0) - a.plough) * ease;
}

// The groups up a lift unseen whose ride is over, stood near the head of a run off a lift's top.
void unseen_groups(GameState& state) {
  CrowdState& crowd = state.crowd;
  for (CrowdGroup& g : crowd.groups) {
    if (g.unseen <= 0) continue;
    g.unseen -= kDt;
    if (g.unseen <= 0) launch_group(state, g, false);
  }
}

}  // namespace

void deal_crowd(GameState& state, int32_t count) {
  CrowdState& crowd = state.crowd;
  crowd = CrowdState{};
  if (count <= 0 || state.mode != Mode::free || !state.runs || state.runs->empty() ||
      !state.lifts) {
    return;
  }
  read_net(state);
  crowd.active = true;
  crowd.rng = core::make_rng(core::num::to_uint32(state.level->seed) ^ kCrowdSalt);
  core::Rng& rng = crowd.rng;
  const int32_t total = count < 0 ? 0 : count > kMost ? kMost : count;
  crowd.amateurs.reserve(static_cast<size_t>(total));
  while (static_cast<int32_t>(crowd.amateurs.size()) < total) {
    const GroupKind kind = pick_group(rng);
    const GroupDef& def = kGroups[static_cast<size_t>(kind)];
    const int32_t left = total - static_cast<int32_t>(crowd.amateurs.size()) - 1;
    const int32_t want = core::int_in(rng, def.follow_lo, def.follow_hi);
    const int32_t follow = want < left ? want : left;
    CrowdGroup g;
    g.kind = kind;
    g.track = def.track;
    g.gap = def.gap;
    const auto index = static_cast<int32_t>(crowd.groups.size());
    const size_t head = crowd.amateurs.size();
    for (int32_t k = 0; k <= follow; k++) {
      // A family's children come first after the parent, so there is one.
      const Kind who = k == 0                                ? pick(rng, def.lead)
                       : kind == GroupKind::family && k == 1 ? Kind::kid
                                                             : pick(rng, def.follow);
      Amateur a;
      a.id = static_cast<int32_t>(crowd.amateurs.size());
      a.group = index;
      a.rank = k;
      a.kind = who;
      a.body = pick(rng, kKinds[static_cast<size_t>(who)].bodies);
      a.knobs = deal_knobs(rng, who);
      // In the snake, the instructor's turns at the children's own skill.
      if (def.track && k > 0) {
        const Knobs& lead = crowd.amateurs[head].knobs;
        a.knobs.style = lead.style;
        a.knobs.turn = lead.turn;
        a.knobs.width = lead.width;
      }
      a.phase = core::range(rng, 0, core::kTau);
      a.think = kThinkS * static_cast<double>(a.id % 8) / 8;
      g.members.push_back(a.id);
      crowd.amateurs.push_back(a);
    }
    crowd.groups.push_back(std::move(g));
    CrowdGroup& dealt = crowd.groups.back();
    int32_t lift = -1;
    const bool riding = core::chance(rng, kRiding);
    if (riding || core::chance(rng, kQueued)) {
      int32_t lifts = 0;
      for (const auto& r : crowd.ramps) lifts += r.empty() ? 0 : 1;
      if (lifts > 0) {
        int32_t pick_k = core::int_in(rng, 0, lifts - 1);
        for (size_t k = 0; k < crowd.ramps.size() && lift < 0; k++) {
          if (!crowd.ramps[k].empty() && pick_k-- == 0) lift = static_cast<int32_t>(k);
        }
      }
    }
    if (lift >= 0 && riding && ride_group(state, dealt, lift)) {
      // Riding it already, part-way up.
    } else if (lift >= 0) {
      queue_group(state, dealt, lift);
    } else {
      launch_group(state, dealt, true);
    }
  }
  crowd.on_run.assign(crowd.net.size(), {});
  for (auto& list : crowd.on_run) list.reserve(crowd.amateurs.size());
}

void step_crowd(GameState& state) {
  CrowdState& crowd = state.crowd;
  if (!crowd.active) return;
  unseen_groups(state);
  step_crowd_lifts(state);
  for (auto& list : crowd.on_run) list.clear();
  for (const Amateur& a : crowd.amateurs) {
    if (!on_lift(a)) crowd.on_run[static_cast<size_t>(a.run)].push_back(a.id);
  }
  for (Amateur& a : crowd.amateurs) {
    if (on_lift(a)) continue;
    a.think -= kDt;
    if (a.think <= 0) {
      a.think += kThinkS;
      decide(state, a);
    }
    move(state, a);
  }
}

void meet_crowd(GameState& state) {
  CrowdState& crowd = state.crowd;
  if (!crowd.active) return;
  for (int32_t i = 0; i < static_cast<int32_t>(state.racers.size()); i++) {
    Racer& racer = state.racers[static_cast<size_t>(i)];
    // On a lift or a machine he is the machine's: nobody shoulders him.
    if (racer.free.lift.phase != LiftPhase::none) continue;
    if (i == 0 && (state.sled.rider || state.heli.rider)) continue;
    ge::skier::SkierState& c = racer.skier;
    for (Amateur& a : crowd.amateurs) {
      if (on_lift(a)) continue;
      if (std::abs(a.x - c.x) > 3 || std::abs(a.z - c.z) > 3 || std::abs(a.y - c.y) > 2) continue;
      const Size& size = kSizes[static_cast<size_t>(a.body)];
      const double dx = a.x - c.x;
      const double dz = a.z - c.z;
      const double dist = core::hypot(dx, dz);
      const double pen = size.radius + kPlayerRadius - dist;
      if (pen <= 0) continue;
      const double nx = dist > 1e-6 ? dx / dist : 1;
      const double nz = dist > 1e-6 ? dz / dist : 0;
      const double share = size.mass / (kPlayerMass + size.mass);
      c.x -= nx * pen * share;
      c.z -= nz * pen * share;
      const Along at = sample_run((*state.runs)[static_cast<size_t>(a.run)], a.s);
      const double px = nx * pen * (1 - share);
      const double pz = nz * pen * (1 - share);
      a.s += px * lm::sin(at.heading) + pz * lm::cos(at.heading);
      a.d += px * lm::cos(at.heading) - pz * lm::sin(at.heading);
      a.x += px;
      a.z += pz;
      const double closing = (c.vx - a.vx) * nx + (c.vz - a.vz) * nz;
      if (closing <= 0) continue;
      const double j = (1 + kRestitution) * closing / (1 / kPlayerMass + 1 / size.mass);
      c.vx -= j / kPlayerMass * nx;
      c.vz -= j / kPlayerMass * nz;
      a.vx += j / size.mass * nx;
      a.vz += j / size.mass * nz;
      a.speed = core::hypot(a.vx, a.vz);
      a.yaw = core::angle_diff(at.heading, lm::atan2(a.vx, a.vz));
      const double knock = kKnock.lo + (kKnock.hi - kKnock.lo) * a.knobs.skill;
      if (closing >= knock && a.mode != AmateurMode::down) fall_down(crowd.rng, a);
      if (closing >= kBumpEvent) {
        state.events.push_back(GameEvent{EventKind::bump, i, a.id, closing});
      }
    }
  }
}

uint64_t crowd_hash(const CrowdState& crowd, uint64_t h) {
  const auto mix = [&h](double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  mix(crowd.rng.a);
  for (const Amateur& a : crowd.amateurs) {
    mix(static_cast<double>(a.mode));
    mix(a.run);
    mix(a.s);
    mix(a.d);
    mix(a.speed);
    mix(a.yaw);
    mix(a.timer);
    mix(a.lift);
    mix(a.ticket);
    mix(a.carrier);
    mix(a.x);
    mix(a.z);
  }
  return h;
}

}  // namespace fall_line::sim
