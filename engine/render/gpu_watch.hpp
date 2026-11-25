// WHAT THE NEXT FRAMES COST (`gpustats`): any game's frame, read off its FrameRecorder's stats
// (frame.hpp) over a number of frames and reported once they are drawn, the dearest GPU passes
// first, with the CPU's side beside it: the frame's whole time (the wall clock), the frame's own
// preparing and encoding, and the rest (the game, the effects, the HUD, waiting for the display),
// so a frame held up by the CPU shows as such. `gpudebug` (gpu_debug.hpp) measures with it too.
//
// The GPU's times are the recorder's last readings (a frame or more late); on a GPU with no
// timestamp queries the report says so and keeps the CPU's side. On a tiled GPU passes overlap, so
// the passes add up to more than the frame's span: the span is the frame's time.
//
// ge:summary gpustats: any game's next frames measured off its recorder, the dearest passes first
// ge:keywords gpustats gpu-watch frame-cost pass-times gpu-span median fps report
// ge:keywords add_gpustats gpu_watch_frame gpu_watch_report gpu_notes
// ge:skill render perf
// ge:test tests/render_gpu_watch_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "render/frame.hpp"

namespace ge::console {
class Console;
}  // namespace ge::console

namespace ge::gpu {
struct Device;
}  // namespace ge::gpu

namespace ge::render {

struct GpuWatch {
  int left = 0;        // frames still to read
  size_t read = 0;     // frames read
  bool quiet = false;  // the report kept in `report`, not printed (gpudebug's)
  bool all = false;    // every pass reported, not the dearest dozen (`gpustats N all`)
  std::vector<double> frame_ms, cpu_ms, span_ms;  // each frame's (span: those timed)
  std::vector<std::pair<std::string, std::vector<double>>> pass_ms;  // by name, where it ran
  std::vector<std::string> report;                                   // the last report's lines
};

// Start reading the next `frames` frames (1..6000); `all`: report every pass.
void start_gpu_watch(GpuWatch& w, int frames, bool quiet, bool all = false);

// After each frame drawn: its stats (`frame_s` the whole frame's wall time) read while a watch
// runs; at its end the report is made, kept in `report` and printed to `con` unless quiet.
void gpu_watch_frame(GpuWatch& w, const FrameStats& stats, double frame_s, console::Console& con);

// The report over the frames read: the frame and the CPU's side, the GPU's span and its median
// in each tenth of the run (a frame that slowed partway shows as a step), then the dearest passes
// (at most `passes`; every one with the watch's `all`).
std::vector<std::string> gpu_watch_report(const GpuWatch& w, size_t passes = 12);

// `gpustats [frames] [all]` on `con` over `w` (which must outlive it): 60 frames by default.
void add_gpustats(console::Console& con, GpuWatch& w);

// gpudebug's "# gpu" notes: the adapter (when `device` is given), then the report.
std::vector<std::string> gpu_notes(const GpuWatch& w, const gpu::Device* device);

}  // namespace ge::render
