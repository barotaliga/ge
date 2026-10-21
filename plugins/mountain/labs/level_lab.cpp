// LAB `level`: ONE MOUNTAIN reasoned about without skiing it: the map from above with every gate
// numbered and every kicker and cliff labelled, as a captioned picture, plus the gate table (the
// arc down the piste, the drop and the grade between gates, and a slalom gate's offset off the
// line) and the map's headline numbers. A claim about "the third gate on seed 38" is a claim about
// a row here. At version 8 the map is a ski area raced down one of its courses: the course is the
// red line, its slalom gates the bars set off it, and the area's other runs the packed blue round
// it.
//
// ge:summary Lab: one generated map from above (captioned) with its gate table and numbers
// ge:keywords lab level map seed gates kickers cliffs grade picture
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <cstdio>
#include <string>

#include "lab/lab.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/digest.hpp"

namespace {

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : lab.arg_nums("seeds")) {
      const auto built = ge::mountain::labs::build_map(lab, seed, version);
      if (!built) continue;
      const ge::mountain::mapgen::Level* level = &*built;
      const std::string id = ge::mountain::labs::map_id(seed, version);
      const auto& cps = level->checkpoints;
      const double drop = cps.empty() ? 0 : cps.front().y - cps.back().y;
      lab.metric(id + ".attempt", level->attempt, "count");
      lab.metric(id + ".piste-length", level->track.length, "m");
      lab.metric(id + ".gates", static_cast<double>(cps.size()), "count");
      lab.metric(id + ".drop", drop, "m");
      lab.metric(id + ".vertical", level->mountain.vertical, "m");
      lab.metric(id + ".trees", static_cast<double>(level->trees.size()), "count");
      lab.metric(id + ".kickers", static_cast<double>(level->kickers.size()), "count");
      lab.metric(id + ".cliffs", static_cast<double>(level->cliffs.size()), "count");
      if (level->resort) {
        lab.metric(id + ".courses", static_cast<double>(level->resort->courses.size()), "count");
        lab.metric(id + ".runs", static_cast<double>(level->resort->runs.size()), "count");
      }

      ge::lab::LabTable gates{
          id + ": gates (0 the start gate, the last the finish)",
          {"gate", "colour", "s m", "gap m", "drop m", "grade %", "width m", "offset m"},
          {}};
      for (size_t i = 0; i < cps.size(); i++) {
        const auto& c = cps[i];
        const double gap = i ? c.s - cps[i - 1].s : 0;
        const double down = i ? cps[i - 1].y - c.y : 0;
        gates.rows.push_back({std::to_string(i),
                              c.colour == ge::mountain::mapgen::GateColour::red ? "red" : "blue",
                              f1(c.s), f1(gap), f1(down), gap > 0 ? f1(100 * down / gap) : "-",
                              f1(c.width), c.offset ? f1(*c.offset) : "-"});
      }
      lab.table(std::move(gates));
      lab.table(
          {id + ": digest", {"level_digest"}, {{ge::mountain::mapgen::level_digest(*level)}}});

      auto inputs = ge::mountain::labs::map_inputs(lab, *level);
      const std::string raced = level->resort
                                    ? " Raced down course " + level->resort->course + " of " +
                                          std::to_string(level->resort->courses.size()) + "."
                                    : "";
      lab.shot(
          {id, "SEED " + std::to_string(static_cast<long long>(seed)) + " FROM ABOVE",
           "The generated mountain from above, +x right and +z down: the face shaded by height "
           "and lit from the upper left, the packed piste tinted blue, trees dark green, the "
           "piste's line red, gates as bars across it (START green, red and blue alternating, "
           "FINISH black), kickers orange (K on the piste, X off it), cliffs and drops "
           "purple. " +
               f1(level->track.length) + " m of piste, " + f1(drop) + " m of drop through " +
               std::to_string(cps.size()) + " gates." + raced,
           "gates in order down the fall line and numbered without gaps; the piste inside the "
           "packed blue; no trees on the piste; every kicker on or beside its line",
           std::move(inputs)},
          ge::mountain::labs::draw_map(*level, px));
    }
  }
}

const ge::lab::LabSpec kSpec{
    "level",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "What did the generator build for this seed: the piste, the gates, the kickers, the woods?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"size", "1024", "the picture's side, px"},
        ge::mountain::labs::kVersionFlag,
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --size=256 --version=1,8",
    run,
};
GE_LAB(kSpec);

}  // namespace
