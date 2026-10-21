// The GPU device (engine/gpu/device.hpp, pipeline.hpp): it is made on whatever adapter the
// machine has (lavapipe in the container and on CI), its feature tiers match the adapter and
// can be forced off, and a validation error, a shader error and a pipeline made after the load
// each become a counted, labelled error line.
//
// ge:summary Tests for the GPU device: adapter, feature tiers, error lines, pipeline counter
// ge:keywords test gpu device feature-tiers force_off validation-error shader-error
// ge:keywords pipeline-counter end_load
// ge:skill gpu testing
#include <string>

#include "check.hpp"
#include "core/output.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu/test_scene.hpp"
#include "gpu_support.hpp"

namespace {

using ge::gpu::Feature;

bool logged(const std::string& needle) {
  for (const auto& line : ge::core::output::recent_logs()) {
    if (line.message.find(needle) != std::string::npos) return true;
  }
  return false;
}

// Let the device deliver its pending callbacks (uncaptured errors arrive on a tick).
void settle(ge::gpu::Device& d) {
  d.device.Tick();
  d.instance.ProcessEvents();
}

}  // namespace

TEST(device_names_its_adapter) {
  auto d = ge::test::test_device();
  if (!d) return;
  CHECK(!d->report.name.empty());
  CHECK(!d->report.backend.empty());
  CHECK(d->validation);
  CHECK(logged("gpu: adapter " + d->report.name));
  std::printf("adapter: %s\n", ge::gpu::adapter_line(*d).c_str());
}

TEST(features_are_the_adapters) {
  auto d = ge::test::test_device();
  if (!d) return;
  // Every tier on is on the device; spot-check the ones every adapter decides either way.
  for (int i = 0; i < ge::gpu::kFeatureCount; ++i) {
    CHECK(ge::gpu::feature_name(static_cast<Feature>(i)) != "unknown");
  }
  CHECK(d->features.has(Feature::timestamp_query) ==
        static_cast<bool>(d->device.HasFeature(wgpu::FeatureName::TimestampQuery)));
  CHECK(d->features.has(Feature::timestamp_query) ==
        static_cast<bool>(d->adapter.HasFeature(wgpu::FeatureName::TimestampQuery)));
}

TEST(a_feature_forced_off_stays_off) {
  ge::gpu::DeviceOptions options;
  for (bool& off : options.force_off) off = true;
  auto d = ge::test::test_device(options);
  if (!d) return;
  for (int i = 0; i < ge::gpu::kFeatureCount; ++i) CHECK(!d->features.on[i]);
  CHECK(!d->device.HasFeature(wgpu::FeatureName::TimestampQuery));
}

TEST(a_validation_error_is_counted_and_names_the_object) {
  auto d = ge::test::test_device();
  if (!d) return;
  wgpu::BufferDescriptor desc{};
  desc.label = "test.bad-usage";
  desc.size = 16;
  desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::Storage;  // not allowed together
  wgpu::Buffer bad = d->device.CreateBuffer(&desc);
  settle(*d);
  CHECK(d->errors.load() == 1);
  CHECK(logged("test.bad-usage"));
  CHECK(logged("skill: gpu"));
}

TEST(a_shader_error_names_its_line) {
  auto d = ge::test::test_device();
  if (!d) return;
  ge::gpu::create_shader(*d, "fn broken( -> {}\n", "test.broken");
  settle(*d);
  CHECK(logged("gpu: shader test.broken.wgsl:1:"));
}

TEST(a_pipeline_after_end_load_is_an_error) {
  auto d = ge::test::test_device();
  if (!d) return;
  ge::gpu::PipelineSet manifest;
  ge::gpu::add_test_scene(*d, manifest, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(manifest.finish(*d));
  CHECK(d->pipelines_created == 1);
  CHECK(d->pipelines_after_load == 0);
  ge::gpu::end_load(*d);
  ge::gpu::PipelineSet late;
  ge::gpu::add_test_scene(*d, late, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(late.finish(*d));
  CHECK(d->pipelines_created == 2);
  CHECK(d->pipelines_after_load == 1);
  CHECK(logged("pipeline 'gpu.test_scene' created mid-run"));
  CHECK(d->errors.load() == 0);
}

TEST(a_device_without_validation_still_draws) {
  // The release build's device: skip_validation and disable_robustness on.
  auto d = ge::gpu::create_device({.validation = 0});
  if (!d) {
    ge::test::test_device();  // the same skip-or-fail as every other test
    return;
  }
  CHECK(!d->validation);
  CHECK(ge::gpu::adapter_line(*d).find("validation off") != std::string::npos);
  ge::gpu::PipelineSet manifest;
  const uint32_t scene = ge::gpu::add_test_scene(*d, manifest, wgpu::TextureFormat::RGBA8Unorm);
  CHECK(manifest.finish(*d));
  const ge::gpu::Target target = ge::gpu::create_target(*d, "test.unvalidated", 16, 16);
  wgpu::CommandEncoder encoder = d->device.CreateCommandEncoder();
  ge::gpu::draw_test_scene(encoder, manifest.render(scene), target.view);
  wgpu::CommandBuffer commands = encoder.Finish();
  d->queue.Submit(1, &commands);
  const ge::gpu::Image img = ge::gpu::read_now(*d, target);
  CHECK(img.rgba.size() == size_t{16} * 16 * 4);
  CHECK(!img.rgba.empty() && img.rgba[3] == 255);
  CHECK(d->errors.load() == 0);
}
