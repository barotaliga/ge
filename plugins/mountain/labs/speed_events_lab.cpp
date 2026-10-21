// LAB `speed-events`: THE DOWNHILL, THE SUPER-G AND THE GIANT SLALOM SET OVER A MAP (R32, R33,
// R36): each course straightened out down the page (the piste's edges, the nets, the jumps, the
// gates where they stand, the racing line through them, the speed trap), the map from above with
// the downhill on it, each course's gate table (colour, arc, offset, side, gate to gate) and its
// numbers with its analysis. On a ski area each is set over the course its rule races (the
// downhill the one with the most vertical in its band, the turning races the one with the most
// vertical); on the single piste, over the piste.
//
// ge:summary Lab: the downhill, super-G and giant slalom set over a map, straightened, with gates
// ge:keywords lab downhill super-g giant-slalom speed-gate racing-line speed-trap a-nets
// ge:keywords crests course-plan picture r32 r33 r36
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/query.hpp"
#include "mountain/mapgen/speed_course.hpp"
#include "mountain/mapgen/speed_events.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;
using ge::lab::Rgba;

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

// A gate's two poles in plan: the first its turning pole on a gate that turns the racer.
std::pair<mg::PlanPoint, mg::PlanPoint> poles_of(const mg::Checkpoint& g) {
  const double rx = ge::core::libm::cos(g.heading);
  const double rz = -ge::core::libm::sin(g.heading);
  const double t = g.turn != 0 ? double(g.turn) : 1.0;
  const double h = g.width / 2;
  return {{g.x + rx * t * h, g.z + rz * t * h}, {g.x - rx * t * h, g.z - rz * t * h}};
}

// THE COURSE STRAIGHTENED: a course down a panel `w` × `h` px, the arc down the page and the
// offset off the piste's line across it, stretched across so every gate reads.
ge::lab::Picture draw_course(const mg::Level& level, int w, int h) {
  const mg::RaceCourse& race = *level.race;
  auto pic = ge::lab::create_picture(w, h, {246, 246, 242});
  const double s0 = race.from - 12;
  const double s1 = race.to + 12;
  const double ky = (h - 24) / (s1 - s0);
  double widest = 0;
  for (const mg::TrackPoint& p : level.track.points) {
    if (p.s >= s0 && p.s <= s1) widest = std::max(widest, p.width);
  }
  const double gap = race.nets ? race.nets->gap : 0;
  const double kx = std::min(8.0, (w - 24) / (widest + 2 * gap + 4));
  const double cx = w / 2.0;
  const auto X = [&](double lateral) { return cx + lateral * kx; };
  const auto Y = [&](double s) { return 12 + (s - s0) * ky; };
  // The piste between its edges, the nets outside them.
  for (const mg::TrackPoint& p : level.track.points) {
    if (p.s < s0 || p.s > s1) continue;
    ge::lab::fill_rect(pic, X(-p.width / 2), Y(p.s), p.width * kx, 2 * ky + 1, {214, 228, 246});
    if (race.nets && p.s >= race.nets->from && p.s <= race.nets->to) {
      const double half = p.width / 2 + race.nets->gap;
      ge::lab::fill_rect(pic, X(-half) - 1, Y(p.s), 2, 2 * ky + 1, {130, 130, 130});
      ge::lab::fill_rect(pic, X(half) - 1, Y(p.s), 2, 2 * ky + 1, {130, 130, 130});
    }
  }
  // The jumps: an orange bar across the piste at each.
  for (const double j : race.jumps) {
    const mg::TrackPoint p = mg::track_point_at(level.track, j);
    ge::lab::fill_rect(pic, X(-p.width / 2), Y(j) - 1, p.width * kx, 3, {240, 140, 20});
  }
  // The speed trap: a purple bar between the nets.
  if (race.trap) {
    const double half = race.trap->width / 2;
    ge::lab::fill_rect(pic, X(-half), Y(race.trap->s) - 1, 2 * half * kx, 3, {140, 40, 170});
  }
  // The racing line.
  double px = X(0), py = Y(race.from);
  for (int k = 0; race.from + 2.0 * k <= race.to; k++) {
    const double s = race.from + 2.0 * k;
    const auto at = mg::speed_line_at(level, s);
    if (!at) break;
    const double nx = X(at->offset), ny = Y(s);
    ge::lab::draw_line(pic, px, py, nx, ny, {20, 20, 20}, 1);
    px = nx;
    py = ny;
  }
  // The gates: the turning pole a big disc and the outside pole a small one, joined (a downhill's
  // two inner poles alike); numbered every tenth; the start wand green, the finish line black.
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
    ge::lab::draw_line(pic, X(ha.lateral), Y(ha.s), X(hb.lateral), Y(hb.s), c, 2);
    if (start || finish) continue;
    ge::lab::fill_disc(pic, X(ha.lateral), Y(ha.s), g.turn != 0 ? 3 : 2, c);
    ge::lab::fill_disc(pic, X(hb.lateral), Y(hb.s), 2, c);
    if (i % 10 == 0) ge::lab::draw_text(pic, 2, Y(g.s) - 3, std::to_string(i), {90, 90, 90});
  }
  return pic;
}

