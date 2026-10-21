// The slalom behind slalom.hpp: the stretch, the setter's lay of the gates (laid again at a
// corrected spacing until the last stands where it should), the bends fitted to a ski's carve, and
// the course stood up as a map.
//
// ge:summary R31's stretch search, gate lay, bend fit and the course stood up as a map
// ge:keywords slalom set-slalom slalom-stretch pole-gate hairpin vertical-combination delay
// ge:keywords racing-line fit-bends lay-gates r31
// ge:skill mountain
// ge:test tests/mountain_slalom_test.cpp
#include "mountain/mapgen/slalom.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/course_prep.hpp"
#include "mountain/mapgen/discipline_rules.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/spawn.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
namespace libm = ge::core::libm;
namespace S = discipline_rules::slalom;
using ge::core::clamp;
using ge::core::kPi;
using ge::core::Rng;

namespace {

// THE LINE A RACER TAKES, m: his body this far outside an open gate's turning pole as he passes
// it (over it, near enough, his feet carried further out by the turn), and this far either side
// of a closed gate's poles from its top to its foot.
constexpr double kLineOutside = 0.6;
constexpr double kLineAcross = 0.5;

// How far down the piste a combination stands from the open gates either side of it, m.
constexpr double kLead = 8;

// What a run's gate stream is seeded with beside the map's seed.
constexpr uint32_t kSalt = 0x51a10;

// One gate as the setter lays it, before it is a checkpoint: an OPEN gate's turning pole
// `across` m to `side` of the line and its `width`; a CLOSED gate's top `s`, its poles `height`
// apart down the hill, `at` m off the line.
struct Laid {
  PoleGate kind;
  double s;
  int side;
  double across = 0;
  double width = 0;
  bool delay = false;
  double at = 0;
  double height = 0;
};

// A combination the setter spreads down the middle.
enum class Combo : uint8_t { hairpin, vertical3, vertical4, delay };

int closed_gates(Combo c) { return c == Combo::hairpin ? 2 : c == Combo::vertical3 ? 3 : 4; }

// How much of the stretch a combination takes down the piste, m (its lead-in and lead-out
// included), and how many direction changes.
struct ComboSize {
  double length;
  int gates;
};
ComboSize combo_size(Combo combo) {
  if (combo == Combo::delay) return {(S::delayed.min + S::delayed.max) / 2, 1};
  const double closed = (S::closed.min + S::closed.max) / 2;
  const double gap = (S::gap.min + S::gap.max) / 2;
  const int k = closed_gates(combo);
  return {2 * kLead + k * closed + (k - 1) * gap, k};
}

// The combinations one run carries, in the order they are skied: shuffled, then no two of a kind
// together where another can go between.
std::vector<Combo> deal_combos(Rng& rng) {
  std::vector<Combo> out;
  const int hairpins = ge::core::int_in(rng, S::hairpins[0], S::hairpins[1]);
  const int verticals = ge::core::int_in(rng, S::verticals[0], S::verticals[1]);
  const int delays = ge::core::int_in(rng, S::delays[0], S::delays[1]);
  out.reserve(static_cast<size_t>(hairpins) + static_cast<size_t>(verticals) +
              static_cast<size_t>(delays));
  for (int i = 0; i < hairpins; i++) out.push_back(Combo::hairpin);
  for (int i = 0; i < verticals; i++) {
    out.push_back(ge::core::chance(rng, 0.5) ? Combo::vertical3 : Combo::vertical4);
  }
  for (int i = 0; i < delays; i++) out.push_back(Combo::delay);
  for (size_t i = out.size() - 1; i > 0; i--) {
    const auto j = static_cast<size_t>(ge::core::int_in(rng, 0, static_cast<int32_t>(i)));
    std::swap(out[i], out[j]);
  }
  for (size_t i = 1; i < out.size(); i++) {
    if (out[i] != out[i - 1]) continue;
    for (size_t j = i + 1; j < out.size(); j++) {
      if (out[j] == out[i]) continue;
      std::swap(out[i], out[j]);
      break;
    }
  }
  return out;
}

struct Lay {
  std::vector<Laid> gates;
  int open;
  double spacing;
};

// One lay of the gates from `first` at `guess` m between open gates (0: the setter's own first
// guess off the count the vertical asks for), with `extra` open gates over that count.
Lay lay_at(const Level& level, const SlalomStretch& stretch, Rng rng, double first, double guess,
           int extra) {
  const std::vector<Combo> combos = deal_combos(rng);
  const double len = stretch.to - S::last - first;
  // The direction changes the vertical asks for, and the open gates that leaves once the
  // combinations have theirs.
  const double share = ge::core::range(rng, S::changes::min, S::changes::max);
  const int asked = static_cast<int>(num::round(stretch.vertical * share));
  double combo_length = 0;
  int combo_gates = 0;
  for (const Combo c : combos) {
    const ComboSize size = combo_size(c);
    combo_length += size.length;
    combo_gates += size.gates;
  }
  const double room = num::max(40.0, len - combo_length);
  // The spacing wins where the count and the hill disagree. (The spacing's top is what the turning
  // poles' rule lets an open gate keep with its poles a full reach apart across the hill.)
  int open = std::max(2 * S::clean + static_cast<int>(combos.size()) + 1, asked - combo_gates);
  const double widest =
      std::sqrt(S::turn.max * S::turn.max - (2 * S::across.max) * (2 * S::across.max));
  const double most_spacing = num::min(S::spacing.max, widest);
  open = static_cast<int>(clamp(open, std::ceil(room / most_spacing) + 1,
                                std::floor(room / S::spacing.min) + 1)) +
         extra;
  const double spacing = guess > 0 ? guess : room / std::max(1, open - 1);
  // Where each combination goes: after which open gate, spread evenly down the middle, never
  // within `clean` of either end.
  std::vector<int> slots;
  for (size_t i = 0; i < combos.size(); i++) {
    slots.push_back(static_cast<int>(num::round(
        S::clean + ((double(i) + 0.5) * (open - 2 * S::clean)) / double(combos.size()))));
  }
  std::vector<Laid> out;
  int side = ge::core::chance(rng, 0.5) ? 1 : -1;
  double s = first;
  const double phase = ge::core::range(rng, 0, 2 * kPi);
  // The rhythm: the turning poles' reach across breathes slowly down the course, a section tight
  // and a section open.
  const auto reach = [&](int k) {
    const double wave = 0.35 * libm::sin(k * 0.45 + phase);
    const double jitter = ge::core::range(rng, -0.15, 0.15);
    return S::across.min + (S::across.max - S::across.min) * clamp(0.5 + wave + jitter, 0, 1);
  };
  double last_s = s - spacing;
  double last_x = -side * 2.0;
  for (int k = 0; k < open; k++) {
    const bool final = k == open - 1;
    const TrackPoint p = track_point_at(level.track, s);
    const double half = p.width / 2 - S::inside;
    const double width = clamp(ge::core::range(rng, S::width.min, S::width.max), S::width.min,
                               num::max(S::width.min, half - S::across.min));
    const double across =
        final ? S::across.min
              : clamp(reach(k), S::across.min, num::max(S::across.min, half - width));
    // Turning pole to turning pole within the rule: closer down the piste where the poles stand
    // far apart across it.
    const double dx = side * across - last_x;
    const double most = std::sqrt(num::max(1.0, S::turn.max * S::turn.max - dx * dx));
    if (s - last_s > most) s = last_s + most;
    out.push_back({PoleGate::open, s, side, across, width});
    last_s = s;
    last_x = side * across;
    side = -side;
    if (final) break;
    const auto slot = std::find(slots.begin(), slots.end(), k);
    if (slot != slots.end()) {
      const Combo combo = combos[static_cast<size_t>(slot - slots.begin())];
      if (combo == Combo::delay) {
        // THE DELAY: the next turning pole far on and well across, a turn held long, the rhythm
        // broken.
        const double on = ge::core::range(rng, S::delayed.min, S::delayed.max);
        const TrackPoint q = track_point_at(level.track, s + on);
        const double w = S::width.min;
        const double wide =
            clamp(ge::core::range(rng, S::delay_across.min, S::delay_across.max), S::across.min,
                  num::max(S::across.min, q.width / 2 - S::inside - w));
        s += on;
        Laid d{PoleGate::open, s, side, wide, w};
        d.delay = true;
        out.push_back(d);
        last_s = s;
        last_x = side * wide;
        side = -side;
      } else {
        // HAIRPIN or VERTICAL: closed gates in a column down the line.
        double top = s + kLead;
        double foot = top;
        for (int j = 0; j < closed_gates(combo); j++) {
          const double height = ge::core::range(rng, S::closed.min, S::closed.max);
          Laid c{PoleGate::closed, top, side};
          c.height = height;
          out.push_back(c);
          foot = top + height;
          top = foot + ge::core::range(rng, S::gap.min, S::gap.max);
          side = -side;
        }
        last_s = foot;
        last_x = 0;
        s = foot + kLead;
        continue;
      }
    }
    // The next open gate: the rhythm's spacing, breathing a little.
    s += spacing * (1 + 0.08 * libm::sin(k * 1.7 + phase));
  }
  return {std::move(out), open, spacing};
}

// R31 — the gates of run `run` down `stretch`, top to bottom: laid at a guessed spacing, then
// laid again at a spacing corrected by how far the last gate missed its place, until it stands
// there; every lay off the same stream, so the course is a pure function of the map and the run.
std::vector<Laid> lay_gates(const Level& level, const SlalomStretch& stretch, int run) {
  const uint32_t seed = num::to_uint32(level.seed) ^ kSalt ^
                        static_cast<uint32_t>(num::imul(static_cast<uint32_t>(run), 0x9e3779b1u));
  const double first = stretch.from + 9;
  const double goal = stretch.to - S::last;
  double spacing = 0;
  int extra = 0;
  std::vector<Laid> out;
  for (int pass = 0; pass < 16; pass++) {
    Lay laid = lay_at(level, stretch, ge::core::make_rng(seed), first, spacing, extra);
    out = std::move(laid.gates);
    const double end = out.back().s;
    if (std::abs(end - goal) < 0.25) break;
    const double next = laid.spacing + (goal - end) / std::max(1, laid.open - 1);
    // A spacing the rule cannot give is a gate more, or one fewer.
    if (next > S::spacing.max) {
      extra += 1;
      spacing = 0;
    } else if (next < S::spacing.min) {
      extra -= 1;
      spacing = 0;
    } else {
      spacing = next;
    }
  }
  std::erase_if(out, [&](const Laid& g) { return g.s >= stretch.to - 4; });
  return out;
}

// THE LINE KEPT TO A SKI'S CARVE (R31): where two gates' racing points would ask the line between
// them to bend tighter than `slalom::bend`, both turning poles are drawn in toward the course's
// line by the same share, no nearer it than a fifth of a metre. A closed gate's line is its own.
void fit_bends(std::vector<Laid>& laid) {
  constexpr double kFloor = 0.2;
  const auto point_of = [](const Laid& g) {
    return g.kind == PoleGate::open ? g.across + kLineOutside : kLineAcross;
  };
  for (int pass = 0; pass < 3; pass++) {
    for (size_t i = 0; i + 1 < laid.size(); i++) {
      Laid& a = laid[i];
      Laid& b = laid[i + 1];
      const double span = b.s - (a.kind == PoleGate::closed ? a.s + a.height : a.s);
      const double room = (2 * span * span) / (kPi * kPi * S::bend);
      const double swing = point_of(a) + point_of(b);
      if (swing <= room) continue;
      const double k = room / swing;
      if (a.kind == PoleGate::open) a.across = num::max(kFloor, a.across * k);
      if (b.kind == PoleGate::open) b.across = num::max(kFloor, b.across * k);
    }
  }
}

// A point of the piste shifted `by` m to the skier's right.
PlanPoint shifted(const TrackPoint& p, double by) {
  return {p.x + libm::cos(p.heading) * by, p.z - libm::sin(p.heading) * by};
}

GateColour colour_after(const std::vector<Checkpoint>& gates) {
  return gates.size() % 2 == 0 ? GateColour::red : GateColour::blue;
}

// Run `run`'s course set over `original`, a map with no course on it.
Level course_over(const std::shared_ptr<const Level>& original, int run) {
  const SlalomStretch stretch = slalom_stretch(*original);
  const Stretch span{stretch.from, stretch.to};
  Level base = prepare_course(*original, span, S::prep);
  std::vector<Laid> laid = lay_gates(base, stretch, run);
  fit_bends(laid);
  RaceCourse course;
  course.discipline = Discipline::slalom;
  course.run = run;
  course.base = original;
  course.from = stretch.from;
  course.to = stretch.to;
  course.vertical = stretch.vertical;
  std::vector<Checkpoint> gates;
  const TrackPoint start = track_point_at(base.track, stretch.from);
  gates.push_back({start.x, start.z, start.y, start.heading, 2, stretch.from, GateColour::red});
  course.line.push_back({stretch.from, 0});
  int column = 0;
  for (size_t i = 0; i < laid.size(); i++) {
    const Laid& g = laid[i];
    const GateColour colour = colour_after(gates);
    if (g.kind == PoleGate::open) {
      const TrackPoint p = track_point_at(base.track, g.s);
      const double off = g.side * (g.across + g.width / 2);
      const PlanPoint c = shifted(p, off);
      Checkpoint cp{c.x, c.z, base.ground_at(c.x, c.z), p.heading, g.width, g.s, colour};
      cp.offset = off;
      cp.span = p.width;
      cp.pole = PoleGate::open;
      cp.turn = g.side > 0 ? -1 : 1;
      gates.push_back(cp);
      // Just outside the turning pole: the racer's body over it, his feet on the gate's side.
      course.line.push_back({g.s, g.side * (g.across + kLineOutside)});
      if (g.delay) course.delays += 1;
      column = 0;
    } else {
      const double mid = g.s + g.height / 2;
      const TrackPoint p = track_point_at(base.track, mid);
      const PlanPoint c = shifted(p, g.at);
      Checkpoint cp{c.x, c.z,   base.ground_at(c.x, c.z), p.heading + (g.side * kPi) / 2, g.height,
                    mid, colour};
      cp.offset = g.at;
      cp.span = p.width;
      cp.pole = PoleGate::closed;
      gates.push_back(cp);
      // Across the middle of it: from the side he comes from at its top to the side he goes to
      // at its foot.
      course.line.push_back({g.s, g.at - g.side * kLineAcross});
      course.line.push_back({g.s + g.height, g.at + g.side * kLineAcross});
      column += 1;
      if (i + 1 >= laid.size() || laid[i + 1].kind != PoleGate::closed) {
        if (column == 2) {
          course.hairpins += 1;
        } else {
          course.verticals += 1;
        }
        column = 0;
      }
    }
  }
  const TrackPoint finish = track_point_at(base.track, stretch.to);
  gates.push_back({finish.x, finish.z, finish.y, finish.heading,
                   num::max(S::finish_width, finish.width + 6), stretch.to, colour_after(gates)});
  course.line.push_back({stretch.to, 0});
  // THE START HUT: the racer stood just above the wand, facing down.
  const TrackPoint hut = track_point_at(base.track, stretch.from - S::prep.stand);
  base.spawn = {hut.x, hut.z, start.heading};
  base.grid = {base.spawn};
  base.checkpoints = std::move(gates);
  base.trees = cleared_trees(base, span, S::prep);
  base.race = std::move(course);
  return base;
}

}  // namespace

