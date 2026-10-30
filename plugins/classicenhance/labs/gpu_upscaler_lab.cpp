// LAB `gpu-upscaler`: the trained texture upscaler on the GPU against the same network on the CPU:
// how much faster it is, and that it draws the same picture. Each texture is a sheet: the source's
// texels, the network's albedo from the CPU, from the GPU, and their difference × 32. The
// numbers: each device's seconds, the speed-up, and the largest difference in 8-bit steps.
//
// By default it runs the small seeded network (upscaler.hpp) on the lab's painted fixtures, so it
// needs no download; `--weights=<file>` runs the trained one (the classicenhance tool fetches it
// into the cache's weights/ folder). `--source=<path>` runs on the classic pack found there;
// those pictures are derived from the pack and stay on this machine: never attach them to a PR
// or publish them. Every picture and every time is measured, never in the digest: each adapter
// sums in its own order, and a machine with no adapter draws the CPU column alone.
//
// ge:summary Lab: the texture upscaler on the GPU beside the CPU: the speed-up and the difference
// ge:keywords lab gpu upscaler network speed-up timing cpu gpu difference texture compute
// ge:skill classicenhance labs gpu
// ge:test tests/classicenhance_upscaler_gpu_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "classicenhance/app/upscaler_gpu.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/labs/fixtures.hpp"
#include "classicenhance/pipeline.hpp"
#include "classicenhance/source.hpp"
#include "classicenhance/texture.hpp"
#include "classicenhance/upscaler.hpp"
#include "gpu/device.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace {

namespace ce = ge::classicenhance;
using ge::lab::Picture;

struct Tex {
  std::string name;
  int w, h;
  std::vector<uint8_t> indexed;
};

Picture pic(const ge::assets::Image& img) { return {img.width, img.height, img.rgba}; }

double since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

