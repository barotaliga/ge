// The wanted level's rules (wanted.hpp): the hunt begun and raised, the look and its sight line
// over the ground and through the woods, the climb, the search running out, the arrest.
//
// ge:summary The wanted level's bodies: the hunt, the sight line, the woods, the step, the hash
// ge:keywords wanted raise_heat step_wanted sees_quarry through_woods under_cover wanted_hash
// ge:keywords line-of-sight crown cone tree-index
// ge:skill sim
// ge:test tests/fall_line_wanted_test.cpp
#include "fall_line/sim/wanted.hpp"

#include <cmath>
#include <cstring>

#include "ai/sight.hpp"
#include "core/math.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace sk = ge::skier;

namespace {

void push(GameState& state, WantedChange change, int32_t stars) {
  state.events.push_back(GameEvent{EventKind::wanted, 0, stars, static_cast<double>(change)});
}

// A HUNT BEGUN: nothing out of the last one but the pursuers already out and the rule of fire.
void begin_hunt(GameState& state) {
  WantedState& w = state.wanted;
  const Spot q = quarry_of(state);
  w.active = true;
  w.stars = 0;
  w.heat = 0;
  w.seen = false;
  w.unseen = 0;
  w.fix = 0;
  ge::ai::see(w.last, q.x, q.y, q.z, state.t);
  w.look = 0;
  w.since = state.t;
  w.stopped = 0;
  w.ended = WantedEnd::running;
  w.peak = 0;
  w.reached.fill(-1);
  w.reached[0] = state.t;
  w.cause = WantedCause::dev;
}

// The hunt ready for a raise: begun on the first, and again after one he escaped. Null once a
// hunt ended in his arrest: that run is over.
WantedState* hunt_of(GameState& state) {
  WantedState& w = state.wanted;
  if (w.active && w.ended == WantedEnd::arrested) return nullptr;
  if (!w.active || w.ended == WantedEnd::escaped || w.stars == 0) begin_hunt(state);
  return &w;
}

// Set the heat to `heat` star units and say what the stars did.
void heat_to(GameState& state, double heat) {
  WantedState& w = state.wanted;
  const int32_t was = w.stars;
  const double most = wanted::kMaxStars;
  w.heat = heat < 0 ? 0 : heat > most ? most : heat;
  const auto stars = static_cast<int32_t>(std::floor(w.heat));
  if (stars == was) return;
  w.stars = stars;
  for (int32_t s = was + 1; s <= stars; s++) w.reached[static_cast<size_t>(s)] = state.t;
  if (stars > w.peak) w.peak = stars;
  push(state, stars > was ? WantedChange::raised : WantedChange::lowered, stars);
}

// THE OFFENCE they now know about: they know where he is for the fix and look again at once.
void spotted(GameState& state) {
  WantedState& w = state.wanted;
  const Spot q = quarry_of(state);
  w.fix = wanted::kFixS;
  w.unseen = 0;
  w.look = 0;
  ge::ai::see(w.last, q.x, q.y, q.z, state.t);
  if (!w.seen) {
    w.seen = true;
    push(state, WantedChange::seen, w.stars);
  }
}

void end_hunt(GameState& state, WantedEnd how) {
  WantedState& w = state.wanted;
  w.ended = how;
  w.stopped = state.t;
}

// THE STARS DROPPED: a search that ran out, or a hand that cleared them.
void lose_stars(GameState& state) {
  WantedState& w = state.wanted;
  w.stars = 0;
  w.heat = 0;
  w.seen = false;
  w.fix = 0;
  end_hunt(state, WantedEnd::escaped);
  push(state, WantedChange::lost, 0);
}

// A crown's reach at `up` m over its trunk's foot: a cone from the tree's own crown radius at the
// foot to nothing at its tip, never narrower than the trunk.
double crown_reach(const mg::TreeDef& t, double up) {
  const double cone = t.crown * (1 - up / t.height);
  return cone > t.radius ? cone : t.radius;
}

bool any_sees(const GameState& state) {
  const WantedState& w = state.wanted;
  if (w.pursuers.empty()) return false;
  const Spot q = quarry_of(state);
  const bool cover = under_cover(state);
  for (const Pursuer& p : w.pursuers) {
    if (sees_quarry(*state.level, state.trees, p, q, cover)) return true;
  }
  return false;
}

}  // namespace

Spot quarry_of(const GameState& state) {
  const sk::SkierState& s = state.racers.front().skier;
  return Spot{s.x, s.y, s.z};
}

int32_t raise_heat(GameState& state, WantedCause cause) {
  WantedState* w = hunt_of(state);
  if (w == nullptr) return 0;
  const Offence& o = wanted::kOffences[static_cast<size_t>(cause)];
  w->cause = cause;
  const double add = w->heat + o.add;
  heat_to(state, add > o.floor ? add : o.floor);
  spotted(state);
  return w->stars;
}

