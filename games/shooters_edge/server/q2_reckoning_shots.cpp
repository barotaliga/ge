// The Reckoning's shots that think (q2_reckoning.hpp states the rules; the numbers are the
// clean-room notes', porting/shooters-edge/q2-reckoning-rules.md §§ 2 and 5 and
// q2-reckoning-monsters.md § 5): the ion ripper's end, the heat rocket's aim, the trap.
//
// ge:summary The Reckoning's thinking shots: the ion ripper's end, the heat rocket, the trap
// ge:keywords q2 reckoning trap food-cube heat-rocket homing ion-ripper welding-sparks
// ge:skill shooter
// ge:test tests/shooters_edge_q2_reckoning_test.cpp
#include <string>

#include "core/prng.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_kinds.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_reckoning.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr double kThink = 0.1;
constexpr double kTrapLife = 30;
constexpr float kPullReach = 256, kCatchReach = 32, kHomingReach = 1024;
constexpr int kTrapGlow = 0x02000000;
constexpr uint32_t kEveryone = 0xffffffffu;

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

sh::Vec3f origin_of(const Q2Player& p) {
  return {p.move.origin[0] / 8.0f, p.move.origin[1] / 8.0f, p.move.origin[2] / 8.0f};
}

bool alive_player(const Q2Server& s, int i) {
  return s.clients[i].active && !s.clients[i].spectator && s.players[i].alive;
}

// Whether `b` is in sight from `a` (no wall between).
bool sees(const Q2Server& s, sh::Vec3f a, sh::Vec3f b) {
  return map_trace(s, a, {}, {}, b, sh::kSqSolid, -1).fraction >= 1;
}

void sound(Q2Server& s, sh::Vec3f at, const char* sample, float attenuation) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.origin = at;
  ev.values[0] = 255;
  ev.values[1] = attenuation;
  ev.text = sample;
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

// The prey's place: a player's origin, a monster's entity's.
sh::Vec3f prey_at(const Q2Server& s, int prey) {
  if (prey >= 0) return origin_of(s.players[prey]);
  return s.entities[size_t(-2 - prey)].origin;
}

// The nearest living player or monster the trap sees within its reach: the first found, each
// later one taken when nearer than the last taken (so the first stands only alone).
int choose_prey(const Q2Server& s, const Q2Missile& m) {
  int chosen = -1;
  bool first = true;
  float best = 8000;
  const auto offer = [&](int who, sh::Vec3f at) {
    const float d = sh::length(at - m.origin);
    if (d > kPullReach || !sees(s, m.origin, at)) return;
    if (first) {
      chosen = who;
      first = false;
    } else if (d < best) {
      chosen = who;
      best = d;
    }
  };
  for (int i = 0; i < sh::kMaxClients; i++)
    if (alive_player(s, i)) offer(i, origin_of(s.players[i]));
  for (const Q2Monster& mo : s.monsters) {
    const Q2Entity& e = s.entities[size_t(mo.entity)];
    if (!mo.dead && !e.gone && e.health > 0) offer(-2 - mo.entity, e.origin);
  }
  return chosen;
}

// The prey within reach: under 400 mass it dies whatever protects it but god mode; heavier, the
// trap bursts harmlessly.
void catch_prey(Q2Server& s, Q2Missile& m) {
  const int prey = m.prey;
  int mass = 200;  // a player's
  bool gekk = false;
  if (prey < -1) {
    for (const Q2Monster& mo : s.monsters)
      if (mo.entity == -2 - prey) {
        mass = roster::kKinds[size_t(mo.kind)].mass;
        gekk = s.entities[size_t(mo.entity)].classname == "monster_gekk";
      }
  }
  if (mass >= 400) {
    fight_effect(s, "te_explosion1", m.origin);
    m.live = false;
    return;
  }
  const sh::Vec3f dir = prey_at(s, prey) - m.origin;
  if (prey >= 0) {
    if (!s.players[prey].god) fight_hurt(s, prey, m.owner, dir, 100000, 1, m.weapon, 2);
  } else {
    monster_damage(s, -2 - prey, m.owner, 100000, dir, false);
  }
  m.frame = 5;
  m.feed = 62;
  m.prey_mass = mass;
  m.prey_gekk = gekk;
  m.effects = kTrapGlow;
  m.dies_at = s.level_time + kTrapLife;
  sound(s, m.origin, "weapons/trapdown.wav", 2);
}

