// The models' ground shadows: the rule (model_shadow.hpp).
//
// ge:summary The models' ground shadows: the mode, the floor, the fade with height, the lean
// ge:keywords shadow gl_shadows shadow_of
// ge:skill render shooter
// ge:test tests/shooters_edge_model_shadow_test.cpp
#include "shooters_edge/client/model_shadow.hpp"

#include <algorithm>
#include <cmath>

namespace shooters_edge::client {

Shadow shadow_of(const ShadowIn& in) {
  Shadow s;
  if (in.mode <= 0 || !in.animated || in.see_through || in.no_shadow || !in.floor_found) return s;
  if (in.floor_up < kShadowSteepUp) return s;
  float share = 1;
  if (in.mode >= 2) {
    const float gone = kShadowFadeRadii * std::max(in.radius_units, 1.0f);
    share = std::clamp(1 - std::max(in.height_units, 0.0f) / gone, 0.0f, 1.0f);
  }
  if (share <= 0) return s;
  s.alpha = kShadowAlpha * share;
  const double yaw = double(in.yaw_deg) * 3.14159265358979323846 / 180;
  s.lean_x = float(kShadowLean * std::cos(yaw));
  s.lean_y = float(kShadowLean * std::sin(yaw));
  return s;
}

}  // namespace shooters_edge::client
