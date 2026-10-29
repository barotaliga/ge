// MEMORY: where a bot last saw what it is after, and when. What a pursuer goes to once it has lost
// sight of its quarry, and how long ago that was (a search that gives up after a while reads the
// age). Seeing it again (`see`) moves the place and the time; nothing else does.
//
// ge:summary Memory: the last place and time a bot saw its quarry, and how long ago
// ge:keywords ai memory last-seen last-known search lost quarry age forget
// ge:skill bots
// ge:test tests/ai_sight_test.cpp
#pragma once

#include <limits>

namespace ge::ai {

struct Memory {
  double x = 0, y = 0, z = 0;                            // where it was last seen, m
  double at = -std::numeric_limits<double>::infinity();  // s, when; -inf: never
  template <class Io>
  friend void describe(Io& io, Memory& v) {
    io(v.x);
    io(v.y);
    io(v.z);
    io(v.at);
  }
};

// Seen at (x, y, z) at time `t`.
inline void see(Memory& m, double x, double y, double z, double t) {
  m.x = x;
  m.y = y;
  m.z = z;
  m.at = t;
}

// Whether it has ever been seen.
inline bool known(const Memory& m) { return m.at > -std::numeric_limits<double>::infinity(); }

// S since it was last seen at time `t` (+inf when never).
inline double age(const Memory& m, double t) { return t - m.at; }

}  // namespace ge::ai
