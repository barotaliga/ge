// The live capture's ring of readbacks (engine/gpu/readback_ring.hpp): frames cleared to a colour
// of their own and captured come back oldest first, each with its stamp and its own colour (the
// copy reads the finished frame); with every slot in flight a capture is dropped and counted
// instead of waiting; `drain` hands back everything left; a BGRA target reads back as RGBA.
// Skips without an adapter.
//
// ge:summary Tests: the readback ring's order, stamps, colours, drops and drain
// ge:keywords test gpu readback ring live-capture drop stamp order drain bgra
// ge:skill gpu testing
// ge:test tests/gpu_readback_ring_test.cpp
#include <cstdint>
#include <vector>

#include "check.hpp"
#include "gpu/readback_ring.hpp"
#include "gpu/target.hpp"
#include "gpu_support.hpp"

namespace {

constexpr uint32_t kW = 40, kH = 24;

// Frame k's colour: red k*20, green 255-k*20, blue 77.
uint8_t red_of(uint64_t k) { return static_cast<uint8_t>((k * 20) % 256); }

void clear(ge::gpu::Device& d, const ge::gpu::Target& t, uint64_t k) {
  wgpu::RenderPassColorAttachment colour{};
  colour.view = t.view;
  colour.loadOp = wgpu::LoadOp::Clear;
  colour.storeOp = wgpu::StoreOp::Store;
  colour.clearValue = {red_of(k) / 255.0, (255 - red_of(k)) / 255.0, 77 / 255.0, 1.0};
  wgpu::RenderPassDescriptor pass{};
  pass.colorAttachmentCount = 1;
  pass.colorAttachments = &colour;
  wgpu::CommandEncoder encoder = d.device.CreateCommandEncoder();
  encoder.BeginRenderPass(&pass).End();
  wgpu::CommandBuffer commands = encoder.Finish();
  d.queue.Submit(1, &commands);
}

bool near(int a, int b) { return a - b <= 1 && b - a <= 1; }

}  // namespace

TEST(captured_frames_come_back_oldest_first_with_their_stamps_and_colours) {
  auto d = ge::test::test_device();
  if (!d) return;
  const ge::gpu::Target t = ge::gpu::create_target(*d, "ring.test", kW, kH);
  ge::gpu::ReadbackRing ring(4);
  ge::gpu::Image img;
  std::vector<uint64_t> got;
  bool colours = true;
  const auto take = [&](const ge::gpu::Image& i, uint64_t stamp) {
    got.push_back(stamp);
    const uint8_t* px = &i.rgba[(size_t{kH / 2} * kW + kW / 2) * 4];
    colours = colours && i.width == kW && i.height == kH && near(px[0], red_of(stamp)) &&
              near(px[1], 255 - red_of(stamp)) && near(px[2], 77);
  };
  int captured = 0;
  for (uint64_t k = 0; k < 40; k++) {
    clear(*d, t, k);
    captured += ring.capture(*d, t, k) ? 1 : 0;
    ring.poll(*d, img, take);
    d->instance.ProcessEvents();
  }
  ring.drain(*d, img, take);
  CHECK(colours);
  CHECK(static_cast<int>(got.size()) == captured);
  CHECK(ring.captured() == captured);
  CHECK(ring.captured() + ring.dropped() == 40);
  CHECK(ring.failed() == 0);
  CHECK(ring.in_flight() == 0);
  for (size_t i = 1; i < got.size(); i++) CHECK(got[i] > got[i - 1]);
  CHECK(d->errors.load() == 0);
}

TEST(a_full_ring_drops_the_capture_instead_of_waiting) {
  auto d = ge::test::test_device();
  if (!d) return;
  const ge::gpu::Target t = ge::gpu::create_target(*d, "ring.full", kW, kH);
  ge::gpu::ReadbackRing ring(2);
  for (uint64_t k = 0; k < 2; k++) {
    clear(*d, t, k);
    CHECK(ring.capture(*d, t, k));
  }
  // Nothing polled: both slots are in flight, whatever the GPU has finished.
  CHECK(!ring.capture(*d, t, 2));
  CHECK(ring.dropped() == 1);
  ge::gpu::Image img;
  std::vector<uint64_t> got;
  ring.drain(*d, img, [&](const ge::gpu::Image&, uint64_t s) { got.push_back(s); });
  CHECK(got.size() == 2 && got[0] == 0 && got[1] == 1);
  CHECK(ring.capture(*d, t, 3));  // a slot is free again
  ring.drain(*d, img, [&](const ge::gpu::Image&, uint64_t s) { got.push_back(s); });
  CHECK(got.size() == 3 && got[2] == 3);
}

TEST(a_bgra_target_reads_back_as_rgba) {
  auto d = ge::test::test_device();
  if (!d) return;
  const ge::gpu::Target t =
      ge::gpu::create_target(*d, "ring.bgra", kW, kH, wgpu::TextureFormat::BGRA8Unorm);
  ge::gpu::ReadbackRing ring(1);
  clear(*d, t, 5);  // red 100, green 155, blue 77
  CHECK(ring.capture(*d, t, 9));
  ge::gpu::Image img;
  bool ok = false;
  ring.drain(*d, img, [&](const ge::gpu::Image& i, uint64_t s) {
    ok = s == 9 && near(i.rgba[0], 100) && near(i.rgba[1], 155) && near(i.rgba[2], 77);
  });
  CHECK(ok);
}
