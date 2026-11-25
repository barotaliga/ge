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
    case OwnMissileKind::laser:
      return "progs/lasrspik.mdl";
    case OwnMissileKind::prox:
      return "progs/proxbomb.mdl";
    case OwnMissileKind::lava_nail:
      return "progs/lspike.mdl";
    case OwnMissileKind::multi_grenade:
      return "progs/mervup.mdl";
    case OwnMissileKind::multi_rocket:
      return "progs/rockup.mdl";
    case OwnMissileKind::plasma:
      return "progs/plasma.mdl";
    case OwnMissileKind::q2_bolt:
      return "models/objects/laser/tris.md2";
    case OwnMissileKind::q2_grenade:
      return "models/objects/grenade/tris.md2";
    case OwnMissileKind::q2_rocket:
      return "models/objects/rocket/tris.md2";
    case OwnMissileKind::q2_bfg:
      return "sprites/s_bfg1.sp2";
    case OwnMissileKind::q2_hand_grenade:
      return "models/objects/grenade2/tris.md2";
  }
  return nullptr;
}

// Thrown and falling: the grenades.
bool falls(OwnMissileKind k) {
  return k == OwnMissileKind::grenade || k == OwnMissileKind::prox ||
         k == OwnMissileKind::multi_grenade || k == OwnMissileKind::q2_grenade ||
         k == OwnMissileKind::q2_hand_grenade;
}

bool q2(OwnMissileKind k) {
  return k == OwnMissileKind::q2_bolt || k == OwnMissileKind::q2_grenade ||
         k == OwnMissileKind::q2_rocket || k == OwnMissileKind::q2_bfg ||
         k == OwnMissileKind::q2_hand_grenade;
}

// What a missile's step meets: the level's clip hull (Quake's) or its brushes (Quake II's).
struct World {
  const sh::Hull* hull = nullptr;
  ge::brush::BrushTracer* brushes = nullptr;
};
struct Hit {
  bool solid = false;  // started in a wall
  float fraction = 1;
  sh::Vec3f end, normal;
};