// The course `discipline` (and `run`) races on `plain`: on a ski area the map rebuilt down the
// course its rule names (each rebuilt map kept in `built` for the next event), then the course set
// over it.
std::optional<mg::Level> course_for(ge::lab::Lab& lab, const mg::Level& plain,
                                    std::vector<std::pair<std::string, mg::Level>>& built,
                                    const std::string& discipline, int run) {
  const bool downhill = discipline == "downhill";
  const std::optional<std::string> id =
      downhill ? mg::downhill_course_of(plain) : mg::most_vertical_course_of(plain);
  const mg::Level* map = &plain;
  if (id) {
    auto it =
        std::find_if(built.begin(), built.end(), [&](const auto& b) { return b.first == *id; });
    if (it == built.end()) {
      auto rebuilt =
          ge::mountain::labs::build_map(lab, plain.seed, plain.version, std::nullopt, id);
      if (!rebuilt) return std::nullopt;
      built.emplace_back(*id, std::move(*rebuilt));
      it = std::prev(built.end());
    }
    map = &it->second;
  }
  if (downhill) return mg::set_downhill(*map);
  if (discipline == "super-g") return mg::set_super_g(*map);
  if (discipline == "giant-slalom") return mg::set_giant_slalom(*map, run);
  lab.fail("--disciplines names " + discipline + ", which is not a speed event",
           "Use downhill, super-g or giant-slalom.");
  return std::nullopt;
}

