// Quake II's campaign (q2_campaign.hpp states the rules; the facts are the clean-room
// note's, porting/shooters-edge/q2-campaign.md).
//
// ge:summary Quake II's campaign at work: help news and its screen, bodies carried, spawn points
// ge:keywords q2 campaign help-computer help-beep carried-body spawn-point coop-spot
// ge:skill shooter
// ge:test tests/shooters_edge_q2_campaign_test.cpp
#include "shooters_edge/server/q2_campaign.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

#include "brush/map.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr const char* kHelpBeep = "misc/pc_up.wav";
constexpr uint32_t kEveryone = 0xffffffffu;

bool deathmatch(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }
bool coop(const Q2Server& s) { return !deathmatch(s) && s.cvars.find("coop")->on(); }

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

std::string skill_name(const Q2Server& s) {
  const int k = static_cast<int>(s.cvars.find("skill")->number);
  return k == 0 ? "easy" : k == 1 ? "medium" : k == 2 ? "hard" : "hard+";
}

std::string level_name(const Q2Server& s) {
  if (s.map != nullptr && !s.map->entities.empty()) {
    const std::string m = s.map->entities[0].get_any_case("message");
    if (!m.empty()) return m;
  }
  return s.map_name;
}

void beep(Q2Server& s, int slot) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = kEveryone;
  ev.ent = slot + 1;
  ev.level = 2;  // the voice
  ev.origin = {float(s.players[slot].move.origin[0]) * 0.125f,
               float(s.players[slot].move.origin[1]) * 0.125f,
               float(s.players[slot].move.origin[2]) * 0.125f};
  ev.values[0] = 255;
  ev.values[1] = 3;  // static: heard close by only
  ev.text = kHelpBeep;
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

LevelString level_string(const std::string& text) {
  LevelString out;
  size_t from = 0;
  while (from <= text.size()) {
    const size_t plus = std::min(text.find('+', from), text.size());
    std::string part = text.substr(from, plus - from);
    from = plus + 1;
    if (part.empty()) continue;
    if (part.ends_with(".cin") || part.ends_with(".pcx")) {
      out.shows += (out.shows.empty() ? "" : "+") + part;
      continue;
    }
    out.unit = part.front() == '*';
    if (out.unit) part.erase(0, 1);
    const size_t spot = part.find('$');
    out.spawn = spot == std::string::npos ? std::string() : part.substr(spot + 1);
    out.map = part.substr(0, spot);
  }
  return out;
}

void campaign_start(Q2Server& s) {
  if (std::find(s.sounds.begin(), s.sounds.end(), kHelpBeep) == s.sounds.end())
    s.sounds.emplace_back(kHelpBeep);
  switch (s.pending_change) {
    case MapChange::new_game:
      s.help[0].clear();
      s.help[1].clear();
      s.help_changed = 0;
      s.cross_bits = 0;
      s.spawn_name.clear();
      for (Q2Player& p : s.players) {
        p.carried = false;
        p.help_seen = 0;
        p.help_unread = 0;
      }
      break;
    case MapChange::change_level:
      s.spawn_name = s.next_spawn;
      for (int i = 0; i < sh::kMaxClients; i++) {
        Q2Player& p = s.players[i];
        p.carried = s.clients[i].active && p.alive && !deathmatch(s);
        if (!p.carried) continue;
        p.carry = leaving(p.body);
        if (s.next_unit && coop(s)) strip_keys(p.carry);
      }
      break;
    case MapChange::restart:  // as each came in
      for (Q2Player& p : s.players) {
        p.carry = p.entered;
        p.carried = p.has_entered;
      }
      break;
  }
  s.next_spawn.clear();
  s.next_unit = false;
  s.next_cinematic.clear();
  s.cinematic.clear();
  s.game_over = false;
  for (Q2Player& p : s.players) p.screen = 0;
}

