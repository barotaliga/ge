// Fall Line's fog: the map's mists dealt into the frame, and the froxels' sizes.
//
// ge:summary Fall Line's fog: the map's mists in the frame, the froxels by r_fog_quality
// ge:keywords fall-line fog mist dress_mists fog_settings froxels
// ge:skill render mountain
// ge:test tests/fall_line_mists_test.cpp
#include "fall_line/app/mists.hpp"

#include <algorithm>

#include "mountain/app/mist_look.hpp"

namespace fall_line::app {

ge::render::FogSettings fog_settings(int quality) {
  // Each step half again the froxels a side of the one under it, and the slices with them.
  static constexpr uint32_t kSizes[4][3] = {
      {80, 45, 32}, {120, 68, 48}, {160, 90, 64}, {240, 135, 96}};
  const auto& k = kSizes[std::clamp(quality, 0, 3)];
  ge::render::FogSettings s;
  s.width = k[0];
  s.height = k[1];
  s.slices = k[2];
  return s;
}

void dress_mists(const ge::mountain::mapgen::Level& level, bool on, const FrameScene& scene,
                 ge::render::FrameView& view, std::vector<ge::render::FogVolume>& out) {
  out.clear();
  if (!on) {
    view.valley = {};
    return;
  }
  if (!scene.fog.empty() || view.valley.density > 0) {
    out.assign(scene.fog.begin(), scene.fog.end());
    return;
  }
  const double hour = scene.hour < 0 ? level.sun.hour : scene.hour;
  ge::mountain::app::MistLook look = ge::mountain::app::mists_at(level, hour, view.eye);
  view.valley = look.valley;
  out = std::move(look.areas);
}

}  // namespace fall_line::app
