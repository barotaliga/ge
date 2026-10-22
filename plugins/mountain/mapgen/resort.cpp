// A massif's plan: its stations, lifts and run slots, leant onto a real face's hints where it
// has them (real_lean.hpp).
//
// ge:summary The stations and lifts on the massif, and the run slots in walk order
// ge:keywords resort plan-resort place-stations slots tilt tall-tilt tilted road-row
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#include "mountain/mapgen/resort.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "mountain/mapgen/lift_chain.hpp"
#include "mountain/mapgen/massif.hpp"
#include "mountain/mapgen/real_lean.hpp"
#include "mountain/mapgen/resort_rules.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace RR = resort_rules;
using PG = PisteGrade;

namespace {

GradeRow road_row() {
  GradeRow row = grade_row(PG::green);
  row.track.steepest = RR::road::grade;
  row.track.max_grade = RR::road::grade;
  row.track.width = RR::road::width;
  row.track.sweeps = {0, 0};
  row.kickers.on = {0, 0};
  row.drift = {0, 0};
  return row;
}

// Every station of the plan.
struct Stations {
  double side;
  PlanPoint village;
  PlanPoint mid;
  PlanPoint peak;
  PlanPoint shoulder;
  PlanPoint shoulder_foot;
  PlanPoint nursery;
  std::optional<PlanPoint> outer;
  std::optional<PlanPoint> outer_foot;
};

// One run slot: the top it leaves, the colour it is built to, where it heads and on which side.
struct Slot {
  const char* from;
  RunKind kind;
  PG aim;
  PlanPoint (*target)(const Stations&);
  double lean;
  double offset;
  double odds;
  double amplitude;
  bool firm;
};

constexpr RunKind P = RunKind::piste;
constexpr RunKind L = RunKind::road;

// R27 — THE SLOTS, in the order they are listed: the nursery, the trunks home from the
// mid-station, the shoulder's, the peak's, the outer top's, and the lanes last.
const std::array<Slot, 17> kSlots{{
    {"D1", P, PG::green, [](const Stations& s) { return PlanPoint{s.nursery.x, s.village.z}; },
     -0.3, 0, 1, 0.7, false},
    {"D1", P, PG::green,
     [](const Stations& s) { return PlanPoint{s.nursery.x - s.side * 320, s.village.z}; }, -1, -90,
     0.6, 0.7, false},
    {"G1", P, PG::red, [](const Stations& s) { return s.village; }, 0.4, 0, 1, 0.45, true},
    {"G1", P, PG::blue,
     [](const Stations& s) { return PlanPoint{s.village.x - s.side * 320, s.village.z}; }, -1, -90,
     1, 0.6, true},
    {"G1", P, PG::red,
     [](const Stations& s) { return PlanPoint{s.mid.x + s.side * 420, s.village.z}; }, 1, 90, 0.7,
     0.4, true},
    {"C2", P, PG::blue, [](const Stations& s) { return s.shoulder_foot; }, -0.3, 0, 1, 0.6, false},
    {"C2", P, PG::blue,
     [](const Stations& s) { return PlanPoint{s.shoulder.x + s.side * 480, s.village.z}; }, 1, 90,
     0.8, 0.6, false},
    {"C2", P, PG::red,
     [](const Stations& s) { return PlanPoint{s.shoulder.x - s.side * 260, s.village.z}; }, -1, -90,
     0.75, 0.4, false},
    {"C1", P, PG::black,
     [](const Stations& s) { return PlanPoint{s.peak.x + s.side * 160, s.village.z}; }, 0.5, 40, 1,
     0.3, false},
    {"C1", P, PG::red, [](const Stations& s) { return s.mid; }, -0.6, -90, 1, 0.45, false},
    {"C1", P, PG::blue,
     [](const Stations& s) { return PlanPoint{s.mid.x - s.side * 380, s.mid.z}; }, -1, -180, 0.7,
     0.5, false},
    {"C3", P, PG::black, [](const Stations& s) { return s.outer_foot.value_or(s.village); }, 0.4, 0,
     1, 0.3, false},
    {"C3", P, PG::red, [](const Stations& s) { return s.village; }, -0.8, -90, 0.7, 0.45, false},
    {"G1", L, PG::green, [](const Stations& s) { return s.shoulder_foot; }, -1, -60, 1, 0.05,
     false},
    {"G1", L, PG::green,
     [](const Stations& s) { return PlanPoint{s.peak.x + s.side * 400, s.mid.z}; }, 1, 60, 0.7,
     0.05, false},
    {"C2", L, PG::green, [](const Stations& s) { return s.mid; }, 1, 92, 0.85, 0.05, false},
    {"C1", L, PG::green, [](const Stations& s) { return s.shoulder; }, -1, -138, 0.5, 0.05, false},
}};

// R21, R27 — how a region tilts the colours its slots are built to. From v8 the fell's mountain
// is as tall as any (R25), so its runs are not tilted gentler (`TALL_TILT`); the low massif's
// `TILT` (a fell's reds blue, its blacks red) went with v1–v7, which no pinned map names.
PG tilted(const Slot& slot, RegionId region) {
  PG to = slot.aim;
  if (region == RegionId::continental) {
    if (slot.aim == PG::blue) to = PG::red;
    if (slot.aim == PG::red) to = PG::black;
  } else if (region == RegionId::maritime) {
    if (slot.aim == PG::black) to = PG::red;
  }
  const bool harder = static_cast<int>(to) > static_cast<int>(slot.aim);
  return slot.firm && harder ? slot.aim : to;
}

struct Placed {
  Stations stations;
  std::vector<LiftPlan> lifts;
};

// R26 — the stations and the lifts between them.
Placed place_stations(ge::core::Rng& rng, const TerrainPlan& plan) {
  if (!plan.massif) throw std::logic_error("place_stations: not a resort's mountain");
  const Massif& m = *plan.massif;
  namespace LR = RR::lift;
  const double size = plan.size;
  const double span = plan.base_z - plan.summit_z;
  const auto at = [&](double u) { return plan.summit_z + span * u; };
  const double side = m.side;
  PlanPoint village{m.village_x, plan.base_z + 60};
  PlanPoint mid{m.bench_x, at(m.bench_u) - 20};
  PlanPoint peak{m.peak_x - side * 40, plan.summit_z + LR::below};
  PlanPoint shoulder{m.shoulder_x, plan.summit_z + LR::below + 10};
  PlanPoint shoulder_foot{(m.shoulder_x + m.village_x) / 2 - side * 60, plan.base_z + 40};
  // The nursery's top as far up from the floor as on the rule book's square.
  const double nursery_x = m.village_x - side * in_band(rng, LR::nursery::across);
  const double nursery_u = in_band(rng, LR::nursery::at);
  PlanPoint nursery{nursery_x, m.wide == 1 ? at(nursery_u) : at(1 - (1 - nursery_u) / m.wide)};
  const double outer_x = m.peak_x + side * in_band(rng, LR::outer::across) * m.wide;
  const double outer_u = in_band(rng, LR::outer::at);
  const bool room = std::abs(outer_x - size / 2) < plan.flank_band.inner - 220;
  std::optional<PlanPoint> outer;
  std::optional<PlanPoint> outer_foot;
  if (room) {
    outer = PlanPoint{outer_x, at(outer_u)};
    outer_foot = PlanPoint{outer_x - side * 80, plan.base_z + 40};
  }
  PlanPoint drag_bottom{nursery.x + side * 60, village.z - 10};
  // A REAL FACE's lifts (real_lean.hpp): every station leant onto the nearest end of a real lift,
  // the village's first (the gondola leaves it), each top as near the real one as it may move;
  // after every draw, so the stream runs on as it would.
  const RealHints* hints = plan.face ? plan.face->hints.get() : nullptr;
  if (hints) {
    HintsUsed used(*hints);
    const auto ride = {HintLiftKind::gondola, HintLiftKind::chair};
    const auto lean = [&](PlanPoint& bottom, PlanPoint& top,
                          std::initializer_list<HintLiftKind> kinds) {
      if (const std::optional<LeantLift> l =
              lean_lift(*hints, used, bottom, top, kinds, true, false)) {
        bottom = l->bottom;
        top = l->top;
      }
    };
    lean(village, mid, ride);
    // Laid 1:1, a real top stands where the real one does, up or down the face as far as it may.
    peak = lean_station(*hints, used, peak, true);
    lean(shoulder_foot, shoulder, ride);
    if (outer && outer_foot) lean(*outer_foot, *outer, ride);
    lean(drag_bottom, nursery, {HintLiftKind::drag});
  }
  Placed out{{side, village, mid, peak, shoulder, shoulder_foot, nursery, outer, outer_foot}, {}};
  const LiftPlan gondola{"G1", LiftKind::gondola, {village.x + side * 30, village.z}, mid};
  out.lifts.push_back(gondola);
  // The peak's chair leaves beside the gondola's top, its queue AHEAD of a rider out of the
  // gondola and to one side, at the first of the aims (R26).
  out.lifts.push_back({"C1", LiftKind::chair,
                       chain_bottom(gondola.kind, gondola.bottom, gondola.top, LiftKind::chair,
                                    peak, RR::lift::chain::aims[0]),
                       peak});
  out.lifts.push_back({"C2", LiftKind::chair, shoulder_foot, shoulder});
  out.lifts.push_back({"D1", LiftKind::drag, drag_bottom, nursery});
  if (outer && outer_foot) out.lifts.push_back({"C3", LiftKind::chair, *outer_foot, *outer});
  return out;
}

}  // namespace

