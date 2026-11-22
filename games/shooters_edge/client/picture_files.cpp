// Pictures read by their file type, and the crosshair picture (picture_files.hpp).
//
// ge:summary Pictures by file type and the crosshair picture under crosshairs/
// ge:keywords decode_picture read_crosshair_image add_crosshair_reader
// ge:skill ui
// ge:test tests/shooters_edge_picture_files_test.cpp
#include "shooters_edge/client/picture_files.hpp"

#include <array>
#include <utility>
#include <vector>

#include "assets/pcx.hpp"
#include "assets/tga.hpp"
#include "capture/jpeg.hpp"
#include "core/output.hpp"

namespace shooters_edge::client {

namespace {

bool ends_with(std::string_view s, std::string_view tail) {
  if (s.size() < tail.size()) return false;
  for (size_t i = 0; i < tail.size(); ++i) {
    char c = s[s.size() - tail.size() + i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != tail[i]) return false;
  }
  return true;
}

}  // namespace

std::optional<ge::assets::Image> decode_picture(std::span<const uint8_t> bytes,
                                                std::string_view path,
                                                std::span<const uint8_t> palette, std::string* why,
                                                int clear) {
  std::string error;
  std::optional<ge::assets::Image> img;
  if (ends_with(path, ".tga")) {
    img = ge::assets::decode_tga(bytes, &error);
  } else if (ends_with(path, ".png")) {
    img = ge::assets::decode_png(bytes);
  } else if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) {
    if (std::optional<ge::lab::Picture> p = ge::capture::decode_jpeg(bytes, error))
      img = ge::assets::Image{p->width, p->height, std::move(p->rgba)};
  } else {
    ge::assets::IndexedImage indexed;
    if (ge::assets::decode_pcx(bytes, path, &indexed, &error)) {
      std::vector<uint8_t> rgba = ge::assets::pcx_rgba(
          indexed, indexed.palette.empty() ? palette : std::span<const uint8_t>{});
      if (clear >= 0)
        for (size_t i = 0; i < indexed.indices.size(); ++i)
          if (indexed.indices[i] == clear) rgba[i * 4 + 3] = 0;
      img = ge::assets::Image{indexed.width, indexed.height, std::move(rgba)};
    }
  }
  if (img && (img->width <= 0 || img->height <= 0)) img.reset();
  if (!img && why != nullptr) *why = error.empty() ? "not a picture it can read" : error;
  return img;
}

namespace {

// `folder/<name>` as a .tga, .png, .jpg or .pcx (`what` names it in the console's line, `instead`
// says what is drawn when it is not there).
bool read_named_picture(const SearchPath& files, std::string_view folder, std::string_view name,
                        std::string_view what, std::string_view instead, ge::ui::Image* out) {
  std::string stem(name);
  for (const std::string_view ext : {".tga", ".png", ".jpg", ".jpeg", ".pcx"})
    if (ends_with(stem, ext)) {
      stem.resize(stem.size() - ext.size());
      break;
    }
  const bool climbs = stem.empty() || stem.front() == '/' || stem.front() == '\\' ||
                      stem.find("..") != std::string::npos || stem.find(':') != std::string::npos;
  if (!climbs) {
    for (const std::string_view ext : {".tga", ".png", ".jpg", ".pcx"}) {
      const std::string path = std::string(folder) + "/" + stem + std::string(ext);
      std::vector<uint8_t> store;
      const std::span<const uint8_t> bytes = find_file(files, path, store);
      if (bytes.empty()) continue;
      std::string why;
      std::optional<ge::assets::Image> img = decode_picture(bytes, path, {}, &why, 255);
      if (!img) {
        std::string line = "Couldn't read the " + std::string(what) + " " + path;
        line.append(" (").append(why).append("); ").append(instead).append(" is drawn.");
        ge::core::output::status(line);
        return false;
      }
      out->width = img->width;
      out->height = img->height;
      out->rgba = std::move(img->rgba);
      return true;
    }
  }
  ge::core::output::status("Couldn't find the " + std::string(what) + " " + std::string(name) +
                           " in the " + std::string(folder) + " folder; " + std::string(instead) +
                           " is drawn.");
  return false;
}

}  // namespace

bool read_crosshair_image(const SearchPath& files, std::string_view name, ge::ui::Image* out) {
  return read_named_picture(files, "crosshairs", name, "crosshair picture", "the crosshair's mark",
                            out);
}

bool read_console_back(const SearchPath& files, std::string_view name, ge::ui::Image* out) {
  return read_named_picture(files, "pics", name, "console picture", "the console's own picture",
                            out);
}

void add_crosshair_reader(ge::hud::CrosshairSettings& s, const SearchPath& files) {
  s.read_image = [&files](std::string_view name, ge::ui::Image* out) {
    return read_crosshair_image(files, name, out);
  };
}

}  // namespace shooters_edge::client
