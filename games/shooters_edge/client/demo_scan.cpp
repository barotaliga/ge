// A demo scanned for its kills (demo_scan.hpp).
//
// ge:summary scan_demo: deaths, frags matched into kills, rockets followed, each player's recent
// ge:summary heights and stacks
// ge:keywords scan deaths frags kills rockets airborne stack history timeline
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#include "shooters_edge/client/demo_scan.hpp"

#include <algorithm>
#include <cmath>

#include "shooters_edge/client/timedemo.hpp"

namespace shooters_edge::client {

namespace {

namespace sh = ge::shooter;

constexpr int kSlots = 32;
constexpr int kWeaponBits[hud::kGuns] = {1, 2, 4, 8, 16, 32, 64};  // the active-weapon stat's bits

hud::Weapon weapon_of(int32_t active) {
  for (int k = 0; k < hud::kGuns; k++)
    if (active == kWeaponBits[k]) return static_cast<hud::Weapon>(k + 1);
  return hud::Weapon::melee;
}

float dist(sh::Vec3f a, sh::Vec3f b) {
  const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return static_cast<float>(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));
}

// One frame of a player, kept for the look back at his death.
struct Past {
  double t = 0;
  sh::Vec3f origin;
  int stack = 0, armour = 0;
};

// What the scan knows at one death, before its killer is matched.
struct Death {
  double t = 0;
  int victim = 0;
  bool gib = false;
  std::array<sh::Vec3f, kSlots> origins{};
  std::array<int32_t, kSlots> weapons{};
  std::array<double, kSlots> jumped{};  // each player's last jump across the map, timeline s
  sh::Vec3f victim_at;                  // where he stood the frame before
  bool airborne = false;
  int stack = 0, stack_armour = 0;
  double stack_s = 0;
};

struct Gain {
  double t = 0;
  int slot = 0;
  int left = 0;  // frags not yet given to a death
};

struct Rocket {
  sh::Vec3f first, last;
  double t_end = 0;
};

}  // namespace

