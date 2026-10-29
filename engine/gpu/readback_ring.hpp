// THE LIVE CAPTURE'S RING OF READBACKS (docs/replays.md § The live capture): a presented frame
// copied back to the CPU without the frame that asked ever waiting for it.
//
// `capture` records the copy of a target (the surface's frame, `Surface::frame_target`, between
// acquire and present) in a command buffer of its own, submits it after the frame's own work (so
// it copies the finished picture) and starts the map. The pixels arrive a frame or more later;
// `poll` hands every slot whose map has completed, OLDEST FIRST, with the stamp it was captured
// under. With every slot still in flight a capture is DROPPED and counted (`dropped`): the
// window never stalls on the copy, and a slow machine records fewer frames rather than a slower
// game. Three or four slots cover the usual one to two frames a map takes.
//
// ge:summary The live capture's ring of readbacks: frames copied back with no stall, oldest first
// ge:keywords gpu readback ring live-capture surface present copy map-async drop stamp in-flight
// ge:keywords capture-frame no-stall
// ge:skill gpu replay
// ge:test tests/gpu_readback_ring_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "gpu/device.hpp"
#include "gpu/target.hpp"

namespace ge::gpu {

class ReadbackRing {
 public:
  explicit ReadbackRing(int slots = 4);

  // Copy `target` (submitted after everything already on the queue) under `stamp`; false when
  // every slot is in flight and the frame is dropped.
  bool capture(Device& device, const Target& target, uint64_t stamp);

  // Every completed copy, oldest first: `img` (its storage reused from call to call) and the
  // stamp. Returns how many were handed. Never blocks.
  template <class Fn>
  int poll(Device& device, Image& img, Fn&& fn) {
    int handed = 0;
    while (in_flight_ > 0) {
      Slot& s = *slots_[oldest_];
      if (!s.readback.ready(device)) break;
      s.readback.take_into(img);
      oldest_ = (oldest_ + 1) % slots_.size();
      in_flight_--;
      if (img.rgba.empty()) {  // the map failed (the device said why)
        failed_++;
        continue;
      }
      fn(img, s.stamp);
      handed++;
    }
    return handed;
  }

  // Block until every copy in flight is handed (the end of a capture).
  template <class Fn>
  int drain(Device& device, Image& img, Fn&& fn) {
    int handed = 0;
    while (in_flight_ > 0) {
      slots_[oldest_]->readback.wait_mapped(device);
      handed += poll(device, img, fn);
    }
    return handed;
  }

  int64_t captured() const { return captured_; }
  int64_t dropped() const { return dropped_; }
  int64_t failed() const { return failed_; }
  int in_flight() const { return static_cast<int>(in_flight_); }
  int slots() const { return static_cast<int>(slots_.size()); }

 private:
  struct Slot {
    Readback readback;  // its map's callback holds its address: a slot never moves
    uint64_t stamp = 0;
  };
  std::vector<std::unique_ptr<Slot>> slots_;
  size_t oldest_ = 0;
  size_t in_flight_ = 0;
  int64_t captured_ = 0, dropped_ = 0, failed_ = 0;
};

}  // namespace ge::gpu
