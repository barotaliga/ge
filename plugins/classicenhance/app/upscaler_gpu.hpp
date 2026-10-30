// THE TRAINED TEXTURE UPSCALER ON THE GPU: the network of upscaler.hpp run through compute
// shaders (app/upscaler.wgsl) on an engine device, the same layers in the same order as the CPU's
// `upscale`, every map in a storage buffer. The weights are uploaded once, packed into one buffer
// (each layer's range aligned for its binding); a picture records every dispatch first, writes
// their parameters into one uniform buffer, then submits them in one command buffer and reads
// the result back.
//
// The sums run in another order than the CPU's, so the result is close to `upscale`'s, not its
// bits (within 1e-4 on the tests' network): the bake records which device made a cache's
// pictures (`UpscaleDevice::name`). A picture whose maps would exceed the device's buffer limits
// is refused, and the bake runs that one on the CPU.
//
// ge:summary The texture upscaler on the GPU: its weights uploaded once, a picture's dispatches
// ge:keywords upscaler gpu compute dawn wgsl dispatch storage-buffer weights upload readback
// ge:skill classicenhance gpu
// ge:test tests/classicenhance_upscaler_gpu_test.cpp
#pragma once

#include <memory>
#include <string>

#include "classicenhance/upscaler.hpp"
#include "gpu/device.hpp"

namespace ge::classicenhance {

class GpuUpscaler final : public UpscaleDevice {
 public:
  // The network's weights on `device` (which must outlive it); nullptr with `error` when the
  // device cannot hold them or a pipeline fails.
  static std::unique_ptr<GpuUpscaler> create(gpu::Device& device, const Upscaler& net,
                                             std::string& error);
  ~GpuUpscaler() override;

  bool run(const net::Map& rgb, net::Map& out, std::string& error) override;
  std::string name() const override;

 private:
  struct Impl;
  explicit GpuUpscaler(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace ge::classicenhance
