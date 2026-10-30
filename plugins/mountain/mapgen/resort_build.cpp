// The ski-area build's stages, run in order over one attempt.
//
// ge:summary The ski-area build's stages over one attempt's state
// ge:keywords resort-build attempt-resort stage mountain pads lay-chain press-pads
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/resort_build.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "core/output.hpp"
#include "mountain/mapgen/lanes.hpp"
#include "mountain/mapgen/massif.hpp"
#include "mountain/mapgen/network_build.hpp"
#include "mountain/mapgen/run_start.hpp"
#include "mountain/mapgen/summit_ramps.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace R = rules;
namespace RR = resort_rules;
using ge::core::hypot;
using ge::core::sample_field;

namespace {

// How far down its heading a start must be clear of every run but its top's (R12, R27), m.
constexpr double kStartClear = R::track::hold + 100;
// How far a run's start and its first stretch keep off a station pad's middle past its radius,
// m; and past its half-width off a station's wheel (R26), m.
constexpr double kPadKeep = 10;
constexpr double kStationKeep = 14;
// How far past its radius a lane's route keeps off a station pad's middle (R26), m.
constexpr double kLaneKeep = 12;
// How far off a drag lift's line a piste keeps its edge, m (R26).
constexpr double kDragRoom = 6;
constexpr double kStartRoom = 12;
// How far past a run's bench a ramp's line is kept as a start is placed, m; and a lane's route
// off a ramp's room.
constexpr double kRampRoom = 6;
constexpr double kLaneRoom = RR::road::width.max / 2 + kBench;

// The colour ceiling a run's grading holds its colour windows under: its row's, but for a black
// (none) and a lane (its own grade is lower).
std::optional<double> colour_cap(const WalkedRun& run) {
  const GradeRow& row = *run.spec.row;
  if (run.spec.kind == RunKind::road || !row.id || *row.id == PisteGrade::black) {
    return std::nullopt;
  }
  return row.steepest.max;
}

// The grade's name as a log line writes it ("null" for none).
std::string grade_word(const GradeRow& row) { return row.id ? grade_name(*row.id) : "null"; }

std::string kind_word(RunKind kind) { return run_kind_name(kind); }

// Walk a run from its spec, again up to its tries, and keep the first that grades (onto the
// ground as the walk found it); whether one stood.
bool lay_run(ResortAttempt& a, const RunSpec& fair, const SharedTop& shared) {
  const ge::core::Heightfield& ground = a.ground;
  const TerrainPlan& plan = a.plan;
  std::string why;
  const int tries = fair.kind == RunKind::road ? RR::road::tries : RR::network::tries;
  // A real piste's bends are steered through on all but the last tries (R25).
  RunSpec plain = fair;
  plain.via.clear();
  plain.follow = nullptr;
  const int via_for = tries - RR::massif::real::via::last;
  for (int t = 0; t < tries; t++) {
    std::variant<WalkedRun, std::string> walked =
        walk_run(a.rng, plan, ground, t < via_for ? fair : plain, a.walking, shared);
    if (auto* refused = std::get_if<std::string>(&walked)) {
      why += (why.empty() ? "" : " | ") + *refused;
      continue;
    }
    WalkedRun& run = std::get<WalkedRun>(walked);
    const bool road = run.spec.kind == RunKind::road;
    const GradeRow& row = road ? kRoadRow : *run.spec.row;
    const WalkedRun* onto = run.into ? &a.walked[size_t(run.into->run)] : nullptr;
    const GradeFace face = grade_face(a);
    std::optional<std::string> graded =
        grade_run(run, ground, row.track.max_grade, colour_cap(run), onto, nullptr, false, face);
    // A run the mountain made gentler than its colour starts as the colour it measures (R12).
    if (!graded && !road) {
      const PisteGrade measured =
          run_colour(run.track.points, onto ? &onto->track.points : nullptr);
      const PisteGrade built = run.spec.row->id.value_or(PisteGrade::black);
      if (static_cast<int>(measured) < static_cast<int>(built)) {
        run.start_slope = grade_row(measured).spawn.max_slope;
        graded = grade_run(run, ground, row.track.max_grade, colour_cap(run), onto, nullptr, false,
                           face);
      }
    }
    if (graded) {
      why += (why.empty() ? "" : " | ") + *graded;
      continue;
    }
    a.walked.push_back(std::move(run));
    const WalkedRun& kept = a.walked.back();
    a.walking.add(kept.track.points, rank_of(kept.spec), kept.into ? kept.into->run : -1,
                  kept.core_from, road);
    return true;
  }
  ge::core::output::debug("resort " + num::to_string(a.seed) + "#" + std::to_string(a.attempt) +
                          ": slot " + fair.id + " (" + fair.from + " " + kind_word(fair.kind) +
                          " " + grade_word(*fair.row) + ") — " + why);
  return false;
}

}  // namespace

