// The frames in flight (engine/gpu/frame_gate.hpp) on the test adapter.
//
// ge:summary Tests the frame gate: at most latency minus one frames left on the GPU
// ge:keywords test frame-gate frames-in-flight frame-latency
// ge:skill gpu testing
#include "gpu/frame_gate.hpp"

#include "check.hpp"
#include "gpu_support.hpp"

TEST(the_gate_holds_at_most_latency_minus_one_frames) {
  auto d = ge::test::test_device();
  if (!d) return;
  ge::gpu::FrameGate gate;
  CHECK(gate.wait(*d, 1) == 0);  // nothing submitted: no wait
  for (int i = 0; i < 6; i++) gate.submitted(*d);
  CHECK(gate.in_flight() == ge::gpu::FrameGate::kMost);  // the deepest it tracks
  gate.wait(*d, 3);
  CHECK(gate.in_flight() == 2);
  gate.wait(*d, 1);
  CHECK(gate.in_flight() == 0);
  gate.submitted(*d);
  gate.wait(*d, 9);  // clamped to kMost: one frame may stay
  CHECK(gate.in_flight() == 1);
  gate.reset();
  CHECK(gate.in_flight() == 0);
}
