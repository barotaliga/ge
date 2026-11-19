// The mouse's feel (mouse_feel.hpp).
//
// ge:summary The mouse's filter and acceleration and their settings
// ge:keywords mouse m_filter m_accel in_raw feel_mouse
// ge:skill input shooter
// ge:test tests/shooter_mouse_feel_test.cpp
#include "shooter/mouse_feel.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"

namespace ge::shooter {

void add_mouse_cvars(console::Cvars& v, MouseFeel& f) {
  v.bind({.name = "m_filter",
          .fallback = "0",
          .flags = console::kArchive,
          .help = "How much each frame's mouse move is blended with the last frame's.",
          .min = 0,
          .max = 1,
          .values = "0: not at all (the default); 1: the two frames averaged; between: the last "
                    "frame weighted half the value"},
         &f.filter);
  v.bind({.name = "m_accel",
          .fallback = "0",
          .flags = console::kArchive,
          .help = "How much faster the view turns the faster you move the mouse.",
          .min = 0,
          .max = 100,
          .values = "0: never faster (the default); above: that much sensitivity added for each "
                    "count a millisecond"},
         &f.accel);
  v.bind({.name = "m_accel_power",
          .fallback = "2",
          .flags = console::kArchive,
          .help = "How the mouse's acceleration grows with its speed.",
          .min = 0,
          .max = 10,
          .values = "2: in step with the speed (the default); above 2: faster than the speed"},
         &f.accel_power);
  v.bind({.name = "m_accel_offset",
          .fallback = "0",
          .flags = console::kArchive,
          .help = "The mouse speed, in counts a millisecond, where its acceleration starts.",
          .min = -100,
          .max = 100,
          .values = "0: at once (the default); above: only past that speed"},
         &f.accel_offset);
  v.bind({.name = "m_accel_senscap",
          .fallback = "0",
          .flags = console::kArchive,
          .help = "The most the mouse's sensitivity may reach.",
          .min = 0,
          .max = 1000,
          .values = "0: no limit (the default); above: the sensitivity held at most that"},
         &f.accel_senscap);
  v.bind({.name = "in_raw",
          .fallback = "1",
          .flags = console::kArchive,
          .help = "Whether the mouse's moves come as the mouse sends them.",
          .kind = console::CvarKind::boolean,
          .values = "1: as sent, the desktop's pointer speed left out (the default); 0: with "
                    "the desktop's pointer speed and acceleration, from the next time the game "
                    "takes the mouse"},
         &f.raw);
}

MouseTurn feel_mouse(MouseFeel& f, double dx, double dy, double sensitivity, double dt) {
  MouseTurn t;
  const double w = std::clamp(f.filter, 0.0, 1.0) / 2;
  t.dx = dx * (1 - w) + f.last_dx * w;
  t.dy = dy * (1 - w) + f.last_dy * w;
  f.last_dx = dx;
  f.last_dy = dy;
  t.sensitivity = sensitivity;
  if (sensitivity < 0.5 || f.accel <= 0) return t;     // the old saves' degrees a count: never
  const double length = std::sqrt(dx * dx + dy * dy);  // the raw counts, not the filtered
  if (f.per_frame) {
    t.sensitivity = sensitivity + length * std::min(f.accel, 1.0);
    return t;
  }
  const double ms = std::max(dt * 1000 - 1, 1.0);
  const double over = (length / ms - f.accel_offset) * f.accel;
  if (over > 0)
    t.sensitivity = f.accel_power > 1 ? sensitivity + core::libm::pow(over, f.accel_power - 1) : 1;
  if (f.accel_senscap > 0 && t.sensitivity > f.accel_senscap) t.sensitivity = f.accel_senscap;
  return t;
}

}  // namespace ge::shooter