SlalomStretch slalom_stretch(const Level& level) {
  const std::vector<TrackPoint>& pts = level.track.points;
  const auto n = static_cast<int>(pts.size());
  const double L = level.track.length;
  const double top = start_gate_arc() + 4;
  struct Block {
    double lo, hi;
  };
  std::vector<Block> blocked;
  for (const Cliff& c : level.cliffs) {
    if (!c.on_track || !c.s) continue;
    blocked.push_back({*c.s - c.shelf - S::clearance, *c.s + c.face + c.landing + S::clearance});
  }
  const double step = L / std::max(1, n - 1);
  const auto at = [&](double s) {
    return static_cast<size_t>(clamp(num::round(s / step), 0, n - 1));
  };
  std::optional<SlalomStretch> found;
  double best = -std::numeric_limits<double>::infinity();
  for (const double drop : S::drops) {
    // The top level's drops compete; a shorter one only where none fits.
    if (found && drop < S::pick::least) break;
    for (int b = n - 1; b > 0; b -= 5) {
      const double sB = pts[size_t(b)].s;
      const double yB = pts[size_t(b)].y;
      // Up the piste to where it has dropped enough.
      int a = b;
      while (a > 0 && pts[size_t(a)].y - yB < drop) a--;
      const double sA = pts[size_t(a)].s;
      if (pts[size_t(a)].y - yB < drop || sA < top) break;
      if (sB - sA > S::max_length) continue;
      // The run-out: gentler past the line, where there is piste to read.
      const double end = num::min(L, sB + S::outrun_length);
      if (end - sB >= S::outrun_length / 2) {
        const double fall = (yB - pts[at(end)].y) / (end - sB);
        if (fall > S::outrun) continue;
      }
      bool narrow = false;
      for (size_t i = at(sA - 10); i <= at(sB + 10); i++) {
        if (pts[i].width < S::min_width) {
          narrow = true;
          break;
        }
      }
      if (narrow) continue;
      bool cliff = false;
      for (const Block& k : blocked) {
        cliff = cliff || (k.hi > sA - 20 && k.lo < sB + S::outrun_length);
      }
      if (cliff) continue;
      const double vertical = pts[size_t(a)].y - yB;
      // The steepest, to the top level's gradient (a slalom hill is never an easy one), then the
      // most vertical; lower down the mountain where two are alike.
      const double score = num::min(vertical / (sB - sA), S::pick::steep) +
                           S::pick::vertical * vertical + S::pick::low * (sB / L);
      if (score > best) {
        best = score;
        found = SlalomStretch{sA, sB, vertical};
      }
    }
  }
  if (found) return *found;
  // A piste with no stretch to set one on: its last few hundred metres.
  const double from = num::max(top, L - 600);
  return {from, L, track_point_at(level.track, from).y - track_point_at(level.track, L).y};
}