void set_wanted(GameState& state, int32_t stars) {
  const int32_t n = stars < 0 ? 0 : stars > wanted::kMaxStars ? wanted::kMaxStars : stars;
  if (n == 0) {
    if (state.wanted.active && state.wanted.ended == WantedEnd::running && state.wanted.stars > 0) {
      lose_stars(state);
    }
    return;
  }
  WantedState* w = hunt_of(state);
  if (w == nullptr) return;
  w->cause = WantedCause::dev;
  heat_to(state, n);
  spotted(state);
}

bool arrest_quarry(GameState& state) {
  WantedState& w = state.wanted;
  if (!w.active || w.lethal || w.ended != WantedEnd::running || w.stars == 0) return false;
  end_hunt(state, WantedEnd::arrested);
  w.seen = true;
  push(state, WantedChange::arrested, w.stars);
  return true;
}

double survived_of(const WantedState& w, double t) {
  const double end = w.ended == WantedEnd::running ? t : w.stopped;
  return end > w.since ? end - w.since : 0;
}

const WantedRoster& roster_of(int32_t stars) {
  const int32_t i = stars < 0 ? 0 : stars > wanted::kMaxStars ? wanted::kMaxStars : stars;
  return wanted::kRoster[static_cast<size_t>(i)];
}

std::optional<WaveMix> ground_wave(int32_t stars, int32_t n) {
  const int32_t i = stars < 0 ? 0 : stars > wanted::kMaxStars ? wanted::kMaxStars : stars;
  const GroundWaves& row = wanted::kWaves[static_cast<size_t>(i)];
  if (!row.sends) return std::nullopt;
  const double k = n < 0 ? 0 : n;
  const auto of = [k](double base, double grow, int32_t cap) {
    const auto v = static_cast<int32_t>(std::floor(base + grow * k));
    return v < cap ? v : cap;
  };
  return WaveMix{of(row.base.snowmobiles, row.grow.snowmobiles, row.cap.snowmobiles),
                 of(row.base.tracked, row.grow.tracked, row.cap.tracked),
                 of(row.base.trucks, row.grow.trucks, row.cap.trucks),
                 of(row.base.tanks, row.grow.tanks, row.cap.tanks),
                 of(row.base.foot, row.grow.foot, row.cap.foot)};
}

int32_t waves_sent(const WantedState& w, double t) {
  if (!w.active || w.stars <= 0) return 0;
  const GroundWaves& row = wanted::kWaves[static_cast<size_t>(w.stars)];
  const double at = w.reached[static_cast<size_t>(w.stars)];
  if (!row.sends || at < 0) return 0;
  const double after = t - at - row.first_s;
  return after < 0 ? 0 : 1 + static_cast<int32_t>(std::floor(after / row.every_s));
}

bool sees_quarry(const mg::Level& level, const sk::TreeIndex& trees, const Pursuer& p, Spot q,
                 bool cover) {
  const ge::ai::Point3 eye{p.x, p.y, p.z};
  const ge::ai::Point3 chest{q.x, q.y + wanted::kChestM, q.z};
  const ge::ai::SightSpec spec{wanted::kSightM[static_cast<size_t>(p.kind)], wanted::kCloseM,
                               wanted::kLineStepM, wanted::kLineClearM};
  const bool low = looks_from_snow(p.kind);
  // From the air a man under the trees is seen between them, unless he is under cover: the woods
  // hide nothing from aloft.
  return ge::ai::sees(
      eye, chest, spec, [&](double x, double z) { return level.ground_at(x, z); },
      [&](double d) {
        if (!low) return cover && d > wanted::kCoverNearM;
        return through_woods(level, trees, Spot{p.x, p.y, p.z}, Spot{chest.x, chest.y, chest.z});
      });
}

bool through_woods(const mg::Level& level, const sk::TreeIndex& trees, Spot a, Spot b) {
  const double run = ge::core::hypot(b.x - a.x, b.z - a.z);
  const auto n = static_cast<int32_t>(std::floor(run / wanted::kWoodStepM));
  std::array<int32_t, wanted::kWoods> met{};
  int32_t count = 0;
  for (int32_t i = 1; i < n && count < wanted::kWoods; i++) {
    const double k = static_cast<double>(i) / n;
    const double x = a.x + (b.x - a.x) * k;
    const double y = a.y + (b.y - a.y) * k;
    const double z = a.z + (b.z - a.z) * k;
    sk::for_each_tree_near(trees, x, z, [&](int32_t ti) {
      if (count >= wanted::kWoods) return;
      for (int32_t m = 0; m < count; m++) {
        if (met[static_cast<size_t>(m)] == ti) return;
      }
      const mg::TreeDef& t = level.trees[static_cast<size_t>(ti)];
      const double up = y - t.y;
      if (up < 0.5 || up > t.height) return;
      if (ge::core::hypot(t.x - x, t.z - z) > crown_reach(t, up)) return;
      met[static_cast<size_t>(count++)] = ti;
    });
  }
  return count >= wanted::kWoods;
}

