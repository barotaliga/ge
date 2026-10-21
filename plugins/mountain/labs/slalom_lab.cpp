// LAB `slalom`: A SLALOM SET OVER A MAP (R31), both runs: the course straightened out down the
// page (the piste's edges, every pole where it stands, the racing line round them, the woods the
// course was cut through), the map from above with the course's gates on it, the gate table
// (each gate's kind, colour, arc, offset and width, turning pole to turning pole), and the
// course's numbers with its analysis. A claim about "the second hairpin of run 2 on seed 38" is a
// claim about a row here.
//
// ge:summary Lab: a slalom set over a map, both runs, straightened and from above, with its gates
// ge:keywords lab slalom set-slalom pole-gate hairpin vertical-combination delay racing-line r31
// ge:keywords course-plan picture
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/slalom.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;
using ge::lab::Rgba;

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

// A gate's two poles in plan: the first its turning pole on an open gate.
std::pair<mg::PlanPoint, mg::PlanPoint> poles_of(const mg::Checkpoint& g) {
  const double rx = ge::core::libm::cos(g.heading);
  const double rz = -ge::core::libm::sin(g.heading);
  const double t = g.turn != 0 ? double(g.turn) : 1.0;
  const double h = g.width / 2;
  return {{g.x + rx * t * h, g.z + rz * t * h}, {g.x - rx * t * h, g.z - rz * t * h}};
}

