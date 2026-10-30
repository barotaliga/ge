// The device's creation and its callbacks: the adapter request, the feature-tier table against
// wgpu::FeatureName, the validation toggles, and the error, loss and log lines through
// core/output.
//
// ge:summary Device creation: adapter request, feature-tier mapping, toggles, error callbacks
// ge:keywords gpu device-creation request-adapter request-device feature-name toggles
// ge:keywords uncaptured-error device-lost logging-callback adapter-info
// ge:skill gpu
// ge:test tests/gpu_device_test.cpp
#include "gpu/device.hpp"

#include <vector>

#include "core/output.hpp"

namespace ge::gpu {

namespace out = core::output;

namespace {

// docs/rendering.md § Feature tiers, row for row, against the API's names.
constexpr std::array<wgpu::FeatureName, kFeatureCount> kFeatureNames = {
    wgpu::FeatureName::TimestampQuery,
    wgpu::FeatureName::IndirectFirstInstance,
    wgpu::FeatureName::MultiDrawIndirect,
    wgpu::FeatureName::Subgroups,
    wgpu::FeatureName::ShaderF16,
    wgpu::FeatureName::Float32Filterable,
    wgpu::FeatureName::TextureCompressionBC,
    wgpu::FeatureName::TextureCompressionASTC,
    wgpu::FeatureName::TextureCompressionETC2,
    wgpu::FeatureName::TransientAttachments,
};

std::string_view backend_name(wgpu::BackendType b) {
  switch (b) {
    case wgpu::BackendType::Vulkan:
      return "vulkan";
    case wgpu::BackendType::Metal:
      return "metal";
    case wgpu::BackendType::D3D12:
      return "d3d12";
    case wgpu::BackendType::D3D11:
      return "d3d11";
    case wgpu::BackendType::OpenGL:
      return "opengl";
    case wgpu::BackendType::OpenGLES:
      return "opengles";
    case wgpu::BackendType::Null:
      return "null";
    default:
      return "unknown";
  }
}

std::string_view error_kind(wgpu::ErrorType t) {
  switch (t) {
    case wgpu::ErrorType::Validation:
      return "validation error";
    case wgpu::ErrorType::OutOfMemory:
      return "out of memory";
    case wgpu::ErrorType::Internal:
      return "internal error";
    default:
      return "error";
  }
}

wgpu::Adapter request_adapter(wgpu::Instance& instance, bool fallback) {
  wgpu::RequestAdapterOptions options{};
  options.powerPreference = wgpu::PowerPreference::HighPerformance;
  options.forceFallbackAdapter = fallback;
  wgpu::Adapter adapter;
  instance.WaitAny(instance.RequestAdapter(&options, wgpu::CallbackMode::WaitAnyOnly,
                                           [&adapter](wgpu::RequestAdapterStatus status,
                                                      wgpu::Adapter a, wgpu::StringView) {
                                             if (status == wgpu::RequestAdapterStatus::Success) {
                                               adapter = std::move(a);
                                             }
                                           }),
                   UINT64_MAX);
  return adapter;
}

}  // namespace

std::string to_string(wgpu::StringView s) {
  if (!s.data) return {};
  if (s.length == wgpu::kStrlen) return std::string(s.data);
  return std::string(s.data, s.length);
}

std::string_view feature_name(Feature f) {
  static constexpr std::array<std::string_view, kFeatureCount> kNames = {
      "timestamp-query",
      "indirect-first-instance",
      "multi-draw-indirect",
      "subgroups",
      "shader-f16",
      "float32-filterable",
      "texture-compression-bc",
      "texture-compression-astc",
      "texture-compression-etc2",
      "transient-attachments",
  };
  const int i = static_cast<int>(f);
  return i >= 0 && i < kFeatureCount ? kNames[i] : "unknown";
}

std::unique_ptr<Device> create_device(const DeviceOptions& options) {
  auto dev = std::make_unique<Device>();
  static constexpr wgpu::InstanceFeatureName kTimedWait = wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor instance_desc{};
  instance_desc.requiredFeatureCount = 1;
  instance_desc.requiredFeatures = &kTimedWait;
  dev->instance = wgpu::CreateInstance(&instance_desc);
  if (!dev->instance) {
    out::error(
        "gpu: wgpuCreateInstance failed. The Dawn library is broken or mislinked; "
        "reconfigure (cmake/Dawn.cmake) and rebuild; skill: gpu.");
    return nullptr;
  }

  dev->adapter = request_adapter(dev->instance, false);
  if (!dev->adapter) dev->adapter = request_adapter(dev->instance, true);
  if (!dev->adapter) {
    out::error(
        "gpu: no adapter. This machine has no GPU driver and no software one. On Linux "
        "install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers) for lavapipe; "
        "docs/rendering.md § Where it is developed; skill: gpu.");
    return nullptr;
  }
  wgpu::AdapterInfo info;
  dev->adapter.GetInfo(&info);
  dev->report.name = to_string(info.device);
  dev->report.driver = to_string(info.description);
  dev->report.backend = std::string(backend_name(info.backendType));
  dev->report.software = info.adapterType == wgpu::AdapterType::CPU;
  dev->adapter.GetLimits(&dev->limits);

  std::vector<wgpu::FeatureName> wanted;
  for (int i = 0; i < kFeatureCount; ++i) {
    if (options.force_off[i] || !dev->adapter.HasFeature(kFeatureNames[i])) continue;
    wanted.push_back(kFeatureNames[i]);
    dev->features.on[i] = true;
  }

  dev->validation = options.validation < 0
#ifdef NDEBUG
                        ? false
#else
                        ? true
#endif
                        : options.validation != 0;
  static constexpr const char* kNoChecks[] = {"skip_validation", "disable_robustness"};
  // Dawn rounds every timestamp down to 2^16 ns by default (a browser's guard against timing
  // attacks), which leaves a pass's GPU time in steps of 0.0655 ms: the engine times its own
  // frames, so the timers keep their full precision.
  static constexpr const char* kFullTimers[] = {"timestamp_quantization"};
  wgpu::DawnTogglesDescriptor toggles{};
  if (!dev->validation) {
    toggles.enabledToggleCount = 2;
    toggles.enabledToggles = kNoChecks;
  }
  toggles.disabledToggleCount = 1;
  toggles.disabledToggles = kFullTimers;

  const std::string label(options.label);
  wgpu::DeviceDescriptor desc{};
  desc.nextInChain = &toggles;
  desc.label = wgpu::StringView(label.data(), label.size());
  desc.requiredFeatureCount = wanted.size();
  desc.requiredFeatures = wanted.data();
  desc.requiredLimits = &dev->limits;
  Device* self = dev.get();
  desc.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message, Device* d) {
        d->errors.fetch_add(1);
        out::error("gpu: " + std::string(error_kind(type)) + ": " + to_string(message) +
                   " The message names the object by its label: fix the call that made or "
                   "used it; skill: gpu.");
      },
      self);
  desc.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView message, Device* d) {
        if (reason == wgpu::DeviceLostReason::Destroyed ||
            reason == wgpu::DeviceLostReason::CallbackCancelled) {
          return;  // our own teardown
        }
        d->lost.store(true);
        out::error("gpu: the device was lost: " + to_string(message) +
                   " A driver reset or a hang in a pass; recreate the device, and if it "
                   "repeats, capture the pass that ran last; skill: gpu.");
      },
      self);

  dev->instance.WaitAny(
      dev->adapter.RequestDevice(
          &desc, wgpu::CallbackMode::WaitAnyOnly,
          [self](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView msg) {
            if (status == wgpu::RequestDeviceStatus::Success) {
              self->device = std::move(device);
            } else {
              out::error("gpu: requestDevice failed: " + to_string(msg) +
                         " A required limit or feature the adapter lacks; skill: gpu.");
            }
          }),
      UINT64_MAX);
  if (!dev->device) return nullptr;
  dev->device.SetLoggingCallback([](wgpu::LoggingType type, wgpu::StringView message) {
    if (type == wgpu::LoggingType::Warning || type == wgpu::LoggingType::Error) {
      out::warn("gpu: dawn: " + to_string(message) + " (skill: gpu)");
    } else {
      out::debug("gpu: dawn: " + to_string(message));
    }
  });
  dev->queue = dev->device.GetQueue();
  out::info("gpu: adapter " + adapter_line(*dev));
  return dev;
}

void end_load(Device& device) { device.loading = false; }

void wait(Device& device, wgpu::Future future) { device.instance.WaitAny(future, UINT64_MAX); }

std::string adapter_line(const Device& d) {
  std::string line = d.report.name + " / " + d.report.driver + " / " + d.report.backend;
  if (d.report.software) line += " (software)";
  std::string on;
  for (int i = 0; i < kFeatureCount; ++i) {
    if (!d.features.on[i]) continue;
    if (!on.empty()) on += ",";
    on += feature_name(static_cast<Feature>(i));
  }
  return line + " / features: " + (on.empty() ? "none" : on) +
         (d.validation ? " / validation on" : " / validation off");
}

}  // namespace ge::gpu