ResortAttempt begin_resort(double seed, int attempt, uint32_t sub, const Region& region,
                           int version, std::shared_ptr<const RealFace> face,
                           std::shared_ptr<const RealHints> hints, bool lean) {
  ResortAttempt a;
  a.face = std::move(face);
  a.hints = std::move(hints);
  a.lean = lean;
  a.seed = seed;
  a.attempt = attempt;
  a.sub = sub;
  a.region = &region;
  a.version = version;
  a.rng = ge::core::make_rng(sub);
  return a;
}

StageResult build_resort(ResortAttempt& a, std::string& reached) {
  using Stage = StageResult (*)(ResortAttempt&);
  struct Step {
    Stage run;
    const char* name;
  };
  static constexpr Step kSteps[] = {
      {resort_mountain, "mountain"}, {resort_pads, "pads"},     {resort_pistes, "pistes"},
      {resort_links, "links"},       {resort_graded, "graded"}, {resort_access, "access"},
      {resort_features, "features"}, {resort_woods, "woods"},   {resort_courses, "courses"},
  };
  reached.clear();
  for (const Step& s : kSteps) {
    if (StageResult why = s.run(a)) return why;
    reached = s.name;
  }
  return std::nullopt;
}

RampFit ramp_fit(const ResortAttempt& a) {
  if (!a.face) return RampFit{};
  return RampFit{RR::massif::real::least::ramp_glide, RR::massif::real::least::ramp_gentle, true};
}

GradeFace grade_face(const ResortAttempt& a) {
  if (!a.face) return GradeFace{};
  return GradeFace{RR::massif::real::least::fall, RR::massif::real::least::cut, true};
}

StageResult resort_mountain(ResortAttempt& a) {
  a.plan = plan_massif(a.rng, *a.region, a.face, a.attempt, a.hints, a.lean);
  a.ground = bake_massif(a.plan);
  return std::nullopt;
}

StageResult resort_pads(ResortAttempt& a) {
  ResortPlan planned = plan_resort(a.rng, a.plan);
  a.lift_plans = std::move(planned.lifts);
  a.specs = std::move(planned.specs);
  a.village = planned.village;
  // ── 2b. THE STATION PADS (R26), before a run is walked off one ──
  a.shape = pad_shape();
  const std::vector<float> raw = a.ground.data;
  // The peak's chair's queue ahead of a rider out of the gondola, at the first aim whose way
  // the pressed snow carries him down (R26).
  std::vector<StationPad> pressed = press_pads(a.ground, a.lift_plans, a.shape);
  // A real face's way may ask more earthworks of a further aim, searched over R26's whole band.
  ChainMost most;
  if (a.face) {
    most = {RR::massif::real::least::chain_fill, RR::massif::real::least::chain_dig, true, true};
  }
  std::optional<LaidChain> chain =
      lay_chain(a.ground, raw, a.lift_plans, std::move(pressed), a.shape, most);
  if (!chain) return "R26: no way off the gondola's top falls to the peak's chair's queue";
  a.way = chain->way;
  a.pads = std::move(chain->pads);
  return std::nullopt;
}