bool trace(const World& w, const sh::Vec3f& from, const sh::Vec3f& to, Hit& h) {
  if (w.hull != nullptr) {
    const sh::HullTrace tr = sh::trace_hull(*w.hull, from, to);
    h = {tr.allsolid || tr.startsolid, tr.fraction, tr.endpos, tr.plane.normal};
    return true;
  }
  if (w.brushes == nullptr) return false;
  constexpr int32_t kStops = 1 | 2;  // solid and window brushes
  const float a[3] = {from.x, from.y, from.z}, b[3] = {to.x, to.y, to.z}, zero[3] = {};
  const ge::brush::Trace tr = w.brushes->trace(a, b, zero, zero, kStops);
  h = {tr.all_solid || tr.start_solid,
       tr.fraction,
       {tr.end[0], tr.end[1], tr.end[2]},
       {tr.plane.n[0], tr.plane.n[1], tr.plane.n[2]}};
  return true;
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
bool fly(OwnMissile& o, const World& world, float gravity, double t) {
  if (o.stuck) {
    o.at_s = t;
    return true;
  }
  if (o.go_s >= 0 && o.at_s < o.go_s) {  // the plasma ball still, then away along the aim
    if (t < o.go_s) {
      o.at_s = t;
      return true;
    }
    o.at_s = o.go_s;
    o.velocity = o.aim * kPlasmaSpeedU;
    o.angles = flight_angles(o.velocity);
  }
  const bool falling = falls(o.kind);
  while (o.at_s < t) {
    const double dt = std::min(t - o.at_s, falling ? kStepS : t - o.at_s);
    const auto f = static_cast<float>(dt);
    if (falling) {
      o.velocity.z -= gravity * f;
      float* axes[3] = {&o.angles.x, &o.angles.y, &o.angles.z};
      for (int k = 0; k < 3; k++) {
        *axes[k] += (o.kind == OwnMissileKind::prox ? kProxSpinDeg[k] : kGrenadeSpinDeg) * f;
        if (*axes[k] >= 360) *axes[k] -= 360;
      }
    }
    const sh::Vec3f to = o.origin + o.velocity * f;
    o.at_s += dt;
    Hit tr;
    if (!trace(world, o.origin, to, tr)) {
      o.origin = to;
      continue;
    }
    if (tr.solid) return falling;
    o.origin = tr.end;
    if (tr.fraction >= 1) continue;
    if (!falling) return false;
    if (o.kind == OwnMissileKind::prox) {  // it sticks where it lands
      o.stuck = true;
      o.velocity = {0, 0, 0};
      o.at_s = t;
      return true;
    }
    const float into = sh::dot(o.velocity, tr.normal);
    o.velocity = o.velocity - tr.normal * (into * kGrenadeBounce);
    if (o.kind == OwnMissileKind::q2_grenade ||
        o.kind == OwnMissileKind::q2_hand_grenade) {  // off the wall a little; at rest on a floor
      o.origin = o.origin + tr.normal * 0.1f;
      if (tr.normal.z > 0.7f && sh::length(o.velocity) < kQ2RestSpeedU) {
        o.velocity = {0, 0, 0};
        o.stuck = true;
        o.at_s = t;
        return true;
      }
    }
  }
  return true;
}

// Quake II's launch (own_missiles.hpp): from under the eye, ahead and to the gun's side.
void q2_launch(const OwnShot& shot, const sh::ViewAxes& v, OwnMissile& o) {
  const float ahead = shot.kind == OwnMissileKind::q2_bolt ? kQ2BoltAheadU : kQ2MissileAheadU;
  o.origin = shot.origin + sh::Vec3f{0, 0, shot.view_height - kQ2EyeDropU} + v.forward * ahead +
             v.right * (kQ2MissileSideU * static_cast<float>(shot.side));
  switch (shot.kind) {
    case OwnMissileKind::q2_hand_grenade:
      o.velocity = v.forward * shot.speed + v.up * kGrenadeUpU;
      break;
    case OwnMissileKind::q2_grenade:
      o.velocity = v.forward * kGrenadeAheadU + v.up * kGrenadeUpU;
      break;
    case OwnMissileKind::q2_rocket:
      o.velocity = v.forward * kQ2RocketSpeedU;
      break;
    case OwnMissileKind::q2_bfg:
      o.velocity = v.forward * kQ2BfgSpeedU;
      break;
    default:
      o.velocity = v.forward * kQ2BoltSpeedU;
      break;
  }
  if (!falls(shot.kind)) o.angles = flight_angles(o.velocity);
}

}  // namespace

int own_shot_kind(ge::classic::WeaponSound sound, int copies, int weapon,
                  ge::classic::WeaponRules rules, OwnMissileKind& kind) {
  namespace wc = ge::classic;
  const bool pack = rules == wc::WeaponRules::dissolution;
  const bool lava = pack && (weapon == wc::kItLavaNailgun || weapon == wc::kItLavaSuperNailgun);
  const bool multi = pack && (weapon == wc::kItMultiGrenade || weapon == wc::kItMultiRocket);
  switch (sound) {
    case wc::WeaponSound::nailgun:
      kind = lava ? OwnMissileKind::lava_nail : OwnMissileKind::nail;
      return 1;
    case wc::WeaponSound::super_nailgun:
      kind = lava ? OwnMissileKind::lava_nail : OwnMissileKind::super_nail;
      return 1;
    case wc::WeaponSound::rocket:
      kind = multi ? OwnMissileKind::multi_rocket : OwnMissileKind::rocket;
      return multi ? 4 : 1;
    case wc::WeaponSound::grenade:
      kind = multi ? OwnMissileKind::multi_grenade : OwnMissileKind::grenade;
      return 1;
    case wc::WeaponSound::laser:  // the twin bolts one to each side, the single one between
      kind = OwnMissileKind::laser;
      return copies > 1 ? 2 : 1;
    case wc::WeaponSound::proximity:
      kind = OwnMissileKind::prox;
      return 1;
    case wc::WeaponSound::plasma:
      kind = OwnMissileKind::plasma;
      return 1;
    default:
      return 0;
  }
}

