// games/fall_line/app/mists.hpp: r_fog_quality's froxels grow a step at a time; the frame deals
// the map's mists only when the scene brings no fog of its own, and r_fog 0 draws none.
//
// ge:summary Test: Fall Line's fog in the frame: the froxels by quality, whose fog is drawn
// ge:keywords test fall-line fog mists dress_mists fog_settings r_fog r_fog_quality
// ge:skill render testing
#include "check.hpp"
#include "fall_line/app/mists.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace r = ge::render;

// A clear, still winter morning over a valley that fogs (mountain_mist_look_test's mountain).
mg::Level morning() {
  mg::Level level{};
  level.sun.hour = 7;
  level.sun.day_of_year = 10;
  level.sun.latitude = 46;
  level.mountain.base = {500, 0, 500};
  level.mountain.summit = {500, 1000, -500};
  level.mountain.vertical = 1000;
  level.mountain.altitude = 900;
  level.ground = ge::core::create_heightfield(0, -1000, 500, 4, 4);
  level.weather = {mg::WeatherKind::clear, 0, 0, 1, 0, false};
  return level;
}

}  // namespace

TEST(the_quality_steps_grow_the_froxels) {
  const r::FogSettings high = fall_line::app::fog_settings(2);
  const r::FogSettings defaults;
  CHECK(high.width == defaults.width && high.height == defaults.height &&
        high.slices == defaults.slices);
  for (int q = 0; q < 3; ++q) {
    const r::FogSettings a = fall_line::app::fog_settings(q),
                         b = fall_line::app::fog_settings(q + 1);
    CHECK(a.width < b.width && a.height < b.height && a.slices < b.slices);
  }
  CHECK(fall_line::app::fog_settings(9).width == fall_line::app::fog_settings(3).width);
}

TEST(the_map_deals_its_mists_when_the_scene_brings_none) {
  const mg::Level level = morning();
  fall_line::app::FrameScene scene;
  r::FrameView view;
  view.eye[0] = view.eye[2] = 500;
  std::vector<r::FogVolume> out;
  fall_line::app::dress_mists(level, true, scene, view, out);
  CHECK(view.valley.density > 0);
}

TEST(a_scenes_own_fog_is_the_only_fog) {
  const mg::Level level = morning();
  const r::FogVolume own;
  fall_line::app::FrameScene scene;
  scene.fog = std::span<const r::FogVolume>(&own, 1);
  r::FrameView view;
  std::vector<r::FogVolume> out;
  fall_line::app::dress_mists(level, true, scene, view, out);
  CHECK(out.size() == 1 && view.valley.density == 0);
}

TEST(r_fog_off_draws_none) {
  const mg::Level level = morning();
  fall_line::app::FrameScene scene;
  r::FrameView view;
  view.valley.density = 0.01;
  std::vector<r::FogVolume> out(3);
  fall_line::app::dress_mists(level, false, scene, view, out);
  CHECK(out.empty() && view.valley.density == 0);
}
