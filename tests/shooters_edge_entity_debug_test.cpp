// A recipe's entity tools: the entities in view listed nearest first with their model, and one
// hidden by number or model left out of what the world draws, then drawn again.
//
// ge:summary Test: entlist's lines and order, enthide by number and model, entshow
// ge:keywords test recipe entlist enthide entshow entities-in-view
// ge:skill window-recipes testing
#include <string>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/entity_debug.hpp"

namespace cl = shooters_edge::client;

namespace {

cl::DemoState world() {
  cl::DemoState s;
  s.map = "aerowalk";
  s.models = {"", "maps/aerowalk.bsp", "progs/flame.mdl", "*3"};
  s.eye = {0, 0, 0};
  s.entities.resize(4);
  s.entities[1] = {.origin = {500, 0, 0}, .model = 2, .present = true};
  s.entities[2] = {.origin = {100, 0, 0}, .model = 3, .alpha = 0.4f, .present = true};
  s.entities[3] = {.origin = {50, 0, 0}, .model = 2, .present = false};  // not this frame
  return s;
}

// What demo_placed asks of each entity before it draws it.
bool drawn(const cl::DemoState& s, int entity) {
  return !cl::entity_hidden(cl::hidden_entities(), entity,
                            s.models[static_cast<size_t>(s.entities[size_t(entity)].model)]);
}

}  // namespace

TEST(entlist_lists_what_is_present_nearest_first) {
  const std::string list = cl::entity_list(world(), 0);
  CHECK_MSG(list.starts_with("2 entities in view\n2    *3"), list);
  CHECK(list.find("alpha=0.40") != std::string::npos);
  CHECK(list.find("progs/flame.mdl") > list.find("*3"));
  CHECK(cl::entity_list(world(), 200).starts_with("1 entities in view"));
}

TEST(enthide_leaves_an_entity_out_by_number_or_model_and_entshow_brings_it_back) {
  const cl::DemoState s = world();
  cl::HiddenEntities& h = cl::hidden_entities();
  h = {};
  CHECK(drawn(s, 1) && drawn(s, 2));
  h.numbers = {2};
  CHECK(drawn(s, 1) && !drawn(s, 2));
  h = {.numbers = {}, .models = {"progs/flame.mdl"}};
  CHECK(!drawn(s, 1) && drawn(s, 2));
  h = {};
  CHECK(drawn(s, 1) && drawn(s, 2));
}

TEST(a_figure_the_eye_stands_in_is_not_drawn) {
  // A match mod's see-through marker at the spawn the player stands on: the eye (22 up) is in it.
  CHECK(cl::encloses_eye({160, 128, 248}, {160, 128, 270}));
  CHECK(!cl::encloses_eye({160, 128, 248}, {160, 180, 270}));  // a step beside it: drawn
  CHECK(!cl::encloses_eye({160, 128, 200}, {160, 128, 270}));  // an item at the feet: drawn
}
