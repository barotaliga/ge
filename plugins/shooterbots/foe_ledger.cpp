// A fighter's temperament at work (temper.hpp): its might, and its enemies' read off their sounds.
//
// ge:summary A fighter's own might and each enemy's, believed off what it hears of him
// ge:keywords own_might foe_heard foe_might arms-of-word fresh-spawn
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_fight_test.cpp
#include "shooterbots/foe_ledger.hpp"

#include "shooterbots/mind.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace ge::shooterbots {

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;

namespace {

// Quake's IT_ bits for the powers (games/shooters_edge/rules).
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

sb::Might own_might(const Stats& st) {
  sb::Might m;
  m.body = st.health + st.armortype * st.armor;
  float arms = 1;
  for (const BotWeapon& w : st.arsenal->weapons)
    if (loaded(st, w)) arms = std::max(arms, w.arms);
  if (st.items & kItQuad) arms *= kQuad;
  if (st.items & kItPent) arms *= kPent;
  m.arms = arms;
  return m;
}

void foe_heard(Fighter& f, const sb::HeardSound& h, double t) {
  if (f.hearing == MapSense::novice || !h.meaning || h.ent < 1) return;
  const auto k = static_cast<size_t>(h.ent);
  if (f.foes.size() <= k) f.foes.resize(k + 1);
  FoeRead& r = f.foes[k];
  if (t - r.t > kFoeFade_s) {
    const double key_t = r.key_t;  // (`rlknown`) the launcher outlasts the fade
    r = FoeRead{};
    r.key_t = key_t;
  }
  r.t = t;
  const std::string_view w = h.meaning->word;
  switch (h.cls) {
    case sb::SoundClass::fire:
      r.arms = arms_of_word(w);
      if (w == "rl") r.key_t = t;
      break;
    case sb::SoundClass::pickup:
      r.news = static_cast<uint8_t>(FoeNews::pickup);
      if (w == "armor") r.body = std::min(r.body + 120.f, 360.f);
      if (w == "mh") r.body = std::min(r.body + 100.f, 360.f);
      // (`ledgerheal`) a health box heard taken: about 20 back, under 100 health and his armour
      if (w == "health" && f.features[sb::Feature::ledgerheal] && r.body < kHealedUnder)
        r.body = std::min(r.body + kBoxHeals, kHealedUnder);
      if (w == "weapon") r.arms = std::max(r.arms, 1.6f);
      break;
    case sb::SoundClass::power:
      if (w == "quad") r.quad_until = t + kQuadFor_s;
      break;
    case sb::SoundClass::pain:
      // (`ledgerhits`) a cry right after its own rocket was counted on him is that rocket's
      if (f.features[sb::Feature::ledgerhits] && t - r.hit_t < kCryAfterHit_s) break;
      r.body = std::max(r.body - 20.f, 20.f);
      r.news = static_cast<uint8_t>(FoeNews::cry);
      break;
    case sb::SoundClass::death:
    case sb::SoundClass::spawn:
      r = FoeRead{};
      r.t = t;
      r.news = static_cast<uint8_t>(FoeNews::spawn);
      break;
    default:
      break;
  }
}

float foe_body(const Fighter& f, int ent, double t) {
  const auto k = static_cast<size_t>(ent);
  if (ent < 1 || k >= f.foes.size() || t - f.foes[k].t > kFoeFade_s) return 100;
  const FoeRead& r = f.foes[k];
  const double keep =
      f.features[sb::Feature::spawnkept] && r.news == static_cast<uint8_t>(FoeNews::spawn)
          ? kSpawnKept_s
          : kForget_s;
  if (f.features[sb::Feature::ledgerforget] && f.team && t - std::max(r.t, r.seen_t) > keep)
    return kFoeTypical;
  return r.body;
}

void ledger_own_shots(Fighter& f, const ge::shooterbots::BotView& s, int me, bool fired, double t) {
  if (!f.features[sb::Feature::ledgerhits]) return;
  const auto& ents = s.world.entities;
  const sh::Vec3f o = ents[static_cast<size_t>(me)].origin;
  if (fired) {
    // Its new rocket: one just out of its muzzle not already counted as its own.
    for (size_t e = static_cast<size_t>(s.sv.set.maxclients) + 1; e < ents.size(); e++) {
      const ge::shooter::WorldEntity& w = ents[e];
      if (w.free || w.shot != ge::shooter::WorldShot::rocket) continue;
      const sh::Vec3f d = w.origin - sh::Vec3f{o.x, o.y, o.z + 16};
      if (d.x * d.x + d.y * d.y + d.z * d.z > 96.f * 96.f) continue;
      bool known = false;
      for (const OwnRocket& r : f.own_rockets) known |= r.ent == static_cast<int>(e);
      if (known) continue;
      OwnRocket* slot = &f.own_rockets[0];
      for (OwnRocket& r : f.own_rockets)
        if (r.ent < 0) slot = &r;
      slot->ent = static_cast<int>(e);
      slot->at = w.origin;
      break;
    }
  }
  for (OwnRocket& r : f.own_rockets) {
    if (r.ent < 0) continue;
    const ge::shooter::WorldEntity& w = ents[static_cast<size_t>(r.ent)];
    if (!w.free && w.shot == ge::shooter::WorldShot::rocket) {
      r.at = w.origin;
      continue;
    }
    // Gone: it burst where it was last seen. Each enemy seen lately takes its share.
    const sh::Vec3f burst = r.at;
    r.ent = -1;
    for (const sb::Contact& c : f.bot.senses.contacts) {
      if (c.ent < 1 || !ge::ai::known(c.seen) || ge::ai::age(c.seen, t) > kHitSeen_s) continue;
      const float a = static_cast<float>(ge::ai::age(c.seen, t));
      const sh::Vec3f him{static_cast<float>(c.seen.x) + c.course.x * a,
                          static_cast<float>(c.seen.y) + c.course.y * a,
                          static_cast<float>(c.seen.z) + 4};
      const sh::Vec3f d = him - burst;
      const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      const float dmg = dist < kDirectReach ? kRocketDirect : kRocketSplash - 0.5f * dist;
      if (dmg <= 0) continue;
      const auto k = static_cast<size_t>(c.ent);
      if (f.foes.size() <= k) f.foes.resize(k + 1);
      FoeRead& fr = f.foes[k];
      if (t - fr.t > kFoeFade_s) fr = FoeRead{};
      fr.body = std::max(1.f, fr.body - dmg * kArmourShare);
      fr.t = t;
      fr.hit_t = t;
      fr.news = static_cast<uint8_t>(FoeNews::hit);
    }
  }
}

void ledger_obituary(Fighter& f, const ge::shooterbots::BotView& s, const std::string& line,
                     double t) {
  if (!f.features[sb::Feature::ledgerobits] || f.name.empty()) return;
  for (size_t i = 0; i < std::size(s.world.clients); i++) {
    const std::string& name = s.world.clients[i].name;
    if (name.empty() || name == f.name || line.size() <= name.size() + 1 ||
        line.compare(0, name.size(), name) != 0 || line[name.size()] != ' ')
      continue;
    if (!death_words(std::string_view(line).substr(name.size() + 1))) return;
    for (const Mate& m : f.mind.mates)
      if (m.name == name) return;  // a mate's: not an enemy's line
    const auto k = i + 1;
    if (f.foes.size() <= k) f.foes.resize(k + 1);
    FoeRead& r = f.foes[k];
    r = FoeRead{};
    r.t = t;
    r.seen_t = t;  // known, as if seen: not forgotten for kForget_s
    r.news = static_cast<uint8_t>(FoeNews::spawn);
    return;
  }
}

void key_obituary(Fighter& f, const ge::shooterbots::BotView& s, const std::string& line,
                  double t) {
  if (f.name.empty()) return;
  const bool rocket = line.find("rocket") != std::string::npos ||
                      line.find("blown apart") != std::string::npos ||
                      line.find(" rips ") != std::string::npos;
  if (!rocket) return;
  // The two players the line names, by where each name stands in it (whole words only).
  size_t at[2] = {std::string::npos, std::string::npos};
  size_t who[2] = {0, 0};
  int n = 0;
  for (size_t i = 0; i < std::size(s.world.clients); i++) {
    const std::string& name = s.world.clients[i].name;
    if (name.empty()) continue;
    for (size_t p = line.find(name); p != std::string::npos; p = line.find(name, p + 1)) {
      const size_t e = p + name.size();
      const bool word = (p == 0 || line[p - 1] == ' ') &&
                        (e == line.size() || line[e] == ' ' || line[e] == '\'' || line[e] == '\n');
      if (!word) continue;
      if (n < 2) at[n] = p, who[n] = i;
      n++;
      break;
    }
  }
  if (n != 2) return;
  if (at[1] < at[0]) std::swap(at[0], at[1]), std::swap(who[0], who[1]);
  // The killer: named second ("A rides B's rocket"), but first where he "rips" the other.
  const size_t killer = line.find(" rips ") != std::string::npos ? who[0] : who[1];
  const std::string& name = s.world.clients[killer].name;
  if (name == f.name) return;
  for (const Mate& m : f.mind.mates)
    if (m.name == name) return;
  const auto k = killer + 1;
  if (f.foes.size() <= k) f.foes.resize(k + 1);
  f.foes[k].key_t = t;
}

void foes_seen(Fighter& f, double t) {
  if (!f.features[sb::Feature::ledgerforget]) return;
  for (const sb::Contact& c : f.bot.senses.contacts) {
    if (!c.visible || c.ent < 1) continue;
    const auto k = static_cast<size_t>(c.ent);
    if (k < f.foes.size()) f.foes[k].seen_t = t;
  }
}

float foe_might(const Fighter& f, int ent, double t) {
  const auto k = static_cast<size_t>(ent);
  if (ent < 1 || k >= f.foes.size() || t - f.foes[k].t > kFoeFade_s)
    return sb::might({100, kFreshArms});
  const FoeRead& r = f.foes[k];
  const float arms = std::max(r.arms, kFreshArms) * (t < r.quad_until ? kQuad : 1.f);
  return sb::might({foe_body(f, ent, t), arms});
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
  return best ? sb::nav_nearest(*f.bot.pilot.pilot.g, *best) : -1;
}

}  // namespace ge::shooterbots
