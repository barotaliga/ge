// The frames in flight (frame_gate.hpp).
//
// ge:summary The frame gate's ring of submitted frames and its wait
// ge:keywords frame-gate frames-in-flight
// ge:skill gpu
#include "gpu/frame_gate.hpp"

#include <algorithm>
#include <chrono>

namespace ge::gpu {

double FrameGate::wait(Device& device, int32_t latency) {
  const int32_t keep = std::clamp(latency, int32_t{1}, kMost) - 1;
  const auto from = std::chrono::steady_clock::now();
  bool waited = false;
  while (count_ > keep) {
    gpu::wait(device, ring_[static_cast<size_t>(head_)]);
    waited = true;
    head_ = (head_ + 1) % kMost;
    count_--;
  }
  if (!waited) return 0;
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - from).count();
}

void FrameGate::submitted(Device& device) {
  if (count_ == kMost) {  // the deepest queue tracked: the oldest is waited for now
    gpu::wait(device, ring_[static_cast<size_t>(head_)]);
    head_ = (head_ + 1) % kMost;
    count_--;
  }
  ring_[static_cast<size_t>((head_ + count_) % kMost)] = device.queue.OnSubmittedWorkDone(
      wgpu::CallbackMode::WaitAnyOnly, [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
  count_++;
}

void FrameGate::reset() { head_ = count_ = 0; }

}  // namespace ge::gpu
