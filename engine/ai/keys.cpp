// The keyboard hand (keys.hpp): the board, the fingers' reach, a finger's presses and releases,
// taps and the hand's shift to a far key.
//
// ge:summary The keyboard hand: the board's places, the fingers' presses, taps and shifts
// ge:keywords keyboard keys key-hand fingers bindings reach tap hand-shift
// ge:skill bots input
// ge:test tests/ai_keys_test.cpp
#include "ai/keys.hpp"

#include <cmath>

namespace ge::ai {

namespace {

constexpr float kSpaceLo = 3.5f, kSpaceHi = 8.5f;  // the space bar's span across, widths
constexpr float kAcross = 1.3f;  // a finger moves across a row this much harder than up a column

uint32_t bit(int a) { return 1u << static_cast<uint32_t>(a); }

// The distance a finger travels between two keys, widths (the space bar: from its nearest part),
// a step across a row weighed kAcross (a finger reaches up its column more easily).
float key_dist(Key a, Key b) {
  const KeyPlace pa = key_place(a), pb = key_place(b);
  float dx = pb.x - pa.x;
  if (a == Key::space || b == Key::space) {
    const float x = a == Key::space ? pb.x : pa.x;
    dx = x < kSpaceLo ? kSpaceLo - x : x > kSpaceHi ? x - kSpaceHi : 0.f;
  }
  dx *= kAcross;
  const float dy = pb.y - pa.y;
  return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

KeyPlace key_place(Key k) {
  switch (k) {
    case Key::n1:
      return {1, 0};
    case Key::n2:
      return {2, 0};
    case Key::n3:
      return {3, 0};
    case Key::n4:
      return {4, 0};
    case Key::n5:
      return {5, 0};
    case Key::n6:
      return {6, 0};
    case Key::n7:
      return {7, 0};
    case Key::n8:
      return {8, 0};
    case Key::n9:
      return {9, 0};
    case Key::n0:
      return {10, 0};
    case Key::tab:
      return {0.25f, 1};
    case Key::q:
      return {1.5f, 1};
    case Key::w:
      return {2.5f, 1};
    case Key::e:
      return {3.5f, 1};
    case Key::r:
      return {4.5f, 1};
    case Key::t:
      return {5.5f, 1};
    case Key::y:
      return {6.5f, 1};
    case Key::caps:
      return {0.4f, 2};
    case Key::a:
      return {1.75f, 2};
    case Key::s:
      return {2.75f, 2};
    case Key::d:
      return {3.75f, 2};
    case Key::f:
      return {4.75f, 2};
    case Key::g:
      return {5.75f, 2};
    case Key::h:
      return {6.75f, 2};
    case Key::shift:
      return {0.6f, 3};
    case Key::z:
      return {2.25f, 3};
    case Key::x:
      return {3.25f, 3};
    case Key::c:
      return {4.25f, 3};
    case Key::v:
      return {5.25f, 3};
    case Key::b:
      return {6.25f, 3};
    case Key::ctrl:
      return {0.5f, 4};
    case Key::alt:
      return {2.5f, 4};
    case Key::space:
      return {6, 4};
    case Key::up:
      return {16.5f, 3};
    case Key::left:
      return {15.5f, 4};
    case Key::down:
      return {16.5f, 4};
    case Key::right:
      return {17.5f, 4};
    default:
      return {0, 0};
  }
}

bool key_on_mouse(Key k) { return k >= Key::mouse1 && k <= Key::mouse5; }

Keyboard::Keyboard(const KeyLayout& layout, const KeyTiming& timing, uint32_t seed)
    : layout_(layout), timing_(timing), rng_(seed) {
  for (int i = 0; i < kKeyFingers; i++)
    fingers_[static_cast<size_t>(i)].on = layout.home[static_cast<size_t>(i)];
  mouse_at_.fill(-1);
  for (int a = 0; a < kKeyActions; a++) {
    const Key k = layout.bind[static_cast<size_t>(a)];
    finger_[static_cast<size_t>(a)] = -1;
    far_[static_cast<size_t>(a)] = false;
    if (k == Key::none) continue;
    if (key_on_mouse(k)) {
      finger_[static_cast<size_t>(a)] = kKeyFingers;
      continue;
    }
    // The finger whose resting key is nearest; past its reach the key is far.
    float best = 1e9f;
    for (int i = 0; i < kKeyFingers; i++) {
      const Key home = layout.home[static_cast<size_t>(i)];
      if (home == Key::none) continue;
      const float d = key_dist(home, k);
      if (d < best) {
        best = d;
        finger_[static_cast<size_t>(a)] = i;
      }
    }
    far_[static_cast<size_t>(a)] = best > layout.reach;
  }
}

double Keyboard::draw(double lo, double hi) { return core::range(rng_, lo, hi); }

double Keyboard::release_lag() {
  double lag = draw(timing_.release_lo, timing_.release_hi);
  if (core::chance(rng_, timing_.long_p)) {
    lag += timing_.long_s;
    stats.long_holds++;
  }
  return lag;
}

uint32_t Keyboard::held() const {
  uint32_t h = mouse_held_;
  for (const FingerState& fs : fingers_)
    if (fs.held >= 0) h |= bit(fs.held);
  if (shift_phase_ == 2 && shift_action_ >= 0) h |= bit(shift_action_);
  return h;
}

void Keyboard::finger_step(int i, uint32_t want, uint32_t* tapped) {
  FingerState& fs = fingers_[static_cast<size_t>(i)];
  if (fs.held >= 0 && fs.release_at >= 0 && t_ >= fs.release_at) {
    fs.held = -1;
    fs.release_at = -1;
    fs.held_tap = false;
  }
  if (fs.next >= 0 && fs.held < 0 && t_ >= fs.press_at) {
    fs.held = fs.next;
    fs.held_tap = fs.next_tap;
    fs.on = layout_.bind[static_cast<size_t>(fs.next)];
    if (fs.next_tap) {
      fs.release_at = t_ + timing_.tap_s;
      if (tapped) *tapped |= bit(fs.next);
      taps_ &= ~bit(fs.next);
      stats.taps++;
    } else {
      stats.presses++;
    }
    fs.next = -1;
    fs.press_at = -1;
  }
  if (fs.held_tap) return;  // a tap is held through
  // What it should hold: a tap of its own first, else the wanted action it holds, else the first
  // wanted action of its own.
  int desired = -1;
  bool tap = false;
  for (int a = 0; a < kKeyActions && desired < 0; a++)
    if (finger_[static_cast<size_t>(a)] == i && !far_[static_cast<size_t>(a)] && (taps_ & bit(a))) {
      desired = a;
      tap = true;
    }
  if (desired < 0 && fs.held >= 0 && (want & bit(fs.held))) desired = fs.held;
  for (int a = 0; a < kKeyActions && desired < 0; a++)
    if (finger_[static_cast<size_t>(a)] == i && !far_[static_cast<size_t>(a)] && (want & bit(a)))
      desired = a;
  if (fs.held >= 0 && fs.held != desired && fs.release_at < 0) {
    fs.release_at = t_ + (tap ? draw(timing_.release_lo, timing_.release_hi) : release_lag());
    if (want & bit(fs.held)) stats.dropped++;
  }
  if (fs.held >= 0 && fs.held == desired) fs.release_at = -1;  // wanted again before it let go
  if (fs.next >= 0 && fs.next != desired) {
    fs.next = -1;
    fs.press_at = -1;
  }
  if (desired >= 0 && fs.held != desired && fs.next < 0) {
    const Key k = layout_.bind[static_cast<size_t>(desired)];
    const double start = fs.held >= 0 ? fs.release_at : t_;
    const double travel =
        fs.on == Key::none || fs.on == k ? 0 : key_dist(fs.on, k) * timing_.travel_s;
    const double lag = t_ + draw(timing_.press_lo, timing_.press_hi);
    fs.next = desired;
    fs.next_tap = tap;
    fs.press_at = start + travel > lag ? start + travel : lag;
  }
}

uint32_t Keyboard::step(double dt, uint32_t want, uint32_t tap, uint32_t* tapped) {
  t_ += dt;
  if (tapped) *tapped = 0;
  for (int a = 0; a < kKeyActions; a++)
    if ((tap & bit(a)) && finger_[static_cast<size_t>(a)] >= 0) taps_ |= bit(a);
  // The other hand's buttons: each change a finger's time after it is wanted.
  for (int a = 0; a < kKeyActions; a++) {
    if (finger_[static_cast<size_t>(a)] != kKeyFingers) continue;
    double& at = mouse_at_[static_cast<size_t>(a)];
    const bool is = (mouse_held_ & bit(a)) != 0;
    const bool tapping = (taps_ & bit(a)) != 0;
    const bool should = tapping || (want & bit(a));
    if (at >= 0 && t_ >= at) {
      at = -1;
      if (is) {
        mouse_held_ &= ~bit(a);
      } else {
        mouse_held_ |= bit(a);
        stats.presses++;
        if (tapping) {
          if (tapped) *tapped |= bit(a);
          taps_ &= ~bit(a);
          stats.taps++;
          at = t_ + timing_.tap_s;  // let go after the tap
        }
      }
      continue;
    }
    if (at < 0 && should != is)
      at = t_ + (should ? draw(timing_.press_lo, timing_.press_hi) : release_lag());
    else if (at >= 0 && should == is && !tapping)
      at = -1;  // changed its mind before the finger moved
  }
  // The hand's shift to a far key: everything let go, there, the tap, back.
  if (shift_phase_ == 0) {
    for (int a = 0; a < kKeyActions; a++)
      if ((taps_ & bit(a)) && far_[static_cast<size_t>(a)]) {
        shift_phase_ = 1;
        shift_action_ = a;
        shift_until_ = t_ + timing_.shift_s;
        stats.shifts++;
        for (FingerState& fs : fingers_) {
          if (fs.held >= 0 && (want & bit(fs.held))) stats.dropped++;
          fs.held = fs.next = -1;
          fs.held_tap = fs.next_tap = false;
          fs.press_at = fs.release_at = -1;
        }
        break;
      }
  } else if (t_ >= shift_until_) {
    if (shift_phase_ == 1) {
      shift_phase_ = 2;
      if (tapped) *tapped |= bit(shift_action_);
      taps_ &= ~bit(shift_action_);
      stats.taps++;
      shift_until_ = t_ + timing_.tap_s;
    } else if (shift_phase_ == 2) {
      shift_phase_ = 3;
      shift_until_ = t_ + timing_.shift_s;
    } else {
      shift_phase_ = 0;
      shift_action_ = -1;
      for (int i = 0; i < kKeyFingers; i++)
        fingers_[static_cast<size_t>(i)].on = layout_.home[static_cast<size_t>(i)];
    }
  }
  if (shift_phase_ == 0)
    for (int i = 0; i < kKeyFingers; i++) finger_step(i, want, tapped);
  return held();
}

}  // namespace ge::ai