bool own_missile_fire(OwnMissiles& m, const OwnShot& shot, double now_s, double latency_s,
                      int ping_ms, const sh::Vec3f& player) {
  if (!m.on || ping_ms <= 0) return false;
  const sh::ViewAxes v = sh::angle_vectors(shot.angles);
  OwnMissile o;
  o.kind = shot.kind;
  o.id = m.next_id++;
  o.effects = shot.effects;
  if (q2(shot.kind)) {
    q2_launch(shot, v, o);
  } else if (falls(shot.kind)) {
    o.origin = shot.origin;
    o.velocity = v.forward * kGrenadeAheadU + v.up * kGrenadeUpU;
  } else if (shot.kind == OwnMissileKind::laser) {
    o.origin = shot.origin + v.forward * kLaserAheadU + sh::Vec3f{0, 0, kLaserUpU} +
               v.right * (kLaserSideU * static_cast<float>(shot.side));
    o.velocity = v.forward * kNailSpeedU;
    o.angles = flight_angles(o.velocity);
  } else if (shot.kind == OwnMissileKind::plasma) {
    o.origin = shot.origin + v.forward * kPlasmaAheadU + sh::Vec3f{0, 0, 16};
    o.aim = v.forward;
    o.angles = flight_angles(v.forward);
  } else if (shot.kind == OwnMissileKind::multi_rocket) {
    const int k = std::clamp(shot.spread, 0, 3);
    o.origin = shot.origin + v.forward * 8 + sh::Vec3f{0, 0, 16};
    if (shot.deathmatch) {
      sh::Vec3f turned = shot.angles;
      turned.y += kMultiRocketYaw[k];
      o.velocity = sh::angle_vectors(turned).forward * kRocketSpeedU;
    } else {
      o.velocity = v.forward * kRocketSpeedU + v.right * kMultiRocketSide[k];
    }
    o.angles = flight_angles(o.velocity);
  } else {
    o.origin = shot.origin + v.forward * 8 + sh::Vec3f{0, 0, 16};
    if (shot.kind == OwnMissileKind::nail || shot.kind == OwnMissileKind::lava_nail)
      o.origin = o.origin + v.right * (4.0f * static_cast<float>(shot.side));
    const float speed = shot.kind == OwnMissileKind::rocket
                            ? (shot.midair ? kMidairRocketSpeedU : kRocketSpeedU)
                            : kNailSpeedU;
    o.velocity = v.forward * speed;
    o.angles = flight_angles(o.velocity);
  }
  o.at_s = now_s - shot.age_s;
  if (shot.kind == OwnMissileKind::plasma) o.go_s = o.at_s + kPlasmaWaitS;
  o.show_s = now_s + std::max(0.0, latency_s - kOwnMissileLeadS - ping_ms * 0.001) - shot.age_s;
  o.end_s = now_s + latency_s + (falls(shot.kind) ? 0 : kOwnMissileLeadS) + kOwnMissileWaitS +
            (q2(shot.kind) ? kQ2FrameS : 0);
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

namespace {

void step_all(OwnMissiles& m, const World& world, float gravity, double now_s) {
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

}  // namespace

void own_missiles_step(OwnMissiles& m, const sh::Hull* world, float gravity, double now_s) {
  step_all(m, World{world, nullptr}, gravity, now_s);
}

void own_missiles_step_brushes(OwnMissiles& m, ge::brush::BrushTracer* brushes, float gravity,
                               double now_s) {
  step_all(m, World{nullptr, brushes}, gravity, now_s);
}

namespace {

// The kind a server model is (false: not a missile thrown ahead).
bool kind_of_model(std::string_view name, OwnMissileKind& k) {
  for (const OwnMissileKind c :
       {OwnMissileKind::nail, OwnMissileKind::super_nail, OwnMissileKind::rocket,
        OwnMissileKind::grenade, OwnMissileKind::laser, OwnMissileKind::prox,
        OwnMissileKind::lava_nail, OwnMissileKind::multi_grenade, OwnMissileKind::multi_rocket,
        OwnMissileKind::plasma, OwnMissileKind::q2_bolt, OwnMissileKind::q2_grenade,
        OwnMissileKind::q2_rocket, OwnMissileKind::q2_bfg, OwnMissileKind::q2_hand_grenade})
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
    e.effects = o.effects;
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
