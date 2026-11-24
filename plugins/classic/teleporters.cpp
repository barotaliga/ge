// The classic games' teleporters as a client foresees them (teleporters.hpp).
//
// ge:summary The classic games' teleporters as a client foresees them, read off the level
// ge:keywords teleporters q1_teleporters q2_teleporters q2_teleport_state
// ge:skill classic shooter net
// ge:test tests/classic_teleporters_test.cpp
#include "classic/teleporters.hpp"

#include <cstdlib>
#include <string>

#include "brush/map.hpp"
#include "core/num.hpp"

namespace ge::classic {

namespace sh = ge::shooter;
namespace num = ge::core::num;

namespace {

sh::Vec3f vec_of(const std::string& text) {
  float v[3] = {0, 0, 0};
  const char* p = text.c_str();
  for (float& x : v) {
    char* end = nullptr;
    x = std::strtof(p, &end);
    if (end == p) break;
    p = end;
  }
  return {v[0], v[1], v[2]};
}

// An entity's angles as the game reads them: `angles` whole, else `angle` as the yaw.
sh::Vec3f angles_of(const ge::brush::Entity& e) {
  if (!e.get("angles").empty()) return vec_of(e.get("angles"));
  if (!e.get("angle").empty()) return {0, std::strtof(e.get("angle").c_str(), nullptr), 0};
  return {};
}

// The first entity named `name`, in the level's order; nullptr none.
const ge::brush::Entity* named(const ge::brush::Map& map, const std::string& name) {
  if (name.empty()) return nullptr;
  for (const ge::brush::Entity& e : map.entities)
    if (e.get("targetname") == name) return &e;
  return nullptr;
}

constexpr sh::Vec3f kGrow{sh::kTeleLinkGrow, sh::kTeleLinkGrow, sh::kTeleLinkGrow};

}  // namespace

sh::Teleporters q1_teleporters(const ge::brush::Map& map) {
  sh::Teleporters out;
  for (const ge::brush::Entity& e : map.entities) {
    if (e.get("classname") != "trigger_teleport" || !e.get("targetname").empty()) continue;
    const std::string& model = e.get("model");
    if (model.size() < 2 || model[0] != '*') continue;
    const int k = std::atoi(model.c_str() + 1);
    if (k <= 0 || static_cast<size_t>(k) >= map.models.size()) continue;
    const ge::brush::Entity* d = named(map, e.get("target"));
    if (d == nullptr || d->get("classname") != "info_teleport_destination") continue;
    const ge::brush::Model& m = map.models[static_cast<size_t>(k)];
    const sh::Vec3f at = vec_of(e.get("origin"));  // a brush entity's origin, 0 0 0 when none
    sh::Teleporter t;
    t.lo = at + sh::Vec3f{m.mins[0], m.mins[1], m.mins[2]} - kGrow;
    t.hi = at + sh::Vec3f{m.maxs[0], m.maxs[1], m.maxs[2]} + kGrow;
    t.dest = vec_of(d->get("origin")) + sh::Vec3f{0, 0, kQ1TeleportRise};
    t.angles = angles_of(*d);
    t.velocity = sh::angle_vectors(t.angles).forward * kQ1TeleportSpeed;
    out.push_back(t);
  }
  return out;
}

sh::Teleporters q2_teleporters(const ge::brush::Map& map) {
  sh::Teleporters out;
  for (const ge::brush::Entity& e : map.entities) {
    if (e.get("classname") != "misc_teleporter") continue;
    const ge::brush::Entity* d = named(map, e.get("target"));
    if (d == nullptr) continue;
    const std::string& kind = d->get("classname");
    if (kind != "misc_teleporter_dest" && kind != "info_notnull") continue;
    const sh::Vec3f at = vec_of(e.get("origin"));
    sh::Teleporter t;
    t.lo = at + sh::Vec3f{-8, -8, 8} - kGrow;
    t.hi = at + sh::Vec3f{8, 8, 24} + kGrow;
    t.dest = vec_of(d->get("origin")) + sh::Vec3f{0, 0, kQ2TeleportRise};
    t.angles = angles_of(*d);
    t.angles.z = 0;
    t.hold_ms = kQ2TeleportHoldMs;
    out.push_back(t);
  }
  return out;
}

sh::Teleporters level_teleporters(const ge::brush::Map& map) {
  return map.family == ge::brush::Family::v38 ? q2_teleporters(map) : q1_teleporters(map);
}

void q2_teleport_state(Q2State& s, const sh::Teleporter& t, const int16_t cmd_angles[3]) {
  const float at[3] = {t.dest.x, t.dest.y, t.dest.z};
  for (int i = 0; i < 3; i++) {
    s.origin[i] = static_cast<int16_t>(num::to_int32(static_cast<double>(at[i]) * 8));
    s.velocity[i] = 0;
  }
  s.flags = static_cast<uint16_t>(s.flags | kSqTimeTeleport);
  s.time = static_cast<uint16_t>(t.hold_ms / 8);
  const float ang[3] = {t.angles.x, t.angles.y, 0};
  for (int i = 0; i < 3; i++) {
    const auto want =
        static_cast<int16_t>(num::to_int32(static_cast<double>(ang[i]) * (65536.0 / 360.0)));
    s.delta_angles[i] = static_cast<int16_t>(want - cmd_angles[i]);
  }
}

}  // namespace ge::classic
