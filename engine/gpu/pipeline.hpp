// SHADERS AND PIPELINES, made at load. A shader module is made from embedded WGSL (the
// `ge_embed_wgsl` CMake step turns `<name>.wgsl` into `<name>.wgsl.hpp`, so no shader is read
// from disk at run time) and its compile messages become error lines with the label and the
// line. A pipeline is made through a `PipelineSet`, the game's pipeline manifest: every entry
// starts compiling asynchronously when it is added and `finish` waits for them all, so a load
// compiles in parallel and a frame never does.
//
// THE CREATION COUNTER: every pipeline counts on its device (`pipelines_created`). After
// `end_load` (device.hpp), adding one is an error line naming it and counts in
// `pipelines_after_load`, which tests and labs read: a pipeline made mid-run is a hitch on a
// real GPU. The fix is always to add it to the manifest.
//
// ge:summary Shader modules from embedded WGSL; the pipeline manifest and its creation counter
// ge:keywords gpu shader wgsl shader-module compilation-info pipeline render-pipeline
// ge:keywords compute-pipeline manifest pipeline-set create-async creation-counter mid-run
// ge:keywords create_shader add_render add_compute finish
// ge:skill gpu
// ge:test tests/gpu_device_test.cpp tests/gpu_capture_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/device.hpp"

namespace ge::gpu {

// Compile `wgsl` into a module labelled `label`. Compile errors are error lines quoting the
// label, line and column; the module is still returned (Dawn makes it an error object, and
// a pipeline that uses it fails validation by name).
wgpu::ShaderModule create_shader(Device& device, std::string_view wgsl, std::string_view label);

// A pipeline manifest being loaded. Entries are indices in the order they were added.
class PipelineSet {
 public:
  // Start compiling a pipeline; `desc.label` names it (required: it is the error's subject).
  uint32_t add_render(Device& device, const wgpu::RenderPipelineDescriptor& desc);
  uint32_t add_compute(Device& device, const wgpu::ComputePipelineDescriptor& desc);

  // Wait for every pipeline added so far. False when any failed (each failure was an error
  // line naming its pipeline).
  bool finish(Device& device);

  const wgpu::RenderPipeline& render(uint32_t i) const { return entries_[i].render; }
  const wgpu::ComputePipeline& compute(uint32_t i) const { return entries_[i].compute; }
  size_t size() const { return entries_.size(); }

 private:
  struct Entry {
    std::string label;
    wgpu::Future future{};
    wgpu::RenderPipeline render;
    wgpu::ComputePipeline compute;
    bool failed = false;
  };
  uint32_t note(Device& device, std::string label);
  std::vector<Entry> entries_;
};

}  // namespace ge::gpu
