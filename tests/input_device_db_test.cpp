// The device database (engine/input/device_db.hpp): a role read through its range, a pedal at
// rest at +1, the defaults by kind, the text both ways, and learning an axis and a button.
//
// ge:summary Tests: roles through their ranges, defaults, the text, learning axes and buttons
// ge:keywords test input device-db role range pedal default learn calibration text
// ge:skill input testing
#include "check.hpp"
#include "input/device_db.hpp"
#include "input/sim.hpp"

namespace in = ge::input;

TEST(a_role_reads_its_axis_through_its_range) {
  in::DeviceRow row;
  row.axes = {{in::Role::steer, 0, -1, 1}, {in::Role::brake, 2, 1, -1}};
  row.buttons = {{in::Role::shift_up, 4}};
  in::Slot slot;
  slot.axes[0] = 0.5f;
  slot.axes[2] = 1.0f;  // the pedal at rest
  bool found = false;
  CHECK(in::role_value(row, slot, in::Role::steer, &found) == 0.5f && found);
  CHECK(in::role_value(row, slot, in::Role::brake) == 0);
  slot.axes[2] = -1.0f;
  CHECK(in::role_value(row, slot, in::Role::brake) == 1);
  slot.axes[2] = 0.0f;
  CHECK(in::role_value(row, slot, in::Role::brake) == 0.5f);
  CHECK(in::role_value(row, slot, in::Role::clutch, &found) == 0 && !found);
  slot.buttons.held = 1u << 4;
  CHECK(in::role_value(row, slot, in::Role::shift_up) == 1);
}

TEST(a_wheel_with_no_row_steers_on_its_first_axis_and_guesses_nothing_else) {
  const in::DeviceInfo wheel = in::sim_wheel();
  const in::DeviceRow row = in::row_for(nullptr, wheel);
  CHECK(row.axes.size() == 1 && row.axes[0].role == in::Role::steer && row.axes[0].axis == 0);
  CHECK(row.buttons.empty());
  in::DeviceDb db;
  db.put({wheel.key, in::DeviceKind::wheel, "mine", 900, {{in::Role::clutch, 1, 1, -1}}, {}});
  CHECK(in::row_for(&db, wheel).axes[0].role == in::Role::clutch);
}

TEST(the_database_writes_back_to_itself_and_names_its_mistakes) {
  in::DeviceDb db;
  db.put({"046d:c24f",
          in::DeviceKind::wheel,
          "a wheel",
          900,
          {{in::Role::steer, 0, -1, 1}, {in::Role::throttle, 2, 1, -1}},
          {{in::Role::gear_reverse, 11}}});
  db.put({"fff2:0003", in::DeviceKind::throttle, "", 0, {{in::Role::throttle, 2, -1, 1}}, {}});
  const std::string text = in::write_db(db);
  in::DeviceDb back;
  std::string error;
  CHECK(in::read_db(text, back, error));
  CHECK(in::write_db(back) == text);
  CHECK(back.find("046d:c24f")->degrees == 900 && back.find("046d:c24f")->name == "a wheel");
  CHECK(!in::read_db("ge-devices 1\naxis steer 0\n", back, error));
  CHECK(error.find("line 2") != std::string::npos);
  CHECK(!in::read_db("ge-devices 1\ndevice k wheel\nbutton steer 1\n", back, error));
  CHECK(!in::read_db("ge-devices 1\ndevice k sled\n", back, error));
}

TEST(learning_finds_the_axis_that_travelled_and_starts_a_lever_where_it_rests) {
  in::Slot slot;
  slot.axes[3] = 1.0f;  // a pedal resting at +1
  slot.axes[0] = 0.02f;
  in::Learn l;
  in::learn_begin(l, slot, in::Role::clutch);
  for (int i = 0; i <= 20; i++) {
    slot.axes[3] = 1.0f - 2.0f * static_cast<float>(i) / 20;  // floored
    slot.axes[0] = 0.02f + (i % 2 ? 0.05f : -0.05f);          // a nudged wheel
    in::learn_feed(l, slot);
  }
  slot.axes[3] = 1.0f;
  in::learn_feed(l, slot);
  const auto a = in::learn_axis(l);
  CHECK(a && a->axis == 3 && a->lo == 1 && a->hi == -1);

  in::learn_begin(l, slot, in::Role::steer);
  in::learn_feed(l, slot);
  CHECK(!in::learn_axis(l));  // nothing travelled
  in::learn_begin(l, slot, in::Role::gear_1);
  slot.buttons.pressed = 1u << 7;
  in::learn_feed(l, slot);
  const auto b = in::learn_button(l);
  CHECK(b && b->button == 7 && b->role == in::Role::gear_1);
}

TEST(every_role_names_and_reads_back) {
  for (int i = 0; i < in::kRoles; i++) {
    const auto r = static_cast<in::Role>(i);
    CHECK(in::role_from_name(in::role_name(r)) == r);
  }
  CHECK(in::is_signed(in::Role::rudder) && !in::is_signed(in::Role::brake));
  CHECK(in::is_button(in::Role::gear_reverse) && !in::is_button(in::Role::slider));
}
