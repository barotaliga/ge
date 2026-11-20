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
#include "shooters_edge/server/q2_items.hpp"
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
  if (name == "item_quad") {
    extend(p.quad_until);
    item_sound(s, slot, "items/damage.wav");
  } else if (name == "item_invulnerability") {
    extend(p.invulnerable_until);
    item_sound(s, slot, "items/protect.wav");
  } else if (name == "item_silencer") {
    p.silencer_shots += it.amount;
  } else if (name == "item_breather") {
    extend(p.breather_until);
  } else if (name == "item_enviro") {
    extend(p.suit_until);
  }
  return true;
}

}  // namespace shooters_edge::server