// A fed trap's think: the count down, the last frames, the food cube, gone a second later.
void feed(Q2Server& s, Q2Missile& m) {
  if (m.frame >= 8) {
    m.live = false;  // a second after the cube
    return;
  }
  if (m.feed > 18) {
    m.feed -= 2;
    return;
  }
  m.frame++;
  if (m.frame < 8) return;
  if (deathmatch(s) && fight_anyflag(s, 1)) {  // no health in this match
    m.think_at = s.level_time + 1;
    return;
  }
  if (Q2Drop* d = fight_drop_at(s, m.origin + sh::Vec3f{0, 0, 16}, "foodcube")) {
    d->count = deathmatch(s) ? m.prey_mass / 4 : m.prey_mass / 10;
    d->velocity = {0, 0, 400};
  }
  m.think_at = s.level_time + 1;
}

}  // namespace

void reckoning_trap_launch(Q2Server& s, Q2Missile& m, sh::Vec3f forward, sh::Vec3f right,
                           sh::Vec3f up, float speed) {
  const float lift = 200 + static_cast<float>(ge::core::range(s.rng, -10.0, 10.0));
  const float across = static_cast<float>(ge::core::range(s.rng, -10.0, 10.0));
  m.velocity = forward * speed + up * lift + right * across;
  m.dies_at = s.level_time + kTrapLife;
  m.think_at = s.level_time + 1;
  m.effects = 0;
}

void reckoning_missile_end(Q2Server& s, Q2Missile& m) {
  if (m.kind == SqShot::ripper) {  // sparks of none, palette 228..231
    NetEvent ev;
    ev.kind = EventKind::point_effect;
    ev.to = kEveryone;
    ev.origin = m.origin;
    ev.level = 228 + ge::core::int_in(s.rng, 0, 3);
    ev.text = "te_welding_sparks";
    ev.time = s.level_time;
    s.out.events.push_back(std::move(ev));
    s.out.events_total++;
  } else if (m.kind == SqShot::trap) {
    fight_effect(s, "te_explosion1", m.origin);
  }
}

bool reckoning_missile_think(Q2Server& s, Q2Missile& m) {
  if (m.kind == SqShot::heat_rocket) {
    if (s.level_time + 0.001 < m.think_at) return false;
    m.think_at = s.level_time + kThink;
    sh::Vec3f way = m.velocity;
    if (sh::normalize(way) <= 0) return false;
    int best = -1;
    float nearest = kHomingReach;
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (!alive_player(s, i)) continue;
      const sh::Vec3f to = origin_of(s.players[i]) - m.origin;
      const float d = sh::length(to);
      if (d >= nearest || sh::dot(to, way) <= 0 || !sees(s, m.origin, origin_of(s.players[i])))
        continue;
      nearest = d;
      best = i;
    }
    if (best >= 0) {
      sh::Vec3f to = origin_of(s.players[best]) - m.origin;
      sh::normalize(to);
      m.velocity = to * 500.0f;
    }
    return false;
  }
  if (m.kind != SqShot::trap) return false;
  if (!m.resting) return false;  // flying: a grenade's flight
  if (s.level_time + 0.001 < m.think_at) return true;
  m.think_at = s.level_time + kThink;
  if (m.feed > 0) {
    feed(s, m);
    return true;
  }
  if (m.frame < 4) {
    m.frame++;
  } else {
    m.effects = kTrapGlow;
  }
  const int prey = choose_prey(s, m);
  m.prey = prey;
  if (prey == -1) return true;
  const sh::Vec3f at = prey_at(s, prey);
  if (sh::length(at - m.origin) < kCatchReach) {
    catch_prey(s, m);
    return true;
  }
  sh::Vec3f pull = m.origin - at;
  sh::normalize(pull);
  if (prey >= 0) {
    fight_kick(s, prey, pull * 250.0f);
  } else {
    for (Q2Monster& mo : s.monsters)
      if (mo.entity == -2 - prey) {
        mo.velocity = pull * 256.0f;
        mo.on_ground = false;
        s.entities[size_t(mo.entity)].origin.z += 1;
      }
  }
  sound(s, m.origin, "weapons/trapsuck.wav", 2);
  return true;
}

}  // namespace shooters_edge::server
