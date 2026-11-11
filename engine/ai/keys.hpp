// THE KEYBOARD HAND: the keys a person presses come off a hand on a keyboard, not off a wish. A
// bot says which of its game's actions it wants held (moving, jumping) and which tapped once (a
// weapon's key); the hand presses what it can reach, when it can:
//
// 1. THE BOARD. A QWERTY board's keys at their places in key widths (`key_place`): the number
//    row, the three letter rows, the modifiers and the space bar, the arrows; the mouse's buttons
//    belong to the other hand and are pressed with no reach to cover.
// 2. THE BINDINGS AND THE PLACEMENT (`KeyLayout`): each action's key, and where each of the five
//    fingers rests (WASD: the ring finger on A, the middle on W, the index on D, the thumb on the
//    space bar, the little finger on shift). An action is pressed by the finger whose resting key
//    is nearest its key (a step across a row weighed 1.3: a finger reaches up its column more
//    easily), within `reach`; a key past every finger's reach (a weapon key at 5 or
//    more from WASD) is FAR.
// 3. ONE KEY A FINGER. A finger holds at most one key, so at most five keys are held at once, and
//    two keys on one finger (W and S on the middle) are never held together: changing from one to
//    the other is a release, the finger's travel (`travel_s` a key width) and a press. Two keys on
//    two fingers (A and D) roll: the new one may be pressed before the old one is let go.
// 4. A FINGER'S TIME. A press lands `press_lo..press_hi` after it is wanted, a release
//    `release_lo..release_hi` after it is no longer (and now and then `long_s` later still,
//    `long_p`: a key held too long); every change of a key waits for the finger, never the
//    command.
// 5. A TAP. A tapped action's key is held `tap_s`. A key in reach is tapped by its finger, which
//    lets go of what it held for that moment (the index on D pressing F lets the right key go).
//    A FAR key moves the hand: every key it held is let go, the hand travels `shift_s`, taps the
//    key and travels back `shift_s`, then presses what is still wanted.
//
// Deterministic: the draws come off the hand's own `core::Rng`; no transcendental. Fixed-size,
// copied cheaply, no allocation.
//
// ge:summary The keyboard hand: actions pressed by fingers on a modelled board, a finger's time
// ge:keywords keyboard keys key-hand fingers bindings layout wasd reach tap hand-shift roll socd
// ge:keywords key-press key-release travel weapon-key human-keys
// ge:skill bots input
// ge:test tests/ai_keys_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "core/prng.hpp"

namespace ge::ai {

enum class Key : uint8_t {
  none,
  n1,
  n2,
  n3,
  n4,
  n5,
  n6,
  n7,
  n8,
  n9,
  n0,
  tab,
  q,
  w,
  e,
  r,
  t,
  y,
  caps,
  a,
  s,
  d,
  f,
  g,
  h,
  shift,
  z,
  x,
  c,
  v,
  b,
  ctrl,
  alt,
  space,
  up,
  down,
  left,
  right,
  mouse1,
  mouse2,
  mouse3,
  mouse4,
  mouse5,
};

// A key's place on the board, key widths from the top left (x across, y down a row); a mouse
// button's is meaningless (`key_on_mouse`).
struct KeyPlace {
  float x = 0, y = 0;
};
KeyPlace key_place(Key k);
bool key_on_mouse(Key k);

constexpr int kKeyFingers = 5;  // little, ring, middle, index, thumb
constexpr int kKeyActions = 16;

struct KeyLayout {
  std::array<Key, kKeyActions> bind{};  // each action's key (none: unbound)
  std::array<Key, kKeyFingers> home{};  // where each finger rests (none: a finger not used)
  float reach = 2.3f;  // a finger reaches this far from its resting key, widths (across x1.3)
};

struct KeyTiming {
  double press_lo = 0.015, press_hi = 0.045;    // wanted to pressed, s
  double release_lo = 0.01, release_hi = 0.04;  // no longer wanted to let go, s
  double long_p = 0.03, long_s = 0.12;          // a release this often held this much longer
  double travel_s = 0.06;                       // a finger from one key to another, s a width
  double tap_s = 0.07;                          // a tapped key held, s
  double shift_s = 0.18;                        // the hand off its place to a far key (and back), s
};

struct KeyStats {
  int64_t presses = 0, taps = 0, shifts = 0, long_holds = 0;
  int64_t dropped = 0;  // keys let go for a tap or a shift while still wanted
};

class Keyboard {
 public:
  Keyboard() = default;
  Keyboard(const KeyLayout& layout, const KeyTiming& timing, uint32_t seed);
  // One step of `dt`: `want` the actions to hold (a bit each), `tap` those to tap once. Returns
  // the actions held at the step's end; `tapped` gets the taps whose key went down this step.
  uint32_t step(double dt, uint32_t want, uint32_t tap, uint32_t* tapped = nullptr);
  uint32_t held() const;
  // The action's finger (0..4), 5 the other hand (a mouse button), -1 unbound; `far`: past reach.
  int finger_of(int action) const { return finger_[static_cast<size_t>(action)]; }
  bool far(int action) const { return far_[static_cast<size_t>(action)]; }
  bool shifted() const { return shift_phase_ != 0; }
  KeyStats stats;

 private:
  struct FingerState {
    Key on = Key::none;      // the key it rests on or last pressed
    int8_t held = -1;        // the action it holds
    int8_t next = -1;        // the action it is on its way to press
    bool next_tap = false;   // that press is a tap
    bool held_tap = false;   // the key held is a tap
    double press_at = -1;    // when that press lands, s
    double release_at = -1;  // when the key held is let go, s
  };
  double draw(double lo, double hi);
  double release_lag();
  void finger_step(int i, uint32_t want, uint32_t* tapped);

  KeyLayout layout_;
  KeyTiming timing_;
  core::Rng rng_{0};
  std::array<int8_t, kKeyActions> finger_{};
  std::array<bool, kKeyActions> far_{};
  std::array<FingerState, kKeyFingers> fingers_{};
  uint32_t mouse_held_ = 0;                     // the other hand's buttons held
  std::array<double, kKeyActions> mouse_at_{};  // a mouse button's change lands then (-1 none)
  uint32_t taps_ = 0;                           // taps asked for and not yet done
  int shift_phase_ = 0;                         // 0 home, 1 going, 2 tapping, 3 coming back
  int8_t shift_action_ = -1;
  double shift_until_ = 0;
  double t_ = 0;
};

}  // namespace ge::ai
