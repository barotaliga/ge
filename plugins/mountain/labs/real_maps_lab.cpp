// LAB `real-maps`: A SKI AREA ON EVERY REAL FACE (R25). Each face named is handed to the
// generator (`GenerateOptions::face`) and the map it builds is read as the generator's own accept
// gate reads it: whether a clean map stands at all, on which attempt (the first
// `massif::real::placed` keep the real peak, shoulder and valley; the later ones the dealt
// ones), how long the build took, and what the ski area carries (pistes, lifts, courses, its
// vertical and how much of it the lifts ride). A face named alone is drawn from above, its
// courses picked out. A claim that "every face builds" is a claim about this lab's table.
//
// ge:summary Lab: a ski area built on every real face, the attempt it took and what it carries
// ge:keywords lab real-face real-maps generate-level ski-area attempts build-time r25 picture
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/math.hpp"
#include "lab/lab.hpp"
#include "mountain/labs/resort_map.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/real_face.hpp"
#include "mountain/mapgen/real_hints.hpp"
#include "mountain/mapgen/regions.hpp"
#include "mountain/mapgen/resort_build.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/versions.hpp"

namespace {

namespace mg = ge::mountain::mapgen;

std::string f0(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.0f", v);
  return b;
}
std::string f2(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}

// The faces asked for: "all", or ids comma-separated.
std::vector<const mg::FaceCrop*> faces_of(ge::lab::Lab& lab) {
  std::vector<const mg::FaceCrop*> out;
  const std::string& asked = lab.arg("faces");
  if (asked == "all") {
    for (const mg::FaceCrop& c : mg::kFaceCrops) out.push_back(&c);
    return out;
  }
  std::stringstream in(asked);
  std::string id;
  while (std::getline(in, id, ',')) {
    if (const mg::FaceCrop* c = mg::face_crop(id)) {
      out.push_back(c);
    } else {
      lab.fail("--faces names " + id + ", which no face is",
               "next: name faces by their ids in plugins/mountain/mapgen/real_face_crops.hpp.");
    }
  }
  return out;
}

// How far the area's pistes lie from the real ones, m: the mean over every fifth bend of every
// piste of the distance to the nearest real piste's line (a ski route's left out).
double off_real(const mg::ResortMap& area, const mg::RealHints& hints) {
  double sum = 0;
  int n = 0;
  for (const mg::Run& r : area.runs) {
    if (r.kind != mg::RunKind::piste) continue;
    for (size_t k = 0; k < r.points.size(); k += 5) {
      const double x = r.points[k].x;
      const double z = r.points[k].z;
      double best = 1e9;
      for (const mg::HintPiste& p : hints.pistes) {
        if (p.grade == mg::HintGrade::route) continue;
        for (size_t i = 1; i < p.points.size(); i++) {
          const mg::PlanPoint a = p.points[i - 1];
          const mg::PlanPoint b = p.points[i];
          const double ex = b.x - a.x;
          const double ez = b.z - a.z;
          const double len2 = ex * ex + ez * ez;
          const double t =
              len2 == 0 ? 0 : std::clamp(((x - a.x) * ex + (z - a.z) * ez) / len2, 0.0, 1.0);
          best = std::min(best, ge::core::hypot(x - (a.x + ex * t), z - (a.z + ez * t)));
        }
      }
      sum += best;
      n++;
    }
  }
  return n > 0 ? sum / n : 0;
}

// The first words of a refusal: what the attempts were refused for, short enough for a cell.
std::string gist(const std::string& why) {
  const size_t dash = why.find(" — ");
  std::string out = dash == std::string::npos ? why : why.substr(dash + 5);
  if (out.size() > 90) out = out.substr(0, 87) + "...";
  return out;
}

void run(ge::lab::Lab& lab) {
  const std::vector<const mg::FaceCrop*> faces = faces_of(lab);
  const double seed = lab.arg_num("seed");
  const int px = static_cast<int>(lab.arg_num("size"));
  const bool draw = faces.size() <= 2;
  const bool hinted = lab.arg_num("hints") != 0;
  ge::lab::LabTable table{"the ski area each face builds (seed " + f0(seed) + ")",
                          {"face", "region", "attempt", "pistes", "lifts", "courses", "vertical m",
                           "lifted m", "trees", "off real m"},
                          {}};
  int built = 0;
  int placed = 0;
  int attempts = 0;
  double seconds = 0;
  double slowest = 0;
  double off_sum = 0;
  int off_faces = 0;
  for (const mg::FaceCrop* c : faces) {
    mg::GenerateOptions opts;
    opts.face = c->id;
    opts.hints = hinted;
    const auto t0 = std::chrono::steady_clock::now();
    std::optional<mg::Level> level;
    std::string why;
    try {
      level = mg::generate_level(seed, opts);
    } catch (const mg::GenerateError& e) {
      why = e.what();
    }
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    seconds += s;
    slowest = std::max(slowest, s);
    lab.metric(std::string(c->id) + ".seconds", s, "s", ge::lab::Det::measured);
    const std::string region = mg::region_name(c->region);
    if (!level) {
      table.rows.push_back({c->id, region, "none", "-", "-", "-", "-", "-", gist(why), "-"});
      lab.fail(
          std::string(c->id) + " builds no clean ski area on seed " + f0(seed) + ": " + gist(why),
          "Build it alone (--faces=" + std::string(c->id) +
              ") and read the generator's refusals with GE_LOG=debug. Skill: mountain.");
      continue;
    }
    built++;
    attempts += level->attempt + 1;
    placed += level->attempt < mg::resort_rules::massif::real::placed ? 1 : 0;
    const mg::ResortMap& area = *level->resort;
    int pistes = 0;
    for (const mg::Run& r : area.runs) pistes += r.kind == mg::RunKind::piste ? 1 : 0;
    double top = -1e9;
    double bottom = 1e9;
    for (const mg::Lift& l : area.lifts) {
      top = std::max(top, l.top.y);
      bottom = std::min(bottom, l.bottom.y);
    }
    // How near the runs lie to the real pistes, hints leant on or not (--hints=0).
    const std::optional<mg::RealHints> hints = mg::real_hints(c->id);
    std::string off = "-";
    if (hints && !hints->pistes.empty()) {
      const double m = off_real(area, *hints);
      off = f0(m);
      off_sum += m;
      off_faces++;
      lab.metric(std::string(c->id) + ".off-real", m, "m");
    }
    table.rows.push_back({c->id, region, std::to_string(level->attempt), std::to_string(pistes),
                          std::to_string(area.lifts.size()), std::to_string(area.courses.size()),
                          f0(level->mountain.vertical), f0(top - bottom),
                          std::to_string(level->trees.size()), off});
    lab.metric(std::string(c->id) + ".attempt", level->attempt, "count");
    lab.metric(std::string(c->id) + ".pistes", pistes, "count");
    if (!draw) continue;
    // The accepted attempt built again stage by stage (the build is a pure function of the seed
    // and the attempt), so the area painter reads it.
    std::optional<mg::RealFace> face = mg::real_face(c->id);
    if (!face) continue;
    mg::ResortAttempt a = mg::begin_resort(
        seed, level->attempt, mg::sub_seed(seed, level->attempt), mg::region_row(c->region),
        level->version, std::make_shared<const mg::RealFace>(std::move(*face)),
        hints ? std::make_shared<const mg::RealHints>(*hints) : nullptr, hinted);
    std::string reached;
    if (mg::build_resort(a, reached)) continue;
    const std::vector<std::pair<std::string, std::string>> inputs{
        {"face", c->id}, {"seed", f0(seed)}, {"attempt", std::to_string(level->attempt)}};
    const std::string place = std::string(c->range) + ", " + c->area + ", " + c->part;
    // The real ski area's pistes and lifts drawn faintly over the plan, to judge the lean by.
    ge::lab::Picture plan = ge::mountain::labs::draw_area(a, px, false);
    if (hints) {
      const double k = px / a.plan.size;
      for (const mg::HintPiste& p : hints->pistes) {
        for (size_t i = 1; i < p.points.size(); i++) {
          ge::lab::draw_line(plan, p.points[i - 1].x * k, p.points[i - 1].z * k, p.points[i].x * k,
                             p.points[i].z * k, {255, 0, 255, 140});
        }
      }
      for (const mg::HintLift& l : hints->lifts) {
        ge::lab::draw_line(plan, l.bottom.x * k, l.bottom.z * k, l.top.x * k, l.top.z * k,
                           {0, 255, 255, 160});
      }
    }
    lab.shot(
        {std::string(c->id) + "-plan", std::string("REAL FACE ") + c->id + " SKI AREA",
         "The ski area the generator built on the real face " + std::string(c->id) + " (" + place +
             ") laid 1:1, from above, +x right and +z down the fall line: the face "
             "shaded by its real heights, every run in its measured colour, the lanes grey, "
             "the lifts black, the hub yellow, the woods dark green; the real ski area's pistes "
             "thin magenta and its lifts thin cyan over it. " +
             std::to_string(pistes) + " pistes, " + std::to_string(area.lifts.size()) +
             " lifts, a vertical of " + f0(level->mountain.vertical) + " m.",
         "the runs follow the real valleys and spurs and, with hints, lie along the magenta real "
         "pistes; every lift tops out on the real ridge or a shoulder of it, near a cyan real "
         "lift; the hub stands on the level valley floor",
         inputs},
        plan);
    lab.shot({std::string(c->id) + "-courses", std::string("REAL FACE ") + c->id + " COURSES",
              "The same area with every course (R28) drawn over its faint runs in the colour it "
              "measures. " +
                  std::to_string(area.courses.size()) + " courses.",
              "each course follows its runs down the real face to the hub without a kink", inputs},
             ge::mountain::labs::draw_area(a, px, true));
  }
  lab.table(std::move(table));
  const double n = static_cast<double>(faces.size());
  lab.metric("faces", n, "count");
  lab.metric("built", built, "count");
  lab.metric("placed", placed, "count");
  lab.metric("attempts-mean", built > 0 ? double(attempts) / built : 0, "count");
  lab.metric("off-real-mean", off_faces > 0 ? off_sum / off_faces : 0, "m");
  lab.metric("seconds-mean", n > 0 ? seconds / n : 0, "s", ge::lab::Det::measured);
  lab.metric("seconds-most", slowest, "s", ge::lab::Det::measured);
  lab.note(std::to_string(built) + " of " + f0(n) + " faces build a clean ski area; " +
           std::to_string(placed) + " on the real peak and valley (attempts under " +
           std::to_string(mg::resort_rules::massif::real::placed) + "); " + f2(seconds) +
           " s in all; the pistes lie " + f0(off_faces > 0 ? off_sum / off_faces : 0) +
           " m off the real ones on average" + (hinted ? "" : " (hints off)") + ".");
}

const ge::lab::LabSpec kSpec{
    "real-maps",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen/massif.hpp",
    "mountain",
    "Does every real face build a clean ski area, on which attempt, and what does it carry?",
    {
        {"faces", "all", "faces, comma-separated ids, or all"},
        {"seed", "1", "the seed every face is built on"},
        {"size", "1024", "the picture's side, px, when two faces or fewer are named"},
        {"hints", "1", "lean on the real ski area's lifts and pistes (0: the face alone)"},
    },
    "--faces=fell-1 --size=256",
    run,
};
GE_LAB(kSpec);

}  // namespace
