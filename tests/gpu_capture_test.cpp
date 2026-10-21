// A headless frame read back (engine/gpu/target.hpp, test_scene.hpp, timer.hpp): the test
// scene drawn offscreen on the software adapter matches tests/golden/gpu_test_scene.ppm within
// tolerance (M6's exit), the embedded shader's #include was resolved, and the pass timer
// reads a time when the adapter has timestamps and NaN when it does not.
//
// ge:summary Tests for headless capture: the test scene against its golden image, the timer
// ge:keywords test gpu capture golden-image readback screenshot timestamp timer wgsl-include
// ge:skill gpu testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu/test_scene.hpp"
#include "gpu/test_scene.wgsl.hpp"
#include "gpu/timer.hpp"
#include "gpu_support.hpp"

namespace {

// The capture's size: small, because the software adapter draws every pixel on the CPU.
constexpr uint32_t kWidth = 96;
constexpr uint32_t kHeight = 64;

// Draw the test scene into a fresh target, timed by `timer` when it is active.
ge::gpu::Image draw_and_read(ge::gpu::Device& d, ge::gpu::PassTimer* timer) {
  ge::gpu::PipelineSet manifest;
  const uint32_t scene = ge::gpu::add_test_scene(d, manifest, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(manifest.finish(d));
  ge::gpu::end_load(d);
  const ge::gpu::Target target = ge::gpu::create_target(d, "test.final", kWidth, kHeight);
  wgpu::CommandEncoder encoder = d.device.CreateCommandEncoder();
  ge::gpu::draw_test_scene(encoder, manifest.render(scene), target.view,
                           timer ? timer->writes(0) : nullptr);
  if (timer) timer->resolve(encoder);
  wgpu::CommandBuffer commands = encoder.Finish();
  d.queue.Submit(1, &commands);
  if (timer) timer->request(d);
  return ge::gpu::read_now(d, target);
}

}  // namespace

TEST(the_shader_include_is_resolved) {
  const std::string_view wgsl = ge::wgsl::test_scene;
  CHECK(wgsl.find("#include") == std::string_view::npos);
  CHECK(wgsl.find("fn linear_to_srgb") != std::string_view::npos);
  CHECK(wgsl.find("fn vs_main") != std::string_view::npos);
}

TEST(the_test_scene_matches_its_golden) {
  auto d = ge::test::test_device();
  if (!d) return;
  const ge::gpu::Image img = draw_and_read(*d, nullptr);
  CHECK(img.width == kWidth && img.height == kHeight);
  CHECK(img.rgba.size() == size_t{kWidth} * kHeight * 4);
  if (img.rgba.empty()) return;
  // Spot checks a reader can verify without the golden: the sky is blue at the top and pale
  // at the bottom, and the triangle's interior is not sky.
  const auto px = [&](uint32_t x, uint32_t y) { return &img.rgba[(size_t{y} * kWidth + x) * 4]; };
  CHECK(px(2, 2)[2] > px(2, 2)[0]);              // top left: blue over red
  CHECK(px(2, kHeight - 3)[0] > px(2, 2)[0]);    // paler at the bottom
  CHECK(px(kWidth / 2, kHeight / 2)[3] == 255);  // opaque
  // Within 2 of 255 per channel in all but 0.5% of the pixels (edges a driver may round), on
  // the pinned adapter only: another driver's picture is never held to this one's pixels.
  if (ge::test::pinned_adapter(*d)) {
    ge::test::check_golden_image("gpu_test_scene", img, 2, 0.005);
  } else {
    std::printf("golden comparison skipped: adapter '%s' is not the pinned lavapipe\n",
                d->report.name.c_str());
  }
  CHECK(d->pipelines_after_load == 0);
  CHECK(d->errors.load() == 0);
}

TEST(the_pass_timer_reads_a_time_or_nan) {
  auto d = ge::test::test_device();
  if (!d) return;
  ge::gpu::PassTimer timer;
  const bool active = timer.init(*d, "test.timer", 1);
  CHECK(active == d->features.has(ge::gpu::Feature::timestamp_query));
  draw_and_read(*d, &timer);
  timer.wait(*d);
  if (active) {
    CHECK(timer.ms(0) >= 0 && std::isfinite(timer.ms(0)));
  } else {
    CHECK(std::isnan(timer.ms(0)));
    CHECK(timer.writes(0) == nullptr);
  }
}

TEST(an_inert_timer_without_timestamps) {
  ge::gpu::DeviceOptions options;
  options.force_off[static_cast<int>(ge::gpu::Feature::timestamp_query)] = true;
  auto d = ge::test::test_device(options);
  if (!d) return;
  ge::gpu::PassTimer timer;
  CHECK(!timer.init(*d, "test.timer", 1));
  CHECK(timer.writes(0) == nullptr);
  draw_and_read(*d, &timer);
  CHECK(std::isnan(timer.ms(0)));
}
