// The devices to the axes: the pairing by name, the latch, the step's values.
//
// ge:summary bind_axes, axes_frame and axes_step: the actions read as the spec's axes
// ge:keywords play axes devices latch press
// ge:skill agent-interface input
// ge:test tests/play_devices_test.cpp
#include "play/devices.hpp"

namespace ge::play {

DeviceAxes bind_axes(const input::ActionMap& map, std::span<const Axis> axes,
                     std::vector<std::string>* missing) {
  DeviceAxes d;
  for (const Axis& a : axes) {
    AxisFeed f;
    f.action = map.find_action(a.name);
    f.kind = a.kind;
    if (f.action < 0 && missing) missing->emplace_back(a.name);
    d.feeds.push_back(f);
  }
  return d;
}

void axes_frame(DeviceAxes& d, const input::ActionValues& v) {
  for (AxisFeed& f : d.feeds) {
    if (f.action >= 0 && f.kind == AxisKind::press && v[static_cast<size_t>(f.action)].pressed) {
      f.latched = true;
    }
  }
}

void axes_step(DeviceAxes& d, const input::ActionValues& v, std::span<double> out) {
  for (size_t i = 0; i < d.feeds.size() && i < out.size(); i++) {
    AxisFeed& f = d.feeds[i];
    out[i] = 0;
    if (f.action < 0) continue;
    const input::ActionValue& a = v[static_cast<size_t>(f.action)];
    switch (f.kind) {
      case AxisKind::signed_unit:
        out[i] = a.x < -1 ? -1 : (a.x > 1 ? 1 : a.x);
        break;
      case AxisKind::lever:
        out[i] = a.x < 0 ? 0 : (a.x > 1 ? 1 : a.x);
        break;
      case AxisKind::toggle:
        out[i] = a.held ? 1 : 0;
        break;
      case AxisKind::press:
        out[i] = f.latched ? 1 : 0;
        f.latched = false;
        break;
    }
  }
}

}  // namespace ge::play
