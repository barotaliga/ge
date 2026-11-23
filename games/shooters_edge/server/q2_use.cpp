// Quake II's `use` of a held item: power armour switched on or off (its sound, or a word when
// there are no cells), a power-up started or stretched (quad and invulnerability say so).
//
// ge:summary Quake II's use of power armour and the power-ups, with their sounds and words
// ge:keywords q2 use-item power-armour power-screen quad invulnerability rebreather enviro silencer
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_powers.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr uint32_t kEveryone = 0xffffffffu;  // a message to every client
constexpr int kItemChannel = 3;              // a player's item sounds

// A sound on player `slot` on his item channel, heard at the normal fall-off.
void item_sound(Q2Server& s, int slot, const char* sample) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = slot + 1;
  ev.level = kItemChannel;
  const int16_t* o = s.players[slot].move.origin;
  ev.origin = {float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f};
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = sample;
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void tell(Q2Server& s, int slot, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.level = 2;
  ev.text = std::move(text);
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

bool fight_use(Q2Server& s, int slot, int item) {
  if (item < 0 || item >= kSqItemCount || slot < 0 || slot >= sh::kMaxClients) return false;
  if (!s.clients[slot].active || s.clients[slot].spectator || !s.players[slot].alive) return false;
  Q2Player& p = s.players[slot];
  const SqItem& it = kSqItems[item];
  if (p.body.counts[item] <= 0) return false;
  if (it.kind == SqKind::power_armour) {
    if (!p.power_on && p.body.ammo[static_cast<int>(SqAmmo::cells)] <= 0) {
      tell(s, slot, "No cells for power armor.\n");
      return true;
    }
    p.power_on = !p.power_on;
    item_sound(s, slot, p.power_on ? "misc/power1.wav" : "misc/power2.wav");
    return true;
  }
  if (it.kind != SqKind::powerup) return false;
  p.body.counts[item]--;
  const auto extend = [&s, &it](double& until) {
    until = std::max(until, s.level_time) + it.amount;
  };
  const std::string_view name = it.classname;
  if (const SqPower* pw = sq_power(it.classname)) {
    extend(p.*pw->until);
    if (pw->use_sound != nullptr) item_sound(s, slot, pw->use_sound);
  } else if (name == "item_silencer") {
    p.silencer_shots += it.amount;
  } else if (name == "ammo_nuke") {  // Ground Zero's A-M bomb, thrown
    gz_throw_nuke(s, slot);
  }
  return true;
}

int fight_power_save(Q2Server& s, int slot, sh::Vec3f dir, int damage) {
  Q2Player& p = s.players[slot];
  const int screen = p.body.counts[sq_item("item_power_screen")];
  const int shield = p.body.counts[sq_item("item_power_shield")];
  int& cells = p.body.ammo[static_cast<int>(SqAmmo::cells)];
  if (!p.power_on || (screen == 0 && shield == 0) || cells <= 0) return 0;
  int part = damage / 3, per_cell = 1;  // the screen's third, from in front
  if (shield > 0) {
    part = damage * 2 / 3;
    per_cell = 2;
  } else {
    const sh::ViewAxes ax = sh::angle_vectors({0, p.view_angles.y, 0});
    if (sh::dot(ax.forward, dir * -1.0f) <= 0.3f) return 0;  // a hit from the side or behind
  }
  const int saved = std::min(part, cells * per_cell);
  if (saved <= 0) return 0;
  cells -= saved / per_cell;
  p.blow_parts[2] += saved;  // the screen's green flash (q2_blend.hpp)
  const int16_t* o = p.move.origin;
  const sh::Vec3f at = {float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f};
  p.power_flash = fight_power_hit(s, at - dir * 16.0f, dir * -1.0f, shield > 0);
  return saved;
}

double fight_power_hit(Q2Server& s, sh::Vec3f at, sh::Vec3f normal, bool shield) {
  fight_effect(s, shield ? "te_shield_sparks" : "te_screen_sparks", at, normal);
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.level = 0;  // no entity's channel: a sound at a point
  ev.origin = at;
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = "weapons/lashit.wav";
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
  return s.level_time + kSqPowerFlashS;
}

void fight_power_frame(Q2Server& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2Player& p = s.players[i];
    if (!s.clients[i].active || !p.power_on || p.body.ammo[static_cast<int>(SqAmmo::cells)] != 0)
      continue;
    if (p.body.counts[sq_item("item_power_screen")] <= 0 &&
        p.body.counts[sq_item("item_power_shield")] <= 0)
      continue;
    p.power_on = false;  // out of cells: off, and off it stays until used again
    item_sound(s, i, "misc/power2.wav");
  }
}

}  // namespace shooters_edge::server
