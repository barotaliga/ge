// The definitions behind dem_tiles.hpp: a little-endian tiled TIFF read by its tags.
//
// ge:summary Lab tooling: the GeoTIFF tile reader (tags, blocks, the floating-point predictor)
// ge:keywords elevation-model geotiff tiff predictor inflate curl
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include "mountain/labs/dem_tiles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>

#include "assets/inflate.hpp"
#include "mountain/labs/fetch.hpp"

namespace ge::mountain::labs {

namespace fs = std::filesystem;

constexpr const char* kBucket = "https://copernicus-dem-30m.s3.amazonaws.com";

struct DemTile {
  std::vector<uint8_t> file;
  int width = 0;
  int height = 0;
  int tw = 0;
  int th = 0;
  std::vector<uint64_t> offsets;
  std::vector<uint64_t> counts;
  std::vector<std::vector<float>> blocks;  // inflated on first read; empty until then
  bool sea = false;

  bool open(std::string& error) {
    if (file.size() < 8 || file[0] != 'I' || file[1] != 'I') {
      error = "not a little-endian TIFF";
      return false;
    }
    const auto u16 = [&](size_t o) { return uint32_t(file[o]) | (uint32_t(file[o + 1]) << 8); };
    const auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
    const size_t ifd = u32(4);
    const uint32_t n = u16(ifd);
    uint32_t compression = 0;
    uint32_t predictor = 1;
    for (uint32_t i = 0; i < n; i++) {
      const size_t e = ifd + 2 + size_t(i) * 12;
      const uint32_t tag = u16(e);
      const uint32_t type = u16(e + 2);
      const uint32_t count = u32(e + 4);
      const size_t size = type == 3 ? 2 : type == 16 ? 8 : 4;
      const size_t at = count * size <= 4 ? e + 8 : u32(e + 8);
      std::vector<uint64_t> v(count);
      for (uint32_t k = 0; k < count; k++) {
        const size_t o = at + size_t(k) * size;
        v[k] = size == 2 ? u16(o) : size == 8 ? (uint64_t(u32(o + 4)) << 32) | u32(o) : u32(o);
      }
      if (v.empty()) continue;
      if (tag == 256) width = int(v[0]);
      if (tag == 257) height = int(v[0]);
      if (tag == 259) compression = uint32_t(v[0]);
      if (tag == 317) predictor = uint32_t(v[0]);
      if (tag == 322) tw = int(v[0]);
      if (tag == 323) th = int(v[0]);
      if (tag == 324) offsets = v;
      if (tag == 325) counts = v;
    }
    if (compression != 8 || predictor != 3 || tw <= 0 || th <= 0 ||
        offsets.size() != counts.size()) {
      error = "not a tiled, deflated TIFF with the floating-point predictor";
      return false;
    }
    blocks.resize(offsets.size());
    return true;
  }

  const std::vector<float>* block(size_t i, std::string& error) {
    if (i >= blocks.size()) return nullptr;
    std::vector<float>& t = blocks[i];
    if (!t.empty()) return &t;
    if (offsets[i] + counts[i] > file.size()) {
      error = "a block runs past the file's end";
      return nullptr;
    }
    const auto raw = ge::assets::inflate_zlib(
        std::span<const uint8_t>(file.data() + offsets[i], size_t(counts[i])));
    const size_t row_bytes = size_t(tw) * 4;
    if (!raw || raw->size() < row_bytes * size_t(th)) {
      error = "a block does not inflate";
      return nullptr;
    }
    t.resize(size_t(tw) * size_t(th));
    std::vector<uint8_t> undiff(row_bytes);
    for (size_t r = 0; r < size_t(th); r++) {
      const uint8_t* src = raw->data() + r * row_bytes;
      // Undo the byte differencing, then gather each sample's bytes out of their four planes
      // (most significant first).
      uint8_t acc = 0;
      for (size_t k = 0; k < row_bytes; k++) undiff[k] = acc = uint8_t(acc + src[k]);
      for (size_t c = 0; c < size_t(tw); c++) {
        const uint32_t bits =
            (uint32_t(undiff[c]) << 24) | (uint32_t(undiff[size_t(tw) + c]) << 16) |
            (uint32_t(undiff[2 * size_t(tw) + c]) << 8) | uint32_t(undiff[3 * size_t(tw) + c]);
        float f = 0;
        std::memcpy(&f, &bits, 4);
        t[r * size_t(tw) + c] = f;
      }
    }
    return &t;
  }