namespace {

constexpr float kCoopReach = 550;  // a co-op spot belongs to a start this near

// Whether `a` and `b` are nearer than the co-op reach (squared, so no root is taken).
bool near(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return d.x * d.x + d.y * d.y + d.z * d.z < kCoopReach * kCoopReach;
}

// The maps whose co-op spots carry names no level change uses: each is renamed after its start.
bool coop_names_wrong(std::string_view map) {
  static constexpr std::string_view kMaps[] = {
      "jail2", "jail4", "mintro", "mine1",  "mine2", "mine3",   "mine4",  "lab",    "boss1",
      "fact1", "fact3", "waste1", "biggun", "space", "command", "power2", "strike", "city2"};
  for (const std::string_view m : kMaps)
    if (same_name(map, m)) return true;
  return false;
}

}  // namespace

void campaign_coop_spots(Q2Server& s) {
  if (!coop(s)) return;
  // On these maps each co-op spot takes the name of the first named start (in map order) within
  // 550 units of it.
  if (coop_names_wrong(s.map_name))
    for (Q2Entity& c : s.entities) {
      if (c.classname != "info_player_coop") continue;
      for (const Q2Entity& e : s.entities)
        if (e.classname == "info_player_start" && !e.targetname.empty() &&
            near(c.origin, e.origin)) {
          c.targetname = e.targetname;
          break;
        }
    }
  // The security level has no co-op spots where the way in from the jail arrives: three are added.
  if (same_name(s.map_name, "security"))
    for (const float x : {124.0f, 252.0f, 316.0f}) {
      Q2Entity e;
      e.classname = "info_player_coop";
      e.targetname = "jail3";
      e.origin = {x, -164, 80};
      e.angles = {0, 90, 0};
      s.entities.push_back(std::move(e));
    }
}

const Q2Entity* campaign_spot(const Q2Server& s, int slot) {
  const std::string& name = s.spawn_name;
  if (coop(s) && slot > 0) {  // the slot-th co-op spot of the name
    int n = 0;
    for (const Q2Entity& e : s.entities)
      if (e.classname == "info_player_coop" && same_name(e.targetname, name) && ++n == slot)
        return &e;
  }
  const Q2Entity* first = nullptr;
  const Q2Entity* unnamed = nullptr;
  const Q2Entity* start = nullptr;
  for (const Q2Entity& e : s.entities) {
    if (e.classname != "info_player_start") continue;
    if (first == nullptr) first = &e;
    if (!name.empty() && same_name(e.targetname, name)) {
      start = &e;
      break;
    }
    if (e.targetname.empty() && unnamed == nullptr) unnamed = &e;
  }
  if (start == nullptr) start = name.empty() && unnamed != nullptr ? unnamed : first;
  // A co-op player past the first with no spot of the name: the slot-th co-op spot within 550
  // units of the start, else the start itself.
  if (coop(s) && slot > 0 && start != nullptr) {
    int n = 0;
    for (const Q2Entity& e : s.entities)
      if (e.classname == "info_player_coop" && near(e.origin, start->origin) && ++n == slot)
        return &e;
  }
  return start;
}

void campaign_body(Q2Server& s, int slot, bool respawn) {
  Q2Player& p = s.players[slot];
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

void campaign_frame(Q2Server& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2Player& p = s.players[i];
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

namespace {

// Text a layout string may hold: its quotes would end it early.
std::string layout_text(std::string_view text) {
  std::string out = "\"";
  for (const char c : text)
    if (c != '"') out += c;
  return out + "\"";
}

}  // namespace

std::string campaign_help_screen(const Q2Server& s) {
  char counts[96];
  std::snprintf(counts, sizeof counts, "%3d/%3d     %d/%d       %d/%d", s.monsters_killed,
                s.monsters_total, s.goals_found, s.goals, s.secrets_found, s.secrets);
  std::string out = "xv 32 yv 8 picn help ";
  out += "xv 202 yv 12 string2 " + layout_text(skill_name(s)) + " ";
  out += "xv 0 yv 24 cstring2 " + layout_text(level_name(s)) + " ";
  out += "xv 0 yv 54 cstring2 " + layout_text(s.help[0]) + " ";
  out += "xv 0 yv 110 cstring2 " + layout_text(s.help[1]) + " ";
  out += "xv 50 yv 164 string2 \" kills     goals    secrets\" ";
  out += "xv 50 yv 172 string2 " + layout_text(counts) + " ";
  return out;
}

}  // namespace shooters_edge::server
