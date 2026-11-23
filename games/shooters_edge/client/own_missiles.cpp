// The player's own missiles thrown ahead of the server (own_missiles.hpp).
//
// ge:summary The player's own nails, rockets and grenades thrown ahead of the server
// ge:keywords own_missiles cl_predict_projectiles handover grenade-bounce
// ge:skill net shooter
// ge:test tests/shooters_edge_own_missiles_test.cpp
#include "shooters_edge/client/own_missiles.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace shooters_edge::client {

namespace {

constexpr double kStepS = 0.02;  // the longest step a grenade falls in one go

const char* model_of(OwnMissileKind k) {
  switch (k) {
    case OwnMissileKind::nail:
      return "progs/spike.mdl";
    case OwnMissileKind::super_nail:
      return "progs/s_spike.mdl";
    case OwnMissileKind::rocket:
      return "progs/missile.mdl";
    case OwnMissileKind::grenade:
      return "progs/grenade.mdl";
  }
  return nullptr;
}

int model_index(const DemoState& s, const char* name) {
  for (size_t i = 1; i < s.models.size(); i++)
    if (s.models[i] == name) return static_cast<int>(i);
  return 0;
}

// The angles a straight missile is drawn at, off its velocity (Quake's: pitch up positive).
sh::Vec3f flight_angles(const sh::Vec3f& v) {
  const double h = std::sqrt(static_cast<double>(v.x) * v.x + static_cast<double>(v.y) * v.y);
  const double yaw = h == 0 ? 0 : std::atan2(v.y, v.x) * 180 / 3.14159265358979323846;
  const double pitch = std::atan2(v.z, h) * 180 / 3.14159265358979323846;
  return {static_cast<float>(pitch), static_cast<float>(yaw), 0};
}

// `o` flown from `o.at_s` to `t`: false when it hit a wall (a nail or a rocket: it goes).
bool fly(OwnMissile& o, const sh::Hull* world, float gravity, double t) {
  while (o.at_s < t) {
    const double dt = std::min(t - o.at_s, o.kind == OwnMissileKind::grenade ? kStepS : t - o.at_s);
    const auto f = static_cast<float>(dt);
    if (o.kind == OwnMissileKind::grenade) {
      o.velocity.z -= gravity * f;
      for (float* a : {&o.angles.x, &o.angles.y, &o.angles.z}) {
        *a += kGrenadeSpinDeg * f;
        if (*a >= 360) *a -= 360;
      }
    }
    const sh::Vec3f to = o.origin + o.velocity * f;
    o.at_s += dt;
    if (world == nullptr) {
      o.origin = to;
      continue;
    }
    const sh::HullTrace tr = sh::trace_hull(*world, o.origin, to);
    if (tr.allsolid || tr.startsolid) return o.kind == OwnMissileKind::grenade;
    o.origin = tr.endpos;
    if (tr.fraction >= 1) continue;
    if (o.kind != OwnMissileKind::grenade) return false;
    const float into = sh::dot(o.velocity, tr.plane.normal);
    o.velocity = o.velocity - tr.plane.normal * (into * kGrenadeBounce);
  }
  return true;
}

}  // namespace

bool own_missile_fire(OwnMissiles& m, const OwnShot& shot, double now_s, double latency_s,
                      int ping_ms, const sh::Vec3f& player) {
  if (!m.on || ping_ms <= 0) return false;
  const sh::ViewAxes v = sh::angle_vectors(shot.angles);
  OwnMissile o;
  o.kind = shot.kind;
  o.id = m.next_id++;
  if (shot.kind == OwnMissileKind::grenade) {
    o.origin = shot.origin;
    o.velocity = v.forward * kGrenadeAheadU + v.up * kGrenadeUpU;
  } else {
    o.origin = shot.origin + v.forward * 8 + sh::Vec3f{0, 0, 16};
    if (shot.kind == OwnMissileKind::nail)
      o.origin = o.origin + v.right * (4.0f * static_cast<float>(shot.side));
    const float speed = shot.kind == OwnMissileKind::rocket
                            ? (shot.midair ? kMidairRocketSpeedU : kRocketSpeedU)
                            : kNailSpeedU;
    o.velocity = v.forward * speed;
    o.angles = flight_angles(o.velocity);
  }
  o.at_s = now_s - shot.age_s;
  o.show_s = now_s + std::max(0.0, latency_s - kOwnMissileLeadS - ping_ms * 0.001) - shot.age_s;
  o.end_s = now_s + latency_s + (shot.kind == OwnMissileKind::grenade ? 0 : kOwnMissileLeadS) +
            kOwnMissileWaitS;
  if (static_cast<int>(m.flying.size()) >= kOwnMissilesMost) {
    auto far = std::max_element(
        m.flying.begin(), m.flying.end(), [&](const OwnMissile& a, const OwnMissile& b) {
          return sh::length(a.origin - player) < sh::length(b.origin - player);
        });
    *far = o;
  } else {
    m.flying.push_back(o);
  }
  m.thrown++;
  return true;
}