void run(ge::lab::Lab& lab) {
  ce::Upscaler net;
  std::string error;
  const std::string weights = lab.arg("weights");
  if (weights.empty()) {
    if (!ce::load_upscaler(ce::seeded_upscaler_weights(7), net, error)) {
      lab.fail(error, "next: the seeded network is the plugin's own; fix load_upscaler.");
      return;
    }
  } else if (!ce::open_upscaler(weights, net, nullptr, error)) {
    lab.fail(error,
             "next: give --weights the upscaler's weights file (the classicenhance tool "
             "fetches it into the cache's weights/ folder).");
    return;
  }
  ce::Palette palette;
  std::vector<Tex> textures;
  const std::string path = lab.arg("source");
  const size_t limit = size_t(std::max(1.0, lab.arg_num("limit")));
  if (path.empty()) {
    palette = ce::labs::fixture_palette();
    for (auto& f : ce::labs::fixture_textures(palette)) {
      if (textures.size() < limit) textures.push_back({f.name, f.width, f.height, f.indexed});
    }
  } else {
    ce::Source src;
    std::vector<ce::PackTexture> all;
    if (!ce::open_source(path, &src, &error) ||
        !ce::pack_textures(src.content(), &palette, &all, &error)) {
      lab.fail(error, "next: give --source a release zip, an installed folder or its pack.");
      return;
    }
    for (auto& t : all) {
      if (textures.size() < limit) textures.push_back({t.name, t.width, t.height, t.indexed});
    }
    lab.note(
        "local run on the classic pack: these pictures are derived from it; keep them on "
        "this machine");
  }

  auto device = ge::gpu::create_device({.label = "gpu-upscaler"});
  if (!device) {
    const char* req = std::getenv("GE_GPU_REQUIRE");
    if (req != nullptr && std::strcmp(req, "1") == 0) {
      lab.fail(
          "no GPU device, and GE_GPU_REQUIRE=1",
          "next: install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers); skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the CPU's run alone.");
  }
  std::unique_ptr<ce::GpuUpscaler> gpu;
  if (device) {
    gpu = ce::GpuUpscaler::create(*device, net, error);
    if (!gpu) {
      lab.fail(error, "next: read the error lines above; skill: gpu.");
      return;
    }
    lab.note("adapter: " + ge::gpu::adapter_line(*device));
  }

  ce::EnhanceOptions on_cpu;
  on_cpu.network = &net;
  on_cpu.scale = net.scale;
  ce::EnhanceOptions on_gpu = on_cpu;
  on_gpu.device = gpu.get();
  double cpu_s = 0, gpu_s = 0;
  int worst = 0;
  for (const Tex& t : textures) {
    auto t0 = std::chrono::steady_clock::now();
    const ce::Enhanced a = ce::enhance_texture(t.name, t.indexed, t.w, t.h, palette, on_cpu);
    cpu_s += since(t0);
    std::vector<std::pair<std::string, Picture>> cols;
    const auto src = ce::to_rgba(t.indexed, t.w, t.h, palette);
    cols.push_back({"SOURCE", ge::lab::resized({src.width, src.height, src.rgba}, a.albedo.width,
                                               a.albedo.height)});
    cols.push_back({"CPU", pic(a.albedo)});
    if (gpu) {
      t0 = std::chrono::steady_clock::now();
      const ce::Enhanced b = ce::enhance_texture(t.name, t.indexed, t.w, t.h, palette, on_gpu);
      gpu_s += since(t0);
      Picture diff = pic(b.albedo);
      for (size_t i = 0; i < diff.rgba.size(); i++) {
        if (i % 4 == 3) continue;
        const int d = std::abs(int(a.albedo.rgba[i]) - int(b.albedo.rgba[i]));
        worst = std::max(worst, d);
        diff.rgba[i] = uint8_t(std::min(255, d * 32));
      }
      cols.push_back({"GPU", pic(b.albedo)});
      cols.push_back({"DIFFERENCE X32", diff});
    }
    const int cw = a.albedo.width, ch = a.albedo.height, gap = 6, label = 14;
    Picture sheet =
        ge::lab::create_picture(int(cols.size()) * (cw + gap) - gap, ch + label, {24, 24, 28});
    for (size_t i = 0; i < cols.size(); i++) {
      const int x = int(i) * (cw + gap);
      ge::lab::draw_text(sheet, x + 2, 3, cols[i].first, {230, 230, 230});
      ge::lab::blit(sheet, cols[i].second, x, label);
    }
    lab.shot({ce::safe_name(t.name),
              t.name + " UPSCALED ON CPU AND GPU",
              "Texture '" + t.name + "' upscaled " + std::to_string(net.scale) + "x by the " +
                  (weights.empty() ? "small seeded" : "trained") +
                  " network: the source's texels, the CPU's albedo, the GPU's, and their "
                  "difference times 32.",
              "the CPU and GPU pictures the same; the difference black (a step of 1 shows as a "
              "faint speck)",
              {{"network", weights.empty() ? "seeded" : "trained"},
               {"source", path.empty() ? "fixtures" : "local pack"}}},
             sheet, ge::lab::Det::measured);
  }
  lab.metric("textures", double(textures.size()), "count");
  lab.metric("cpu-seconds", cpu_s, "s", ge::lab::Det::measured);
  if (gpu) {
    lab.metric("gpu-seconds", gpu_s, "s", ge::lab::Det::measured);
    lab.metric("gpu-speed-up", cpu_s / std::max(1e-9, gpu_s), "x", ge::lab::Det::measured);
    lab.metric("largest-difference", double(worst), "8-bit steps", ge::lab::Det::measured);
  }
}

const ge::lab::LabSpec kSpec{
    "gpu-upscaler",
    ge::lab::LabKind::bench,
    "plugin:classicenhance",
    "plugins/classicenhance/app/upscaler_gpu",
    "classicenhance",
    "How much faster is the texture upscaler on the GPU than on the CPU, and does it draw the same "
    "picture?",
    {
        {"weights", "", "the trained upscaler's weights file; empty: the small seeded network"},
        {"source", "", "the classic pack (zip, folder or pack); empty: the lab's fixtures"},
        {"limit", "6", "how many textures (in the fixtures' or the pack's order)"},
    },
    "--limit=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
