// The palette's and the colour map's readers and writers (palette.hpp).
//
// ge:summary The palette's and the colour map's size-checked readers and fixture writers
// ge:keywords vfs palette colormap read write refuse
// ge:skill vfs
// ge:test tests/vfs_palette_test.cpp
#include "vfs/palette.hpp"

#include <algorithm>

namespace ge::vfs {
namespace {

bool refuse(std::string* error, const std::string& what) {
  if (error)
    *error = what + ". The file is damaged or not classic content: fetch it again (skill: vfs)";
  return false;
}

}  // namespace

bool read_palette(std::span<const uint8_t> bytes, Palette* out, std::string* error) {
  if (bytes.size() != out->rgb.size()) {
    return refuse(error, "palette: " + std::to_string(bytes.size()) + " bytes, not the " +
                             std::to_string(out->rgb.size()) + " of 256 colours");
  }
  std::copy(bytes.begin(), bytes.end(), out->rgb.begin());
  return true;
}

bool read_colormap(std::span<const uint8_t> bytes, Colormap* out, std::string* error) {
  const size_t n = out->map.size();
  if (bytes.size() != n && bytes.size() != n + 1) {
    return refuse(error, "colormap: " + std::to_string(bytes.size()) + " bytes, not the " +
                             std::to_string(n) + " of 64 levels x 256 indices (or one more)");
  }
  std::copy_n(bytes.begin(), n, out->map.begin());
  out->fullbright = bytes.size() > n ? int(bytes[n]) : -1;
  return true;
}

std::vector<uint8_t> write_palette(const Palette& palette) {
  return {palette.rgb.begin(), palette.rgb.end()};
}

std::vector<uint8_t> write_colormap(const Colormap& colormap) {
  std::vector<uint8_t> out(colormap.map.begin(), colormap.map.end());
  if (colormap.fullbright >= 0) out.push_back(uint8_t(colormap.fullbright));
  return out;
}

}  // namespace ge::vfs
