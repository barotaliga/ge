// The pictures a map is built with (library.hpp).
//
// ge:summary Defines the texture library: packs and loose maps read, pictures kept by name
// ge:keywords map-compiler texture library pak bsp pictures miptex
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include "brush/compile/library.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "brush/pictures.hpp"
#include "vfs/pak.hpp"

namespace ge::brush::mapc {

namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool ends_with(const std::string& s, const char* e) {
  const std::string x = e;
  return s.size() >= x.size() && s.compare(s.size() - x.size(), x.size(), x) == 0;
}

void take(std::span<const uint8_t> bsp, TextureLibrary* lib) {
  if (!carries_pictures(bsp)) return;
  ++lib->maps;
  for (const MapPicture& p : read_map_pictures(bsp)) {
    if (p.width == 0 || p.height == 0 || p.level[0] == nullptr) continue;
    const std::string key = lower(p.name);
    if (lib->by_name.count(key)) continue;
    TextureImage img;
    img.width = p.width;
    img.height = p.height;
    for (uint32_t k = 0; k < 4; ++k) {
      const size_t n = size_t(p.width >> k) * size_t(p.height >> k);
      img.levels[k].assign(p.level[k], p.level[k] + n);
    }
    lib->by_name.emplace(key, std::move(img));
  }
}

bool read_file(const fs::path& p, std::vector<uint8_t>* out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

}  // namespace

bool load_texture_library(const std::string& folder, TextureLibrary* out, std::string* error) {
  std::error_code ec;
  std::vector<fs::path> files;
  if (fs::is_regular_file(folder, ec)) {
    files.emplace_back(folder);
  } else {
    for (fs::recursive_directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
      if (it.depth() > 2) {
        it.disable_recursion_pending();
        continue;
      }
      if (it->is_regular_file(ec)) files.push_back(it->path());
    }
  }
  if (ec && files.empty()) {
    *error = "cannot read " + folder + ": " + ec.message();
    return false;
  }
  // Packs first (the game's own maps), then loose maps (a built map's own patterns never win).
  std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
    const bool pa = lower(a.extension().string()) == ".pak";
    const bool pb = lower(b.extension().string()) == ".pak";
    return pa != pb ? pa : a < b;
  });
  for (const fs::path& f : files) {
    const std::string name = lower(f.filename().string());
    std::vector<uint8_t> bytes;
    if (ends_with(name, ".bsp")) {
      if (read_file(f, &bytes)) take(bytes, out);
    } else if (ends_with(name, ".pak")) {
      if (!read_file(f, &bytes)) continue;
      vfs::Pak pak;
      std::string err;
      if (!vfs::read_pak(bytes, &pak, &err)) continue;
      for (const vfs::PakEntry& e : pak.entries) {
        if (ends_with(lower(e.path), ".bsp")) take(vfs::pak_file(bytes, e), out);
      }
    }
  }
  if (out->maps == 0) {
    *error = "no maps with pictures in " + folder;
    return false;
  }
  return true;
}

TextureFinder finder_of(const TextureLibrary& lib) {
  return [&lib](const std::string& name, TextureImage* out) {
    const auto it = lib.by_name.find(lower(name));
    if (it == lib.by_name.end()) return false;
    *out = it->second;
    return true;
  };
}

}  // namespace ge::brush::mapc
