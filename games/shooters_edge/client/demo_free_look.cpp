// Free look in a played demo (demo_free_look.hpp).
//
// ge:summary Free look in a played demo: the offset kept and laid on the view
// ge:keywords demo free-look shift offset
// ge:skill camera shooter
// ge:test tests/shooters_edge_demo_free_look_test.cpp
#include "shooters_edge/client/demo_free_look.hpp"

#include <algorithm>

namespace shooters_edge::client {

namespace {
constexpr double kRadPerDeg = 3.14159265358979323846 / 180;
}  // namespace

void free_look_step(DemoFreeLook& f, bool held, double dx, double dy,
                    const ge::shooter::Controls& c) {
  if (!held) {
    f = DemoFreeLook{};
    return;
  }
  f.held = true;
  f.yaw += dx * ge::shooter::yaw_per_count(c);
  f.pitch += dy * ge::shooter::pitch_per_count(c) * (c.invert ? -1 : 1);
  f.pitch = std::clamp(f.pitch, -2 * kFreeLookMostPitch, 2 * kFreeLookMostPitch);
}

void free_look_apply(const DemoFreeLook& f, ge::render::FrameView& view) {
  if (!f.held) return;
  view.heading += f.yaw * kRadPerDeg;
  const double most = kFreeLookMostPitch * kRadPerDeg;
  view.pitch = std::clamp(view.pitch - f.pitch * kRadPerDeg, -most, most);
}

}  // namespace shooters_edge::client