void own_missiles_step(OwnMissiles& m, const sh::Hull* world, float gravity, double now_s) {
  std::erase_if(m.flying, [&](OwnMissile& o) {
    if (o.adopted == 0 && o.end_s <= now_s) {
      m.ended++;
      return true;
    }
    if (!fly(o, world, gravity, now_s)) {
      m.walled++;
      if (o.adopted != 0) m.hidden.push_back(o.adopted);  // its copy hidden till it is gone
      return true;
    }
    if (o.path_n == 0 || now_s - o.path_s >= kOwnMissilePathS) {
      o.path[o.path_n % kOwnMissilePath] = o.origin;
      o.path_n++;
      o.path_s = now_s;
    }
    return false;
  });
}

namespace {

// The kind a server model is (false: not a missile thrown ahead).
bool kind_of_model(std::string_view name, OwnMissileKind& k) {
  for (const OwnMissileKind c : {OwnMissileKind::nail, OwnMissileKind::super_nail,
                                 OwnMissileKind::rocket, OwnMissileKind::grenade})
    if (name == model_of(c)) {
      k = c;
      return true;
    }
  return false;
}

bool on_path(const OwnMissile& o, const sh::Vec3f& at) {
  const int n = std::min(o.path_n, kOwnMissilePath);
  for (int i = 0; i < n; i++)
    if (sh::length(o.path[i] - at) <= kOwnMissileHandU) return true;
  return false;
}

}  // namespace

void own_missiles_handover(OwnMissiles& m, DemoState& s) {
  const size_t players = s.format == DemoFormat::single ? 0 : s.players.size();
  const auto copy_of = [&](int n, OwnMissileKind& k) {
    if (n <= static_cast<int>(players) || static_cast<size_t>(n) >= s.entities.size()) return false;
    const DemoEntity& e = s.entities[static_cast<size_t>(n)];
    return e.present && e.model > 0 && static_cast<size_t>(e.model) < s.models.size() &&
           kind_of_model(s.models[static_cast<size_t>(e.model)], k);
  };
  // A copy gone (its hit, its explosion): its stand-in goes, and nothing stays hidden for it.
  std::erase_if(m.flying, [&](const OwnMissile& o) {
    OwnMissileKind k{};
    return o.adopted != 0 && (!copy_of(o.adopted, k) || k != o.kind);
  });
  std::erase_if(m.hidden, [&](int n) {
    OwnMissileKind k{};
    return !copy_of(n, k);
  });
  if (m.flying.empty() && m.hidden.empty()) return;
  for (size_t n = players + 1; n < s.entities.size(); n++) {
    OwnMissileKind k{};
    const int number = static_cast<int>(n);
    if (!copy_of(number, k)) continue;
    const auto taken = [&](const OwnMissile& o) { return o.adopted == number; };
    if (std::find(m.hidden.begin(), m.hidden.end(), number) != m.hidden.end() ||
        std::any_of(m.flying.begin(), m.flying.end(), taken))
      continue;
    const auto mine = std::find_if(m.flying.begin(), m.flying.end(), [&](const OwnMissile& o) {
      return o.adopted == 0 && o.kind == k && on_path(o, s.entities[n].origin);
    });
    if (mine == m.flying.end()) continue;
    mine->adopted = number;
    m.handed++;
  }
  // The adopted copies and the hidden ones are not drawn: their stand-ins are.
  for (const OwnMissile& o : m.flying)
    if (o.adopted != 0) s.entities[static_cast<size_t>(o.adopted)].present = false;
  for (const int n : m.hidden) s.entities[static_cast<size_t>(n)].present = false;
}

void own_missiles_draw(const OwnMissiles& m, DemoState& s, double now_s) {
  s.own_missiles.clear();
  s.own_missile_ids.clear();
  for (const OwnMissile& o : m.flying) {
    if (o.show_s > now_s) continue;
    const int model = model_index(s, model_of(o.kind));
    if (model == 0) continue;
    DemoEntity e;
    e.model = model;
    e.origin = o.origin;
    e.angles = o.angles;
    e.present = true;
    s.own_missiles.push_back(e);
    s.own_missile_ids.push_back(o.id);
  }
}

void own_missiles_clear(OwnMissiles& m) {
  m.flying.clear();
  m.hidden.clear();
}

}  // namespace shooters_edge::client
