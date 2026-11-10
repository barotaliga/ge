// Who has the mouse: the want from the frame's facts and the release bits, and the step to it.
//
// ge:summary want_capture and follow_capture: the pointer taken on focus, let go by release bits
// ge:keywords input mouse capture focus release follow_capture want_capture
// ge:skill input
// ge:test tests/input_mouse_capture_test.cpp
#include "input/mouse_capture.hpp"

namespace ge::input {

bool want_capture(const CaptureFacts& f, uint32_t release) {
  if (!f.focused) return false;
  if (f.console && (release & kReleaseConsole)) return false;
  if (f.menu && (release & kReleaseMenu)) return false;
  if (f.watching && (release & kReleaseWatch)) return false;
  return true;
}

CaptureStep follow_capture(MouseCapture& c, const CaptureFacts& f, uint32_t release) {
  const bool want = want_capture(f, release);
  if (want == c.held) return CaptureStep::none;
  c.held = want;
  return want ? CaptureStep::take : CaptureStep::let_go;
}

}  // namespace ge::input
