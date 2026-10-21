// What the ski-area labs share: one attempt built through every stage under the lab's flags, and
// the area drawn from above (the face shaded by height, the packed snow, the woods, the hub and
// its wind tunnels, every run in its colour, every lift, every kicker and cliff), or with its
// courses picked out.
//
// ge:summary The ski-area labs' build-from-flags and the area painter, plan and courses
// ge:keywords lab resort ski-area draw plan courses lifts runs hub hillshade build-resort
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/mapgen/resort_build.hpp"

namespace ge::mountain::labs {

// The flag every ski-area lab takes beside `region`: how many attempts a seed is tried for.
inline const ge::lab::LabFlag kAttemptsFlag{"attempts", "4",
                                            "attempts tried a seed, the first whole one drawn"};

// One attempt tried: its number, the last stage it reached and why it stopped there ("" when its
// courses stand).
struct AttemptTried {
  int attempt;
  std::string reached;
  std::string why;
};

// The first of `--attempts` attempts at `seed` (in `--region`) whose courses stand, into `out`;
// every attempt tried, in order. A lab.fail names the seed when none stands.
std::vector<AttemptTried> build_area(ge::lab::Lab& lab, double seed, mapgen::ResortAttempt& out);

// The area from above, `px` pixels a side: +x to the right, +z down the picture. With `courses`,
// the runs are drawn faint and every course over them in the colour it measures.
ge::lab::Picture draw_area(const mapgen::ResortAttempt& a, int px, bool courses);

// The colour a piste is signed in on the area's pictures.
ge::lab::Rgba grade_colour(mapgen::PisteGrade grade);

// The flags an area was built under, for a shot's inputs.
std::vector<std::pair<std::string, std::string>> area_inputs(const ge::lab::Lab& lab,
                                                             const mapgen::ResortAttempt& a);

}  // namespace ge::mountain::labs
