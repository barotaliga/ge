// FRAME PACING: when a window's next frame starts, and how evenly the frames came. The display
// shows a picture once a refresh (a fixed one, or whenever a frame lands on a variable-refresh
// display); a frame that starts at the wrong moment either queues behind the display (latency) or
// misses its refresh (a repeated picture: judder). This file is the arithmetic of it, with no
// clock and no platform: the host reads the clock, waits, and hands the times in.
//
// THE SETTINGS (`PaceSettings`, the player's cvars: platform/pacing.hpp binds them):
// - `vsync`: present at the display's refresh (FIFO) or as soon as drawn (immediate).
// - THE CAP: `max_fps` frames a second, or `hz_mult` times the display's refresh (1 locks to the
//   refresh, 2 is twice it, 0.5 half), which wins when both are set and the refresh is known.
//   `cap_hz` picks the one in force; 0 is no cap.
// - `latency`: how many frames the GPU may still have queued when the next frame reads its input.
//   1 reads the input as late as the GPU allows, but a frame whose CPU and GPU time together pass
//   a refresh misses it; 2 (the default) lets the CPU build one frame while the GPU draws the
//   last, a few milliseconds more lag and no missed refresh until either alone passes one.
// - `steady`: at vsync, the game's frame time snapped to whole refreshes (`SteadyClock`).
// - `late_start`: at vsync, each frame started just in time for its refresh (`VsyncPhase`).
//
// THE CAP'S GRID (`FramePacer`): the frames start on a fixed grid of deadlines one interval
// apart, not one interval after the last frame began, so a frame that ran a little long is paid
// back by the next and the average is exactly the cap. A frame later than a whole interval resets
// the grid at that moment rather than rushing frames to catch up.
//
// THE STEADY CLOCK (`SteadyClock`): at vsync a picture is shown on a refresh, whatever moment its
// frame happened to start; a game that advances by the measured time between frame starts moves a
// little more one picture and a little less the next (the start wanders with the CPU's and the
// GPU's load), which the eye sees as judder even at a perfect frame rate. The steady clock hands
// the game whole refresh periods instead (one, or two after a missed refresh), and repays the
// difference from the wall clock a twentieth a frame, so the game's time never drifts from the
// wall's. A frame time far from any whole number of refreshes (a hitch, a load) passes through as
// measured.
//
// THE RECORD (`FrameTimes`): the intervals between frames, the last `kFrameTimes`, and the
// waits the pacing spent; `summarize` answers how fast and how even: the mean, the median, the
// 99th and 99.9th percentiles, the worst, the 1% low frame rate, the jitter (the mean distance
// from the median) and the hitches (frames longer than 1.5 times the median: a refresh missed).
//
// ge:summary Frame pacing arithmetic: the cap and its grid, frames in flight, the frame-time record
// ge:keywords frame-pacing pacing frame-cap maxfps cl_maxfps vsync refresh hz-multiple fps-cap
// ge:keywords frame-latency frames-in-flight frame-times percentile one-percent-low jitter hitch
// ge:keywords stutter judder framestats pacesettings framepacer frametimes summarize
// ge:keywords steady-clock steadyclock vid_steady snapped-frame-time display-time
// ge:keywords vsync-lag late-start just-in-time vsyncphase vid_vsync_lag_fix vid_vsync_lag_tweak
// ge:skill platform perf
// ge:test tests/loop_pacing_test.cpp
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::loop {

struct PaceSettings {
  bool vsync = true;          // vid_vsync
  double max_fps = 0;         // cl_maxfps: 0 no cap
  double hz_mult = 0;         // cl_maxfps_hz: the cap as a multiple of the display's refresh; 0 off
  int32_t latency = 2;        // vid_framelatency: GPU frames queued when the next frame reads input
  bool steady = true;         // vid_steady: at vsync, the game's time in whole refreshes
  bool late_start = false;    // vid_vsync_lag_fix: at vsync, start each frame just in time
  double late_margin_ms = 1;  // vid_vsync_lag_tweak: the spare time left before the refresh, ms
};

inline constexpr int32_t kMinLatency = 1, kMaxLatency = 3;

// The cap in force, frames a second: `hz_mult` times the refresh when both are known, else
// `max_fps`; 0 when neither caps.
inline double cap_hz(const PaceSettings& s, double refresh_hz) {
  if (s.hz_mult > 0 && refresh_hz > 0) return s.hz_mult * refresh_hz;
  return s.max_fps > 0 ? s.max_fps : 0;
}

// The frame interval of a rate, ns (0 for no rate).
inline int64_t interval_ns(double hz) {
  return hz > 0 ? static_cast<int64_t>(core::num::round(1e9 / hz)) : 0;
}

// The game's time a frame at vsync (see THE STEADY CLOCK above).
struct SteadyClock {
  double debt = 0;  // wall time measured and not yet handed to the game, s (negative: handed early)

  // The frame's time for the game, s, from its measured `dt` and the refresh `period` (s; 0 or
  // less: `dt` as measured, and the debt forgotten).
  double step(double dt, double period) {
    if (period <= 0 || dt <= 0) {
      debt = 0;
      return dt;
    }
    const double whole = core::num::round(dt / period);
    const double snapped = (whole < 1 ? 1 : whole) * period;
    const double off = dt - snapped;
    if (off > 0.25 * period || off < -0.25 * period) {  // a hitch: as measured
      const double out = dt + debt;
      debt = 0;
      return out > 0 ? out : dt;
    }
    debt += off;
    const double repay = debt * 0.05;
    debt -= repay;
    return snapped + repay;
  }
};

// THE LATE START at vsync (`VsyncPhase`, `vid_vsync_lag_fix`): a vsynced frame that starts as
// soon as the last one was handed over waits in the swap chain for the refresh, its input read up
// to a refresh before the picture shows. Started late instead, as late as its own cost allows, the
// frame is finished a margin before the refresh it is meant for and its input is that much
// fresher: at 120 Hz with a 5 ms frame, about 8 ms less lag than two images and two frames in
// flight. It needs two things measured:
// - WHEN THE REFRESHES FALL: a swap chain's acquire that blocks returns when the display frees an
//   image, which is at a refresh (`blank`); the refreshes after it are a period apart. A frame
//   started too late misses its refresh and the next acquire blocks again, and one started a whole
//   refresh too early finds no free image and blocks too, so a drifting display clock corrects the
//   phase by itself.
// - WHAT A FRAME COSTS, start to the GPU's last work done (`cost`; the window waits for its GPU
//   right after presenting while the late start is on, which delays nothing already presented): the
//   largest of the last `kCosts`, so one slow frame in a run of quick ones does not miss.
// The frame starts at the next refresh less the cost and the margin. When that has passed, it
// starts at the refresh after it less the same: the refresh the frame before waits for cannot
// take this one too. When the cost and the margin are more than a refresh (a frame that cannot
// finish in one), it starts at once and the late start does nothing.
struct VsyncPhase {
  static constexpr size_t kCosts = 8;
  int64_t blank_ns = 0;  // a refresh seen; 0 none yet
  std::array<int64_t, kCosts> costs{};
  size_t count = 0, head = 0;

  void blank(int64_t at_ns) { blank_ns = at_ns; }
  void cost(int64_t ns) {
    costs[head] = ns;
    head = (head + 1) % kCosts;
    count = std::min(count + 1, kCosts);
  }
  int64_t worst() const {
    int64_t w = 0;
    for (size_t i = 0; i < count; i++) w = std::max(w, costs[i]);
    return w;
  }
  // When the frame about to start should start, ns (at or before `now_ns`: at once), the display
  // refreshing every `period_ns` and `margin_ns` kept spare.
  int64_t start_at(int64_t now_ns, int64_t period_ns, int64_t margin_ns) const {
    if (blank_ns == 0 || period_ns <= 0 || count < kCosts) return now_ns;
    const int64_t need = worst() + margin_ns;
    if (need >= period_ns) return now_ns;
    const int64_t since = now_ns - blank_ns;
    int64_t next = blank_ns + (since >= 0 ? since / period_ns + 1 : 0) * period_ns;
    if (next - need < now_ns) next += period_ns;  // too late for that one: the one after
    return next - need;
  }
};

// The cap's grid of deadlines.
struct FramePacer {
  int64_t next_ns = 0;  // the next frame's slot on the grid; 0 before the first

  // When the frame that is about to start may start (a time to wait until, ns; at or before
  // `now_ns` means at once), with the cap at `hz` (0: no cap, the grid forgotten).
  int64_t start_at(int64_t now_ns, double hz) {
    const int64_t step = interval_ns(hz);
    if (step == 0) {
      next_ns = 0;
      return now_ns;
    }
    if (next_ns == 0 || now_ns - next_ns > step) next_ns = now_ns;  // late a whole frame: reset
    const int64_t at = next_ns;
    next_ns += step;
    return at;
  }
};

inline constexpr size_t kFrameTimes = 1024;

struct FrameSummary {
  size_t frames = 0;
  double mean_ms = 0, p50_ms = 0, p99_ms = 0, p999_ms = 0, max_ms = 0;
  double fps = 0;            // 1000 / mean
  double low1_fps = 0;       // the frame rate of the slowest 1% of frames (their mean interval)
  double jitter_ms = 0;      // the mean |interval - median|
  size_t hitches = 0;        // intervals over 1.5 x the median
  double cap_wait_ms = 0;    // the mean time a frame waited on the cap
  double gpu_wait_ms = 0;    // the mean time a frame waited on the GPU's queue
  double image_wait_ms = 0;  // the mean time a frame waited for a free swap-chain image (display)
};

class FrameTimes {
 public:
  // One frame: its interval since the last, and what it waited on, s: the cap, the GPU's queue,
  // and the swap chain for a free image (the display handing one back: a frame that waits there
  // every time is held to the display's pace, not the GPU's).
  void add(double interval_s, double cap_wait_s = 0, double gpu_wait_s = 0,
           double image_wait_s = 0) {
    ring_[head_] = {interval_s, cap_wait_s, gpu_wait_s, image_wait_s};
    head_ = (head_ + 1) % kFrameTimes;
    count_ = std::min(count_ + 1, kFrameTimes);
  }
  void clear() { head_ = count_ = 0; }
  size_t size() const { return count_; }
  // The i-th newest interval, s (0 the last frame's).
  double recent(size_t i) const { return ring_[(head_ + kFrameTimes - 1 - i) % kFrameTimes].dt; }

  FrameSummary summarize() const {
    FrameSummary s;
    s.frames = count_;
    if (count_ == 0) return s;
    std::vector<double> ms(count_);
    double sum = 0, cap = 0, gpu = 0, image = 0;
    for (size_t i = 0; i < count_; i++) {
      const Entry& e = ring_[i];
      ms[i] = e.dt * 1e3;
      sum += ms[i];
      cap += e.cap * 1e3;
      gpu += e.gpu * 1e3;
      image += e.image * 1e3;
    }
    const double n = static_cast<double>(count_);
    s.mean_ms = sum / n;
    s.cap_wait_ms = cap / n;
    s.gpu_wait_ms = gpu / n;
    s.image_wait_ms = image / n;
    s.fps = s.mean_ms > 0 ? 1e3 / s.mean_ms : 0;
    std::sort(ms.begin(), ms.end());
    const auto at = [&](double q) {
      return ms[std::min(
          count_ - 1, static_cast<size_t>(core::num::round(q * static_cast<double>(count_ - 1))))];
    };
    s.p50_ms = at(0.5);
    s.p99_ms = at(0.99);
    s.p999_ms = at(0.999);
    s.max_ms = ms.back();
    const size_t worst = std::max<size_t>(1, count_ / 100);
    double slow = 0;
    for (size_t i = count_ - worst; i < count_; i++) slow += ms[i];
    s.low1_fps = slow > 0 ? 1e3 * static_cast<double>(worst) / slow : 0;
    double dev = 0;
    for (const double v : ms) {
      dev += v > s.p50_ms ? v - s.p50_ms : s.p50_ms - v;
      if (v > 1.5 * s.p50_ms) s.hitches++;
    }
    s.jitter_ms = dev / n;
    return s;
  }

 private:
  struct Entry {
    double dt = 0, cap = 0, gpu = 0, image = 0;
  };
  std::array<Entry, kFrameTimes> ring_{};
  size_t head_ = 0, count_ = 0;
};

// The summary as one line a player pastes back: "frames 1024: 120.0 fps (1% low 117.6), ms mean
// 8.33 p50 8.33 p99 8.61 p99.9 9.02 max 9.10, jitter 0.05 ms, 0 hitches; waits cap 2.10 gpu 0.40
// image 0.10".
inline std::string summary_text(const FrameSummary& s) {
  using core::num::to_fixed;
  return "frames " + std::to_string(s.frames) + ": " + to_fixed(s.fps, 1) + " fps (1% low " +
         to_fixed(s.low1_fps, 1) + "), ms mean " + to_fixed(s.mean_ms, 2) + " p50 " +
         to_fixed(s.p50_ms, 2) + " p99 " + to_fixed(s.p99_ms, 2) + " p99.9 " +
         to_fixed(s.p999_ms, 2) + " max " + to_fixed(s.max_ms, 2) + ", jitter " +
         to_fixed(s.jitter_ms, 2) + " ms, " + std::to_string(s.hitches) + " hitches; waits cap " +
         to_fixed(s.cap_wait_ms, 2) + " gpu " + to_fixed(s.gpu_wait_ms, 2) + " image " +
         to_fixed(s.image_wait_ms, 2);
}

}  // namespace ge::loop
