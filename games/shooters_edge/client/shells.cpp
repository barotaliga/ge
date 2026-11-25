// The guns' spent shells: the games' tables and the throw (shells.hpp).
//
// ge:summary The guns' spent shells: the games' ejecting guns, the throw, the case angles
// ge:keywords shells spent-case eject throw_case step_cases held_cases case-sound
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_shells_test.cpp
#include "shooters_edge/client/shells.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "brush/map.hpp"
#include "core/prng.hpp"
#include "detail/surfaces.hpp"
#include "shooters_edge/server/q2_flash.hpp"
#include "shooters_edge/view/weapons.hpp"

namespace shooters_edge::client {

namespace {

namespace gd = ge::detail;
namespace ce = ge::classicenhance;

// A pump throws its hull as the slide comes back; a break drops both as it opens, down and back.
constexpr Ejection kPump{Casing::hull, 1, 0.3f, 0.03f, 8, 2, 1, 1.4f, 1.6f, 0.3f};
constexpr Ejection kBreak{Casing::hull, 2, 0.5f, 0.04f, 4, 1, 3, 0.8f, 0.6f, 1.6f};
constexpr Ejection kQ2Pump{Casing::hull, 1, 0.35f, 0.03f, 8, 2, 1, 1.4f, 1.6f, 0.3f};
constexpr Ejection kQ2Break{Casing::hull, 2, 0.55f, 0.04f, 4, 1, 3, 0.8f, 0.6f, 1.6f};
// A machine gun throws its brass at once, out to the right.
constexpr Ejection kBrass{Casing::brass, 1, 0, 0.03f, 8, 2, 1, 1.8f, 1.6f, 0.3f};
constexpr Ejection kBrass2{Casing::brass, 2, 0, 0.03f, 8, 2, 1, 1.8f, 1.6f, 0.3f};
constexpr Ejection kBrass3{Casing::brass, 3, 0, 0.025f, 8, 2, 1, 1.8f, 1.6f, 0.3f};
constexpr Ejection kHeavy{Casing::heavy, 1, 0, 0.03f, 16, 4, 2, 1.8f, 1.6f, 0.3f};
// A monster's shotgun: its pump a moment after.
constexpr Ejection kMonsterPump{Casing::hull, 1, 0.25f, 0.03f, 4, 2, 2, 1.4f, 1.6f, 0.3f};

struct HeldRow {
  std::string_view model;
  const Ejection* e;
};
constexpr HeldRow kHeld[] = {
    // Quake's.
    {"progs/v_shot.mdl", &kPump},
    {"progs/v_shot2.mdl", &kBreak},
    // Quake II's.
    {"models/weapons/v_shotg/tris.md2", &kQ2Pump},
    {"models/weapons/v_shotg2/tris.md2", &kQ2Break},
    {"models/weapons/v_machn/tris.md2", &kBrass},
    {"models/weapons/v_chain/tris.md2", &kBrass2},
};

// Quake II's big guns (its monsters' flash values): the tank's, the supertank's, the second boss's,
// the final boss's and the carrier's chain guns.
constexpr std::pair<int, int> kQ2HeavyGuns[] = {{4, 22},    {64, 69},   {73, 77},  {120, 131},
                                                {133, 137}, {138, 139}, {152, 153}};

void cross3(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

}  // namespace

const Ejection* held_ejection(std::string_view model) {
  for (const HeldRow& r : kHeld)
    if (r.model == model) return r.e;
  return nullptr;
}

const Ejection* own_set_ejection(int weapon) {
  if (weapon == static_cast<int>(view::Weapon::shotgun)) return &kPump;
  if (weapon == static_cast<int>(view::Weapon::double_shotgun)) return &kBreak;
  return nullptr;
}

const Ejection* q1_player_ejection(std::string_view frame) {
  // The shotguns' firing frames (the super shotgun's are the same: one hull for either).
  return frame.starts_with("shotatt") ? &kPump : nullptr;
}

const Ejection* q1_monster_ejection(std::string_view model) {
  return model == "progs/soldier.mdl" ? &kMonsterPump : nullptr;
}

const Ejection* q2_flash_ejection(int value, bool monster) {
  if (!monster) {
    switch (value & ~shooters_edge::server::kQ2FlashSilenced) {
      case 1:  // the machine gun
        return &kBrass;
      case 2:  // the shotgun
        return &kQ2Pump;
      case 3:  // the chain gun: one, two or three rounds this frame
        return &kBrass;
      case 4:
        return &kBrass2;
      case 5:
        return &kBrass3;
      case 13:  // the super shotgun
        return &kQ2Break;
      default:
        return nullptr;
    }
  }
  const shooters_edge::server::Q2Flash* f = shooters_edge::server::q2_monster_flash(value);
  if (f == nullptr || f->look == nullptr || std::string_view(f->look) != "gun") return nullptr;
  if (!f->puff) return &kMonsterPump;  // a soldier's shotgun: smoke, no specks
  for (const auto& [first, last] : kQ2HeavyGuns)
    if (value >= first && value <= last) return &kHeavy;
  return &kBrass;
}

void case_port(const Ejection& e, const double at[3], const double fwd[3], const double up[3],
               double out[3]) {
  double right[3];
  cross3(up, fwd, right);
  const double u = ge::brush::kUnitM;
  for (int k = 0; k < 3; ++k)
    out[k] = at[k] - fwd[k] * e.back_u * u + right[k] * e.right_u * u - up[k] * e.down_u * u;
}

void throw_case(Cases& c, const Ejection& e, const double port[3], const double fwd[3],
                const double up_in[3], const double carry[3], double now_s) {
  if (c.pool.pieces.empty()) c.pool = gd::make_debris_pool(kCases, ce::casing_shapes());
  ge::core::Rng rng = ge::core::make_rng(c.seed);
  c.seed = c.seed * 1664525u + 1013904223u;
  // The gun's top square to its way.
  const double d = up_in[0] * fwd[0] + up_in[1] * fwd[1] + up_in[2] * fwd[2];
  double up[3] = {up_in[0] - fwd[0] * d, up_in[1] - fwd[1] * d, up_in[2] - fwd[2] * d};
  const double n = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
  if (n < 1e-9) {
    up[0] = 0;
    up[1] = 1;
    up[2] = 0;
  } else {
    for (double& v : up) v /= n;
  }
  double right[3];
  cross3(up, fwd, right);
  const double r = e.right_mps * ge::core::range(rng, 0.8, 1.2);
  const double u = e.up_mps * ge::core::range(rng, 0.7, 1.3);
  const double b = e.back_mps * ge::core::range(rng, 0.6, 1.4);
  gd::DebrisThrow t;
  t.at = {port[0], port[1], port[2]};
  t.vel = {right[0] * r + up[0] * u - fwd[0] * b + carry[0],
           right[1] * r + up[1] * u - fwd[1] * b + carry[1],
           right[2] * r + up[2] * u - fwd[2] * b + carry[2]};
  t.turn = gd::turn_of_axes({fwd[0], fwd[1], fwd[2]}, {up[0], up[1], up[2]});
  // Turning end over end about its top and rolling a little: a case flicked out sideways.
  t.spin = {ge::core::range(rng, -8, 8),
            ge::core::range(rng, 12, 30) * (ge::core::chance(rng, 0.5) ? 1 : -1),
            ge::core::range(rng, -6, 6)};
  t.kind = static_cast<uint16_t>(e.casing);
  gd::throw_debris(c.pool, t, now_s);
}

void owe_cases(Cases& c, const Ejection& e, const double port[3], const double fwd[3],
               const double up[3], bool held, double now_s) {
  for (int i = 0; i < e.count; ++i) {
    DueCase d;
    d.due = now_s + e.delay_s + double(e.gap_s) * i;
    d.e = &e;
    d.held = held;
    for (int k = 0; k < 3; ++k) {
      d.port[k] = port[k];
      d.fwd[k] = fwd[k];
      d.up[k] = up[k];
    }
    if (c.due.size() < kCases) c.due.push_back(d);
  }
}

ge::shooter::Vec3f angles_of_turn(const ge::core::Quat& q) {
  // The turn's matrix, as Ry(a) Rz(b) Rx(c): a model's yaw is -a, its pitch b, its roll -c.
  const double x = q.x, y = q.y, z = q.z, w = q.w;
  const double m00 = 1 - 2 * (y * y + z * z);
  const double m10 = 2 * (x * y + w * z);
  const double m20 = 2 * (x * z - w * y);
  const double m11 = 1 - 2 * (x * x + z * z);
  const double m12 = 2 * (y * z - w * x);
  const double deg = 180 / ge::core::kPi;
  const double a = std::atan2(-m20, m00);
  const double b = std::asin(std::clamp(m10, -1.0, 1.0));
  const double c = std::atan2(-m12, m11);
  return {float(b * deg), float(-a * deg), float(-c * deg)};
}

const char* case_sound_name(Casing c, gd::SurfaceClass on, int variant) {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> v;
    for (int k = 0; k < ce::kCasings; ++k)
      for (int s = 0; s < gd::kSurfaceClasses; ++s)
        for (int n = 0; n < ce::kCasingVariants; ++n)
          v.push_back(
              ce::casing_sound_name(static_cast<Casing>(k), static_cast<gd::SurfaceClass>(s), n));
    return v;
  }();
  const size_t at = (size_t(c) * gd::kSurfaceClasses + size_t(on)) * ce::kCasingVariants +
                    size_t(std::abs(variant) % ce::kCasingVariants);
  return names[std::min(at, names.size() - 1)].c_str();
}

}  // namespace shooters_edge::client
