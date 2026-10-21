// THE GPU DEVICE: the one place an adapter is chosen, a device is made and its errors are
// heard. engine/gpu is the only directory that includes webgpu.h (here through Dawn's C++
// header, webgpu_cpp.h, whose `wgpu::` handles are already reference-counted RAII: the engine
// adds no second handle layer over them, so every call reads as the WebGPU spec spells it).
//
// What `create_device` decides, each once:
// - THE ADAPTER: the default (high-performance) one, or the first software adapter when the
//   machine has nothing else, as in the GPU-less container and on CI, where it is Mesa's
//   lavapipe on Vulkan (docs/rendering.md § Where it is developed). The adapter's name, driver
//   and backend are printed once, so a run says what drew it.
// - THE LIMITS: the adapter's own, not WebGPU's low defaults.
// - THE FEATURE TIERS: every optional feature of docs/rendering.md § Feature tiers that the
//   adapter has is turned on and recorded in `Features`; a pass branches on that struct,
//   never on a vendor string. `DeviceOptions::force_off` turns one off, so a test covers the
//   path for when it is missing.
// - VALIDATION: on in a debug build, off in a release one (Dawn's `skip_validation` and
//   `disable_robustness` toggles), unless the options say otherwise.
// - ERRORS: a validation error, an out-of-memory and a lost device go through core/output as
//   agent-actionable lines and are counted (`Device::errors`). Dawn's own log lines go to
//   debug, warnings to warn.
// - PIPELINES AFTER LOAD: `end_load` closes the load; a pipeline made after it is an error
//   line naming it (pipeline.hpp counts them).
//
// ge:summary The GPU device: adapter choice, limits, feature tiers, validation, error routing
// ge:keywords gpu device adapter webgpu dawn wgpu limits features feature-tiers validation
// ge:keywords skip_validation disable_robustness software-adapter lavapipe llvmpipe
// ge:keywords uncaptured-error device-lost create_device force_off end_load wait
// ge:skill gpu
// ge:test tests/gpu_device_test.cpp
#pragma once

#include <webgpu/webgpu_cpp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace ge::gpu {

// The optional features of docs/rendering.md § Feature tiers, in its order.
enum class Feature : uint8_t {
  timestamp_query,
  indirect_first_instance,
  multi_draw_indirect,
  subgroups,
  shader_f16,
  float32_filterable,
  texture_compression_bc,
  texture_compression_astc,
  texture_compression_etc2,
  transient_attachments,
  count,
};
inline constexpr int kFeatureCount = static_cast<int>(Feature::count);

// The feature's name as the rendering doc and a log line spell it ("timestamp-query").
std::string_view feature_name(Feature f);

// Which tiers are on for this device. Read it; never test the adapter again.
struct Features {
  std::array<bool, kFeatureCount> on{};
  bool has(Feature f) const { return on[static_cast<int>(f)]; }
};

struct DeviceOptions {
  std::string_view label = "ge.device";
  // Validation and robustness: -1 the build's default (on in debug, off in release), 0 off,
  // 1 on. A test that expects a validation error sets 1.
  int validation = -1;
  // Features to leave off even when the adapter has them (a test of the fallback path).
  std::array<bool, kFeatureCount> force_off{};
};

// What drew the frames: printed once at creation, and stamped on lab captures.
struct AdapterReport {
  std::string name;     // "llvmpipe (LLVM 20.1.2, 256 bits)", "Apple M3 Max"
  std::string driver;   // "Mesa 25.2.8 (LLVM 20.1.2)"
  std::string backend;  // "vulkan", "metal", "d3d12"
  bool software = false;
};

struct Device {
  wgpu::Instance instance;
  wgpu::Adapter adapter;
  wgpu::Device device;
  wgpu::Queue queue;
  wgpu::Limits limits;
  Features features;
  AdapterReport report;
  bool validation = false;

  // Counters the error callbacks and the pipeline set keep. Read them; a test checks them.
  std::atomic<uint32_t> errors{0};    // validation, out-of-memory, internal
  std::atomic<bool> lost{false};      // the device was lost
  uint32_t pipelines_created = 0;     // every pipeline, at load or after
  uint32_t pipelines_after_load = 0;  // each one is an error line
  bool loading = true;                // until end_load()
};

// Make a device. Returns nullptr after an agent-actionable error line when there is no
// adapter (no GPU and no software driver: install Mesa's Vulkan drivers) or the device fails.
std::unique_ptr<Device> create_device(const DeviceOptions& options = {});

// Every pipeline from here on is made mid-run: an error (skill: gpu, § Pipelines).
void end_load(Device& device);

// Block until `future` completes, processing the device's events. For load, tests and lab
// captures only: never inside a frame.
void wait(Device& device, wgpu::Future future);

// A WebGPU string (a message, a label) as a std::string.
std::string to_string(wgpu::StringView s);

// One line for logs and lab stamps: "llvmpipe (...) / Mesa ... / vulkan (software)".
std::string adapter_line(const Device& device);

}  // namespace ge::gpu