// THE COURSE STRAIGHTENED: one run's course down a panel `w` × `h` px, the arc down the page and
// the offset off the piste's line across it, wider than it is long so every pole reads.
ge::lab::Picture draw_course(const mg::Level& level, int w, int h) {
  const mg::RaceCourse& race = *level.race;
  auto pic = ge::lab::create_picture(w, h, {246, 246, 242});
  const double s0 = race.from - 12;
  const double s1 = race.to + 24;
  const double ky = (h - 24) / (s1 - s0);
  double widest = 0;
  for (const mg::TrackPoint& p : level.track.points) {
    if (p.s >= s0 && p.s <= s1) widest = std::max(widest, p.width);
  }
  const double kx = std::min(14.0, (w - 16) / (widest + 4));
  const double cx = w / 2.0;
  const auto X = [&](double lateral) { return cx + lateral * kx; };
  const auto Y = [&](double s) { return 12 + (s - s0) * ky; };
  // The piste: groomed snow between its edges.
  for (const mg::TrackPoint& p : level.track.points) {
    if (p.s < s0 || p.s > s1) continue;
    ge::lab::fill_rect(pic, X(-p.width / 2), Y(p.s), p.width * kx, 2 * ky + 1, {214, 228, 246});
  }
  // The woods round it, where the course was cut through.
  for (const mg::TreeDef& t : level.trees) {
    mg::TrackHit hit;
    mg::nearest_within(level.track, t.x, t.z, widest / 2 + 30, hit);
    if (!std::isfinite(hit.distance) || hit.s < s0 || hit.s > s1) continue;
    ge::lab::fill_disc(pic, X(hit.lateral), Y(hit.s), std::max(1.0, 0.6 * kx), {40, 100, 50});
  }
  // The racing line.
  double px = X(0), py = Y(race.from);
  for (int k = 0; race.from + 0.5 * k <= race.to; k++) {
    const double s = race.from + 0.5 * k;
    const auto at = mg::slalom_line_at(level, s);
    if (!at) break;
    const double nx = X(at->offset), ny = Y(s);
    ge::lab::draw_line(pic, px, py, nx, ny, {20, 20, 20}, 1);
    px = nx;
    py = ny;
  }
  // The gates: the turning pole a big disc, the outside pole a small one, joined; numbered every
  // fifth; the start wand green, the finish line black.
  const auto& cps = level.checkpoints;
  for (size_t i = 0; i < cps.size(); i++) {
    const mg::Checkpoint& g = cps[i];
    const auto [a, b] = poles_of(g);
    const mg::TrackHit ha = mg::nearest_track_point(level.track, a.x, a.z);
    const mg::TrackHit hb = mg::nearest_track_point(level.track, b.x, b.z);
    const bool start = i == 0, finish = i + 1 == cps.size();
    const Rgba c = start ? Rgba{30, 160, 60}
                   : finish
                       ? Rgba{0, 0, 0}
                       : (g.colour == mg::GateColour::red ? Rgba{220, 30, 30} : Rgba{30, 70, 220});
    ge::lab::draw_line(pic, X(ha.lateral), Y(ha.s), X(hb.lateral), Y(hb.s), c, 1);
    if (start || finish) continue;
    ge::lab::fill_disc(pic, X(ha.lateral), Y(ha.s), 3, c);
    ge::lab::fill_disc(pic, X(hb.lateral), Y(hb.s), g.pole == mg::PoleGate::closed ? 3 : 2, c);
    if (i % 5 == 0) ge::lab::draw_text(pic, 2, Y(g.s) - 3, std::to_string(i), {90, 90, 90});
  }
  return pic;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : lab.arg_nums("seeds")) {
      const auto built = ge::mountain::labs::build_map(lab, seed, version, mg::PisteGrade::red);
      if (!built) continue;
      const std::string id = ge::mountain::labs::map_id(seed, version);
      std::vector<mg::Level> runs;
      for (const double r : lab.arg_nums("runs")) runs.push_back(mg::set_slalom(*built, int(r)));
      const int panel = std::max(160, px / 3);
      auto plan = ge::lab::create_picture(panel * static_cast<int>(runs.size()), px, {0, 0, 0});
      std::string told;
      for (size_t k = 0; k < runs.size(); k++) {
        const mg::Level& level = runs[k];
        const mg::RaceCourse& race = *level.race;
        const std::string rid = id + ".run" + std::to_string(race.run);
        const an::LevelAnalysis checked = an::analyze_level(level);
        size_t errors = 0;
        for (const an::Finding& f : checked.findings) {
          if (f.severity != an::Severity::error) continue;
          errors++;
          lab.fail(rid + ": " + f.rule + " " + f.message,
                   "Read the course in this lab's pictures and fix the setter in "
                   "plugins/mountain/mapgen/slalom.cpp (skill: mountain).");
        }
        lab.metric(rid + ".from", race.from, "m");
        lab.metric(rid + ".length", race.to - race.from, "m");
        lab.metric(rid + ".vertical", race.vertical, "m");
        lab.metric(rid + ".gates", double(level.checkpoints.size() - 2), "count");
        lab.metric(rid + ".hairpins", race.hairpins, "count");
        lab.metric(rid + ".verticals", race.verticals, "count");
        lab.metric(rid + ".delays", race.delays, "count");
        lab.metric(rid + ".trees-cut", double(built->trees.size() - level.trees.size()), "count");
        lab.metric(rid + ".kickers-levelled", double(built->kickers.size() - level.kickers.size()),
                   "count");
        lab.metric(rid + ".errors", double(errors), "count");
        ge::lab::LabTable gates{
            rid + ": gates (0 the start wand, the last the finish line)",
            {"gate", "kind", "colour", "s m", "offset m", "width m", "turn-to-turn m"},
            {}};
        const auto& cps = level.checkpoints;
        for (size_t i = 0; i < cps.size(); i++) {
          const mg::Checkpoint& c = cps[i];
          std::string kind = i == 0 ? "start" : i + 1 == cps.size() ? "finish" : "-";
          if (c.pole) kind = *c.pole == mg::PoleGate::open ? "open" : "closed";
          std::string turn = "-";
          if (i > 1 && c.pole == mg::PoleGate::open && cps[i - 1].pole == mg::PoleGate::open) {
            const auto a = poles_of(cps[i - 1]).first;
            const auto b = poles_of(c).first;
            turn = f1(ge::core::hypot(b.x - a.x, b.z - a.z));
          }
          gates.rows.push_back({std::to_string(i), kind,
                                c.colour == mg::GateColour::red ? "red" : "blue", f1(c.s),
                                c.offset ? f1(*c.offset) : "-", f1(c.width), turn});
        }
        lab.table(std::move(gates));
        lab.table({rid + ": digest", {"level_digest"}, {{mg::level_digest(level)}}});
        ge::lab::blit(plan, draw_course(level, panel, px), panel * static_cast<int>(k), 0);
        told += " Run " + std::to_string(race.run) + ": " + std::to_string(cps.size() - 2) +
                " gates, " + std::to_string(race.hairpins) + " hairpins, " +
                std::to_string(race.verticals) + " verticals, " + std::to_string(race.delays) +
                " delays.";
      }
      if (runs.empty()) continue;
      const mg::RaceCourse& first = *runs.front().race;
      auto inputs = ge::mountain::labs::map_inputs(lab, runs.front());
      lab.shot({id + "-course",
                "SEED " + std::to_string(static_cast<long long>(seed)) + " SLALOM, STRAIGHTENED",
                "Each run's course straightened down the page, start at the top: the groomed "
                "piste pale blue between its edges, stretched across so every pole reads; the "
                "woods dark green; each gate's turning pole the big disc and its outside pole the "
                "small one, red and blue in turn (a closed gate two big discs one above the "
                "other); the racing line black; the start wand green, the finish black. " +
                    f1(first.to - first.from) + " m of piste dropping " + f1(first.vertical) +
                    " m." + told,
                "the line passes outside every turning pole; hairpins and verticals in the "
                "middle, never in the first or last three gates; every pole on the groomed snow; "
                "no tree within the cleared band",
                inputs},
               plan);
      lab.shot(
          {id + "-map",
           "SEED " + std::to_string(static_cast<long long>(seed)) + " SLALOM FROM ABOVE",
           "The map from above with run " + std::to_string(first.run) +
               "'s course set over it: the face shaded by height, the packed snow blue, the "
               "woods dark green, the piste's line red, every pole gate a bar, the start "
               "green and the finish black.",
           "the course on the steepest stretch the piste offers; the finish arena cleared", inputs},
          ge::mountain::labs::draw_map(runs.front(), px));
    }
  }
}

const ge::lab::LabSpec kSpec{
    "slalom",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "Where does a slalom stand on this map, and is every gate set to the rule?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"runs", "1,2", "the runs to set, comma-separated"},
        {"size", "1024", "the pictures' height, px"},
        {"version", "8", "generator versions, comma-separated: 1 the single piste, 8 the ski area"},
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --size=384 --version=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