Level set_slalom(const Level& level, int run) {
  const std::shared_ptr<const Level> original =
      level.race ? level.race->base : std::make_shared<const Level>(level);
  Level course = course_over(original, run);
  course.sun = level.sun;
  course.weather = level.weather;
  return course;
}

std::optional<LineAt> slalom_line_at(const Level& level, double s) {
  if (!level.race || level.race->line.size() < 2) return std::nullopt;
  const std::vector<RaceCourse::LinePoint>& line = level.race->line;
  size_t lo = 0;
  size_t hi = line.size() - 1;
  if (s <= line[0].s) return LineAt{line[0].x, 0, 0, 0};
  if (s >= line[hi].s) return LineAt{line[hi].x, 0, 0, 0};
  while (hi - lo > 1) {
    const size_t mid = (lo + hi) >> 1;
    if (line[mid].s <= s) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  const RaceCourse::LinePoint& a = line[lo];
  const RaceCourse::LinePoint& b = line[hi];
  const double span = num::max(0.5, b.s - a.s);
  const double t = clamp((s - a.s) / span, 0, 1);
  const double w = kPi / span;
  const double c = libm::cos(kPi * t);
  const double bend = ((b.x - a.x) * w * w * c) / 2;
  return LineAt{a.x + ((b.x - a.x) * (1 - c)) / 2, std::abs(bend),
                ((b.x - a.x) * w * libm::sin(kPi * t)) / 2, bend};
}

}  // namespace ge::mountain::mapgen
