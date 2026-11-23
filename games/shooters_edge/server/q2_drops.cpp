// Quake II's dropped items (q2_fight.hpp): an item thrown by `drop` or at death, falling,
// sliding and lying where it lands, taken by a player who touches it, and an item thrown up at a
// point.
//
// ge:summary Quake II's dropped items: the throw, the fall, the slide, the touch that takes them
// ge:keywords q2 drop dropped-item toss touch pickup fight_drops fight_touch_drops death-drops
// ge:keywords not-dropable coop-stay weapons-stay
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "brush/trace.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr int32_t kWorldMask = sh::kSqSolid | sh::kSqWindow;
constexpr float kItemHalf = 16;              // a dropped item's box, ±16 each way (a map's ±15)
constexpr uint32_t kEveryone = 0xffffffffu;  // a message to every client

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }
bool coop(const Q2Server& s) { return !deathmatch(s) && s.cvars.find("coop")->on(); }
int skill(const Q2Server& s) {
  return std::clamp(static_cast<int>(s.cvars.find("skill")->number), 0, 3);
}
bool fightable(const Q2Server& s, int i) {
  return s.clients[i].active && !s.clients[i].spectator && s.players[i].alive;
}
sh::Vec3f unpack(const int32_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

// The item's pick-up sound from player `ent`'s place, on the weapon's channel (an item's own on
// channel 3).
void sound(Q2Server& s, int ent, sh::Vec3f at, const char* sample, int channel = 1) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = ent;
  ev.level = channel;
  ev.origin = at;
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = sample;
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

// Item `item` thrown from player `slot`: 100 forward along his view turned `turn` degrees, 300
// up, from 24 ahead along his whole view (its pitch too) and 16 below his origin, pulled back to
// where a box of ±16 traced from his origin stops; his to take back after 1 s, gone after 30 s in
// deathmatch. Null when every drop's row is in use.
Q2Drop* toss(Q2Server& s, int slot, int item, float turn) {
  Q2Drop* d = nullptr;
  for (Q2Drop& free : s.drops)
    if (!free.live) {
      d = &free;
      break;
    }
  if (d == nullptr) return nullptr;
  const Q2Player& p = s.players[slot];
  const sh::Vec3f origin = unpack(p.move.origin);
  const sh::ViewAxes ax = sh::angle_vectors(p.view_angles + sh::Vec3f{0, turn, 0});
  sh::Vec3f start = origin + ax.forward * 24;
  start.z -= 16;
  const float a[3] = {origin.x, origin.y, origin.z}, b[3] = {start.x, start.y, start.z};
  const float lo[3] = {-kItemHalf, -kItemHalf, -kItemHalf};
  const float hi[3] = {kItemHalf, kItemHalf, kItemHalf};
  const ge::brush::Trace t = s.tracer->trace(a, b, lo, hi, kWorldMask);
  if (t.fraction < 1) start = {t.end[0], t.end[1], t.end[2]};
  *d = Q2Drop{};
  d->live = true;
  d->item = item;
  d->owner = slot;
  d->origin = start;
  d->velocity = ax.forward * 100;
  d->velocity.z = 300;
  d->takeable_at = s.level_time + 1;
  d->dies_at = deathmatch(s) ? s.level_time + 30 : 0;
  return d;
}

// Whether item `item` stays where it lies in co-op once taken: every weapon and key, the
// rebreather and the environment suit.
bool coop_stays(int item) {
  const SqItem& it = kSqItems[item];
  const std::string_view cn = it.classname;
  return it.kind == SqKind::weapon || it.kind == SqKind::key || cn == "item_breather" ||
         cn == "item_enviro";
}

}  // namespace

bool fight_droppable(const Q2Server& s, int item) {
  if (item < 0 || item >= kSqItemCount) return false;
  switch (kSqItems[item].kind) {
    case SqKind::weapon:
    case SqKind::ammo:
    case SqKind::powerup:
    case SqKind::key:
    case SqKind::power_armour:
      return !(coop(s) && coop_stays(item));
    default:
      return false;
  }
}

