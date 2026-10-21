// THE GPU TEST SCENE: the smallest frame that exercises the whole path (an embedded shader
// with an #include, a pipeline from the manifest, a render pass, a readback or a present). It
// is the picture `tests/gpu_capture_test.cpp` holds to its golden image, the `gpu-clear` lab's
// shot and what the demo's window shows until the frame core draws a scene (M7): a sky
// gradient, pale at the bottom and deep blue at the top, and one triangle with a red (bottom
// left), a green (bottom right) and a blue (top) corner.
//
// ge:summary The GPU test scene: a sky gradient and an RGB triangle, the golden capture's frame
// ge:keywords gpu test-scene smoke golden-image capture triangle sky-gradient clear
// ge:keywords add_test_scene draw_test_scene
// ge:skill gpu
// ge:test tests/gpu_capture_test.cpp
#pragma once

#include <cstdint>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"

namespace ge::gpu {

// Add the scene's pipeline, for a target of `format`, to a manifest being loaded.
uint32_t add_test_scene(Device& device, PipelineSet& pipelines, wgpu::TextureFormat format);

// Record the scene into `target` (cleared first). `timestamps` may be nullptr.
void draw_test_scene(wgpu::CommandEncoder& encoder, const wgpu::RenderPipeline& pipeline,
                     const wgpu::TextureView& target,
                     const wgpu::PassTimestampWrites* timestamps = nullptr);

}  // namespace ge::gpu
