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

#include "lab/lab.hpp"
#include "mountain/labs/resort_map.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/real_face.hpp"
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
  ge::lab::LabTable table{"the ski area each face builds (seed " + f0(seed) + ")",
                          {"face", "region", "attempt", "pistes", "lifts", "courses", "vertical m",
                           "lifted m", "trees"},
                          {}};
  int built = 0;
  int placed = 0;
  int attempts = 0;
  double seconds = 0;
  double slowest = 0;
  for (const mg::FaceCrop* c : faces) {
    mg::GenerateOptions opts;
    opts.face = c->id;
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
      table.rows.push_back({c->id, region, "none", "-", "-", "-", "-", "-", gist(why)});
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
    table.rows.push_back({c->id, region, std::to_string(level->attempt), std::to_string(pistes),
                          std::to_string(area.lifts.size()), std::to_string(area.courses.size()),
                          f0(level->mountain.vertical), f0(top - bottom),
                          std::to_string(level->trees.size())});
    lab.metric(std::string(c->id) + ".attempt", level->attempt, "count");
    lab.metric(std::string(c->id) + ".pistes", pistes, "count");
    if (!draw) continue;
    // The accepted attempt built again stage by stage (the build is a pure function of the seed
    // and the attempt), so the area painter reads it.
    std::optional<mg::RealFace> face = mg::real_face(c->id);
    if (!face) continue;
    mg::ResortAttempt a = mg::begin_resort(seed, level->attempt, mg::sub_seed(seed, level->attempt),
                                           mg::region_row(c->region), level->version,
                                           std::make_shared<const mg::RealFace>(std::move(*face)));
    std::string reached;
    if (mg::build_resort(a, reached)) continue;
    const std::vector<std::pair<std::string, std::string>> inputs{
        {"face", c->id}, {"seed", f0(seed)}, {"attempt", std::to_string(level->attempt)}};
    const std::string place = std::string(c->range) + ", " + c->area + ", " + c->part;
    lab.shot(
        {std::string(c->id) + "-plan", std::string("REAL FACE ") + c->id + " SKI AREA",
         "The ski area the generator built on the real face " + std::string(c->id) + " (" + place +
             ") laid 1:1, from above, +x right and +z down the fall line: the face "
             "shaded by its real heights, every run in its measured colour, the lanes grey, "
             "the lifts black, the hub yellow, the woods dark green. " +
             std::to_string(pistes) + " pistes, " + std::to_string(area.lifts.size()) +
             " lifts, a vertical of " + f0(level->mountain.vertical) + " m.",
         "the runs follow the real valleys and spurs; every lift tops out on the real ridge "
         "or a shoulder of it; the hub stands on the level valley floor",
         inputs},
        ge::mountain::labs::draw_area(a, px, false));
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
  lab.metric("seconds-mean", n > 0 ? seconds / n : 0, "s", ge::lab::Det::measured);
  lab.metric("seconds-most", slowest, "s", ge::lab::Det::measured);
  lab.note(std::to_string(built) + " of " + f0(n) + " faces build a clean ski area; " +
           std::to_string(placed) + " on the real peak and valley (attempts under " +
           std::to_string(mg::resort_rules::massif::real::placed) + "); " + f2(seconds) +
           " s in all.");
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
    },
    "--faces=fell-1 --size=256",
    run,
};
GE_LAB(kSpec);

}  // namespace
