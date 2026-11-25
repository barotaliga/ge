// client/model_filters: cl_gibfilter hides the models flagged to bleed and nothing else;
// cl_deadbodyfilter hides a player's figure on the last frame of a death, not while it falls;
// r_drawflame 0 hides the flames alone; app/figure_frames' resting_frames finds the frames by name.
//
// ge:summary Tests the models a player hides: gibs and still bodies, the resting frames
// ge:keywords test cl_gibfilter cl_deadbodyfilter hidden_by resting_frames r_drawflame
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/app/figure_frames.hpp"
#include "shooters_edge/client/model_filters.hpp"

namespace cl = shooters_edge::client;

TEST(a_death_rests_on_its_last_frame) {
  const std::vector<std::string> names = {"stand1", "deatha1", "deatha2", "deathb1", "pain1"};
  const std::vector<bool> r = shooters_edge::models::resting_frames(names);
  CHECK(r.size() == 5 && !r[0] && !r[1] && r[2] && r[3] && !r[4]);
}

// Quake II's figure: three deaths standing, numbered by variant, and one crouched.
TEST(q2_figure_rests_after_each_death_crouched_too) {
  const std::vector<std::string> names = {"stand01",  "death101", "death102", "death201",
                                          "death202", "crdeath1", "crdeath2", "crstnd01"};
  const std::vector<bool> r = shooters_edge::models::resting_frames(names);
  CHECK(r.size() == 8 && !r[0] && !r[1] && r[2] && !r[3] && r[4] && !r[5] && r[6] && !r[7]);
}

TEST(each_switch_hides_its_own_models) {
  const std::vector<bool> resting = shooters_edge::models::resting_frames(
      std::vector<std::string>{"stand1", "deatha1", "deatha2", "run1"});
  const uint32_t gib = 4, rocket = 1;  // the file's flags: bleeds as it flies; a rocket's trail
  cl::ModelFilters f;
  CHECK(!f.any() && !cl::hidden_by(f, gib, {}, 0) && !cl::hidden_by(f, 0, resting, 2));
  f.gibs = true;
  CHECK(cl::hidden_by(f, gib, {}, 0) && !cl::hidden_by(f, rocket, {}, 0));
  CHECK(!cl::hidden_by(f, 0, resting, 2));  // a still body stays with the gibs hidden
  f = {};
  f.still_bodies = true;
  CHECK(cl::hidden_by(f, 0, resting, 2));   // the death's last frame
  CHECK(!cl::hidden_by(f, 0, resting, 1));  // still falling
  CHECK(!cl::hidden_by(f, 0, resting, 3) && !cl::hidden_by(f, 0, resting, 9));
  CHECK(!cl::hidden_by(f, gib, {}, 0));      // a gib stays with the bodies hidden
  CHECK(!cl::hidden_by(f, 0, {}, 0, true));  // a flame burns with the bodies hidden
  f = {};
  f.flames = true;  // r_drawflame 0
  CHECK(f.any() && cl::hidden_by(f, 0, {}, 0, true) && !cl::hidden_by(f, gib, {}, 0));
}
