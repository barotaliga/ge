// The shader compile-message lines and the async pipeline creation behind PipelineSet.
//
// ge:summary Shader compile messages as error lines; async pipeline creation and its counter
// ge:keywords gpu compilation-message create-render-pipeline-async
// ge:keywords create-compute-pipeline-async pipelines_after_load
// ge:skill gpu
// ge:test tests/gpu_device_test.cpp
#include "gpu/pipeline.hpp"

#include "core/output.hpp"

namespace ge::gpu {

namespace out = core::output;

wgpu::ShaderModule create_shader(Device& device, std::string_view wgsl, std::string_view label) {
  wgpu::ShaderSourceWGSL source{};
  source.code = wgpu::StringView(wgsl.data(), wgsl.size());
  wgpu::ShaderModuleDescriptor desc{};
  desc.nextInChain = &source;
  desc.label = wgpu::StringView(label.data(), label.size());
  wgpu::ShaderModule module = device.device.CreateShaderModule(&desc);
  const std::string name(label);
  wait(device, module.GetCompilationInfo(
                   wgpu::CallbackMode::WaitAnyOnly,
                   [&name](wgpu::CompilationInfoRequestStatus, const wgpu::CompilationInfo* info) {
                     if (!info) return;
                     for (size_t i = 0; i < info->messageCount; ++i) {
                       const wgpu::CompilationMessage& m = info->messages[i];
                       const std::string where = name + ".wgsl:" + std::to_string(m.lineNum) + ":" +
                                                 std::to_string(m.linePos);
                       if (m.type == wgpu::CompilationMessageType::Error) {
                         out::error("gpu: shader " + where + ": " + to_string(m.message) +
                                    " Run make shaders-check for every shader's errors; "
                                    "skill: gpu.");
                       } else {
                         out::warn("gpu: shader " + where + ": " + to_string(m.message) +
                                   " Run make shaders-check; skill: gpu.");
                       }
                     }
                   }));
  return module;
}

uint32_t PipelineSet::note(Device& device, std::string label) {
  ++device.pipelines_created;
  if (!device.loading) {
    ++device.pipelines_after_load;
    out::error("gpu: pipeline '" + label +
               "' created mid-run, after end_load. A pipeline compiles for milliseconds on a "
               "real GPU, so this is a hitch: add it to the manifest; skill: gpu, § Pipelines.");
  }
  if (label.empty()) {
    out::warn("gpu: a pipeline with no label; next: give it one, errors and captures name it.");
  }
  Entry entry;
  entry.label = std::move(label);
  entries_.push_back(std::move(entry));
  return static_cast<uint32_t>(entries_.size() - 1);
}

uint32_t PipelineSet::add_render(Device& device, const wgpu::RenderPipelineDescriptor& desc) {
  const uint32_t i = note(device, to_string(desc.label));
  entries_[i].future = device.device.CreateRenderPipelineAsync(
      &desc, wgpu::CallbackMode::WaitAnyOnly,
      [this, i](wgpu::CreatePipelineAsyncStatus status, wgpu::RenderPipeline p,
                wgpu::StringView msg) {
        Entry& e = entries_[i];
        if (status == wgpu::CreatePipelineAsyncStatus::Success) {
          e.render = std::move(p);
          return;
        }
        e.failed = true;
        out::error("gpu: render pipeline '" + e.label + "' failed: " + to_string(msg) +
                   " Fix its descriptor or its shader; skill: gpu.");
      });
  return i;
}

uint32_t PipelineSet::add_compute(Device& device, const wgpu::ComputePipelineDescriptor& desc) {
  const uint32_t i = note(device, to_string(desc.label));
  entries_[i].future = device.device.CreateComputePipelineAsync(
      &desc, wgpu::CallbackMode::WaitAnyOnly,
      [this, i](wgpu::CreatePipelineAsyncStatus status, wgpu::ComputePipeline p,
                wgpu::StringView msg) {
        Entry& e = entries_[i];
        if (status == wgpu::CreatePipelineAsyncStatus::Success) {
          e.compute = std::move(p);
          return;
        }
        e.failed = true;
        out::error("gpu: compute pipeline '" + e.label + "' failed: " + to_string(msg) +
                   " Fix its descriptor or its shader; skill: gpu.");
      });
  return i;
}

bool PipelineSet::finish(Device& device) {
  bool ok = true;
  for (Entry& e : entries_) {
    if (e.future.id != 0) wait(device, e.future);
    e.future = {};
    ok = ok && !e.failed;
  }
  return ok;
}

}  // namespace ge::gpu
