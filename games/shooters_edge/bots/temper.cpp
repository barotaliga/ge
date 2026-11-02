// A fighter's temperament at work (temper.hpp): its might, and its enemies' read off their sounds.
//
// ge:summary A fighter's own might and each enemy's, believed off what it hears of him
// ge:keywords own_might foe_heard foe_might arms-of-word fresh-spawn
// ge:skill shooter bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooters_edge/bots/temper.hpp"

#include <algorithm>
#include <string_view>

namespace shooters_edge::bots {

namespace sh = ge::shooter;

namespace {

// The W_ and IT_ bits (games/shooters_edge/qc).
constexpr int32_t kWDouble = 2, kWSpiker = 4, kWHeavy = 8, kWGrenades = 16, kWRockets = 32;
constexpr int32_t kWArc = 64;
constexpr int32_t kItQuad = 4194304, kItPent = 1048576;
constexpr float kQuad = 3, kPent = 10;  // a power's multiplier on the might
constexpr double kQuadFor_s = 30;
constexpr double kMateFreshFor_s = 10;  // a teammate's place this old is still where he is
constexpr float kAtHand = 250;          // a teammate this near is with him already, units
constexpr float kFreshArms =
    1.2f;  // a fresh enemy's believed arms: he may have picked something up

// A weapon's arms by its call word (the profile's words: the fire sounds carry them).
float arms_of_word(std::string_view w) {
  if (w == "rl" || w == "lg") return 2.f;
  if (w == "sng" || w == "gl") return 1.5f;
  if (w == "ssg") return 1.3f;
  if (w == "ng") return 1.2f;
  if (w == "axe") return 0.4f;
  return 1.f;
}

}  // namespace

sh::Might own_might(const Stats& st) {
  sh::Might m;
  m.body = st.health + st.armortype * st.armor;
  float arms = 1;
  if (st.items & kWDouble && st.shells >= 2) arms = std::max(arms, 1.3f);
  if (st.items & kWSpiker && st.nails > 0) arms = std::max(arms, 1.2f);
  if (st.items & kWHeavy && st.nails >= 2) arms = std::max(arms, 1.5f);
  if (st.items & kWGrenades && st.rockets > 0) arms = std::max(arms, 1.5f);
  if (st.items & kWRockets && st.rockets > 0) arms = std::max(arms, 2.f);
  if (st.items & kWArc && st.cells > 0) arms = std::max(arms, 2.f);
  if (st.items & kItQuad) arms *= kQuad;
  if (st.items & kItPent) arms *= kPent;
  m.arms = arms;
  return m;
}

void foe_heard(Fighter& f, const sh::HeardSound& h, double t) {
  if (f.sense == MapSense::novice || !h.meaning || h.ent < 1) return;
  const auto k = static_cast<size_t>(h.ent);
  if (f.foes.size() <= k) f.foes.resize(k + 1);
  FoeRead& r = f.foes[k];
  if (t - r.t > kFoeFade_s) r = FoeRead{};
  r.t = t;
  const std::string_view w = h.meaning->word;
  switch (h.cls) {
    case sh::SoundClass::fire:
      r.arms = arms_of_word(w);
      break;
    case sh::SoundClass::pickup:
      if (w == "armor") r.body = std::min(r.body + 120.f, 360.f);
      if (w == "mh") r.body = std::min(r.body + 100.f, 360.f);
      if (w == "weapon") r.arms = std::max(r.arms, 1.6f);
      break;
    case sh::SoundClass::power:
      if (w == "quad") r.quad_until = t + kQuadFor_s;
      break;
    case sh::SoundClass::pain:
      r.body = std::max(r.body - 20.f, 20.f);
      break;
    case sh::SoundClass::death:
    case sh::SoundClass::spawn:
      r = FoeRead{};
      r.t = t;
      break;
    default:
      break;
  }
}

float foe_might(const Fighter& f, int ent, double t) {
  const auto k = static_cast<size_t>(ent);
  if (ent < 1 || k >= f.foes.size() || t - f.foes[k].t > kFoeFade_s)
    return sh::might({100, kFreshArms});
  const FoeRead& r = f.foes[k];
  const float arms = std::max(r.arms, kFreshArms) * (t < r.quad_until ? kQuad : 1.f);
  return sh::might({r.body, arms});
}

int32_t nearest_mate_node(const Fighter& f, sh::Vec3f at, double t) {
  if (!f.team) return -1;
  const sh::Vec3f* best = nullptr;
  float near = 0;
  for (const Mate& m : f.mind.mates) {
    if (t - m.at_t > kMateFreshFor_s) continue;
    const sh::Vec3f d = m.at - at;
    const float dd = d.x * d.x + d.y * d.y + d.z * d.z;
    if (dd < kAtHand * kAtHand) continue;
    if (!best || dd < near) {
      best = &m.at;
      near = dd;
    }
  }
  return best ? sh::nav_nearest(*f.bot.pilot.pilot.g, *best) : -1;
}

}  // namespace shooters_edge::bots