StageResult resort_pistes(ResortAttempt& a) {
  const ge::core::Heightfield& ground = a.ground;
  const TerrainPlan& plan = a.plan;
  const RampFit fit = ramp_fit(a);
  // The tops: runs started under them where a ramp has room (R26, R27).
  const bool tight = a.shape.lean > 0;
  // Every station standing as the runs are walked: a lift's two ends, the valley floor's aside.
  std::vector<PlanPoint> stations;
  for (const LiftPlan& l : a.lift_plans) {
    stations.push_back(l.top);
    if (l.bottom.z < plan.base_z - 1) stations.push_back(l.bottom);
  }
  // Whether a piste `half` m wide either side of (x, z) would run over a drag lift's line.
  std::vector<const LiftPlan*> drags;
  for (const LiftPlan& l : a.lift_plans) {
    if (l.kind == LiftKind::drag) drags.push_back(&l);
  }
  const AvoidAt over_drag = [drags](double x, double z, double half) {
    for (const LiftPlan* l : drags) {
      const double dx = l->top.x - l->bottom.x;
      const double dz = l->top.z - l->bottom.z;
      const double len2 = dx * dx + dz * dz;
      const double t =
          num::max(0.0, num::min(1.0, ((x - l->bottom.x) * dx + (z - l->bottom.z) * dz) /
                                          (len2 == 0 ? 1 : len2)));
      if (hypot(x - (l->bottom.x + dx * t), z - (l->bottom.z + dz * t)) < half + kDragRoom) {
        return true;
      }
    }
    return false;
  };
  // Whether a lane would cross a station's pad (R26).
  const auto on_pad = [&](double x, double z) {
    for (const StationPad& p : a.pads) {
      if (hypot(x - p.x, z - p.z) < p.r + kLaneKeep) return true;
    }
    return false;
  };
  // ── 3. THE RUNS, WALKED ──
  const std::vector<RampRoom> way_rooms = chain_rooms(a.way);
  // Whether a ramp's line at (x, z) comes onto the way to the next lift's queue (R26).
  const auto on_queue = [&](double x, double z) {
    return a.way && off_queue(*a.way, x, z) < RR::lift::top::ramp::width / 2 + 2 * ground.cell;
  };
  NetHit start_hit;
  const auto height = [&](double x, double z) { return sample_field(ground, x, z); };

  const auto lay = [&](const RunSpec& fair, const SharedTop& shared) {
    return lay_run(a, fair, shared);
  };

  for (const RunSpec& spec : a.specs) {
    std::optional<RunSpec> fair;
    std::optional<RampRoom> placed;
    // Under its top by a glide's fall from the pad's rim (a drag's top, from the top itself).
    const LiftPlan* lift = nullptr;
    for (const LiftPlan& l : a.lift_plans) {
      if (l.id == spec.from) {
        lift = &l;
        break;
      }
    }
    const std::optional<StartTop> top =
        lift ? std::optional(start_top(ground, *lift, a.shape.r, fit.glide)) : std::nullopt;
    if (spec.kind == RunKind::road) {
      // The lane's route to the cheapest join on a piste off another top, from under its own top.
      const std::optional<double> z =
          top ? head_on_contour(ground, spec.x, spec.z, *top) : std::optional(spec.z);
      std::optional<LaneRoute> lane;
      if (z) {
        LaneAsk ask;
        ask.keep_off = [&](double x, double zz) {
          if (on_pad(x, zz)) return true;
          for (const RampRoom& r : way_rooms) {
            if (near_room(r, x, zz) < kLaneRoom) return true;
          }
          for (const RampRoom& r : a.rooms) {
            if (near_room(r, x, zz) < kLaneRoom) return true;
          }
          return false;
        };
        const double sgn = num::sign(spec.target.x - spec.x);
        lane = route_lane(
            ground, a.walking, spec.x, *z, sgn == 0 ? 1 : sgn,
            [&](int r) {
              const WalkedRun& w = a.walked[size_t(r)];
              return w.spec.kind == RunKind::piste && w.spec.from != spec.from;
            },
            ask);
      }
      if (lane && z) {
        const PlanPoint ahead = lane->route[std::min<size_t>(5, lane->route.size() - 1)];
        RunSpec f = spec;
        f.z = *z;
        f.target = {lane->x, lane->z};
        f.join = lane->run;
        f.route = lane->route;
        f.heading = ge::core::libm::atan2(ahead.x - spec.x, num::max(1.0, ahead.z - *z));
        fair = std::move(f);
      }
    } else {
      std::vector<double> siblings;
      for (const WalkedRun& w : a.walked) {
        if (w.spec.from == spec.from) siblings.push_back(w.spec.x);
      }
      // The leaning pad its ramp comes off, and what that ramp keeps off.
      const StationPad* ramp_pad = nullptr;
      if (tight) {
        for (const StationPad& p : a.pads) {
          if (p.lift == spec.from) {
            ramp_pad = &p;
            break;
          }
        }
      }
      std::optional<RampRoom> room;
      const BlockedAt on_run = [&](double x, double z) {
        return a.walking
                   .nearest(
                       x, z, kWidest + kBench + kStartRoom, [](int, double) { return false; },
                       start_hit)
                   .distance < start_hit.width / 2 + kBench + kRampRoom;
      };
      const BlockedAt blocked =
          room_blocked(ramp_pad, a.pads, stations, a.rooms, kPadKeep, kStationKeep, on_run);
      const BlockedAt ramp_blocked = [&](double x, double z) {
        return blocked(x, z) || on_queue(x, z);
      };
      const double wide = spec.row->id ? RR::piste::width[size_t(*spec.row->id)].max : kWidest;
      const StartClear clear = [&](double x, double z, double h) {
        const double sx = ge::core::libm::sin(h);
        const double cz = ge::core::libm::cos(h);
        double u = 0;
        while (u <= kStartClear) {
          const double px = x + sx * u;
          const double pz = z + cz * u;
          // Off every station's pad and the ground eased into it (R26), and its corridor off
          // every station standing.
          if (u <= a.shape.r + kPadKeep) {
            for (const StationPad& p : a.pads) {
              if (hypot(px - p.x, pz - p.z) < p.r + kPadKeep) return false;
            }
            for (const PlanPoint& p : stations) {
              if (hypot(px - p.x, pz - p.z) < kStationKeep + wide / 2) return false;
            }
          }
          // The runs off its own top it leaves beside, off their width and bench; every other
          // by the clearance.
          const double reach = kWidest + 2 * kBench + RR::network::gap + kStartRoom;
          a.walking.nearest(
              px, pz, reach,
              [&](int r, double) { return a.walked[size_t(r)].spec.from == spec.from; }, start_hit);
          if (start_hit.distance < clearance(wide, start_hit.width) + kStartRoom) return false;
          a.walking.nearest(
              px, pz, reach,
              [&](int r, double) { return a.walked[size_t(r)].spec.from != spec.from; }, start_hit);
          if (start_hit.distance < start_hit.width / 2 + kBench + kStartRoom) return false;
          u += 10;
        }
        // A ramp off its top's leaning pad has room to come down to it (R26).
        if (!ramp_pad) return true;
        room = ramp_room(*ramp_pad, a.pads, a.lift_plans, x, z, wide / 2, *spec.row, height,
                         ramp_blocked, fit);
        return room.has_value();
      };
      fair = place_start(ground, spec, spec.lean, siblings, clear, top, a.face != nullptr);
      // Never over a drag lift's line (R26): the runs off a drag's own top.
      if (fair && std::any_of(drags.begin(), drags.end(),
                              [&](const LiftPlan* l) { return l->id == spec.from; })) {
        fair->avoid = over_drag;
      }
      placed = room;
    }
    if (!fair) {
      ge::core::output::debug("resort " + num::to_string(a.seed) + "#" + std::to_string(a.attempt) +
                              ": slot " + spec.id + " (" + spec.from + " " + kind_word(spec.kind) +
                              " " + grade_word(*spec.row) + ") — no start");
      continue;
    }
    const SharedTop shared = [&a, from = spec.from](int r) {
      return a.walked[size_t(r)].spec.from == from;
    };
    // Off the room kept for the ramp down to every start before it (R26).
    std::vector<RampRoom> before = way_rooms;
    before.insert(before.end(), a.rooms.begin(), a.rooms.end());
    const AvoidAt off_rooms = [before](double x, double z, double half) {
      for (const RampRoom& r : before) {
        if (near_room(r, x, z) < half) return true;
      }
      return false;
    };
    const AvoidAt drag = fair->avoid;
    const AvoidAt avoid = [drag, off_rooms](double x, double z, double half) {
      return (drag && drag(x, z, half)) || off_rooms(x, z, half);
    };
    const auto with_avoid = [](RunSpec s, AvoidAt how) {
      s.avoid = std::move(how);
      return s;
    };
    bool laid = lay(before.empty() ? *fair : with_avoid(*fair, avoid), shared) ||
                (drag && lay(with_avoid(*fair, before.empty() ? nullptr : off_rooms), shared));
    // A piste the mountain is too steep to lay at its colour is walked again a colour harder.
    if (!laid && spec.kind == RunKind::piste && spec.row->id &&
        *spec.row->id != PisteGrade::black) {
      RunSpec again = *fair;
      again.row = &grade_row(static_cast<PisteGrade>(static_cast<int>(*spec.row->id) + 1));
      laid = lay(before.empty() ? again : with_avoid(again, avoid), shared);
    }
    if (laid && placed) a.rooms.push_back(*placed);
  }
  int pistes = 0;
  for (const WalkedRun& w : a.walked) pistes += w.spec.kind == RunKind::piste ? 1 : 0;
  const int fewest = a.face ? RR::massif::real::least::runs : RR::network::runs_min;
  if (pistes < fewest) {
    return "only " + std::to_string(pistes) + " piste(s) could be walked down the mountain";
  }
  // On a real face, a nursery's drag no run could be walked off (a valley floor with no slope
  // beside the village) is not laid.
  if (a.face && std::none_of(a.walked.begin(), a.walked.end(),
                             [](const WalkedRun& w) { return w.spec.from == "D1"; })) {
    std::erase_if(a.lift_plans, [](const LiftPlan& l) { return l.id == "D1"; });
  }
  return std::nullopt;
}

