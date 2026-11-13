// A classic map's own fog (map_fog.hpp).
//
// ge:summary The worldspawn's fog and skyfog keys and the fog command's words read
// ge:keywords brush fog map_fog parse_fog fog_words worldspawn
// ge:skill brush
// ge:test tests/brush_map_fog_test.cpp
#include "brush/map_fog.hpp"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace ge::brush {

namespace {

// Every word of `words` as a number, or false when one is not.
bool numbers(std::string_view words, std::vector<double>* out) {
  std::istringstream in{std::string(words)};
  std::string word;
  while (in >> word) {
    char* end = nullptr;
    const double v = std::strtod(word.c_str(), &end);
    if (end == word.c_str() || *end != '\0') return false;
    out->push_back(v);
  }
  return true;
}

}  // namespace

bool parse_fog(std::string_view words, MapFog* out) {
  std::vector<double> v;
  if (!numbers(words, &v) || v.empty()) return false;
  MapFog f;
  f.sky_share = out->sky_share;
  if (v.size() == 1) {
    f.density = v[0];
  } else if (v.size() == 2) {
    f.density = v[0];
    f.colour[0] = f.colour[1] = f.colour[2] = v[1];
  } else if (v.size() == 3) {
    f.density = 0.05;
    for (int i = 0; i < 3; ++i) f.colour[i] = v[static_cast<size_t>(i)];
  } else {
    f.density = v[0];
    for (int i = 0; i < 3; ++i) f.colour[i] = v[static_cast<size_t>(i) + 1];
    if (v.size() >= 5) f.alpha = v[4];
    if (v.size() >= 6) f.bias = v[5];
  }
  *out = f;
  return true;
}

MapFog map_fog(const Map& map) {
  MapFog f;
  if (map.entities.empty()) return f;
  const Entity& world = map.entities[0];
  for (const char* key : {"fog", "_fog", "airfog"}) {
    const std::string& words = world.get(key);
    if (!words.empty()) {
      parse_fog(words, &f);
      break;
    }
  }
  for (const char* key : {"skyfog", "_skyfog"}) {
    const std::string& words = world.get(key);
    std::vector<double> v;
    if (!words.empty() && numbers(words, &v) && v.size() == 1) {
      f.sky_share = v[0];
      break;
    }
  }
  return f;
}

std::string fog_words(const MapFog& fog) {
  std::ostringstream o;
  o << fog.density << ' ' << fog.colour[0] << ' ' << fog.colour[1] << ' ' << fog.colour[2] << ' '
    << fog.alpha << ' ' << fog.bias;
  return o.str();
}

}  // namespace ge::brush
