// Surface classes: the name's words and the colour test.
//
// ge:summary Surface classes by a texture's name's words and its colour
// ge:keywords detail surface class texture name colour classify
// ge:skill detail
// ge:test tests/detail_clutter_test.cpp
#include "detail/surfaces.hpp"

#include <string>

namespace ge::detail {
namespace {

struct Word {
  std::string_view word;
  SurfaceClass cls;
};

// Checked in this order: the first word found wins, so the more telling words come first (a sky
// or a liquid over all; grass before the rock it may grow on; metal before a "floor").
constexpr Word kWords[] = {
    {"sky", SurfaceClass::sky},        {"water", SurfaceClass::liquid},
    {"lava", SurfaceClass::liquid},    {"slime", SurfaceClass::liquid},
    {"grass", SurfaceClass::grass},    {"lawn", SurfaceClass::grass},
    {"turf", SurfaceClass::grass},     {"leaf", SurfaceClass::grass},
    {"metal", SurfaceClass::metal},    {"steel", SurfaceClass::metal},
    {"iron", SurfaceClass::metal},     {"plate", SurfaceClass::metal},
    {"grate", SurfaceClass::metal},    {"tech", SurfaceClass::metal},
    {"rust", SurfaceClass::metal},     {"wood", SurfaceClass::wood},
    {"plank", SurfaceClass::wood},     {"crate", SurfaceClass::wood},
    {"timber", SurfaceClass::wood},    {"tile", SurfaceClass::tile},
    {"marble", SurfaceClass::tile},    {"sand", SurfaceClass::sand},
    {"dirt", SurfaceClass::dirt},      {"mud", SurfaceClass::dirt},
    {"earth", SurfaceClass::dirt},     {"ground", SurfaceClass::dirt},
    {"soil", SurfaceClass::dirt},      {"gravel", SurfaceClass::dirt},
    {"rock", SurfaceClass::stone},     {"stone", SurfaceClass::stone},
    {"brick", SurfaceClass::stone},    {"cobble", SurfaceClass::stone},
    {"flag", SurfaceClass::stone},     {"slab", SurfaceClass::stone},
    {"concrete", SurfaceClass::stone}, {"cave", SurfaceClass::stone},
    {"cliff", SurfaceClass::stone},
};

bool green(const double* c) { return c[1] > 0.12 && c[1] > c[0] * 1.12 && c[1] > c[2] * 1.15; }

}  // namespace

std::string_view surface_name(SurfaceClass c) {
  constexpr std::string_view kNames[kSurfaceClasses] = {
      "unknown", "stone", "dirt", "grass", "sand", "wood", "metal", "tile", "sky", "liquid"};
  return kNames[static_cast<int>(c)];
}

SurfaceClass classify_name(std::string_view name) {
  std::string low(name);
  for (char& ch : low) {
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  }
  for (const Word& w : kWords) {
    if (low.find(w.word) != std::string::npos) return w.cls;
  }
  return SurfaceClass::unknown;
}

SurfaceClass classify_surface(std::string_view name, const double* mean_rgb) {
  const SurfaceClass by_name = classify_name(name);
  if (mean_rgb == nullptr || !green(mean_rgb)) return by_name;
  switch (by_name) {
    case SurfaceClass::unknown:
    case SurfaceClass::stone:
    case SurfaceClass::dirt:
    case SurfaceClass::sand:
      return SurfaceClass::grass;
    default:
      return by_name;
  }
}

}  // namespace ge::detail
