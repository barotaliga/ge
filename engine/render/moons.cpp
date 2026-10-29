// The ten kinds of moon and the sets of them (render/moons.hpp says what each number is). The
// numbers are the moons' published ones: mean radii and semi-axes, orbits' semi-major axes less
// their planets' equatorial radii, geometric albedos, sidereal periods, inclinations to their
// planets' equators and eccentricities.
//
// ge:summary The ten real moons' numbers and the words that name a set of them
// ge:keywords render moons moon-kinds catalogue moon_set_named phobos deimos io europa ganymede
// ge:keywords callisto titan enceladus miranda triton
// ge:skill render
// ge:test tests/render_moons_test.cpp
#include "render/moons.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace ge::render {

namespace {

const std::array<MoonKind, 10>& catalogue() {
  // name, caption, radius, semi-axes, height, albedo, tint, period, inclination, eccentricity,
  // epoch, Lambert's share, dents, haze.
  static const std::array<MoonKind, 10> kinds = {{
      {"phobos",
       "Mars's inner moon: a dark grooved lump a third of the Moon's size, rising in "
       "the west twice a day",
       11.08,
       {13.0, 11.4, 9.1},
       5986,
       0.071,
       {1.10, 0.98, 0.88},
       0.31891,
       1.09,
       0.0151,
       35,
       0.05,
       0.16,
       0},
      {"deimos",
       "Mars's outer moon: a dark smooth lump seen as a bright star",
       6.27,
       {7.8, 6.0, 5.1},
       20074,
       0.068,
       {1.08, 0.99, 0.90},
       1.26244,
       0.93,
       0.00033,
       160,
       0.05,
       0.10,
       0},
      {"io",
       "Jupiter's volcanic moon: sulphur yellow, a little larger than the Moon",
       1821.6,
       {1829.4, 1819.4, 1815.7},
       350208,
       0.63,
       {1.0, 1.0, 1.0},
       1.769138,
       0.05,
       0.0041,
       280,
       0.35,
       0,
       0},
      {"europa",
       "Jupiter's ice moon: white, cracked brown",
       1560.8,
       {1560.8, 1560.8, 1560.8},
       599542,
       0.67,
       {1.06, 1.00, 0.92},
       3.551181,
       0.47,
       0.0090,
       95,
       0.55,
       0,
       0},
      {"ganymede",
       "Jupiter's largest moon: dark and bright grooved terrain, rayed craters",
       2634.1,
       {2634.1, 2634.1, 2634.1},
       998920,
       0.43,
       {1.0, 1.0, 1.0},
       7.154553,
       0.20,
       0.0013,
       200,
       0.35,
       0,
       0},
      {"callisto",
       "Jupiter's outer moon: dark, crowded with bright craters",
       2410.3,
       {2410.3, 2410.3, 2410.3},
       1811217,
       0.22,
       {1.07, 1.00, 0.90},
       16.689017,
       0.19,
       0.0074,
       20,
       0.10,
       0,
       0},
      {"titan",
       "Saturn's great moon: an orange haze hiding its ground",
       2574.7,
       {2574.7, 2574.7, 2574.7},
       1161602,
       0.22,
       {1.40, 0.97, 0.45},
       15.945,
       0.35,
       0.0288,
       130,
       0.80,
       0,
       0.12},
      {"enceladus",
       "Saturn's snowball: whiter than snow, blue stripes round its south pole",
       252.1,
       {256.6, 251.4, 248.3},
       177680,
       1.375,
       {0.97, 1.00, 1.05},
       1.370218,
       0.02,
       0.0047,
       320,
       0.60,
       0,
       0},
      {"miranda",
       "Uranus's patchwork moon: grey, ringed by giant racetrack ridges",
       235.8,
       {240.4, 234.2, 232.9},
       103831,
       0.32,
       {0.99, 1.00, 1.01},
       1.413479,
       4.23,
       0.0013,
       60,
       0.35,
       0.03,
       0},
      {"triton",
       "Neptune's backward moon: a pink polar cap, going round the wrong way",
       1353.4,
       {1353.4, 1353.4, 1353.4},
       329995,
       0.76,
       {1.07, 0.98, 0.94},
       5.876854,
       156.89,
       0.000016,
       240,
       0.50,
       0,
       0},
  }};
  return kinds;
}

bool same_word(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

// A number after a ':' (empty: `fallback`); false when it does not read.
bool number(std::string_view text, double fallback, double& out) {
  if (text.empty()) {
    out = fallback;
    return true;
  }
  const std::string s(text);
  char* end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end != s.c_str() && *end == '\0' && std::isfinite(out);
}

}  // namespace

std::span<const MoonKind> moon_kinds() { return catalogue(); }

int moon_kind_named(std::string_view name) {
  const auto& kinds = catalogue();
  for (size_t i = 0; i < kinds.size(); ++i) {
    if (same_word(kinds[i].name, name)) return static_cast<int>(i);
  }
  return -1;
}

std::string moon_kind_list() {
  std::string line;
  for (const MoonKind& k : catalogue()) {
    if (!line.empty()) line += ", ";
    line += k.name;
  }
  return line;
}

double moon_kind_radius(const MoonKind& kind) { return std::asin(kind.radius_km / kind.height_km); }

bool moon_set_named(std::string_view words, MoonSet& out, std::string& problem) {
  out = MoonSet{};
  out.earth = false;
  problem.clear();
  size_t i = 0;
  while (i < words.size()) {
    while (i < words.size() && (words[i] == ' ' || words[i] == ',' || words[i] == '+')) ++i;
    size_t j = i;
    while (j < words.size() && words[j] != ' ' && words[j] != ',' && words[j] != '+') ++j;
    const std::string_view word = words.substr(i, j - i);
    i = j;
    if (word.empty()) continue;
    if (same_word(word, "none")) continue;
    if (same_word(word, "moon") || same_word(word, "earth")) {
      out.earth = true;
      continue;
    }
    // name[:size[:spin hours]]
    const size_t c1 = word.find(':');
    const std::string_view name = word.substr(0, c1);
    std::string_view size_text, spin_text;
    if (c1 != std::string_view::npos) {
      const std::string_view rest = word.substr(c1 + 1);
      const size_t c2 = rest.find(':');
      size_text = rest.substr(0, c2);
      if (c2 != std::string_view::npos) spin_text = rest.substr(c2 + 1);
    }
    MoonPick pick;
    pick.kind = moon_kind_named(name);
    if (pick.kind < 0) {
      problem = "'" + std::string(name) + "' names no moon (the kinds: moon, none, " +
                moon_kind_list() + ")";
      return false;
    }
    if (!number(size_text, 1, pick.size) || pick.size <= 0 ||
        !number(spin_text, -1, pick.spin_hours)) {
      problem = "'" + std::string(word) +
                "' does not read as name:size:spin (a size over 0, a spin in hours)";
      return false;
    }
    if (out.count == kMaxMoons) {
      problem = "more than " + std::to_string(kMaxMoons) + " moons besides the Earth's";
      return false;
    }
    out.moons[out.count++] = pick;
  }
  return true;
}

}  // namespace ge::render
