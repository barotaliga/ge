// The game's files' effects thrown in place of the classic ones (world_fx.hpp § THE GAME'S
// FILES' EFFECTS): the book read from `particles/*.fx`, a burst, a trail, their lights.
//
// ge:summary The game's files' particle effects: read from particles/*.fx, thrown, lit
// ge:keywords load_effects throw_scripted trail_scripted scripted_lights particles-fx book
// ge:keywords mod-effects data-driven-particles
// ge:skill render shooter
// ge:test tests/particles_effect_test.cpp
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "render/spark.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace shooters_edge::client {

namespace {

constexpr double kUnitM = ge::brush::kUnitM;  // a map unit, m

thread_local std::vector<ge::particles::Spark> sparks;  // scratch

}  // namespace

void load_effects(const SearchPath& files, ge::particles::Book& book,
                  std::vector<std::string>& faults) {
  for (const ge::vfs::Listed& l : ge::vfs::list(files.vfs)) {
    if (!l.name.starts_with("particles/") || !l.name.ends_with(".fx")) continue;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = find_file(files, l.name, store);
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::string error;
    if (!ge::particles::parse_effects(text, l.name, book, error)) faults.push_back(error);
  }
}

bool throw_scripted(FxState& s, std::string_view name, const double p[3], const double way[3],
                    int times, double floor_y, double t, FxFrame& out) {
  const ge::particles::Effect* e = s.book != nullptr ? s.book->find(name) : nullptr;
  if (e == nullptr) return false;
  ge::particles::Throw th;
  for (int k = 0; k < 3; ++k) {
    th.at[k] = p[k];
    th.way[k] = way[k];
  }
  th.metres = kUnitM;
  th.floor_y = floor_y;
  th.times = times;
  th.seed = s.seed++;
  th.palette = s.palette;
  sparks.clear();
  std::vector<ge::particles::Glow> glows;
  ge::particles::throw_effect(*e, th, sparks, &glows);
  ge::render::add_sparks(sparks, out.births);
  for (const ge::particles::Glow& g : glows) s.glows.push_back({g, t});
  return true;
}

bool trail_scripted(FxState& s, std::string_view name, const double from[3], const double to[3],
                    FxFrame& out) {
  const ge::particles::Effect* e = s.book != nullptr ? s.book->find(name) : nullptr;
  if (e == nullptr) return false;
  ge::particles::Throw th;
  th.metres = kUnitM;
  th.seed = s.seed++;
  th.palette = s.palette;
  sparks.clear();
  ge::particles::throw_trail(*e, from, to, th, sparks);
  ge::render::add_sparks(sparks, out.births);
  return true;
}

void scripted_lights(FxState& s, double t, FxFrame& out) {
  std::erase_if(s.glows,
                [t](const LitGlow& g) { return t - g.born >= g.glow.fade_s || t < g.born; });
  for (const LitGlow& g : s.glows) {
    effects::Light l;
    const double left = 1 - (t - g.born) / g.glow.fade_s;
    for (int k = 0; k < 3; ++k) l.pos[k] = g.glow.at[k];
    l.radius = g.glow.radius_m * left;
    // The classic rule: a light's level is its reach in units over 128.
    const double level = l.radius / kUnitM / 128;
    for (int k = 0; k < 3; ++k) l.colour[k] = g.glow.rgb[k] * level;
    out.lights.push_back(l);
  }
}

}  // namespace shooters_edge::client
