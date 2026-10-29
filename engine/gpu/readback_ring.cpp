// The ring's capture: a slot taken, the copy recorded and submitted, its map started.
//
// ge:summary The readback ring's capture: a free slot, its own command buffer, the map started
// ge:keywords gpu readback ring capture submit map-async drop
// ge:skill gpu replay
// ge:test tests/gpu_readback_ring_test.cpp
#include "gpu/readback_ring.hpp"

#include <algorithm>

namespace ge::gpu {

ReadbackRing::ReadbackRing(int slots) {
  const int n = std::max(1, slots);
  slots_.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; i++) slots_.push_back(std::make_unique<Slot>());
}

bool ReadbackRing::capture(Device& device, const Target& target, uint64_t stamp) {
  if (!target.texture || target.width == 0 || target.height == 0) return false;
  if (in_flight_ == slots_.size()) {
    dropped_++;
    return false;
  }
  Slot& s = *slots_[(oldest_ + in_flight_) % slots_.size()];
  wgpu::CommandEncoder encoder = device.device.CreateCommandEncoder();
  s.readback.copy(device, encoder, target);
  wgpu::CommandBuffer commands = encoder.Finish();
  device.queue.Submit(1, &commands);
  s.readback.request(device);
  s.stamp = stamp;
  in_flight_++;
  captured_++;
  return true;
}

}  // namespace ge::gpu
