// LAB `speed-ski`: THE SPEED-SKIING TRACK SET OVER A MAP (R34): its profile from the side (the
// ground under the track's middle before it was graded, the graded profile, the final's start and
// the qualification's, the timing zone, the braking line, the stop and the finish enclosure, with
// the point mass's speed down it), the map from above with the track cut into it, and the track's
// numbers with its analysis. A claim about "the launch on seed 38" is a claim about a row here.
//
// ge:summary Lab: a speed-skiing track set over a map, its profile from the side and from above
// ge:keywords lab speed-skiing speed-track timing-zone launch run-out profile r34 picture
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/analysis/index.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/mapgen/digest.hpp"
#include "mountain/mapgen/speed_ski.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace an = ge::mountain::analysis;
using ge::lab::Rgba;

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

// THE TRACK FROM THE SIDE: arc across the page, height up it (stretched so the knees read), the
// map's own ground under the track's middle grey, the graded profile black, the marks across it.
ge::lab::Picture draw_profile(const mg::Level& final_run, const mg::Level& qualify, int w, int h) {
  const mg::RaceCourse& race = *final_run.race;
  const mg::Level& base = *race.base;
  const auto& pts = final_run.track.points;
  auto pic = ge::lab::create_picture(w, h, {246, 246, 242});
  double lo = pts.back().y;
  double hi = pts.front().y;
  for (const mg::TrackPoint& p : pts) {
    lo = std::min(lo, base.ground_at(p.x, p.z));
    hi = std::max(hi, base.ground_at(p.x, p.z));
  }
  const double length = final_run.track.length;
  const auto X = [&](double s) { return 12 + (s / length) * (w - 24); };
  const auto Y = [&](double y) { return 12 + (hi - y) / std::max(1.0, hi - lo) * (h - 40); };
  // The timing zone and the finish enclosure, shaded behind.
  const mg::RaceCourse::Zone& zone = *race.zone;
  ge::lab::fill_rect(pic, X(zone.from), 0, X(zone.to) - X(zone.from), h, {250, 214, 214});
  for (size_t i = 1; i < pts.size(); i++) {
    const mg::TrackPoint& a = pts[i - 1];
    const mg::TrackPoint& b = pts[i];
    ge::lab::draw_line(pic, X(a.s), Y(base.ground_at(a.x, a.z)), X(b.s),
                       Y(base.ground_at(b.x, b.z)), {150, 150, 150}, 1);
    ge::lab::draw_line(pic, X(a.s), Y(a.y), X(b.s), Y(b.y), {20, 20, 20}, 2);
  }
  // The marks: the final's start green, the qualification's light green, the zone's lines red,
  // the braking line blue, the stop black.
  const auto mark = [&](double s, Rgba c, const std::string& label, int row) {
    ge::lab::draw_line(pic, X(s), 4, X(s), h - 22, c, 1);
    ge::lab::draw_text(pic, X(s) + 2, h - 20 + 9 * row, label, c);
  };
  mark(race.top, {30, 160, 60}, "FINAL", 0);
  mark(qualify.race->from, {110, 200, 120}, "QUAL", 1);
  mark(zone.from, {210, 30, 30}, "ZONE", 0);
  mark(zone.to, {210, 30, 30}, "", 0);
  if (const auto lines = mg::speed_ski_lines(final_run)) {
    mark(lines->braking.s, {30, 70, 220}, "BRAKE", 1);
  }
  mark(race.stop, {0, 0, 0}, "STOP", 0);
  return pic;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("size"));
  for (const int version : ge::mountain::labs::map_versions(lab)) {
    for (const double seed : lab.arg_nums("seeds")) {
      const auto built = ge::mountain::labs::build_map(lab, seed, version);
      if (!built) continue;
      const mg::Level& map = *built;
      const std::string id = ge::mountain::labs::map_id(seed, version);
      const mg::Level final_run = mg::set_speed_ski(map, 2);
      const mg::Level qualify = mg::set_speed_ski(final_run, 1);
      const mg::RaceCourse race = final_run.race.value_or(mg::RaceCourse{});
      const mg::RaceCourse::Zone zone = race.zone.value_or(mg::RaceCourse::Zone{0, 0, 0});
      const double qualify_from = qualify.race ? qualify.race->from : 0;
      size_t errors = 0;
      for (const mg::Level* level : {&final_run, &qualify}) {
        for (const an::Finding& f : an::analyze_level(*level).findings) {
          if (f.severity != an::Severity::error) continue;
          errors++;
          lab.fail(id + ".run" + std::to_string(level->race ? level->race->run : 0) + ": " +
                       f.rule + " " + f.message,
                   "Read the track in this lab's pictures and fix the search or the grading in "
                   "plugins/mountain/mapgen/speed_ski.cpp (skill: mountain).");
        }
      }
      const double final_speed = mg::speed_ski_aim(final_run);
      const double qualify_speed = mg::speed_ski_aim(qualify);
      const double heading = final_run.track.points.front().heading * 180 / 3.141592653589793;
      lab.metric(id + ".length", final_run.track.length, "m");
      lab.metric(id + ".heading", heading, "deg");
      lab.metric(id + ".launch", zone.from - race.top, "m");
      lab.metric(id + ".zone", zone.length, "m");
      lab.metric(id + ".run-out", race.stop - zone.to, "m");
      lab.metric(id + ".vertical", race.vertical, "m");
      lab.metric(id + ".final", final_speed, "km/h");
      lab.metric(id + ".qualification", qualify_speed, "km/h");
      lab.metric(id + ".trees-cut", double(map.trees.size() - final_run.trees.size()), "count");
      lab.metric(id + ".errors", double(errors), "count");
      const auto lines = mg::speed_ski_lines(final_run);
      const std::string braking = lines ? f1(lines->braking.s) : "-";
      lab.table({id + ": the track (arcs down it, m)",
                 {"final", "qualification", "zone top", "zone bottom", "braking", "stop", "end"},
                 {{f1(race.top), f1(qualify_from), f1(zone.from), f1(zone.to), braking,
                   f1(race.stop), f1(final_run.track.length)}}});
      lab.table({id + ": digest",
                 {"final", "qualification"},
                 {{mg::level_digest(final_run), mg::level_digest(qualify)}}});
      auto inputs = ge::mountain::labs::map_inputs(lab, map);
      std::string told = "The track runs ";
      told.append(f1(final_run.track.length)).append(" m at ").append(f1(heading));
      told.append("° off the fall line; the final comes through the zone at ");
      told.append(f1(final_speed)).append(" km/h, the qualification at ");
      told.append(f1(qualify_speed)).append(" km/h.");
      lab.shot({id + "-profile",
                "SEED " + std::to_string(static_cast<long long>(seed)) + " SPEED TRACK, SIDE",
                "The speed track from the side, start at the left, height stretched: the map's "
                "ground under the track's middle grey, the graded profile black, the timing zone "
                "shaded red; the final's start green, the qualification's light green, the zone's "
                "lines red, the braking line blue and the stop black. " +
                    told,
                "the profile never rising and as smooth as the ground allows; the zone on a "
                "gentler stretch past the steep; the stop well past the braking line and short of "
                "the track's end",
                inputs},
               draw_profile(final_run, qualify, px, std::max(200, px / 3)));
      lab.shot({id + "-map",
                "SEED " + std::to_string(static_cast<long long>(seed)) + " SPEED TRACK FROM ABOVE",
                "The map from above with the speed track cut into it: the face shaded by height, "
                "the packed snow blue, the woods dark green, the track's line red, the start "
                "green and the zone's lines black.",
                "the track straight down the face, its margin and finish enclosure cleared, clear "
                "of the lifts and the village",
                inputs},
               ge::mountain::labs::draw_map(final_run, px));
    }
  }
}

const ge::lab::LabSpec kSpec{
    "speed-ski",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/mapgen",
    "mountain",
    "Where is a speed-skiing track cut on this map, and is it graded, timed and run out to rule?",
    {
        {"seeds", "38", "seeds, comma-separated"},
        {"size", "1024", "the pictures' width, px"},
        {"version", "8", "generator versions, comma-separated: 1 the single piste, 8 the ski area"},
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --size=384 --version=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
