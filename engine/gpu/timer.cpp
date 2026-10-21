// The timer's query set, its resolve and copy, and the readings' conversion to ms.
//
// ge:summary Timestamp query set, resolve, readback and conversion to milliseconds
// ge:keywords gpu timestamp resolve-query-set nanoseconds
// ge:skill gpu perf
// ge:test tests/gpu_capture_test.cpp
#include "gpu/timer.hpp"

#include <limits>
#include <string>

namespace ge::gpu {

bool PassTimer::init(Device& device, std::string_view label, uint32_t passes) {
  ms_.assign(passes, std::numeric_limits<double>::quiet_NaN());
  if (!device.features.has(Feature::timestamp_query) || passes == 0) return false;
  const std::string name(label);
  wgpu::QuerySetDescriptor qd{};
  qd.label = wgpu::StringView(name.data(), name.size());
  qd.type = wgpu::QueryType::Timestamp;
  qd.count = passes * 2;
  queries_ = device.device.CreateQuerySet(&qd);
  const uint64_t bytes = uint64_t{passes} * 2 * sizeof(uint64_t);
  wgpu::BufferDescriptor bd{};
  bd.size = bytes;
  bd.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
  resolved_ = device.device.CreateBuffer(&bd);
  bd.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  readback_ = device.device.CreateBuffer(&bd);
  writes_.resize(passes);
  for (uint32_t i = 0; i < passes; ++i) {
    writes_[i].querySet = queries_;
    writes_[i].beginningOfPassWriteIndex = i * 2;
    writes_[i].endOfPassWriteIndex = i * 2 + 1;
  }
  return true;
}

const wgpu::PassTimestampWrites* PassTimer::writes(uint32_t i) const {
  return active() && i < writes_.size() ? &writes_[i] : nullptr;
}

void PassTimer::resolve(wgpu::CommandEncoder& encoder) {
  if (!active() || pending_) return;  // the last readings are still on their way: skip a frame
  const uint32_t count = static_cast<uint32_t>(writes_.size() * 2);
  encoder.ResolveQuerySet(queries_, 0, count, resolved_, 0);
  encoder.CopyBufferToBuffer(resolved_, 0, readback_, 0, uint64_t{count} * sizeof(uint64_t));
}

void PassTimer::request(Device&) {
  if (!active() || pending_) return;
  pending_ = true;
  future_ = readback_.MapAsync(
      wgpu::MapMode::Read, 0, readback_.GetSize(), wgpu::CallbackMode::AllowProcessEvents,
      [this](wgpu::MapAsyncStatus status, wgpu::StringView) {
        if (status == wgpu::MapAsyncStatus::Success) {
          const auto* t = static_cast<const uint64_t*>(readback_.GetConstMappedRange());
          for (size_t i = 0; i < ms_.size(); ++i) {
            ms_[i] = t[i * 2 + 1] >= t[i * 2] ? static_cast<double>(t[i * 2 + 1] - t[i * 2]) / 1e6
                                              : std::numeric_limits<double>::quiet_NaN();
          }
          readback_.Unmap();
        }
        pending_ = false;
      });
}

bool PassTimer::ready(Device& device) {
  if (pending_) device.instance.ProcessEvents();
  return !pending_;
}

void PassTimer::wait(Device& device) {
  if (pending_) gpu::wait(device, future_);
}

double PassTimer::ms(uint32_t i) const {
  return i < ms_.size() ? ms_[i] : std::numeric_limits<double>::quiet_NaN();
}

}  // namespace ge::gpu
