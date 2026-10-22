// LAB `gpu-capture`: the headless GPU capture every picture lab of the renderer will stand on.
// It makes a device (the software adapter in the GPU-less container and on CI), draws the GPU
// test scene (engine/gpu/test_scene.hpp) into an offscreen target, reads it back and hands it
// to `Lab::shot` as a measured picture: each adapter rasterises in its own way, so the pixels
// stay out of the digest. On the pinned adapter (Mesa's lavapipe) at the golden's size it holds
// the capture to tests/golden/gpu_test_scene.ppm, the same tolerance as
// tests/gpu_capture_test.cpp, and fails when it is off. Without an adapter it notes that and
// draws a placeholder, so the digest is the same on every row; GE_GPU_REQUIRE=1 fails instead.
// Built only with GE_GPU.
//
// ge:summary Lab: the GPU test scene captured headless, read back and held to its golden
// ge:keywords lab gpu capture headless readback screenshot golden-image adapter lavapipe
// ge:keywords software-adapter measured-picture timestamp
// ge:skill gpu labs
// ge:test tests/gpu_capture_test.cpp
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/target.hpp"
#include "gpu/test_scene.hpp"
#include "gpu/timer.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace {

using ge::lab::Det;

// The golden image's size and tolerance (tests/gpu_capture_test.cpp holds the same).
constexpr int kGoldenW = 96;
constexpr int kGoldenH = 64;
constexpr int kTolerance = 2;
constexpr double kShare = 0.005;
constexpr const char* kGolden = "tests/golden/gpu_test_scene.ppm";

bool env_on(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool on = _dupenv_s(&v, &n, name) == 0 && v && std::strcmp(v, "1") == 0;
  std::free(v);
  return on;
#else
  const char* v = std::getenv(name);
  return v && std::strcmp(v, "1") == 0;
#endif
}

// A binary PPM (P6, 8 bits) as an opaque picture; an empty one when it cannot be read.
ge::lab::Picture read_ppm(const char* path) {
  std::ifstream f(path, std::ios::binary);
  std::string magic;
  int w = 0, h = 0, max = 0;
  if (!(f >> magic >> w >> h >> max) || magic != "P6" || max != 255 || w <= 0 || h <= 0) {
    return {};
  }
  f.get();
  ge::lab::Picture p = ge::lab::create_picture(w, h, {0, 0, 0});
  for (size_t i = 0; i < p.rgba.size(); i += 4) {
    if (!f.read(reinterpret_cast<char*>(&p.rgba[i]), 3)) return {};
  }
  return p;
}

std::string ms(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.3f", v);
  return b;
}

