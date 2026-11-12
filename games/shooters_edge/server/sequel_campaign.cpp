// The sequel's campaign (sequel_campaign.hpp states the rules; the facts are the clean-room
// note's, porting/shooters-edge/sequel-campaign.md).
//
// ge:summary The sequel's campaign at work: help news and its screen, bodies carried, spawn points
// ge:keywords sequel campaign help-computer help-beep carried-body spawn-point coop-spot
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_campaign_test.cpp
#include "shooters_edge/server/sequel_campaign.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

#include "brush/map.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr const char* kHelpBeep = "misc/pc_up.wav";
constexpr uint32_t kEveryone = 0xffffffffu;

bool deathmatch(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }
bool coop(const SequelServer& s) { return !deathmatch(s) && s.cvars.find("coop")->on(); }

bool same_name(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    const char x = a[i] >= 'A' && a[i] <= 'Z' ? char(a[i] - 'A' + 'a') : a[i];
    const char y = b[i] >= 'A' && b[i] <= 'Z' ? char(b[i] - 'A' + 'a') : b[i];
    if (x != y) return false;
  }
  return true;
}

// The body a level is left with: health cut to its maximum.
SqBody leaving(const SqBody& b) {
  SqBody out = b;
  out.health = std::min(out.health, out.max_health);
  out.mega_until_drain = 0;
  return out;
}

void strip_keys(SqBody& b) {
  for (int i = 0; i < kSqItemCount; i++)
    if (kSqItems[i].kind == SqKind::key) b.counts[i] = 0;
}

std::string skill_name(const SequelServer& s) {
  const int k = static_cast<int>(s.cvars.find("skill")->number);
  return k == 0 ? "easy" : k == 1 ? "medium" : k == 2 ? "hard" : "hard+";
}

std::string level_name(const SequelServer& s) {
  if (s.map != nullptr && !s.map->entities.empty()) {
    const std::string m = s.map->entities[0].get("message");
    if (!m.empty()) return m;
  }
  return s.map_name;
}

void beep(SequelServer& s, int slot) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = slot + 1;
  ev.level = 2;  // the voice
  ev.origin = {float(s.players[slot].move.origin[0]) * 0.125f,
               float(s.players[slot].move.origin[1]) * 0.125f,
               float(s.players[slot].move.origin[2]) * 0.125f};
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = kHelpBeep;
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

void campaign_start(SequelServer& s) {
  if (std::find(s.sounds.begin(), s.sounds.end(), kHelpBeep) == s.sounds.end())
    s.sounds.emplace_back(kHelpBeep);
  switch (s.pending_change) {
    case MapChange::new_game:
      s.help[0].clear();
      s.help[1].clear();
      s.help_changed = 0;
      s.cross_bits = 0;
      s.spawn_name.clear();
      for (SequelPlayer& p : s.players) {
        p.carried = false;
        p.help_seen = 0;
        p.help_unread = 0;
      }
      break;
    case MapChange::change_level:
      s.spawn_name = s.next_spawn;
      for (int i = 0; i < sh::kMaxClients; i++) {
        SequelPlayer& p = s.players[i];
        p.carried = s.clients[i].active && p.alive && !deathmatch(s);
        if (!p.carried) continue;
        p.carry = leaving(p.body);
        if (s.next_unit && coop(s)) strip_keys(p.carry);
      }
      break;
    case MapChange::restart:  // as each came in
      for (SequelPlayer& p : s.players) {
        p.carry = p.entered;
        p.carried = p.has_entered;
      }
      break;
  }
  s.next_spawn.clear();
  s.next_unit = false;
  for (SequelPlayer& p : s.players) p.help_open = false;
}

const SequelEntity* campaign_spot(const SequelServer& s, int slot) {
  const std::string& name = s.spawn_name;
  if (coop(s) && slot > 0) {  // the slot-th co-op spot of the name
    int n = 0;
    for (const SequelEntity& e : s.entities)
      if (e.classname == "info_player_coop" && same_name(e.targetname, name) && ++n == slot)
        return &e;
  }
  const SequelEntity* first = nullptr;
  const SequelEntity* unnamed = nullptr;
  for (const SequelEntity& e : s.entities) {
    if (e.classname != "info_player_start") continue;
    if (first == nullptr) first = &e;
    if (!name.empty() && same_name(e.targetname, name)) return &e;
    if (e.targetname.empty() && unnamed == nullptr) unnamed = &e;
  }
  if (name.empty() && unnamed != nullptr) return unnamed;
  return first;
}

void campaign_body(SequelServer& s, int slot, bool respawn) {
  SequelPlayer& p = s.players[slot];
  if (deathmatch(s) || s.clients[slot].spectator) return;
  if (!respawn) {
    if (p.carried && p.carry.health > 0) p.body = p.carry;
    p.entered = p.body;
    p.has_entered = true;
  } else if (coop(s) && p.has_entered) {  // as he entered the level
    p.body = p.entered;
  }
  p.carried = false;
  p.health = static_cast<float>(p.body.health);
}

void campaign_frame(SequelServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    SequelPlayer& p = s.players[i];
    if (!s.clients[i].active) continue;
    if (p.help_seen != s.help_changed) {
      p.help_seen = s.help_changed;
      p.help_unread = 1;
    }
    if (p.help_unread >= 1 && p.help_unread <= 3 && s.frame_number % 64 == 0) {
      beep(s, i);
      p.help_unread++;
    }
  }
}

std::string campaign_help_screen(const SequelServer& s) {
  char counts[96];
  std::snprintf(counts, sizeof counts, "%3d/%3d     %d/%d       %d/%d", s.monsters_killed,
                s.monsters_total, s.goals_found, s.goals, s.secrets_found, s.secrets);
  std::string out = skill_name(s) + "\n" + level_name(s) + "\n\n";
  out += s.help[0] + "\n\n" + s.help[1] + "\n\n";
  out += "kills     goals     secrets\n";
  out += counts;
  return out;
}

bool campaign_command(SequelServer& s, int slot, const std::string& cmd) {
  SequelPlayer& p = s.players[slot];
  if (cmd == "putaway" || cmd == "inven") {
    p.help_open = false;
    return cmd == "putaway";
  }
  if (cmd != "help" || deathmatch(s)) return false;
  if (p.help_open && p.help_seen == s.help_changed) {
    p.help_open = false;
    return true;
  }
  p.help_open = true;
  p.help_unread = 0;
  NetEvent ev;
  ev.kind = EventKind::centerprint;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.text = campaign_help_screen(s);
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
  return true;
}

}  // namespace shooters_edge::server
