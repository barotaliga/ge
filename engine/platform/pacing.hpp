// A WINDOW'S FRAME PACING, whole: the player's settings as cvars, the cap's wait on the real clock
// and the record of how the frames came (the arithmetic is loop/pacing.hpp's). A game's part is
// its wiring: `add_pacing_cvars` before its config runs, then a frame loop of
//
//   const double gpu_s = gate.wait(device, pacing.settings.latency);  // gpu/frame_gate.hpp
//   ge::platform::pace(window, pacing, gpu_s);   // the cap waited, the interval recorded
//   ... poll the input, step, draw, present ... gate.submitted(device);
//
// and, when `pacing.settings.vsync` or `latency` changed, the surface configured again
// (gpu/surface.hpp) and `set_swap_images(window, swap_images(pacing.settings))`. The swap chain
// stays at two images until latency 3: a third image is a finished frame waiting a whole refresh
// for the display, the lag the gate is there to remove. Waiting BEFORE the input is read
// is the point: the time a frame spends waiting is time its input does not age.
//
// THE CVARS (all archived: the player's to keep):
// - `vid_vsync 1`: present at the display's refresh; 0 as soon as drawn (tearing where the
//   platform allows it, the least lag).
// - `cl_maxfps 0`: a cap in frames a second; 0 none.
// - `cl_maxfps_hz 0`: the cap as a multiple of the display's refresh (1 the refresh, 2 twice);
//   it wins over cl_maxfps while the platform knows the refresh; 0 off.
// - `vid_framelatency 2`: GPU frames queued when a frame reads its input, 1..3; the swap chain
//   holds two images (three at 3) where the platform takes it (`swap_images`).
// - `vid_steady 1`: at vsync the game's frame time in whole refreshes (loop::SteadyClock): read it
//   through `game_dt`.
// - `scr_showfps 0`: 1 the frame rate in a corner, 2 with the last frames' times as a graph.
// - `framestats` (a command): the last frames summarised in one line (loop::summary_text), with the
//   refresh, the cap and the present mode; `framestats clear` starts the record again.
//
// ge:summary The window's frame pacing: the pacing cvars, the cap waited, the frames recorded
// ge:keywords pacing frame-pacing add_pacing_cvars vid_vsync cl_maxfps cl_maxfps_hz
// ge:keywords vid_framelatency scr_showfps framestats frame-cap refresh-multiple pace
// ge:keywords vid_steady game_dt
// ge:skill platform perf
// ge:test tests/platform_pacing_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "loop/pacing.hpp"
#include "platform/window.hpp"

namespace ge::console {
class Console;
}  // namespace ge::console

namespace ge::platform {

struct Pacing {
  loop::PaceSettings settings;  // the cvars write it
  int32_t show = 0;             // scr_showfps
  loop::FramePacer grid;
  loop::FrameTimes times;
  double refresh = 0;    // the display's refresh, Hz, read by the last `pace` (0: unknown)
  double cap = 0;        // the cap in force at the last `pace`, Hz (0: none)
  uint64_t last_ns = 0;  // when the last frame started
  std::string present;   // the present mode in use, for `framestats` ("fifo", "immediate")
  loop::SteadyClock steady;
};

// The pacing cvars over `p` (it must outlive `con`) and the `framestats` command.
void add_pacing_cvars(console::Console& con, Pacing& p);

// The start of a frame: the display's refresh read, the cap waited for, the interval since the
// last frame recorded with the waits (`gpu_wait_s` the frame gate's). Returns the frame's start,
// ns on now_ns's clock.
uint64_t pace(const Window& window, Pacing& p, double gpu_wait_s = 0);

// The game's time for a frame measured as `dt` s: whole refreshes when vsync and `vid_steady` are
// on and the refresh is known (loop::SteadyClock), else `dt`.
double game_dt(Pacing& p, double dt);

// The swap chain's images for the settings: 2, or 3 at vid_framelatency 3.
inline int swap_images(const loop::PaceSettings& s) { return s.latency >= 3 ? 3 : 2; }

// `framestats`'s line.
std::string pacing_text(const Pacing& p);

}  // namespace ge::platform