void report(ge::lab::Lab& lab, const std::string& rid, const mg::Level& level) {
  const mg::RaceCourse& race = *level.race;
  const an::LevelAnalysis checked = an::analyze_level(level);
  size_t errors = 0;
  for (const an::Finding& f : checked.findings) {
    if (f.severity != an::Severity::error) continue;
    errors++;
    lab.fail(rid + ": " + f.rule + " " + f.message,
             "Read the course in this lab's pictures and fix the setter in "
             "plugins/mountain/mapgen/speed_events.cpp (skill: mountain).");
  }
  const auto& cps = level.checkpoints;
  lab.metric(rid + ".from", race.from, "m");
  lab.metric(rid + ".length", race.to - race.from, "m");
  lab.metric(rid + ".vertical", race.vertical, "m");
  lab.metric(rid + ".gates", double(cps.size() - 2), "count");
  lab.metric(rid + ".jumps", double(race.jumps.size()), "count");
  if (race.trap) lab.metric(rid + ".trap", race.trap->s, "m");
  lab.metric(rid + ".errors", double(errors), "count");
  ge::lab::LabTable gates{rid + ": gates (0 the start wand, the last the finish line)",
                          {"gate", "colour", "s m", "offset m", "width m", "turn", "gap m"},
                          {}};
  for (size_t i = 0; i < cps.size(); i++) {
    const mg::Checkpoint& c = cps[i];
    gates.rows.push_back({std::to_string(i), c.colour == mg::GateColour::red ? "red" : "blue",
                          f1(c.s), c.offset ? f1(*c.offset) : "-", f1(c.width),
                          c.turn > 0   ? "right"
                          : c.turn < 0 ? "left"
                                       : "-",
                          i > 0 ? f1(c.s - cps[i - 1].s) : "-"});
  }
  lab.table(std::move(gates));
  lab.table({rid + ": digest", {"level_digest"}, {{mg::level_digest(level)}}});
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  std::vector<std::string> disciplines;
  std::string word;
  for (const char c : lab.arg("disciplines") + ",") {
    if (c != ',') {
      word += c;
    } else if (!word.empty()) {
      disciplines.push_back(word);
      word.clear();
    }
  }
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : lab.arg_nums("seeds")) {
      const std::string id = ge::mountain::labs::map_id(seed, version);
      const auto plain = ge::mountain::labs::build_map(lab, seed, version);
      if (!plain) continue;
      std::vector<std::pair<std::string, mg::Level>> built;
      std::vector<mg::Level> courses;
      for (const std::string& d : disciplines) {
        const std::vector<double> runs =
            d == "giant-slalom" ? lab.arg_nums("runs") : std::vector<double>{1};
        for (const double r : runs) {
          auto course = course_for(lab, *plain, built, d, int(r));
          if (course) courses.push_back(std::move(*course));
        }
      }
      if (courses.empty()) continue;
      const int panel = std::max(140, px / 4);
      auto plan = ge::lab::create_picture(panel * static_cast<int>(courses.size()), px, {0, 0, 0});
      std::string told;
      for (size_t k = 0; k < courses.size(); k++) {
        const mg::Level& level = courses[k];
        const mg::RaceCourse& race = *level.race;
        std::string name = mg::discipline_name(race.discipline);
        if (race.discipline == mg::Discipline::giant_slalom) {
          name += ".run" + std::to_string(race.run);
        }
        std::string rid = id;
        rid.append(".").append(name);
        report(lab, rid, level);
        ge::lab::blit(plan, draw_course(level, panel, px), panel * static_cast<int>(k), 0);
        told.append(" ").append(name).append(": ").append(f1(race.to - race.from));
        told.append(" m dropping ").append(f1(race.vertical)).append(" m, ");
        told.append(std::to_string(level.checkpoints.size() - 2)).append(" gates, ");
        told.append(std::to_string(race.jumps.size())).append(" jumps.");
      }
      auto inputs = ge::mountain::labs::map_inputs(lab, *courses.front().race->base);
      lab.shot(
          {id + "-courses",
           "SEED " + std::to_string(static_cast<long long>(seed)) + " SPEED EVENTS, STRAIGHTENED",
           "Each course straightened down its own panel, start at the top, each panel "
           "scaled to its own length: the groomed piste pale blue between its edges, the "
           "nets grey outside them, the jumps orange bars, the speed trap a purple bar; each "
           "gate's two poles joined, red and blue in turn (a turning gate's turning pole the "
           "bigger disc); the racing line black; the start wand green, the finish black." +
               told,
           "the downhill's gates on its line, its line straighter than the piste; the "
           "turning races' line swinging gate to gate outside every turning pole; no gate on "
           "a jump's lip or landing; every pole on the groomed snow; the trap late on a "
           "straight",
           inputs},
          plan);
      lab.shot({id + "-map",
                "SEED " + std::to_string(static_cast<long long>(seed)) + " " +
                    mg::discipline_name(courses.front().race->discipline) + " FROM ABOVE",
                "The map from above with the first course set over it: the face shaded by "
                "height, the packed snow blue, the woods dark green, the piste's line red, every "
                "gate a bar, the start green and the finish black.",
                "the course down the piste its rule races; its woods cleared", inputs},
               ge::mountain::labs::draw_map(courses.front(), px));
    }
  }
}

const ge::lab::LabSpec kSpec{
    "speed-events",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "Where do the downhill, the super-G and the giant slalom stand, and is every gate to the rule?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"disciplines", "downhill,super-g,giant-slalom", "the events to set, comma-separated"},
        {"runs", "1,2", "the giant slalom's runs to set, comma-separated"},
        {"size", "1024", "the pictures' height, px"},
        {"version", "8", "generator versions, comma-separated: 1 the single piste, 8 the ski area"},
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --size=384 --version=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
