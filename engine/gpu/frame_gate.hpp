// THE FRAMES IN FLIGHT: how many frames of GPU work may still be queued when the next frame reads
// its input. A GPU that is handed frames faster than it draws them queues them, and every queued
// frame is a frame of lag between the mouse and the picture; the gate bounds that queue by waiting,
// before the frame's input is read, until the GPU has finished all but the last `latency - 1`
// frames (1: the GPU is idle when the input is read; loop/pacing.hpp's `PaceSettings::latency`, 2
// by default). A swap chain's own images queue too: platform/pacing.hpp's `set_swap_images` bounds
// those.
//
//   gate.wait(device, settings.latency);   // before the input is read
//   ... read input, step, draw, present ...
//   gate.submitted(device);                // after the frame's last submit
//
// ge:summary Frames in flight: wait for the GPU's queue before reading the next frame's input
// ge:keywords frame-gate frames-in-flight frame-latency max-frame-latency gpu-queue input-lag
// ge:keywords onsubmittedworkdone low-latency reflex
// ge:skill gpu perf
// ge:test tests/gpu_frame_gate_test.cpp
#pragma once

#include <array>
#include <cstdint>

#include "gpu/device.hpp"

namespace ge::gpu {

class FrameGate {
 public:
  static constexpr int32_t kMost = 4;  // the deepest queue it tracks

  // Wait until at most `latency - 1` of the frames marked by `submitted` are still on the GPU
  // (`latency` clamped to 1..kMost). Returns the seconds it waited.
  double wait(Device& device, int32_t latency);
  // The frame's work is all submitted: its completion is tracked from now.
  void submitted(Device& device);
  // Forget the tracked frames (a device or surface made again).
  void reset();
  int32_t in_flight() const { return count_; }

 private:
  std::array<wgpu::Future, kMost> ring_{};  // the oldest at head_
  int32_t head_ = 0, count_ = 0;
};

}  // namespace ge::gpu
