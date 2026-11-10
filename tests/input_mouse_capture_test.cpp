// Who has the mouse (engine/input/mouse_capture.hpp): the focus takes it with no click, the
// console lets it go and closing it takes it back, a menu keeps it by default and lets it go when
// its bit is set, a lost focus always lets it go, and a step is reported once per change.
//
// ge:summary Tests: the mouse taken on focus, let go only for the release bits, steps once each
// ge:keywords test input mouse capture focus release follow_capture
// ge:skill input testing
#include "check.hpp"
#include "input/mouse_capture.hpp"

namespace in = ge::input;

TEST(the_focus_takes_the_mouse_without_a_click) {
  in::MouseCapture c;
  in::CaptureFacts f;
  CHECK(in::follow_capture(c, f, in::kReleaseDefault) == in::CaptureStep::none);
  CHECK(!c.held);
  f.focused = true;  // the Dock icon clicked: the focus comes back
  CHECK(in::follow_capture(c, f, in::kReleaseDefault) == in::CaptureStep::take);
  CHECK(c.held);
  CHECK(!c.pointer());
  CHECK(in::follow_capture(c, f, in::kReleaseDefault) == in::CaptureStep::none);
  f.focused = false;
  CHECK(in::follow_capture(c, f, in::kReleaseDefault) == in::CaptureStep::let_go);
  CHECK(!c.held);
}

TEST(the_console_lets_it_go_and_closing_it_takes_it_back) {
  in::MouseCapture c;
  in::CaptureFacts f{.focused = true};
  in::follow_capture(c, f, in::kReleaseDefault);
  f.console = true;
  CHECK(in::follow_capture(c, f, in::kReleaseDefault) == in::CaptureStep::let_go);
  CHECK(c.pointer());
  f.console = false;
  CHECK(in::follow_capture(c, f, in::kReleaseDefault) == in::CaptureStep::take);
}

TEST(a_menu_keeps_it_by_default_and_lets_it_go_when_asked) {
  in::MouseCapture c;
  in::CaptureFacts f{.focused = true, .menu = true};
  CHECK(in::follow_capture(c, f, in::kReleaseDefault) == in::CaptureStep::take);
  CHECK(!c.pointer());  // the menu reads the keys, not the frozen pointer
  CHECK(in::follow_capture(c, f, in::kReleaseDefault | in::kReleaseMenu) ==
        in::CaptureStep::let_go);
  CHECK(c.pointer());
}

TEST(each_bit_lets_go_alone_and_none_keeps_it_through_everything) {
  const in::CaptureFacts console{.focused = true, .console = true};
  const in::CaptureFacts menu{.focused = true, .menu = true};
  const in::CaptureFacts watch{.focused = true, .watching = true};
  CHECK(!in::want_capture(console, in::kReleaseConsole));
  CHECK(in::want_capture(menu, in::kReleaseConsole));
  CHECK(!in::want_capture(menu, in::kReleaseMenu));
  CHECK(!in::want_capture(watch, in::kReleaseWatch));
  CHECK(in::want_capture(watch, in::kReleaseConsole | in::kReleaseMenu));
  const in::CaptureFacts all{.focused = true, .console = true, .menu = true, .watching = true};
  CHECK(in::want_capture(all, 0));
  CHECK(!in::want_capture(in::CaptureFacts{.console = true}, 0));  // no focus, never held
}
