// plugins/brush/app/light_pick.hpp's frame lights: the drawn set by the view's cone (lights behind
// the eye left out, none by distance), the map's lights alone cast, nearest reach first; the cube
// slots kept by their lights, new ones faded in from dark, a light waiting for its cube dark, a
// slot in use this frame never taken from its light; and every light's reach listed in clusters.
//
// ge:summary Tests for the BSP's frame lights: the drawn set, the casters, the cube slots and fades
// ge:keywords test brush bsp light-pick choose_lights deal_cubes light-fade light-pop light_reach
// ge:skill brush testing
#include <array>
#include <cmath>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "brush/app/light_pick.hpp"
#include "brush/map.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "render/view.hpp"

namespace {

namespace ba = ge::brush::app;
namespace gb = ge::brush;

ba::PointLight light_at(double x, double y, double z, double reach) {
  ba::PointLight l;
  l.pos[0] = x;
  l.pos[1] = y;
  l.pos[2] = z;
  l.reach = reach;
  l.physical = true;  // its reach as given, no fire's lift
  return l;
}

std::array<float, 4> key(float x) { return {x, 0, 0, 1}; }

ge::render::FrameView looking_down_z() {
  ge::render::FrameView v;
  v.heading = 0;  // +z
  v.fov_y = 1.0;
  return v;
}

}  // namespace

TEST(the_drawn_set_keeps_every_light_in_view_and_leaves_out_those_behind_the_eye) {
  const gb::Map map;  // no tree, no visibility: the cone alone decides
  std::vector<ba::PointLight> lights;
  for (int i = 0; i < 300; ++i) lights.push_back(light_at(0, 0, 5.0 + i, 2));  // ahead, far
  lights.push_back(light_at(0, 0, -50, 2));                                    // behind
  lights.push_back(light_at(0, 0, -1, 2));  // behind, its reach round the eye
  std::vector<ba::LightOffer> offers;
  for (const ba::PointLight& l : lights) offers.push_back({&l, false});
  ba::PickRules rules;
  ba::LightChoice choice;
  ba::choose_lights(map, offers, nullptr, looking_down_z(), rules, choice);
  // Every light ahead, however far, and the one whose reach takes in the eye; nearest reach first.
  CHECK(choice.drawn.size() == 301);
  CHECK(choice.drawn.front().light == &lights[301]);
  for (const ba::LightOffer& o : choice.drawn) CHECK(o.light != &lights[300]);
  // Physical lights (a pool's glow) cast no shadow.
  CHECK(choice.casters.empty());
}

TEST(only_the_maps_own_lights_cast_and_none_with_shadows_off) {
  const gb::Map map;
  ba::PointLight lamp = light_at(0, 0, 4, 2);
  lamp.physical = false;
  ba::PointLight flash = lamp;
  ba::PointLight glow = light_at(0, 0, 6, 2);
  const std::vector<ba::LightOffer> offers = {{&lamp, false}, {&flash, true}, {&glow, false}};
  ba::PickRules rules;
  ba::LightChoice choice;
  ba::choose_lights(map, offers, nullptr, looking_down_z(), rules, choice);
  CHECK(choice.drawn.size() == 3);
  CHECK(choice.casters.size() == 1);
  CHECK(choice.drawn[choice.casters[0]].light == &lamp);
  rules.shadowed = 0;
  ba::choose_lights(map, offers, nullptr, looking_down_z(), rules, choice);
  CHECK(choice.drawn.size() == 3);
  CHECK(choice.casters.empty());
}

TEST(a_cube_is_kept_by_its_light_and_a_new_one_fades_in) {
  ba::CubeSlots slots;
  slots.slots = 4;
  ba::CubeDeal deal;
  const double dt = 1.0 / 60, fade = 0.25;
  // The load's frame: every cube at once, whole.
  const std::vector<std::array<float, 4>> first = {key(1), key(2)};
  ba::deal_cubes(slots, first, size_t(slots.slots), dt, fade, deal);
  CHECK(deal.fresh.size() == 2);
  CHECK(deal.fade[0] == 1.0f && deal.fade[1] == 1.0f);
  // A new light in play: its slot drawn, its light faded in from dark over `fade` seconds.
  const std::vector<std::array<float, 4>> next = {key(3), key(1), key(2)};
  ba::deal_cubes(slots, next, 1, dt, fade, deal);
  CHECK(deal.fresh.size() == 1);
  CHECK(deal.slot[1] == 0 && deal.slot[2] == 1);  // kept
  CHECK(deal.fade[1] == 1.0f && deal.fade[2] == 1.0f);
  CHECK(deal.fade[0] > 0.0f && deal.fade[0] < 0.1f);
  float last = deal.fade[0];
  int frames = 1;
  while (deal.fade[0] < 1.0f && frames < 100) {
    ba::deal_cubes(slots, next, 1, dt, fade, deal);
    CHECK(deal.fade[0] > last);
    CHECK(deal.fade[0] - last < 0.1f);
    last = deal.fade[0];
    ++frames;
  }
  CHECK(std::abs(frames - int(fade / dt)) <= 1);
}

