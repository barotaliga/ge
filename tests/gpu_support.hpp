// What the GPU and window tests share: a device or a skip, and golden images.
//
// A DEVICE OR A SKIP: `test_device` makes a device with validation on. Where none can be made
// (no adapter: a machine with no GPU driver and no software one) the test skips, unless the
// environment sets GE_GPU_REQUIRE=1, as CI's Linux x64 rows do with lavapipe installed: there
// it fails, because a skip would hide a broken device path.
//
// GOLDEN IMAGES are binary PPM files (P6: a short text header, then RGB bytes; any image
// viewer opens one) under tests/golden/. A capture matches when no channel of any pixel
// differs by more than `max_delta` except in at most `max_share` of the pixels: a software
// driver's update may move an edge pixel, never the picture. GE_UPDATE_GOLDEN=1 rewrites the
// golden from the capture instead of comparing (review the new image in the PR).
//
// ge:summary GPU test support: a validated device or a skip; golden PPM images with tolerance
// ge:keywords test gpu golden-image ppm tolerance image-compare skip ge_gpu_require
// ge:keywords ge_update_golden test_device check_golden_image pinned_adapter lavapipe
// ge:skill testing gpu
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"

namespace ge::test {

inline bool env_on(const char* name) {
  const char* v = std::getenv(name);
  return v && std::strcmp(v, "1") == 0;
}

inline std::unique_ptr<gpu::Device> test_device(gpu::DeviceOptions options = {}) {
  options.validation = 1;
  auto device = gpu::create_device(options);
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      fail(__FILE__, __LINE__,
           "no GPU device, and GE_GPU_REQUIRE=1. Install Mesa's Vulkan drivers "
           "(apt install mesa-vulkan-drivers); skill: gpu.");
    } else {
      skip("no GPU adapter on this machine (set GE_GPU_REQUIRE=1 to fail instead)");
    }
  }
  return device;
}

// Whether `device` runs on the pinned software adapter (Mesa's lavapipe, which reports itself as
// llvmpipe): the only adapter golden images are made on and compared against. Any other
// adapter's capture is checked by its spot checks alone.
inline bool pinned_adapter(const gpu::Device& device) {
  return device.report.name.rfind("llvmpipe", 0) == 0;
}

inline bool write_ppm(const std::string& path, const gpu::Image& img) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%u %u\n255\n", img.width, img.height);
  for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) std::fwrite(&img.rgba[i], 1, 3, f);
  return std::fclose(f) == 0;
}

// An RGB golden read back as RGBA (alpha 255). Empty when missing or malformed.
inline gpu::Image read_ppm(const std::string& path) {
  gpu::Image img;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return img;
  unsigned w = 0, h = 0, maxval = 0;
  if (std::fscanf(f, "P6 %u %u %u", &w, &h, &maxval) == 3 && maxval == 255 &&
      std::fgetc(f) != EOF && w > 0 && h > 0 && w < 16384 && h < 16384) {
    std::vector<uint8_t> rgb(size_t{w} * h * 3);
    if (std::fread(rgb.data(), 1, rgb.size(), f) == rgb.size()) {
      img.width = w;
      img.height = h;
      img.rgba.resize(size_t{w} * h * 4);
      for (size_t p = 0; p < size_t{w} * h; ++p) {
        std::memcpy(&img.rgba[p * 4], &rgb[p * 3], 3);
        img.rgba[p * 4 + 3] = 255;
      }
    }
  }
  std::fclose(f);
  return img;
}

struct ImageDiff {
  int max_delta = 0;  // the largest channel difference, 0..255
  double over = 0;    // the share of pixels with a channel beyond the tolerance
};

inline ImageDiff diff_images(const gpu::Image& a, const gpu::Image& b, int max_delta) {
  ImageDiff d;
  size_t over = 0;
  const size_t n = size_t{a.width} * a.height;
  for (size_t p = 0; p < n; ++p) {
    int worst = 0;
    for (int c = 0; c < 3; ++c) {
      const int delta = std::abs(int{a.rgba[p * 4 + c]} - int{b.rgba[p * 4 + c]});
      worst = worst > delta ? worst : delta;
    }
    d.max_delta = d.max_delta > worst ? d.max_delta : worst;
    if (worst > max_delta) ++over;
  }
  d.over = n ? static_cast<double>(over) / static_cast<double>(n) : 0;
  return d;
}

// Compare `got` with tests/golden/<name>.ppm, or rewrite it under GE_UPDATE_GOLDEN=1. On a
// mismatch the capture is written beside the build as <name>.got.ppm for a look.
inline void check_golden_image(const char* name, const gpu::Image& got, int max_delta,
                               double max_share) {
  const std::string path = root_path((std::string("tests/golden/") + name + ".ppm").c_str());
  if (env_on("GE_UPDATE_GOLDEN")) {
    if (!write_ppm(path, got)) fail(__FILE__, __LINE__, "cannot write " + path);
    std::printf("golden image rewritten: %s\n", path.c_str());
    return;
  }
  const gpu::Image want = read_ppm(path);
  if (want.rgba.empty()) {
    fail(__FILE__, __LINE__,
         path +
             " is missing or not a binary PPM. Make it with GE_UPDATE_GOLDEN=1 on the "
             "pinned software adapter (lavapipe) and review it; skill: testing.");
    return;
  }
  if (want.width != got.width || want.height != got.height) {
    fail(__FILE__, __LINE__,
         std::string(name) + ": size " + std::to_string(got.width) + "x" +
             std::to_string(got.height) + ", golden " + std::to_string(want.width) + "x" +
             std::to_string(want.height));
    return;
  }
  const ImageDiff d = diff_images(got, want, max_delta);
  if (d.over > max_share) {
    const std::string seen = std::string(name) + ".got.ppm";
    write_ppm(seen, got);
    char m[256];
    std::snprintf(m, sizeof m,
                  "%s: %.2f%% of pixels differ by more than %d (worst %d; allowed %.2f%%). The "
                  "capture is %s. A changed picture is regenerated with GE_UPDATE_GOLDEN=1 and "
                  "reviewed; a driver difference is fixed by pinning the adapter; skill: gpu.",
                  name, d.over * 100, max_delta, d.max_delta, max_share * 100, seen.c_str());
    fail(__FILE__, __LINE__, m);
  }
}

}  // namespace ge::test
