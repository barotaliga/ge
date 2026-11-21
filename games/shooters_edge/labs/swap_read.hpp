// THE HOP'S SWAP, READ OFF A RECORDING (se-moves, M18r.md P2): while a player hops (past 340
// units/s across), the view's lean off the velocity (yaw - heading) read every 25 ms as se-people
// reads the hand; a SWAP is the lean changing sides (5 degrees or more on each side three ticks
// out). Each swap is read for whether it falls at the landing (a ground frame within two ticks) and
// whether the view's turn reverses within 0.2 s of it (two ticks past 60 degrees a second, opposite
// ways). People swap at the landing two times in three; the bots' aim swapped in the air.
//
// ge:summary se-moves' read of the hop's swaps: a second, at the landing, with a reversal of the view
// ge:keywords hop swap lean reversal landing air-swap bunny-hop se-moves humanness
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "shooter/hops.hpp"

namespace shooters_edge::labs {

constexpr double kSwapTick = 0.025;  // read at 40 Hz, as se-people reads the hand
constexpr float kSwapHop = 340;      // hopping: this fast across
constexpr float kSwapLean = 5;       // a side: the lean this far off the velocity
constexpr double kSwapTurn = 60;     // a reversal's two turns past this, degrees/s
constexpr int kSwapWindow = 8;       // ticks each way a reversal belongs to the swap (0.2 s)

struct SwapRead {
  double hop_s = 0;
  int64_t swaps = 0, at_landing = 0, with_reversal = 0, reversals = 0;
};

inline float swap_wrap(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

// Every swap of one player's frames (`r`, shooter/hops.hpp) into `out`.
inline void swap_read(const ge::shooter::HopReading& r, SwapRead& out) {
  namespace sh = ge::shooter;
  if (r.frames.size() < 2) return;
  struct Tick {
    float speed, yaw, heading;
    bool ground, cut;
  };
  std::vector<Tick> ticks;
  size_t j = 0;
  for (double t = r.frames.front().t; t <= r.frames.back().t; t += kSwapTick) {
    while (j + 1 < r.frames.size() && r.frames[j + 1].t <= t) j++;
    const sh::HopFrame& f = r.frames[j];
    ticks.push_back(
        {f.speed, f.yaw, f.heading, f.phase == sh::HopPhase::ground, f.phase == sh::HopPhase::cut});
  }
  const size_t n = ticks.size();
  std::vector<double> rate(n, 0);  // rightward, degrees/s
  std::vector<uint8_t> hop(n, 0);
  for (size_t i = 1; i < n; i++) {
    rate[i] = -swap_wrap(ticks[i].yaw - ticks[i - 1].yaw) / kSwapTick;
    hop[i] = ticks[i].speed > kSwapHop && !ticks[i].cut && !ticks[i - 1].cut &&
             std::fabs(rate[i]) < 3000;  // a teleport's turn is the server's
  }
  const auto reversal = [&](size_t i) {
    return i >= 2 && hop[i] && hop[i - 1] && std::fabs(rate[i]) > kSwapTurn &&
           std::fabs(rate[i - 1]) > kSwapTurn && rate[i] * rate[i - 1] < 0;
  };
  const auto lean = [&](size_t i) { return swap_wrap(ticks[i].yaw - ticks[i].heading); };
  for (size_t i = 1; i < n; i++) {
    if (!hop[i]) continue;
    out.hop_s += kSwapTick;
    out.reversals += reversal(i);
    if (i < 3 || i + 3 >= n) continue;
    bool all = true;
    for (size_t k = i - 3; k <= i + 3; k++) all = all && hop[k];
    if (!all || lean(i - 1) * lean(i) >= 0 || std::fabs(lean(i - 3)) < kSwapLean ||
        std::fabs(lean(i + 3)) < kSwapLean || lean(i - 3) * lean(i + 3) >= 0)
      continue;
    out.swaps++;
    bool ground = false, rev = false;
    for (size_t k = i - 2; k <= i + 2; k++) ground = ground || ticks[k].ground;
    const size_t lo = i > kSwapWindow ? i - kSwapWindow : 1;
    for (size_t k = lo; k <= i + kSwapWindow && k < n; k++) rev = rev || reversal(k);
    out.at_landing += ground;
    out.with_reversal += rev;
  }
}

inline void add(SwapRead& a, const SwapRead& b) {
  a.hop_s += b.hop_s;
  a.swaps += b.swaps;
  a.at_landing += b.at_landing;
  a.with_reversal += b.with_reversal;
  a.reversals += b.reversals;
}

// `swap.*`: swaps a second hopping, the share at the landing and with a reversal, and the view's
// reversals a second hopping (se-people's `hop.reversals-per-s` by this read).
inline void report_swap(ge::lab::Lab& lab, const SwapRead& s) {
  if (s.hop_s <= 0) return;
  const double sw = static_cast<double>(s.swaps);
  lab.metric("swap.per-s", sw / s.hop_s, "per-s");
  lab.metric("swap.at-landing", sw > 0 ? static_cast<double>(s.at_landing) / sw : 0, "share");
  lab.metric("swap.with-reversal", sw > 0 ? static_cast<double>(s.with_reversal) / sw : 0, "share");
  lab.metric("swap.reversals-per-s", static_cast<double>(s.reversals) / s.hop_s, "per-s");
  lab.metric("swap.hop-s", s.hop_s, "s");
}

}  // namespace shooters_edge::labs
