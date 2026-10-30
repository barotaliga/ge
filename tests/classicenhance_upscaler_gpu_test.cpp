// plugins/classicenhance/app/upscaler_gpu.hpp: the small seeded network run through the compute
// shaders gives the CPU's picture within 1e-4 at every size the window attention pads (a map
// smaller than a window, one that is not a whole number of windows, one that is), and the
// texture enhancer runs it through the device it is handed.
//
// ge:summary Test: the texture upscaler on the GPU against the CPU's run of the same network
// ge:keywords test classicenhance upscaler gpu compute dawn cpu-reference tolerance
// ge:skill classicenhance testing gpu
#include <cmath>
#include <string>

#include "check.hpp"
#include "classicenhance/app/upscaler_gpu.hpp"
#include "classicenhance/texture.hpp"
#include "classicenhance/upscaler.hpp"
#include "gpu_support.hpp"

namespace ce = ge::classicenhance;

namespace {

ce::net::Map pattern(int h, int w) {
  ce::net::Map x(h, w, 3);
  for (size_t i = 0; i < x.v.size(); i++) x.v[i] = float(double((i * 37) % 101) / 100.0);
  return x;
}

}  // namespace

TEST(gpu_upscale_matches_the_cpu) {
  auto device = ge::test::test_device();
  ce::Upscaler u;
  std::string error;
  CHECK_MSG(ce::load_upscaler(ce::seeded_upscaler_weights(7), u, error), error.c_str());
  auto gpu = ce::GpuUpscaler::create(*device, u, error);
  CHECK_MSG(gpu != nullptr, error.c_str());
  if (!gpu) return;
  for (const auto& [h, w] :
       {std::pair{10, 13}, std::pair{3, 2}, std::pair{8, 16}, std::pair{5, 17}}) {
    const ce::net::Map x = pattern(h, w);
    const ce::net::Map want = ce::upscale(u, x);
    ce::net::Map got;
    CHECK_MSG(gpu->run(x, got, error), error.c_str());
    CHECK(got.h == want.h && got.w == want.w && got.c == 3);
    if (got.v.size() != want.v.size()) continue;
    float worst = 0;
    for (size_t i = 0; i < got.v.size(); i++)
      worst = std::max(worst, std::fabs(got.v[i] - want.v[i]));
    CHECK_MSG(worst < 1e-4f,
              (std::to_string(w) + "x" + std::to_string(h) + ": " + std::to_string(worst)).c_str());
  }
  CHECK(device->errors.load() == 0);
}

TEST(texture_enhancer_runs_the_network_on_the_device_it_is_handed) {
  auto device = ge::test::test_device();
  ce::Upscaler u;
  std::string error;
  CHECK_MSG(ce::load_upscaler(ce::seeded_upscaler_weights(7), u, error), error.c_str());
  auto gpu = ce::GpuUpscaler::create(*device, u, error);
  CHECK_MSG(gpu != nullptr, error.c_str());
  if (!gpu) return;
  ce::Palette palette;
  for (int i = 0; i < 256; i++) {
    for (int c = 0; c < 3; c++)
      palette.rgb[size_t(i) * 3 + size_t(c)] = uint8_t((i * (c + 3)) & 255);
  }
  std::vector<uint8_t> indexed(size_t{16} * 16);
  for (size_t i = 0; i < indexed.size(); i++) indexed[i] = uint8_t((i * 29) % 211);
  ce::EnhanceOptions cpu;
  cpu.network = &u;
  ce::EnhanceOptions on_gpu = cpu;
  on_gpu.device = gpu.get();
  const ce::Enhanced a = ce::enhance_texture("wall", indexed, 16, 16, palette, cpu);
  const ce::Enhanced b = ce::enhance_texture("wall", indexed, 16, 16, palette, on_gpu);
  CHECK(a.albedo.width == b.albedo.width && a.albedo.rgba.size() == b.albedo.rgba.size());
  int worst = 0;
  for (size_t i = 0; i < a.albedo.rgba.size() && i < b.albedo.rgba.size(); i++) {
    worst = std::max(worst, std::abs(int(a.albedo.rgba[i]) - int(b.albedo.rgba[i])));
  }
  CHECK_MSG(worst <= 1, std::to_string(worst).c_str());
}