bool scan_demo(const Demo& opened, DemoScan& out, std::string& error) {
  out = DemoScan{};
  Timedemo t;
  t.demo = opened;
  const Demo& d = t.demo;
  const DemoState& s = d.state;
  out.format = s.format;

  std::array<std::vector<Past>, kSlots> past;
  std::array<bool, kSlots> was_dead{};
  std::array<int32_t, kSlots> was_health{};
  std::array<int32_t, kSlots> was_frags{};
  std::array<double, kSlots> jumped;
  jumped.fill(-1e9);
  std::vector<Death> deaths;
  std::vector<Gain> gains;
  std::vector<Rocket> rockets;
  std::vector<int> live_rocket;  // by entity number: an index into `flying` + 1 (0: none)
  std::vector<Rocket> flying;
  std::vector<int> flying_entity;

  const auto stat_of = [&](int slot, int stat) -> int32_t {
    if (s.format == DemoFormat::multiview && static_cast<size_t>(slot) < d.player_stats.size())
      return d.player_stats[static_cast<size_t>(slot)][static_cast<size_t>(stat)];
    if (slot == s.player) return s.stats[stat];
    return -1;  // unknown
  };

  for (;;) {
    const TimedemoStep st = timedemo_step(t, error);
    if (st == TimedemoStep::error) return false;
    if (st == TimedemoStep::end) break;
    const double now = t.timeline.t;
    out.frames++;
    if (st == TimedemoStep::new_level) {
      out.levels++;
      was_dead.fill(false);
      was_health.fill(0);
      was_frags = s.frags;
      for (auto& p : past) p.clear();
      flying.clear();
      flying_entity.clear();
      live_rocket.clear();
    }
    out.tracked = s.player;
    // The rockets in flight: each missile entity followed while it is in the list.
    live_rocket.resize(s.entities.size(), 0);
    std::vector<bool> seen(flying.size(), false);
    for (size_t n = 1; n < s.entities.size(); n++) {
      const DemoEntity& e = s.entities[n];
      const bool missile =
          e.present && e.model > 0 && static_cast<size_t>(e.model) < s.models.size() &&
          s.models[static_cast<size_t>(e.model)].find("missile") != std::string::npos;
      int& at = live_rocket[n];
      if (!missile) continue;
      if (at == 0) {
        flying.push_back({e.origin, e.origin, now});
        flying_entity.push_back(static_cast<int>(n));
        seen.push_back(true);
        at = static_cast<int>(flying.size());
        out.rockets++;
      } else {
        flying[static_cast<size_t>(at - 1)].last = e.origin;
        flying[static_cast<size_t>(at - 1)].t_end = now;
        seen[static_cast<size_t>(at - 1)] = true;
      }
    }
    for (size_t i = flying.size(); i-- > 0;) {  // gone this frame: landed
      if (seen[i]) continue;
      rockets.push_back(flying[i]);
      rockets.back().t_end = now;
      live_rocket[static_cast<size_t>(flying_entity[i])] = 0;
      flying.erase(flying.begin() + static_cast<std::ptrdiff_t>(i));
      flying_entity.erase(flying_entity.begin() + static_cast<std::ptrdiff_t>(i));
      for (int& a : live_rocket)
        if (a > static_cast<int>(i) + 1) a--;
    }

    for (int k = 0; k < kSlots && static_cast<size_t>(k) < s.players.size(); k++) {
      const DemoEntity& p = s.players[static_cast<size_t>(k)];
      if (!p.present) continue;
      out.players |= 1u << k;
      if (!s.names[static_cast<size_t>(k)].empty())
        out.names[static_cast<size_t>(k)] = s.names[static_cast<size_t>(k)];
      const int32_t health = stat_of(k, kStatHealth);
      const int32_t armour = stat_of(k, kStatArmour);
      const bool dead =
          p.dead || (health >= 0 && health <= 0 && was_health[static_cast<size_t>(k)] > 0);
      std::vector<Past>& h = past[static_cast<size_t>(k)];
      if (!h.empty() && dist(p.origin, h.back().origin) >= kTeleportJump)
        jumped[static_cast<size_t>(k)] = now;
      if (dead && !was_dead[static_cast<size_t>(k)]) {
        Death dd;
        dd.t = now;
        dd.victim = k;
        dd.gib = p.gib;
        for (int j = 0; j < kSlots && static_cast<size_t>(j) < s.players.size(); j++) {
          dd.origins[static_cast<size_t>(j)] = s.players[static_cast<size_t>(j)].origin;
          dd.weapons[static_cast<size_t>(j)] = stat_of(j, kStatActiveWeapon);
        }
        dd.jumped = jumped;
        dd.victim_at = h.empty() ? p.origin : h.back().origin;
        // In the air: well over the lowest he stood lately, and moving up or down.
        float floor = dd.victim_at.z;
        for (const Past& q : h)
          if (now - q.t <= kAirLookS) floor = std::min(floor, q.origin.z);
        float vz = 0;
        for (size_t i = h.size(); i-- > 0;)
          if (now - h[i].t >= 0.1 && i + 1 < h.size()) {
            const double dt = h.back().t - h[i].t;
            if (dt > 0) vz = static_cast<float>((h.back().origin.z - h[i].origin.z) / dt);
            break;
          }
        dd.airborne = dd.victim_at.z - floor >= kAirHeight && std::fabs(vz) >= kAirSpeed;
        // His stack at its most lately, and the latest moment it stood there.
        for (const Past& q : h) {
          if (now - q.t > kStackLookS) continue;
          if (q.stack >= dd.stack) {
            dd.stack = q.stack;
            dd.stack_armour = q.armour;
            dd.stack_s = now - q.t;
          }
        }
        deaths.push_back(dd);
        out.deaths++;
      }
      was_dead[static_cast<size_t>(k)] = dead;
      if (health >= 0) was_health[static_cast<size_t>(k)] = health;
      if (!dead) {
        h.push_back(
            {now, p.origin, health >= 0 ? health + std::max(armour, 0) : 0, std::max(armour, 0)});
        while (!h.empty() && now - h.front().t > kStackLookS + 0.5) h.erase(h.begin());
      }
    }
    for (int k = 0; k < kSlots; k++) {
      const int32_t gain = s.frags[static_cast<size_t>(k)] - was_frags[static_cast<size_t>(k)];
      if (gain > 0) gains.push_back({now, k, gain});
      was_frags[static_cast<size_t>(k)] = s.frags[static_cast<size_t>(k)];
    }
  }
  for (const Rocket& r : flying) rockets.push_back(r);
  out.length_s = t.timeline.t;

  // Each death given the nearest frag in time of another player within kKillMatchS.
  for (const Death& dd : deaths) {
    DemoKill k;
    k.t = dd.t;
    k.victim = dd.victim;
    k.gib = dd.gib;
    k.airborne = dd.airborne;
    k.stack = dd.stack;
    k.stack_armour = dd.stack_armour;
    k.stack_s = dd.stack_s;
    Gain* best = nullptr;
    for (Gain& g : gains) {
      if (g.left <= 0 || g.slot == dd.victim || std::fabs(g.t - dd.t) > kKillMatchS) continue;
      if (best == nullptr || std::fabs(g.t - dd.t) < std::fabs(best->t - dd.t)) best = &g;
    }
    if (best == nullptr) {
      out.unmatched++;
    } else {
      best->left--;
      k.killer = best->slot;
      k.weapon = weapon_of(dd.weapons[static_cast<size_t>(k.killer)]);
      k.distance = dist(dd.origins[static_cast<size_t>(k.killer)], dd.victim_at);
      k.telefrag = k.distance <= kTelefragReach &&
                   dd.t - dd.jumped[static_cast<size_t>(k.killer)] <= kTelefragS;
      if (k.weapon == hud::Weapon::rocket_launcher) {
        float nearest = kRocketReach;
        for (const Rocket& r : rockets) {
          if (std::fabs(r.t_end - dd.t) > kRocketWindowS) continue;
          const float dv = dist(r.last, dd.victim_at);
          if (dv <= nearest) {
            nearest = dv;
            k.rocket_flight = dist(r.first, r.last);
          }
        }
      }
    }
    out.kills.push_back(k);
  }
  return true;
}

}  // namespace shooters_edge::client
