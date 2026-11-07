// ge:summary Each player's shots and hits per weapon, off ammunition spent and blows taken
// ge:keywords hits_frame hit_weapon_name pellets direct-hit splash dmg_inflictor
// ge:skill shooter bots
// ge:test tests/shooters_edge_team_test.cpp
#include "shooters_edge/net/classic_hits.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "progs/vm.hpp"

namespace shooters_edge::net {

namespace {

namespace pg = ge::progs;
namespace sh = ge::shooter;

// The baseline mod's weapon bits (games/shooters_edge/qc/common/defs.qc) and the quad's.
constexpr int32_t kScatter = 1, kDouble = 2, kSpiker = 4, kHeavy = 8, kGrenades = 16;
constexpr int32_t kRockets = 32, kArc = 64;
constexpr int32_t kQuadBit = 4194304;
// Its blows: a pellet, a beam, a rocket's direct hit (all four times under the quad).
constexpr float kPellet = 4, kBeam = 30, kDirect = 100;
// A rocket this near a player it killed hit him (units from his box).
constexpr float kNear = 32;
// The least blow counted (points of health and armour).
constexpr float kLeast = 3;

enum Kind : uint8_t { kNone, kSpike, kHeavySpike, kGrenade, kRocket };

float field(const server::QcServer& s, int e, int f) {
  return f < 0 ? 0 : pg::as_float(*pg::entity_field(s.q.vm, e, f));
}

int entity_of(const server::QcServer& s, int e, int f) {
  return f < 0 ? -1
               : pg::entity_number(s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, f)));
}

Kind kind_of(const server::QcServer& s, int e) {
  if (s.sv.edicts[static_cast<size_t>(e)].free) return kNone;
  const char* cn =
      pg::vm_string(s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, pg::kFClassname)));
  if (cn == nullptr) return kNone;
  if (std::strcmp(cn, "rocket") == 0) return kRocket;
  if (std::strcmp(cn, "grenade") == 0) return kGrenade;
  if (std::strcmp(cn, "spike") == 0) {
    const char* m =
        pg::vm_string(s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, e, pg::kFModel)));
    return m != nullptr && std::strstr(m, "s_spike") != nullptr ? kHeavySpike : kSpike;
  }
  return kNone;
}

float gap_to_box(const sh::Edict& p, sh::Vec3f at) {
  float g = 0;
  const float lo[3] = {p.absmin.x, p.absmin.y, p.absmin.z},
              hi[3] = {p.absmax.x, p.absmax.y, p.absmax.z};
  const float v[3] = {at.x, at.y, at.z};
  for (int k = 0; k < 3; k++) {
    const float d = v[k] < lo[k] ? lo[k] - v[k] : (v[k] > hi[k] ? v[k] - hi[k] : 0);
    g = d > g ? d : g;
  }
  return g;
}

}  // namespace

const char* hit_weapon_name(HitWeapon w) {
  static const char* kNames[kHitWeapons] = {"scattergun",   "double",   "spiker",
                                            "heavy spiker", "grenades", "rocket direct",
                                            "rocket blast", "arc"};
  return kNames[static_cast<int>(w)];
}

