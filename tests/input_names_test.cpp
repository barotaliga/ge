// The names (engine/input/names.hpp): every key, pad control, kind and event type reads back to
// itself; an unnamed key is key<usage>; each family prints its own face buttons.
//
// ge:summary Tests: every input name round-trips; unnamed keys; each family's button labels
// ge:keywords test input names key-name round-trip label family
// ge:skill input testing
#include "check.hpp"
#include "input/names.hpp"

namespace in = ge::input;

TEST(every_key_usage_names_and_reads_back_to_itself) {
  for (uint16_t u = 0; u < 512; u++) {
    const auto back = in::key_from_name(in::key_name(u));
    CHECK(back && *back == u);
  }
  CHECK(in::key_name(4) == "a" && in::key_name(39) == "0" && in::key_name(225) == "left_shift");
  CHECK(in::key_name(135) == "key135");
  CHECK(!in::key_from_name("key512") && !in::key_from_name("nonsense"));
}

TEST(pad_controls_kinds_families_and_event_types_round_trip) {
  for (int i = 0; i < in::kPadButtons; i++) {
    const auto b = static_cast<in::PadButton>(i);
    CHECK(in::pad_button_from_name(in::pad_button_name(b)) == b);
  }
  for (int i = 0; i < in::kPadAxes; i++) {
    const auto a = static_cast<in::PadAxis>(i);
    CHECK(in::pad_axis_from_name(in::pad_axis_name(a)) == a);
  }
  for (int i = 0; i < in::kDeviceKinds; i++) {
    const auto k = static_cast<in::DeviceKind>(i);
    CHECK(in::kind_from_name(in::kind_name(k)) == k);
  }
  for (int i = 0; i < in::kEventTypes; i++) {
    const auto t = static_cast<in::EventType>(i);
    CHECK(in::event_type_from_name(in::event_type_name(t)) == t);
  }
}

TEST(each_family_prints_its_own_face_buttons_on_the_same_positions) {
  CHECK(in::pad_label(in::Family::xbox, in::PadButton::south) == "A");
  CHECK(in::pad_label(in::Family::playstation, in::PadButton::south) == "cross");
  CHECK(in::pad_label(in::Family::playstation, in::PadButton::north) == "triangle");
  CHECK(in::pad_label(in::Family::nintendo, in::PadButton::south) == "B");
  CHECK(in::pad_label(in::Family::nintendo, in::PadButton::east) == "A");
  CHECK(in::pad_label(in::Family::playstation, in::PadButton::left_shoulder) == "L1");
  CHECK(in::pad_label(in::Family::generic, in::PadButton::dpad_up) == "dpad_up");
  CHECK(in::usb_key(0x46d, 0xc24f) == "046d:c24f");
}