TEST(a_light_past_the_budget_or_the_slots_waits_dark_and_no_light_in_use_loses_its_cube) {
  ba::CubeSlots slots;
  slots.slots = 2;
  ba::CubeDeal deal;
  const std::vector<std::array<float, 4>> two = {key(1), key(2)};
  ba::deal_cubes(slots, two, 2, 1.0 / 60, 0.25, deal);
  // A third light while both slots are read this frame: it waits, dark.
  const std::vector<std::array<float, 4>> three = {key(3), key(1), key(2)};
  ba::deal_cubes(slots, three, 2, 1.0 / 60, 0.25, deal);
  CHECK(deal.slot[0] == -1 && deal.fade[0] == 0.0f);
  CHECK(deal.waiting == 1);
  CHECK(deal.slot[1] == 0 && deal.slot[2] == 1);
  // Once the second has left the frame, its slot (the one unused longest) is the third's.
  const std::vector<std::array<float, 4>> again = {key(3), key(1)};
  ba::deal_cubes(slots, again, 0, 1.0 / 60, 0.25, deal);
  CHECK(deal.slot[0] == -1 && deal.waiting == 1);  // no budget this frame
  ba::deal_cubes(slots, again, 1, 1.0 / 60, 0.25, deal);
  CHECK(deal.slot[0] == 1 && deal.slot[1] == 0);
}

TEST(every_light_of_the_fixture_has_its_reach_in_clusters) {
  std::vector<uint8_t> bytes;
  std::string err;
  CHECK(gb::write_bsp(gb::sample_fixture(), gb::Family::v29, &bytes, &err));
  ba::Pack pack;
  pack.bytes = bytes;
  pack.files.push_back({"maps/fixture.bsp", 0, uint32_t(bytes.size())});
  const std::vector<uint8_t> palette(768, 128);
  ba::Scene scene;
  CHECK(ba::load_scene(pack, "maps/fixture.bsp", palette, &scene, &err));
  CHECK(!scene.lights.empty());
  const ba::LightReach reach = ba::light_reach(scene.map, scene.lights);
  CHECK(reach.clusters.size() == scene.lights.size());
  for (const std::vector<int32_t>& list : reach.clusters) {
    if (scene.map.pvs.empty()) {
      CHECK(list.empty());
    } else {
      CHECK(!list.empty());
    }
  }
}

TEST(a_more_important_light_takes_a_slot_by_fading_the_least_important_out_first) {
  ba::CubeSlots slots;
  slots.slots = 2;
  ba::CubeDeal deal;
  const double dt = 1.0 / 60, fade = 0.25;
  const std::vector<std::array<float, 4>> two = {key(1), key(2)};
  ba::deal_cubes(slots, two, 2, dt, fade, deal);
  // A nearer light: the third in importance (key 2) fades out, never a jump, then gives way.
  const std::vector<std::array<float, 4>> three = {key(3), key(1), key(2)};
  float out_last = 1, in_last = 0;
  int frames = 0;
  bool given = false;
  while (frames < 200) {
    ba::deal_cubes(slots, three, 1, dt, fade, deal);
    ++frames;
    CHECK(deal.slot[1] == 0 && deal.fade[1] == 1.0f);  // the second keeps its cube whole
    CHECK(deal.fade[2] <= out_last && out_last - deal.fade[2] < 0.1f);
    CHECK(deal.fade[0] >= in_last && deal.fade[0] - in_last < 0.1f);
    out_last = deal.fade[2];
    in_last = deal.fade[0];
    given = given || deal.slot[0] == 1;
    if (deal.fade[0] >= 1.0f) break;
  }
  CHECK(given);
  CHECK(deal.slot[2] == -1 && deal.fade[2] == 0.0f);
  CHECK(frames < 40);
}
