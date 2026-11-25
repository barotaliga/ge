// The mouse's feel (shooter/mouse_feel.hpp): the filter blends a frame with the last raw one; the
// acceleration adds sensitivity by speed (counts a millisecond, or Quake II's counts a frame),
// with its power, offset and cap.
//
// ge:summary Tests m_filter and m_accel with its power, offset, cap and Quake II's per frame
// ge:keywords test mouse m_filter m_accel m_accel_power m_accel_offset m_accel_senscap
// ge:skill input testing
#include "check.hpp"
#include "console/console.hpp"
#include "shooter/mouse_feel.hpp"

namespace sh = ge::shooter;

TEST(the_filter_blends_with_the_last_raw_frame) {
  sh::MouseFeel f;
  f.filter = 1;
  sh::MouseTurn t = sh::feel_mouse(f, 10, 4, 3, 0.01);
  CHECK(t.dx == 5 && t.dy == 2);  // the first frame against nothing
  t = sh::feel_mouse(f, 0, 0, 3, 0.01);
  CHECK(t.dx == 5 && t.dy == 2);  // the last frame's RAW counts, not its blended ones
  f.filter = 0.5;
  sh::feel_mouse(f, 8, 0, 3, 0.01);
  t = sh::feel_mouse(f, 0, 0, 3, 0.01);
  CHECK(t.dx == 2);  // weighted a quarter
}

TEST(acceleration_adds_sensitivity_by_speed) {
  sh::MouseFeel f;
  CHECK(sh::feel_mouse(f, 30, 40, 3, 0.011).sensitivity == 3);  // off
  f.accel = 0.5;
  // 50 counts over 10 ms (11 less one): 5 a millisecond, times 0.5, power 2: 3 + 2.5.
  CHECK(sh::feel_mouse(f, 30, 40, 3, 0.011).sensitivity == 5.5);
  f.accel_offset = 6;  // below the offset: plain
  CHECK(sh::feel_mouse(f, 30, 40, 3, 0.011).sensitivity == 3);
  f.accel_offset = 0;
  f.accel_senscap = 4;
  CHECK(sh::feel_mouse(f, 30, 40, 3, 0.011).sensitivity == 4);
  f.accel_senscap = 0;
  f.accel_power = 3;  // (2.5) squared
  CHECK(sh::feel_mouse(f, 30, 40, 3, 0.011).sensitivity == 9.25);
  f.accel_power = 1;  // the quirk: exactly 1
  CHECK(sh::feel_mouse(f, 30, 40, 3, 0.011).sensitivity == 1);
  CHECK(sh::feel_mouse(f, 30, 40, 0.1, 0.011).sensitivity == 0.1);  // old saves: never
}

TEST(q2_acceleration_is_per_frame) {
  sh::MouseFeel f;
  f.per_frame = true;
  f.accel = 0.1;
  CHECK(sh::feel_mouse(f, 30, 40, 3, 0.5).sensitivity == 3 + 50 * 0.1);
  f.accel = 5;  // held at 1
  CHECK(sh::feel_mouse(f, 3, 4, 3, 0.5).sensitivity == 8);
}

TEST(the_settings_bind) {
  ge::console::Console con;
  sh::MouseFeel f;
  sh::add_mouse_cvars(con.cvars(), f);
  con.execute(
      "m_filter 1; m_accel 0.2; m_accel_power 2.5; m_accel_offset 1; m_accel_senscap 9; "
      "in_raw 0");
  CHECK(f.filter == 1 && f.accel == 0.2 && f.accel_power == 2.5 && f.accel_offset == 1 &&
        f.accel_senscap == 9 && !f.raw);
}