StageResult resort_links(ResortAttempt& a) {
  const ge::core::Heightfield& ground = a.ground;
  int next = 0;
  for (const RunSpec& sp : a.specs) next = std::max(next, std::stoi(sp.id));
  const auto next_id = [&next]() { return std::to_string(++next); };
  const double base_z = a.plan.base_z;
  NetHit hit;
  LinkBuilder b;
  b.ground.height = [&](double x, double z) { return sample_field(ground, x, z); };
  b.ground.piste = [&](double x, double z) {
    a.walking.nearest(
        x, z, kWidest,
        [&](int r, double) { return a.walked[size_t(r)].spec.kind == RunKind::road; }, hit);
    return PisteNear{hit.distance, hit.width, hit.heading};
  };
  // A station on the valley floor stands in the hub (R29).
  b.ground.floor = [base_z](PlanPoint p) { return p.z >= base_z - 1; };
  // R29 on a real face: the way back from a piste's foot may run a colour harder.
  b.ground.slack = a.face ? RR::massif::real::least::harder : 0;
  b.walked = &a.walked;
  b.lifts = &a.lift_plans;
  b.route = [&](double x, double z, double side, const std::function<bool(int)>& may,
                const LaneAsk& ask) {
    LaneAsk with = ask;
    with.keep_off = [&](double px, double pz) {
      for (const StationPad& p : a.pads) {
        if (hypot(px - p.x, pz - p.z) < p.r + kLaneKeep) return true;
      }
      return a.way.has_value() && off_way(*a.way, px, pz) < kLaneRoom;
    };
    return route_lane(ground, a.walking, x, z, side, may, with);
  };
  b.lay = [&](const RunSpec& spec, const SharedTop& shared) { return lay_run(a, spec, shared); };
  b.fall = [&](double x, double z) {
    const double w = R::track::grade_window * 2;
    return hypot(sample_field(ground, x + w, z) - sample_field(ground, x - w, z),
                 sample_field(ground, x, z + w) - sample_field(ground, x, z - w)) /
           (2 * w);
  };
  // R29 first (every piste back to its top, every lift reached), then the lanes between sectors.
  a.access = lay_access(b, next_id);
  lay_links(b, next_id);
  return std::nullopt;
}

}  // namespace ge::mountain::mapgen