bool under_cover(const GameState& state) {
  const sk::SkierState& c = state.racers.front().skier;
  if (ge::core::hypot(c.vx, c.vz) > wanted::kCoverSpeed) return false;
  bool covered = false;
  sk::for_each_tree_near(state.trees, c.x, c.z, [&](int32_t ti) {
    if (covered) return;
    const mg::TreeDef& t = state.level->trees[static_cast<size_t>(ti)];
    const double up = c.y + wanted::kCoverHeadM - t.y;
    if (up < 0 || up > t.height) return;
    covered = ge::core::hypot(t.x - c.x, t.z - c.z) <= crown_reach(t, up) * wanted::kCoverCrown;
  });
  return covered;
}

void step_wanted(GameState& state) {
  WantedState& w = state.wanted;
  if (!w.active || w.ended != WantedEnd::running || w.stars == 0) return;
  // DEAD of his wounds: the hunt is over.
  if (state.racers.front().body.dead >= 0) {
    end_hunt(state, WantedEnd::killed);
    push(state, WantedChange::killed, w.stars);
    return;
  }
  w.fix = w.fix > kDt ? w.fix - kDt : 0;
  w.look -= kDt;
  if (w.look <= 0) {
    w.look += wanted::kLookEveryS;
    const bool seen = w.fix > 0 || any_sees(state);
    if (seen != w.seen) {
      w.seen = seen;
      push(state, seen ? WantedChange::seen : WantedChange::searching, w.stars);
    }
    if (seen) {
      const Spot q = quarry_of(state);
      ge::ai::see(w.last, q.x, q.y, q.z, state.t);
    }
  }
  w.unseen = w.seen ? 0 : w.unseen + kDt;
  if (!w.seen && w.unseen >= wanted::kLoseS[static_cast<size_t>(w.stars)]) {
    lose_stars(state);
    return;
  }
  // THE LONGER HE SURVIVES, the hotter it gets.
  const double climb = wanted::kClimbS[static_cast<size_t>(w.stars)];
  if (climb < wanted::kNever) {
    const double pace = w.seen ? 1 : wanted::kSearchClimb;
    heat_to(state, w.heat + kDt * pace / climb);
  }
}

WantedChange wanted_change(const GameEvent& e) { return static_cast<WantedChange>(e.value); }

const char* wanted_change_name(WantedChange c) {
  switch (c) {
    case WantedChange::raised:
      return "raised";
    case WantedChange::lowered:
      return "lowered";
    case WantedChange::lost:
      return "lost";
    case WantedChange::seen:
      return "seen";
    case WantedChange::searching:
      return "searching";
    case WantedChange::arrested:
      return "arrested";
    case WantedChange::killed:
      return "killed";
  }
  return "?";
}

const char* wanted_cause_name(WantedCause c) {
  switch (c) {
    case WantedCause::stole_ambulance:
      return "stole-ambulance";
    case WantedCause::stole_police:
      return "stole-police";
    case WantedCause::downed_helicopter:
      return "downed-helicopter";
    case WantedCause::hit_soldier:
      return "hit-soldier";
    case WantedCause::stole_snowmobile:
      return "stole-snowmobile";
    case WantedCause::stole_vehicle:
      return "stole-vehicle";
    case WantedCause::stole_army_heli:
      return "stole-army-heli";
    case WantedCause::stole_tank:
      return "stole-tank";
    case WantedCause::dev:
      return "dev";
  }
  return "?";
}

const char* wanted_end_name(WantedEnd e) {
  switch (e) {
    case WantedEnd::running:
      return "running";
    case WantedEnd::escaped:
      return "escaped";
    case WantedEnd::arrested:
      return "arrested";
    case WantedEnd::killed:
      return "killed";
  }
  return "?";
}

uint64_t wanted_hash(const WantedState& w, uint64_t h) {
  if (!w.active) return h;
  const auto mix = [&h](double v) {
    uint64_t u;
    std::memcpy(&u, &v, sizeof u);
    for (int b = 0; b < 64; b += 8) {
      h ^= (u >> b) & 0xffu;
      h *= 0x100000001b3ull;
    }
  };
  mix(w.stars);
  mix(w.heat);
  mix(w.seen ? 1 : 0);
  mix(w.unseen);
  mix(w.fix);
  mix(w.last.x);
  mix(w.last.y);
  mix(w.last.z);
  mix(w.look);
  mix(w.lethal ? 1 : 0);
  mix(w.since);
  mix(w.stopped);
  mix(static_cast<double>(w.ended));
  mix(w.peak);
  for (const double r : w.reached) mix(r);
  mix(static_cast<double>(w.cause));
  return h;
}

}  // namespace fall_line::sim