bool fight_drop(Q2Server& s, int slot, int item) {
  if (item < 0 || item >= kSqItemCount || !fightable(s, slot)) return false;
  if (!fight_droppable(s, item)) {
    fight_print(s, slot, "Item is not dropable.\n");
    return false;
  }
  Q2Player& p = s.players[slot];
  const SqItem& it = kSqItems[item];
  switch (it.kind) {
    case SqKind::weapon: {
      if (fight_anyflag(s, 4)) return false;  // weapons stay: nothing said
      if (p.body.counts[item] <= 0) return false;
      const int w = sq_weapon_of_item(item);
      if ((w == p.hand.weapon || w == p.hand.pending) && p.body.counts[item] == 1) {
        fight_print(s, slot, "Can't drop current weapon\n");
        return false;
      }
      Q2Drop* d = toss(s, slot, item, 0);
      if (d == nullptr) return false;
      d->bare = true;  // a weapon thrown so carries no ammunition
      p.body.counts[item]--;
      return true;
    }
    case SqKind::ammo: {
      int& held = p.body.ammo[static_cast<int>(it.ammo)];
      const int n = std::min(held, it.amount);
      if (n <= 0) return false;
      if (p.hand.weapon == int(SqWeapon::hand_grenade) && it.ammo == SqAmmo::grenades &&
          held - n <= 0) {
        fight_print(s, slot, "Can't drop current weapon\n");
        return false;
      }
      Q2Drop* d = toss(s, slot, item, 0);
      if (d == nullptr) return false;
      d->count = n;
      held -= n;
      return true;
    }
    default: {  // a powerup, a key, power armour: one thrown
      if (p.body.counts[item] <= 0) return false;
      // The last power armour, switched on, is switched off first.
      if (it.kind == SqKind::power_armour && p.power_on && p.body.counts[item] == 1) {
        p.power_on = false;
        sound(s, slot + 1, unpack(p.move.origin), "misc/power2.wav", 3);
      }
      if (toss(s, slot, item, 0) == nullptr) return false;
      p.body.counts[item]--;
      return true;
    }
  }
}

void fight_death_drops(Q2Server& s, int slot) {
  if (!deathmatch(s)) return;
  Q2Player& p = s.players[slot];
  const SqWeaponRow& r = kSqWeapons[p.hand.weapon];
  struct Toss {
    double left;  // a power's time left, s (0: not a power)
    int item;
    int shots;  // the silencer's shots left
  };
  Toss out[6];
  int n = 0;
  if (r.item != nullptr && r.ammo != SqAmmo::none && p.body.ammo[static_cast<int>(r.ammo)] > 0)
    out[n++] = {0, sq_item(r.item), 0};
  const auto on = [&](const char* name) { return s.cvars.find(name)->on(); };
  const auto timed = [&](bool wanted, double until, const char* item) {
    if (wanted && until > s.level_time + 1) out[n++] = {until - s.level_time, sq_item(item), 0};
  };
  timed(fight_dmflag(s, 16384) || on("sv_dropquad"), p.quad_until, "item_quad");
  timed(on("sv_droppent"), p.invulnerable_until, "item_invulnerability");
  timed(on("sv_dropsuit"), p.suit_until, "item_enviro");
  timed(on("sv_dropbreather"), p.breather_until, "item_breather");
  if (on("sv_dropsilencer") && p.silencer_shots > 0)
    out[n++] = {0, sq_item("item_silencer"), p.silencer_shots};
  for (int i = 0; i < n; i++) {
    Q2Drop* d = toss(s, slot, out[i].item, (float(i) - float(n - 1) * 0.5f) * 45.0f);
    if (d == nullptr) continue;
    d->power_left = out[i].left;
    d->shots = out[i].shots;
  }
}

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
    const int grenades = p.body.ammo[static_cast<int>(SqAmmo::grenades)];
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
    } else if (it.kind == SqKind::weapon && d.bare) {  // thrown by `drop`: the weapon alone
      p.body.counts[d.item]++;
    } else {
      p.body.health = static_cast<int>(p.health);
      if (!sq_pick_up(d.item, p.body, deathmatch(s), skill(s), s.level_time)) continue;
      p.health = static_cast<float>(p.body.health);
      if (it.kind == SqKind::weapon && fight_anyflag(s, 8192)) {  // infinite ammo: a full stock
        const int k = static_cast<int>(it.ammo);
        p.body.ammo[k] = std::max(p.body.ammo[k], std::min(1000, p.body.max_ammo[k]));
      }
    }
    d.live = false;
    p.taken.push_back(d.item);
    sound(s, slot + 1, at, it.sound);
    if (!power) fight_took(s, slot, d.item, grenades);
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
