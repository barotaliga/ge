// Quake II's dropped items (q2_fight.hpp): a dropped weapon, ammo or powerup falling, sliding
// and lying where it lands, taken by a player who touches it, and an item thrown up at a point.
//
// ge:summary Quake II's dropped items: the fall, the slide, the touch that takes them
// ge:keywords q2 drop dropped-item toss touch pickup fight_drops fight_touch_drops
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "brush/trace.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr int32_t kWorldMask = sh::kSqSolid | sh::kSqWindow;
constexpr float kItemHalf = 15;              // a dropped item's box, ±15 each way
constexpr uint32_t kEveryone = 0xffffffffu;  // a message to every client

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }
int skill(const Q2Server& s) {
  return std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
}
bool fightable(const Q2Server& s, int i) {
  return s.clients[i].active && !s.clients[i].spectator && s.players[i].alive;
}
sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

// The item's pick-up sound from player `ent`'s place, on the weapon's channel.
void sound(Q2Server& s, int ent, sh::Vec3f at, const char* sample) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = ent;
  ev.level = 1;
  ev.origin = at;
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = sample;
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

void fight_drops(Q2Server& s) {
  const float gravity = static_cast<float>(s.cvars.find("sv_gravity")->number);
  const float lo[3] = {-kItemHalf, -kItemHalf, -kItemHalf};
  const float hi[3] = {kItemHalf, kItemHalf, kItemHalf};
  for (Q2Drop& d : s.drops) {
    if (!d.live) continue;
    if (d.dies_at > 0 && s.level_time + 0.001 >= d.dies_at) {
      d.live = false;
      continue;
    }
    if (d.resting) continue;
    q2_hold_speed(s, d.velocity);
    d.velocity.z -= gravity * s.step;
    const sh::Vec3f to = d.origin + d.velocity * s.step;
    const float a[3] = {d.origin.x, d.origin.y, d.origin.z}, b[3] = {to.x, to.y, to.z};
    const ge::brush::Trace t = s.tracer->trace(a, b, lo, hi, kWorldMask);
    if (t.start_solid || t.all_solid) {
      d.velocity = {};
      d.resting = true;
      continue;
    }
    d.origin = {t.end[0], t.end[1], t.end[2]};
    if (t.fraction >= 1) continue;
    const sh::Vec3f n{t.plane.n[0], t.plane.n[1], t.plane.n[2]};
    if (n.z > 0.7f) {  // a floor: it lies there
      d.velocity = {};
      d.resting = true;
    } else {  // a wall: it slides along
      d.velocity = d.velocity - n * sh::dot(d.velocity, n);
    }
  }
}

void fight_touch_drops(Q2Server& s, int slot) {
  if (!fightable(s, slot)) return;
  Q2Player& p = s.players[slot];
  const sh::Vec3f at = unpack(p.move.origin);
  const sh::Vec3f lo = at + p.mins, hi = at + p.maxs;
  for (Q2Drop& d : s.drops) {
    if (!d.live || (d.owner == slot && s.level_time + 0.001 < d.takeable_at)) continue;
    if (lo.x > d.origin.x + kItemHalf || hi.x < d.origin.x - kItemHalf ||
        lo.y > d.origin.y + kItemHalf || hi.y < d.origin.y - kItemHalf ||
        lo.z > d.origin.z + kItemHalf || hi.z < d.origin.z - kItemHalf)
      continue;
    const SqItem& it = kSqItems[d.item];
    const bool power = d.power_left > 0 || d.shots > 0;
    if (power) {  // a dropped powerup is used at once with the time (or shots) it had left
      const auto add = [&](double& until) { until = std::max(until, s.level_time) + d.power_left; };
      if (d.item == sq_item("item_quad")) add(p.quad_until);
      if (d.item == sq_item("item_invulnerability")) add(p.invulnerable_until);
      if (d.item == sq_item("item_enviro")) add(p.suit_until);
      if (d.item == sq_item("item_breather")) add(p.breather_until);
      p.silencer_shots += d.shots;
    } else if (it.kind == SqKind::ammo && d.count > 0) {
      const int k = static_cast<int>(it.ammo);
      if (p.body.ammo[k] >= p.body.max_ammo[k]) continue;
      p.body.ammo[k] = std::min(p.body.ammo[k] + d.count, p.body.max_ammo[k]);
    } else {
      p.body.health = static_cast<int>(p.health);
      if (!sq_pick_up(d.item, p.body, deathmatch(s), skill(s), s.level_time)) continue;
      p.health = static_cast<float>(p.body.health);
    }
    d.live = false;
    p.taken.push_back(d.item);
    sound(s, slot + 1, at, it.sound);
    if (!power) fight_took(s, slot, d.item);
  }
}

void fight_drop_at(Q2Server& s, sh::Vec3f at, const std::string& classname) {
  const int item = sq_item(classname);
  if (item < 0) return;
  for (Q2Drop& d : s.drops) {
    if (d.live) continue;
    d = Q2Drop{};
    d.live = true;
    d.item = item;
    d.origin = at;
    d.velocity = {0, 0, 300};
    return;
  }
}

}  // namespace shooters_edge::server