const GradeRow kRoadRow = road_row();

ResortPlan plan_resort(ge::core::Rng& rng, const TerrainPlan& plan) {
  Placed placed = place_stations(rng, plan);
  const Stations& st = placed.stations;
  const auto top_of = [&](const char* id) -> const PlanPoint* {
    for (const LiftPlan& l : placed.lifts) {
      if (l.id == id) return &l.top;
    }
    return nullptr;
  };
  const RealHints* hints = plan.face ? plan.face->hints.get() : nullptr;
  HintsUsed pistes(hints ? *hints : RealHints{});
  const HintWidth widths = hints ? piste_width(*hints) : nullptr;
  std::vector<RunSpec> specs;
  for (const Slot& slot : kSlots) {
    const PlanPoint* top = top_of(slot.from);
    // Every slot draws its odds whether or not its lift was built.
    const bool laid = ge::core::chance(rng, slot.odds);
    if (!top || !laid) continue;
    const double x = top->x + slot.offset * st.side;
    const PlanPoint target = slot.target(st);
    const double toward = libm::atan2(target.x - x, num::max(200.0, target.z - top->z));
    // A REAL FACE's pistes: a piste slot laid on the real piste leaving nearest its start takes
    // that piste's colour and follows its line, the run's width its real one; with none near, it
    // is steered through the bends of a real one of its own colour.
    const bool hinted = hints && slot.kind == RunKind::piste;
    const PlanPoint start{x, top->z};
    const HintPiste* real = hinted ? piste_for(*hints, pistes, start, target) : nullptr;
    const std::optional<PG> signed_as = real ? piste_grade_of(real->grade) : std::nullopt;
    // `plan.sea` is set on every massif (v8 on), so the tall tilt.
    const PG aim = signed_as                    ? *signed_as
                   : slot.kind == RunKind::road ? PG::green
                                                : tilted(slot, plan.region->id);
    const GradeRow* row = slot.kind == RunKind::road ? &kRoadRow : &grade_row(aim);
    RunSpec spec{
        std::to_string(specs.size() + 1),
        slot.from,
        slot.kind,
        row,
        x,
        top->z,
        num::max(-1.0, num::min(1.0, toward + slot.lean * st.side * 0.6)),
        target,
        slot.amplitude,
        slot.lean * st.side,
    };
    if (hinted) {
      const double margin = RR::massif::real::via::margin;
      if (real) {
        for (size_t i = 1; i < real->points.size(); i++) {
          const PlanPoint q = real->points[i];
          if (q.z > top->z + margin && q.z < target.z - margin) spec.via.push_back(q);
        }
      } else {
        spec.via = piste_via(*hints, pistes, start, target, aim);
      }
      HintSteer any = piste_ahead(*hints, aim);
      if (real) {
        spec.follow = [on = along_piste(*real), any = std::move(any)](double fx, double fz) {
          std::optional<PlanPoint> p = on(fx, fz);
          return p ? p : any(fx, fz);
        };
        spec.width_at = widths;
        spec.signed_as = signed_as;
      } else {
        spec.follow = std::move(any);
      }
      // A run following a real piste swings less of its own.
      spec.amplitude *= RR::massif::real::via::swing;
    }
    specs.push_back(std::move(spec));
  }
  // The pistes the gentlest first, each colour in slot order; the lanes last.
  const auto rank = [](const RunSpec& sp) {
    return sp.row->id ? static_cast<int>(*sp.row->id) : 0;
  };
  std::vector<RunSpec> ordered;
  for (const RunSpec& sp : specs) {
    if (sp.kind == RunKind::piste) ordered.push_back(sp);
  }
  std::stable_sort(ordered.begin(), ordered.end(),
                   [&](const RunSpec& a, const RunSpec& c) { return rank(a) < rank(c); });
  for (const RunSpec& sp : specs) {
    if (sp.kind == RunKind::road) ordered.push_back(sp);
  }
  return {std::move(placed.lifts), std::move(ordered), st.village};
}

}  // namespace ge::mountain::mapgen
