// THE KEYBOARD HAND (engine/ai/keys.hpp): WASD's fingers are found from their resting keys and a
// weapon key past 4 is far; a key is pressed and let go a finger's time after it is wanted, never
// in the same command; W to S on one finger passes through a moment with neither, A to D on two
// fingers may overlap; a tap in reach lets its finger's key go and comes back; a far tap moves the
// hand and lets every key go; a mouse button needs no reach; no more than five keys are ever held;
// the same seed presses the same keys.
//
// ge:summary Tests: the keyboard hand's fingers, reach, press and release times, taps and shifts
// ge:keywords test keyboard keys fingers reach tap hand-shift roll wasd
// ge:skill bots testing
#include <cstdint>
#include <string>
#include <vector>

#include "ai/keys.hpp"
#include "check.hpp"

namespace ai = ge::ai;

namespace {

constexpr double kDt = 1.0 / 77;
enum Act { fwd, back, left, right, jump, w1, w2, w5, w7, attack2 };
uint32_t b(int a) { return 1u << a; }

ai::KeyLayout wasd() {
  ai::KeyLayout l;
  l.bind[fwd] = ai::Key::w;
  l.bind[back] = ai::Key::s;
  l.bind[left] = ai::Key::a;
  l.bind[right] = ai::Key::d;
  l.bind[jump] = ai::Key::space;
  l.bind[w1] = ai::Key::n1;
  l.bind[w2] = ai::Key::n2;
  l.bind[w5] = ai::Key::n5;
  l.bind[w7] = ai::Key::e;
  l.bind[attack2] = ai::Key::mouse2;
  l.home = {ai::Key::shift, ai::Key::a, ai::Key::w, ai::Key::d, ai::Key::space};
  return l;
}

// The commands until `a` is held (or let go when `held` is false), from now.
int steps_until(ai::Keyboard& k, uint32_t want, int a, bool held, int most = 200) {
  for (int n = 1; n <= most; n++)
    if (((k.step(kDt, want, 0) & b(a)) != 0) == held) return n;
  return -1;
}

}  // namespace

TEST(wasd_fingers_and_reach) {
  const ai::Keyboard k(wasd(), {}, 1);
  CHECK_MSG(k.finger_of(fwd) == 2 && k.finger_of(back) == 2, "W and S are not the middle finger's");
  CHECK_MSG(k.finger_of(left) == 1 && k.finger_of(right) == 3, "A and D are not ring and index");
  CHECK_MSG(k.finger_of(jump) == 4, "the space bar is not the thumb's");
  CHECK_MSG(k.finger_of(attack2) == 5, "a mouse button is not the other hand's");
  CHECK_MSG(!k.far(w2) && !k.far(w1), "1 and 2 are out of WASD's reach");
  CHECK_MSG(k.far(w5), "5 is within WASD's reach");
}

TEST(a_key_lands_a_fingers_time_after_it_is_wanted) {
  ai::Keyboard k(wasd(), {}, 2);
  const int press = steps_until(k, b(fwd), fwd, true);
  CHECK_MSG(press >= 2 && press <= 5,
            "forward pressed " + std::to_string(press) + " commands after wanted, not 15-45 ms");
  const int release = steps_until(k, 0, fwd, false);
  CHECK_MSG(release >= 1 && release <= 13,
            "forward let go " + std::to_string(release) + " commands after, not a finger's time");
}

TEST(one_finger_changes_keys_through_a_gap_two_fingers_roll) {
  ai::Keyboard k(wasd(), {}, 3);
  steps_until(k, b(fwd), fwd, true);
  bool gap = false, both = false;
  for (int n = 0; n < 40; n++) {
    const uint32_t h = k.step(kDt, b(back), 0);
    both = both || ((h & b(fwd)) && (h & b(back)));
    gap = gap || !(h & (b(fwd) | b(back)));
  }
  CHECK_MSG(!both, "W and S held together on one finger");
  CHECK_MSG(gap, "W to S on one finger had no moment with neither");
  // A to D: ring and index; over many seeds some changes overlap and none is in one command.
  int overlaps = 0;
  for (uint32_t seed = 1; seed <= 40; seed++) {
    ai::Keyboard r(wasd(), {}, seed);
    steps_until(r, b(left), left, true);
    for (int n = 0; n < 20; n++) {
      const uint32_t h = r.step(kDt, b(right), 0);
      overlaps += (h & b(left)) && (h & b(right));
      if (h & b(right) && !(h & b(left))) break;
    }
  }
  CHECK_MSG(overlaps > 0, "A to D never rolled (both held a moment)");
}

TEST(a_tap_in_reach_lets_its_fingers_key_go_and_comes_back) {
  ai::Keyboard k(wasd(), {}, 4);
  steps_until(k, b(fwd), fwd, true);
  uint32_t tapped = 0, seen = 0;
  bool dropped = false, back_on = false;
  k.step(kDt, b(fwd), b(w2), &tapped);
  seen |= tapped;
  for (int n = 0; n < 60; n++) {
    const uint32_t h = k.step(kDt, b(fwd), 0, &tapped);
    seen |= tapped;
    dropped = dropped || !(h & b(fwd));
    if (dropped && (h & b(fwd))) back_on = true;
  }
  CHECK_MSG(seen & b(w2), "the 2 key was never tapped");
  CHECK_MSG(dropped, "forward stayed held while its finger pressed 2");
  CHECK_MSG(back_on, "forward was not pressed again after the tap");
}

TEST(a_far_tap_moves_the_hand_and_lets_every_key_go) {
  ai::Keyboard k(wasd(), {}, 5);
  const uint32_t run = b(fwd) | b(right);
  for (int n = 0; n < 20; n++) k.step(kDt, run, 0);
  uint32_t tapped = 0, seen = 0;
  int free = 0, until = -1;
  k.step(kDt, run, b(w5), &tapped);
  for (int n = 1; n < 80; n++) {
    const uint32_t h = k.step(kDt, run, 0, &tapped);
    seen |= tapped;
    if (tapped & b(w5)) until = n;
    free += (h & run) == 0;
  }
  CHECK_MSG(seen & b(w5), "the far key was never tapped");
  CHECK_MSG(until >= 12, "the far key was tapped " + std::to_string(until) +
                             " commands on, quicker than the hand's travel");
  CHECK_MSG(free >= 25, "the move keys were let go for only " + std::to_string(free) +
                            " commands while the hand was away");
  CHECK_MSG(k.stats.shifts == 1, "the hand did not shift once");
  CHECK_MSG((k.step(kDt, run, 0) & run) == run, "the move keys were not pressed again after");
}

TEST(never_more_than_five_keys_and_the_same_seed_the_same_keys) {
  ai::Keyboard k(wasd(), {}, 6), j(wasd(), {}, 6);
  uint32_t want = 0;
  for (int n = 0; n < 2000; n++) {
    want = (n % 37 < 20 ? b(fwd) : b(back)) | (n % 23 < 11 ? b(left) : b(right)) |
           (n % 9 < 2 ? b(jump) : 0) | (n % 50 < 25 ? b(attack2) : 0);
    const uint32_t tap = n % 300 == 0 ? b(w7) : n % 450 == 0 ? b(w5) : 0;
    const uint32_t h = k.step(kDt, want, tap), g = j.step(kDt, want, tap);
    int keys = 0;
    for (int a = 0; a < 16; a++) keys += (h >> a) & 1;
    CHECK_MSG(keys <= 6, "more keys held than five fingers and a mouse button");
    CHECK_MSG(h == g, "the same seed pressed different keys");
  }
}
