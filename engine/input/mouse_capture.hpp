// WHO HAS THE MOUSE: a first-person look takes the pointer (hidden, held, raw counts) whenever its
// window has the focus, the moment it gets it (a click on the window or on the app's icon in the
// Dock or the taskbar, or an alt-tab back), and gives it back only for what needs a pointer. What
// lets it go is a set of `Release` bits the game keeps in a cvar: the console by default; the
// menu and a watched demo's play bar when the game asks. A lost focus always lets it go (the OS
// would anyway) and a regained one takes it back without a click, so there is never a frame where
// the window has the focus, nothing asked for the pointer, and the look is dead.
//
// It is a pure function of the frame's facts: `follow_capture` says what to do this frame (take,
// let go or nothing) and the backend acts (`platform::set_relative_mouse`). While it is held, a
// menu that stays up reads the keys, never the frozen pointer (`MouseCapture::pointer`).
//
// ge:summary Who has the mouse: taken on focus, let go only for what the game's release bits name
// ge:keywords input mouse capture grab relative-mouse pointer focus focus-gained dock alt-tab
// ge:keywords release console menu in_mouse_release follow_capture
// ge:skill input
// ge:test tests/input_mouse_capture_test.cpp
#pragma once

#include <cstdint>

namespace ge::input {

// What may take the pointer back from the look while the window has the focus. A game keeps
// these as one cvar's bits (Shooter's Edge's `in_mouse_release`).
enum Release : uint8_t {
  kReleaseConsole = 1u,  // the console is down
  kReleaseMenu = 2u,     // a menu is up (it then reads the pointer: hover and click)
  kReleaseWatch = 4u,    // a demo watched at its pace (its play bar is clicked)
};
inline constexpr uint32_t kReleaseDefault = kReleaseConsole | kReleaseWatch;

// This frame's facts.
struct CaptureFacts {
  bool focused = false;   // the window has the input focus now
  bool console = false;   // the console is down
  bool menu = false;      // a menu is up
  bool watching = false;  // a demo plays at its pace
};

struct MouseCapture {
  bool held = false;  // the look has the pointer
  // Whether a menu up now may read the pointer: only when the look does not hold it (a held
  // pointer is frozen where it was, so its place means nothing).
  bool pointer() const { return !held; }
};

enum class CaptureStep : uint8_t { none, take, let_go };

// Whether the look should hold the pointer with these facts and these release bits.
bool want_capture(const CaptureFacts& facts, uint32_t release);

// The step to `want_capture`'s answer from what is held, and `held` updated to it; the caller
// turns `take` and `let_go` into the backend's call.
CaptureStep follow_capture(MouseCapture& capture, const CaptureFacts& facts, uint32_t release);

}  // namespace ge::input
