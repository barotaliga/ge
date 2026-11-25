// A WINDOW'S FRAMES PROFILED: the timing zones (zones.hpp) of the next N frames, kept frame by
// frame, so a player's "it runs slowly" or "it stutters" turns into which part of the frame costs
// what, on the player's own machine and build:
//
//   prof::FrameProfile fp;
//   frames_start(fp, 600);                  // `profile 600` in the console
//   while (running) {
//     if (frames_tick(fp)) print(frames_report(fp));  // at the TOP of the loop: no zone is open
//     { GE_ZONE("input"); ... } { GE_ZONE("world"); ... }
//   }
//
// `frames_tick` closes the frame before it (the wall time since the last tick) and folds that
// frame's spans into each zone's own time (its exclusive time, so nested zones are not counted
// twice), then empties the recording for the next. The report: the frame's median, 99th
// percentile and worst; each zone's median, p99, worst, share of the frames' total and heap
// allocations a frame (counted where the executable links ge::prof_alloc); the time in no zone;
// and the slowest frames, each with the zones that made it slow (a hitch named).
//
// Recording costs one span a zone (reserved at the start, so the frame allocates nothing in the
// zones) and the fold at each tick; nothing is recorded while no profile runs. Wall-clock times:
// they feed no simulation and no digest.
//
// ge:summary A window's next N frames profiled by zone: medians, p99, worst, the slowest frames
// ge:keywords prof profile frame-profile frames zones hitch stutter slow-frame worst-frames
// ge:keywords frames_start frames_tick frames_report per-frame median p99 share
// ge:skill perf
// ge:test tests/prof_frames_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "prof/zones.hpp"

namespace ge::prof {

struct FrameProfile {
  int asked = 0;                         // frames to record; 0 while none runs
  int64_t last_ns = 0;                   // the last tick, on the steady clock
  std::vector<int64_t> frame_ns;         // each recorded frame's wall time
  std::vector<uint32_t> zones;           // zone ids, in the order they first appeared
  std::vector<std::vector<int64_t>> ns;  // ns[zone][frame]: the zone's own time in that frame
  std::vector<int64_t> allocs;           // allocs[zone]: its own heap allocations, all frames
  std::vector<ZoneTotal> fold;           // a frame's fold, kept to reuse its room
  size_t dropped = 0;                    // spans that did not fit a frame's room
  bool done = false;                     // the record is whole: the report is ready
};

// Starts recording the next `frames` frames (1 to 100000), `spans_a_frame` spans room each.
void frames_start(FrameProfile& fp, int frames, size_t spans_a_frame = 8192);
// Ends a recording early (the report covers the frames recorded so far).
void frames_stop(FrameProfile& fp);
bool frames_running(const FrameProfile& fp);
// At the top of the window's loop: closes the frame before and folds its zones. True on the tick
// that completes the record (the report is then ready, and recording has stopped).
bool frames_tick(FrameProfile& fp);
// For a run measured outside a window loop (a test, a lab): the frame's time given, not read.
bool frames_tick_with(FrameProfile& fp, int64_t frame_ns);
// The report as console lines; `worst` slowest frames listed with their dearest zones.
std::string frames_report(const FrameProfile& fp, int worst = 5);

}  // namespace ge::prof