  double at(int r, int c, std::string& error) {
    r = std::clamp(r, 0, height - 1);
    c = std::clamp(c, 0, width - 1);
    const int across = (width + tw - 1) / tw;
    const auto* b = block(size_t(r / th) * size_t(across) + size_t(c / tw), error);
    if (!b) return 0;
    return (*b)[size_t(r % th) * size_t(tw) + size_t(c % tw)];
  }
};

Dem::Dem(std::string cache) : cache_(std::move(cache)) {}
Dem::~Dem() = default;

std::string Dem::tile_name(double lat, double lon) {
  const int la = int(std::floor(lat));
  const int lo = int(std::floor(lon));
  char b[32];
  std::snprintf(b, sizeof b, "%c%02d_00_%c%03d_00", la >= 0 ? 'N' : 'S', std::abs(la),
                lo >= 0 ? 'E' : 'W', std::abs(lo));
  return b;
}

bool Dem::kept(const std::string& name) const {
  return fs::exists(fs::path(cache_) / (name + ".tif")) ||
         fs::exists(fs::path(cache_) / (name + ".none"));
}

bool Dem::fetch(const std::string& name, std::string& error) {
  if (kept(name)) return true;
  const std::string key = "Copernicus_DSM_COG_10_" + name + "_DEM";
  const std::string url = std::string(kBucket) + "/" + key + "/" + key + ".tif";
  const std::string http = fetch_to(url, (fs::path(cache_) / (name + ".tif")).string());
  if (http == "404" || http == "403") {
    // A tile over open sea is not published.
    const std::ofstream sea(fs::path(cache_) / (name + ".none"));
    return true;
  }
  if (http != "200") {
    error = url + ": HTTP " + (http.empty() ? std::string("none (is curl installed?)") : http);
    return false;
  }
  return true;
}

DemTile* Dem::tile(const std::string& name, std::string& error) {
  if (auto it = tiles_.find(name); it != tiles_.end()) return it->second.get();
  auto t = std::make_unique<DemTile>();
  const fs::path tif = fs::path(cache_) / (name + ".tif");
  if (fs::exists(tif)) {
    std::ifstream in(tif, std::ios::binary);
    t->file.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (!t->open(error)) {
      error = tif.string() + ": " + error;
      return nullptr;
    }
  } else if (fs::exists(fs::path(cache_) / (name + ".none"))) {
    t->sea = true;
  } else {
    error = tif.string() + " is missing: run the bake with --fetch";
    return nullptr;
  }
  return tiles_.emplace(name, std::move(t)).first->second.get();
}

double Dem::height_at(double lat, double lon, std::string& error) {
  DemTile* t = tile(tile_name(lat, lon), error);
  if (!t || t->sea) return 0;
  const double la = std::floor(lat);
  const double lo = std::floor(lon);
  const double r = (la + 1 - lat) * t->height - 0.5;
  const double c = (lon - lo) * t->width - 0.5;
  const int r0 = int(std::floor(r));
  const int c0 = int(std::floor(c));
  const double fr = r - r0;
  const double fc = c - c0;
  const double a = t->at(r0, c0, error) * (1 - fc) + t->at(r0, c0 + 1, error) * fc;
  const double b = t->at(r0 + 1, c0, error) * (1 - fc) + t->at(r0 + 1, c0 + 1, error) * fc;
  return a * (1 - fr) + b * fr;
}

}  // namespace ge::mountain::labs