void hits_frame(HitBook& b, const server::QcServer& s) {
  const pg::LayoutFields& f = s.q.layout->f;
  const int clients = s.sv.set.maxclients;
  const size_t n = s.sv.edicts.size();
  const bool first = b.players.empty();
  if (first) {
    b.players.assign(static_cast<size_t>(clients), {});
    b.given.assign(static_cast<size_t>(clients), 0);
    b.life.assign(n, 0);
    b.health.assign(n, 0);
    b.ammo.assign(n, {});
    b.weapon.assign(n, 0);
    b.at.assign(n, {});
    b.kind.assign(n, kNone);
    b.owner.assign(n, -1);
  }
  const auto count = [&](int slot, HitWeapon w) -> HitCount& {
    return b.players[static_cast<size_t>(slot)][static_cast<size_t>(w)];
  };
  // Each missile counted at most once a frame (one blast hurts several).
  std::vector<uint8_t> struck(n, 0);  // 1 a blast's hit, 2 a direct one
  for (int e = 1; e <= clients; e++) {
    const size_t i = static_cast<size_t>(e);
    const int slot = e - 1;
    const bool in = s.clients[static_cast<size_t>(slot)].active;
    const float health = field(s, e, f.health);
    const float life = (health > 0 ? health : 0) + field(s, e, f.armorvalue);
    const std::array<float, 4> ammo{field(s, e, f.ammo_shells), field(s, e, f.ammo_nails),
                                    field(s, e, f.ammo_rockets), field(s, e, f.ammo_cells)};
    const int32_t weapon = static_cast<int32_t>(field(s, e, f.weapon));
    if (!first && in) {
      // Shots: ammunition spent on the weapon held at the frame before, alive in both frames (a
      // player coming back in has his ammunition emptied, which is no shot).
      const int32_t w = health > 0 && b.health[i] > 0 ? b.weapon[i] : 0;
      const auto spent = [&](int k) {
        const float d = b.ammo[i][static_cast<size_t>(k)] - ammo[static_cast<size_t>(k)];
        return d > 0 ? static_cast<int64_t>(std::lround(d)) : 0;
      };
      if (w == kScatter) count(slot, HitWeapon::scatter).fired += 6 * spent(0);
      if (w == kDouble) count(slot, HitWeapon::dbl).fired += 14 * (spent(0) / 2);
      if (w == kSpiker) count(slot, HitWeapon::spiker).fired += spent(1);
      if (w == kHeavy) count(slot, HitWeapon::heavy).fired += spent(1) / 2;
      if (w == kGrenades) count(slot, HitWeapon::grenades).fired += spent(2);
      if (w == kRockets) {
        count(slot, HitWeapon::rocket_direct).fired += spent(2);
        count(slot, HitWeapon::rocket_blast).fired += spent(2);
      }
      if (w == kArc) count(slot, HitWeapon::arc).fired += spent(3);
      // Hits: a blow taken, from whatever last hurt him.
      const float blow = b.life[i] - life;
      const int from = entity_of(s, e, f.dmg_inflictor);
      // (A blow under 3 points is a mega health's rot, not a hit.)
      // A missile's hit proves its shot: one the ammunition did not show (fired on the frame he
      // died, or took ammunition up) is counted with it.
      const auto proven = [&](int at, HitWeapon hw) {
        HitCount& c = count(at, hw);
        if (c.fired >= c.hit) return;
        c.fired = c.hit;
        if (hw == HitWeapon::rocket_direct || hw == HitWeapon::rocket_blast) {
          HitCount& d = count(at, HitWeapon::rocket_direct);
          HitCount& bl = count(at, HitWeapon::rocket_blast);
          d.fired = bl.fired = std::max(d.fired, bl.fired);
        }
      };
      if (blow >= kLeast && from > 0 && static_cast<size_t>(from) < n) {
        const bool missile = from > clients;
        // A missile is gone by the time its blow is read: what it was and whose, a frame ago.
        const Kind k = missile ? static_cast<Kind>(b.kind[static_cast<size_t>(from)]) : kNone;
        const int by = missile ? b.owner[static_cast<size_t>(from)] : from;
        if (by >= 1 && by <= clients && by != e && (!missile || k != kNone)) {
          const int at = by - 1;
          const bool quad = (static_cast<int32_t>(field(s, by, f.items)) & kQuadBit) != 0;
          const float scale = quad ? 4.f : 1.f;
          const float team = field(s, by, f.team);
          if (team == 0 || team != field(s, e, f.team)) b.given[static_cast<size_t>(at)] += blow;
          if (!missile) {
            const int32_t held = static_cast<int32_t>(field(s, by, f.weapon));
            const auto many = [&](float each) {
              const int64_t m = static_cast<int64_t>(std::lround(blow / (each * scale)));
              return m > 0 ? m : 1;
            };
            if (held == kScatter) count(at, HitWeapon::scatter).hit += many(kPellet);
            if (held == kDouble) count(at, HitWeapon::dbl).hit += many(kPellet);
            if (held == kArc) count(at, HitWeapon::arc).hit += many(kBeam);
          } else if (struck[static_cast<size_t>(from)] != 2) {
            const uint8_t was = struck[static_cast<size_t>(from)];
            if (k == kRocket) {
              const bool direct =
                  blow == kDirect * scale ||
                  (health <= 0 &&
                   gap_to_box(s.sv.edicts[i], b.at[static_cast<size_t>(from)]) <= kNear);
              if (direct) {
                if (was == 1) count(at, HitWeapon::rocket_blast).hit--;
                count(at, HitWeapon::rocket_direct).hit++;
                proven(at, HitWeapon::rocket_direct);
                struck[static_cast<size_t>(from)] = 2;
              } else if (was == 0) {
                count(at, HitWeapon::rocket_blast).hit++;
                proven(at, HitWeapon::rocket_blast);
                struck[static_cast<size_t>(from)] = 1;
              }
            } else if (was == 0) {
              const HitWeapon w2 = k == kGrenade      ? HitWeapon::grenades
                                   : k == kHeavySpike ? HitWeapon::heavy
                                                      : HitWeapon::spiker;
              count(at, w2).hit++;
              proven(at, w2);
              struck[static_cast<size_t>(from)] = 2;
            }
          }
        }
      }
    }
    b.life[i] = life;
    b.health[i] = health;
    b.ammo[i] = ammo;
    b.weapon[i] = weapon;
  }
  // The missiles as they stand now, for the next frame's blows.
  for (size_t e = static_cast<size_t>(clients) + 1; e < n; e++) {
    const Kind k = kind_of(s, static_cast<int>(e));
    b.kind[e] = k;
    if (k == kNone) continue;
    b.at[e] = s.sv.edicts[e].origin;
    b.owner[e] = entity_of(s, static_cast<int>(e), f.owner);
  }
}

}  // namespace shooters_edge::net
