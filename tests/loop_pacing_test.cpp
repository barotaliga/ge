// The pacing arithmetic (engine/loop/pacing.hpp): the cap in force, the grid of deadlines, the
// frame-time record's summary.
//
// ge:summary Tests the pacing arithmetic: the cap, its grid, the steady clock, the summary
// ge:keywords test pacing frame-cap framepacer steady-clock frametimes summary
// ge:skill platform testing
#include "loop/pacing.hpp"

#include "check.hpp"

using namespace ge::loop;

TEST(the_refresh_multiple_wins_over_the_fixed_cap) {
  PaceSettings s;
  CHECK(cap_hz(s, 120) == 0);
  s.max_fps = 250;
  CHECK(cap_hz(s, 120) == 250);
  s.hz_mult = 2;
  CHECK(cap_hz(s, 120) == 240);
  CHECK(cap_hz(s, 0) == 250);  // the refresh unknown: the fixed cap
  CHECK(interval_ns(120) == 8'333'333);
  CHECK(interval_ns(0) == 0);
}

TEST(frames_start_on_an_even_grid) {
  FramePacer p;
  const int64_t step = interval_ns(100);   // 10 ms
  CHECK(p.start_at(1'000, 100) == 1'000);  // the first frame: at once
  // A frame that asks early waits for its slot; one a little late starts at once and the next
  // slot stays on the grid, so the late one is paid back.
  CHECK(p.start_at(5'000'000, 100) == 1'000 + step);
  CHECK(p.start_at(1'000 + 2 * step + 3'000'000, 100) == 1'000 + 2 * step);
  CHECK(p.start_at(1'000 + 2 * step + 4'000'000, 100) == 1'000 + 3 * step);
  // Later than a whole frame: the grid starts again from now, no burst of frames to catch up.
  const int64_t late = 1'000 + 10 * step;
  CHECK(p.start_at(late, 100) == late);
  CHECK(p.start_at(late + 1, 100) == late + step);
  // No cap: at once, and the grid forgotten.
  CHECK(p.start_at(42, 0) == 42);
  CHECK(p.next_ns == 0);
}

TEST(the_summary_finds_the_hitches_and_the_one_percent_low) {
  FrameTimes t;
  for (int i = 0; i < 990; i++) t.add(1.0 / 120, 0.002, 0.001, 0.004);
  for (int i = 0; i < 10; i++) t.add(2.0 / 120);  // ten refreshes missed
  const FrameSummary s = t.summarize();
  CHECK(s.frames == 1000);
  CHECK(s.hitches == 10);
  CHECK(s.p50_ms > 8.33 && s.p50_ms < 8.34);
  CHECK(s.max_ms > 16.6 && s.max_ms < 16.7);
  CHECK(s.low1_fps > 59.9 && s.low1_fps < 60.1);  // the worst 1%: the ten long frames
  CHECK(s.fps > 118 && s.fps < 120);
  CHECK(s.cap_wait_ms > 1.97 && s.cap_wait_ms < 1.99);
  CHECK(t.recent(0) == 2.0 / 120);
  CHECK(summary_text(s).find("10 hitches") != std::string::npos);
  CHECK(s.image_wait_ms > 3.95 && s.image_wait_ms < 3.97);  // the display's hold, shown apart
  CHECK(summary_text(s).find("image 3.96") != std::string::npos);
  // The ring keeps the newest kFrameTimes.
  for (size_t i = 0; i < kFrameTimes; i++) t.add(0.004);
  CHECK(t.size() == kFrameTimes);
  CHECK(t.summarize().hitches == 0);
  t.clear();
  CHECK(t.summarize().frames == 0);
}

TEST(the_steady_clock_hands_whole_refreshes_and_repays_the_rest) {
  SteadyClock c;
  const double period = 1.0 / 120;
  double game = 0, wall = 0;
  // Frame starts wandering by up to a millisecond around a perfect 120 Hz.
  const double wander[6] = {0.0009, -0.0007, 0.0003, -0.001, 0.0006, -0.0002};
  for (int i = 0; i < 600; i++) {
    const double dt = period + wander[i % 6] - (i % 6 ? wander[i % 6 - 1] : wander[5]);
    wall += dt;
    const double g = c.step(dt, period);
    game += g;
    CHECK(g > period * 0.98 && g < period * 1.02);  // even steps (the measured ones stray 23%)
  }
  CHECK(game - wall < 0.001 && wall - game < 0.001);  // and no drift
  // A missed refresh: two periods. A hitch far from any whole refresh: as measured.
  const double two = c.step(2 * period + 0.0003, period);
  CHECK(two > 1.98 * period && two < 2.02 * period);
  const double hitch = c.step(0.0545, period);
  CHECK(hitch > 0.0539);
  CHECK(c.debt == 0);
  CHECK(c.step(0.004, 0) == 0.004);  // no refresh known
}

TEST(the_lag_fix_starts_just_in_time_for_the_refresh) {
  VsyncPhase v;
  const int64_t period = 10'000'000, margin = 1'000'000;
  CHECK(v.start_at(500, period, margin) == 500);  // no refresh seen: at once
  v.blank(100'000'000);
  for (size_t i = 0; i < VsyncPhase::kCosts - 1; i++) v.cost(2'000'000);
  CHECK(v.start_at(101'000'000, period, margin) == 101'000'000);  // too few costs yet: at once
  v.cost(3'000'000);
  CHECK(v.worst() == 3'000'000);
  // Next refresh at 110 ms; the frame needs 3 + 1 ms: it starts at 106 ms.
  CHECK(v.start_at(101'000'000, period, margin) == 106'000'000);
  // Past 106 ms that refresh is too near: the one after, at 116 ms.
  CHECK(v.start_at(107'000'000, period, margin) == 116'000'000);
  // A frame that cannot finish in a refresh: at once.
  v.cost(9'500'000);
  CHECK(v.start_at(101'000'000, period, margin) == 101'000'000);
}