void run(ge::lab::Lab& lab) {
  const int width = static_cast<int>(lab.arg_num("width"));
  const int height = static_cast<int>(lab.arg_num("height"));
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
    lab.fail("--width and --height must be 1..4096",
             "next: rerun with a size such as --width=320.");
    return;
  }
  ge::lab::Shot shot{
      "test-scene",
      "GPU TEST SCENE, HEADLESS",
      "The GPU test scene drawn offscreen and read back: a sky gradient, deep blue at the top "
      "and pale at the bottom, and one triangle with a red (bottom left), a green (bottom "
      "right) and a blue (top) corner.",
      "smooth colour across the triangle, straight edges, no black or torn rows (a readback "
      "with the wrong row pitch shears the picture)",
      {{"size", std::to_string(width) + "x" + std::to_string(height)}}};
  lab.metric("width", width, "px");
  lab.metric("height", height, "px");

  auto device = ge::gpu::create_device({.label = "gpu-capture", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    // The same deterministic metrics as a capture, so the digest is the same on every row.
    lab.metric("gpu-errors", 0, "count");
    lab.metric("pipelines-after-load", 0, "count");
    lab.note("No GPU adapter on this machine: the picture is a placeholder.");
    ge::lab::Picture none = ge::lab::create_picture(width, height, {40, 40, 48});
    ge::lab::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
    lab.shot(std::move(shot), none, Det::measured);
    return;
  }
  const auto& r = device->report;
  lab.note("adapter: " + r.name + " / " + r.driver + " / " + r.backend +
           (r.software ? " (software)" : ""));
  shot.inputs.push_back({"adapter", r.name});

  const auto start = std::chrono::steady_clock::now();
  ge::gpu::PipelineSet manifest;
  const uint32_t scene =
      ge::gpu::add_test_scene(*device, manifest, wgpu::TextureFormat::RGBA8Unorm);
  if (!manifest.finish(*device)) {
    lab.fail("the test scene's pipeline did not build", "Read the gpu: lines above. Skill: gpu.");
    return;
  }
  ge::gpu::end_load(*device);
  ge::gpu::PassTimer timer;
  const bool timed = timer.init(*device, "gpu-capture.timer", 1);
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "gpu-capture.final", static_cast<uint32_t>(width), static_cast<uint32_t>(height));
  wgpu::CommandEncoder encoder = device->device.CreateCommandEncoder();
  ge::gpu::draw_test_scene(encoder, manifest.render(scene), target.view,
                           timed ? timer.writes(0) : nullptr);
  if (timed) timer.resolve(encoder);
  wgpu::CommandBuffer commands = encoder.Finish();
  device->queue.Submit(1, &commands);
  if (timed) timer.request(*device);
  const ge::gpu::Image img = ge::gpu::read_now(*device, target);
  if (timed) timer.wait(*device);
  const double wall =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

  lab.metric("capture-ms", wall, "ms", Det::measured);
  if (timed) lab.metric("gpu-pass-ms", timer.ms(0), "ms", Det::measured);
  lab.metric("gpu-errors", device->errors.load(), "count");
  lab.metric("pipelines-after-load", device->pipelines_after_load, "count");
  if (device->errors.load() != 0) {
    lab.fail(std::to_string(device->errors.load()) + " GPU errors during the capture",
             "Read the gpu: lines above. Skill: gpu.");
  }
  if (img.width != static_cast<uint32_t>(width) || img.height != static_cast<uint32_t>(height) ||
      img.rgba.size() != size_t{img.width} * img.height * 4) {
    lab.fail("the readback came back " + std::to_string(img.width) + "x" +
                 std::to_string(img.height) + ", not the target's size",
             "Check engine/gpu/target.cpp's Readback. Skill: gpu.");
    return;
  }
  ge::lab::Picture picture{width, height, img.rgba};

  const bool pinned = r.name.rfind("llvmpipe", 0) == 0;
  if (pinned && width == kGoldenW && height == kGoldenH) {
    const ge::lab::Picture golden = read_ppm(kGolden);
    if (golden.rgba.size() != picture.rgba.size()) {
      lab.fail(std::string("could not read ") + kGolden + " at 96x64",
               "Run the lab from the repository's root. Skill: gpu.");
    } else {
      int worst = 0;
      size_t off = 0;
      for (size_t i = 0; i < golden.rgba.size(); i += 4) {
        int d = 0;
        for (size_t c = 0; c < 3; c++) {
          const int e = std::abs(int{picture.rgba[i + c]} - int{golden.rgba[i + c]});
          d = e > d ? e : d;
        }
        worst = d > worst ? d : worst;
        off += d > kTolerance;
      }
      const double share = static_cast<double>(off) * 4 / static_cast<double>(golden.rgba.size());
      lab.metric("golden.max-delta", worst, "levels", Det::measured);
      lab.metric("golden.off-share", share * 100, "%", Det::measured);
      if (share > kShare) {
        lab.fail("the capture is off its golden in " + ms(share * 100) + "% of the pixels (max " +
                     std::to_string(worst) + " levels)",
                 "Open previews/gpu-capture/test-scene.png beside the golden; if the change was "
                 "meant, rewrite it with GE_UPDATE_GOLDEN=1 ctest -R gpu_capture. Skill: gpu.");
      }
    }
  } else {
    lab.note(pinned ? "Golden comparison only at --width=96 --height=64."
                    : "Golden comparison skipped: the adapter is not the pinned lavapipe.");
  }
  lab.shot(std::move(shot), picture, Det::measured);
}

const ge::lab::LabSpec kSpec{
    "gpu-capture",
    ge::lab::LabKind::picture,
    "engine",
    "engine/gpu (device, target, readback, timer, test scene)",
    "gpu",
    "Does the GPU draw headless and read its frame back right, and on the pinned adapter, "
    "does the frame match its golden?",
    {
        {"width", "96", "the capture's width, px (the golden is 96x64)"},
        {"height", "64", "the capture's height, px"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
