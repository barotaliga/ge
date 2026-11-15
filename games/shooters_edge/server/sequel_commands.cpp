// The sequel's player commands (sequel_commands.hpp).
//
// ge:summary The sequel's player commands: chat and its flood guard, the cheats, give, gestures
// ge:keywords sequel client-command say chat flood cheats give god notarget noclip wave
// ge:skill shooter
// ge:test tests/shooters_edge_sequel_commands_test.cpp
#include "shooters_edge/server/sequel_commands.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "progs/text.hpp"
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_poses.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace shooters_edge::server {

namespace {

namespace con = ge::console;

constexpr size_t kChatCut = 150;  // a chat line's characters, its name and marks included
constexpr int kFloodKept = 10;    // the said times a player keeps

void send(SequelServer& s, NetEvent ev) {
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

bool fold_is(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) {
    const auto lo = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
    if (lo(a[i]) != lo(b[i])) return false;
  }
  return true;
}

std::string name_of(const SequelServer& s, int slot) {
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return "someone";
}

int cvar_int(const SequelServer& s, const char* name) {
  const con::Cvar* c = s.cvars.find(name);
  return c != nullptr ? static_cast<int>(c->integer()) : 0;
}

double cvar_number(const SequelServer& s, const char* name) {
  const con::Cvar* c = s.cvars.find(name);
  return c != nullptr ? c->number : 0;
}

bool deathmatch_on(const SequelServer& s) { return s.cvars.find("deathmatch")->on(); }

// The cheats are the server's to allow in deathmatch; single player and co-op always have them.
bool cheats_allowed(SequelServer& s, int slot) {
  if (!deathmatch_on(s) || cvar_int(s, "cheats") != 0) return true;
  fight_print(s, slot, "Cheats are off on this server (cheats 1 turns them on).\n");
  return false;
}

// The player's chat line: refused while quiet, else counted; true when it may be said.
bool flood_ok(SequelServer& s, int slot) {
  SequelPlayer& p = s.players[slot];
  const int most = std::min(cvar_int(s, "flood_msgs"), kFloodKept);
  if (most <= 0) return true;
  if (s.level_time < p.quiet_until) {
    const int left = static_cast<int>(p.quiet_until - s.level_time);
    fight_print(s, slot, "You can't talk for " + std::to_string(left) + " more seconds.\n");
    return false;
  }
  // The `most`-th last line said: too recent, and the player is quiet for a while.
  const int back = (p.said_next - most + kFloodKept) % kFloodKept;
  const double then = p.said_at[back];
  if (then > 0 && s.level_time - then < cvar_number(s, "flood_persecond")) {
    const double wait = cvar_number(s, "flood_waitdelay");
    p.quiet_until = s.level_time + wait;
    NetEvent ev;
    ev.kind = EventKind::print;
    ev.to = 1u << slot;
    ev.reliable = true;
    ev.level = 3;
    ev.text = "Flood protection: you can't talk for " + std::to_string(static_cast<int>(wait)) +
              " seconds.\n";
    send(s, std::move(ev));
    return false;
  }
  p.said_at[p.said_next] = s.level_time > 0 ? s.level_time : 1e-6;
  p.said_next = (p.said_next + 1) % kFloodKept;
  return true;
}

// `text` said by `slot`, to his team when `team` and teams are on (dmflags 64 or 128).
void say(SequelServer& s, int slot, std::string text, bool team) {
  if (text.empty()) return;
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
    text = text.substr(1, text.size() - 2);
  const bool teams = deathmatch_on(s) && (fight_dmflag(s, 64) || fight_dmflag(s, 128));
  team = team && teams;
  if (!flood_ok(s, slot)) return;
  std::string line = team ? "(" + name_of(s, slot) + "): " : name_of(s, slot) + ": ";
  line += text;
  if (line.size() > kChatCut) line.resize(kChatCut);
  line += '\n';
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.reliable = true;
  ev.level = 3;
  ev.text = line;
  ev.to = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    if (!s.clients[i].active) continue;
    if (team && i != slot && !fight_teammates(s, slot, i)) continue;
    ev.to |= 1u << i;
  }
  send(s, std::move(ev));
  s.console.push_back(line.substr(0, line.size() - 1));
}

// `give`'s forms, the cheat already allowed.
void give(SequelServer& s, int slot, const std::vector<std::string>& w, const std::string& rest) {
  SequelPlayer& p = s.players[slot];
  SqBody& b = p.body;
  const bool all = fold_is(rest, "all");
  const std::string first = w.size() > 1 ? w[1] : std::string();
  if (all || fold_is(first, "health")) {
    if (!all && w.size() == 3) {
      p.health = static_cast<float>(std::atoi(w[2].c_str()));
    } else {
      p.health = static_cast<float>(b.max_health);
    }
    b.health = static_cast<int>(p.health);
    if (!all) return;
  }
  if (all || fold_is(rest, "weapons")) {
    for (int i = 0; i < kSqItemCount; i++)
      if (kSqItems[i].kind == SqKind::weapon) b.counts[i]++;
    b.ammo[int(SqAmmo::grenades)]++;  // the hand grenades, a weapon held while stocked
    if (!all) return;
  }
  if (all || fold_is(rest, "ammo")) {
    for (int k = 1; k < kSqAmmoKinds; k++) b.ammo[k] = b.max_ammo[k];
    if (!all) return;
  }
  if (all || fold_is(rest, "armor")) {
    b.armour = SqArmour::body;
    b.armour_count = kSqArmours[int(SqArmour::body)].maximum;
    if (!all) return;
  }
  if (all) {  // the power shield as a pickup; every powerup, key and pack held once
    const int shield = sq_item_named("Power Shield");
    if (shield >= 0 && b.counts[shield] == 0 && p.alive) sq_give(s, slot, shield);
    for (int i = 0; i < kSqItemCount; i++) {
      const SqKind k = kSqItems[i].kind;
      if (k == SqKind::powerup || k == SqKind::key || k == SqKind::pack) b.counts[i] = 1;
    }
    return;
  }
  // An item by its pickup name: the whole text, else its first word with a count after it.
  int item = sq_item_named(rest);
  const bool whole = item >= 0;
  if (!whole) item = sq_item_named(first);
  if (item < 0) {
    fight_print(s, slot,
                sq_weapon_named(rest) == int(SqWeapon::blaster)
                    ? std::string("The Blaster is always in your hands.\n")
                    : "There is no item called " + rest + ".\n");
    return;
  }
  const SqItem& it = kSqItems[item];
  if (it.kind == SqKind::ammo) {
    int& held = b.ammo[int(it.ammo)];
    if (!whole && w.size() == 3)
      held = std::atoi(w[2].c_str());
    else
      held += it.amount;
    return;
  }
  if (p.alive) sq_give(s, slot, item);
}

void toggle(SequelServer& s, int slot, bool& flag, const char* name) {
  flag = !flag;
  fight_print(s, slot, std::string(name) + (flag ? " on\n" : " off\n"));
}

const char* const kGestureWords[5] = {"flipoff", "salute", "taunt", "wave", "point"};

void wave(SequelServer& s, int slot, const std::string& arg) {
  SequelPlayer& p = s.players[slot];
  if (!p.alive || !sq_can_gesture(p.pose, p.on_ground)) return;
  int n = std::atoi(arg.c_str());
  if (n < 0 || n > 4) n = 4;
  p.pose.gesture = n;
  fight_print(s, slot, std::string(kGestureWords[n]) + "\n");
}

void weaplast(SequelServer& s, int slot) {
  SequelPlayer& p = s.players[slot];
  if (p.last_weapon < 0 || p.held_weapon < 0) return;
  if (!sq_holds(p.body, p.last_weapon)) return;
  fight_command(s, slot, "use", kSqWeapons[p.last_weapon].name);
}

void players(SequelServer& s, int slot) {
  std::vector<int> order;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active) order.push_back(i);
  std::stable_sort(order.begin(), order.end(),
                   [&s](int a, int b) { return s.players[a].frags < s.players[b].frags; });
  std::string text;
  for (const int i : order) {
    char head[16];
    std::snprintf(head, sizeof head, "%3d ", static_cast<int>(s.players[i].frags));
    const std::string row = head + name_of(s, i) + "\n";
    if (text.size() + row.size() > 1180) {
      text += "...\n";
      break;
    }
    text += row;
  }
  text += "\n" + std::to_string(order.size()) + " players\n";
  fight_print(s, slot, text);
}

void playerlist(SequelServer& s, int slot) {
  std::string text;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& c = s.clients[i];
    if (!c.active) continue;
    const int secs = std::max(0, static_cast<int>(s.level_time - c.entered_at));
    char head[48];
    std::snprintf(head, sizeof head, "%02d:%02d %4d %3d ", secs / 60, secs % 60, c.ping_ms,
                  static_cast<int>(s.players[i].frags));
    const std::string row = head + name_of(s, i) + (c.spectator ? " (spectator)" : "") + "\n";
    if (text.size() + row.size() > 1350) {
      text += "And more...\n";
      break;
    }
    text += row;
  }
  fight_print(s, slot, text);
}

}  // namespace

void commands_cvars(SequelServer& s) {
  s.cvars.add({.name = "cheats",
               .fallback = "0",
               .flags = con::kServer | con::kServerInfo,
               .help = "Whether players may use give, god, notarget and noclip in deathmatch.",
               .values = "0: not in deathmatch (the default); 1: always"});
  s.cvars.add({.name = "flood_msgs",
               .fallback = "4",
               .flags = con::kServer,
               .help = "The chat lines a player may say in flood_persecond seconds.",
               .values = "0: no flood protection; 1..10: that many lines (4 the default)"});
  s.cvars.add({.name = "flood_persecond",
               .fallback = "4",
               .flags = con::kServer,
               .help = "The seconds flood_msgs lines may be said in.",
               .values = "N: that many seconds (4 the default)"});
  s.cvars.add({.name = "flood_waitdelay",
               .fallback = "10",
               .flags = con::kServer,
               .help = "The seconds a player who said too much is kept quiet.",
               .values = "N: that many seconds (10 the default)"});
}

void commands_spawn(SequelServer& s, int slot) {
  SequelPlayer& p = s.players[slot];
  p.noclip = false;
  for (double& t : p.said_at) t = 0;
  p.said_next = 0;
  p.quiet_until = 0;
  p.held_weapon = p.last_weapon = p.hand.weapon;
}

void commands_frame(SequelServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    SequelPlayer& p = s.players[i];
    if (!s.clients[i].active || !p.alive || p.hand.weapon == p.held_weapon) continue;
    p.last_weapon = p.held_weapon;
    p.held_weapon = p.hand.weapon;
  }
}

bool commands_in_intermission(const std::string& cmd) {
  return cmd == "players" || cmd == "say" || cmd == "say_team" || cmd == "score" || cmd == "help";
}

bool commands_refuse_item(SequelServer& s, int slot, bool use, const std::string& name) {
  const SequelPlayer& p = s.players[slot];
  const int w = sq_weapon_named(name);
  const int item = sq_item_named(name);
  if (w < 0 && item < 0) {
    fight_print(s, slot, "There is no item called " + name + ".\n");
    return true;
  }
  const SqKind kind = w >= 0 ? SqKind::weapon : kSqItems[item].kind;
  const bool able =
      use ? kind == SqKind::weapon || kind == SqKind::powerup || kind == SqKind::power_armour
          : kind == SqKind::weapon || kind == SqKind::ammo || kind == SqKind::powerup;
  if (!able) {
    fight_print(s, slot, name + (use ? " can't be used.\n" : " can't be dropped.\n"));
    return true;
  }
  const bool held = w >= 0                 ? sq_holds(p.body, w)
                    : kind == SqKind::ammo ? p.body.ammo[int(kSqItems[item].ammo)] > 0
                                           : p.body.counts[item] > 0;
  if (!held || !p.alive) {
    fight_print(s, slot, "You have no " + name + ".\n");
    return true;
  }
  return false;
}

bool commands_run(SequelServer& s, int slot, const std::string& line) {
  ge::progs::Words words;
  ge::progs::tokenize_words(line.c_str(), words);
  if (words.count == 0) return false;
  std::vector<std::string> w;
  for (int i = 0; i < words.count; i++) w.emplace_back(ge::progs::word(words, i));
  const std::string cmd = w[0];
  std::string rest;
  for (size_t i = 1; i < w.size(); i++) rest.append(i > 1 ? " " : "").append(w[i]);
  SequelPlayer& p = s.players[slot];
  if (cmd == "say" || cmd == "say_team") {
    const size_t at = line.find_first_not_of(" \t", line.find(cmd) + cmd.size());
    say(s, slot, at == std::string::npos ? std::string() : line.substr(at), cmd == "say_team");
  } else if (cmd == "players") {
    players(s, slot);
  } else if (s.intermission_at > 0) {
    // the rest wait for the next level, the unknown word's chat too
  } else if (cmd == "give") {
    if (cheats_allowed(s, slot) && !rest.empty()) give(s, slot, w, rest);
  } else if (cmd == "god") {
    if (cheats_allowed(s, slot)) toggle(s, slot, p.god, "God mode");
  } else if (cmd == "notarget") {
    if (cheats_allowed(s, slot)) toggle(s, slot, p.notarget, "Notarget");
  } else if (cmd == "noclip") {
    if (cheats_allowed(s, slot)) toggle(s, slot, p.noclip, "Noclip");
  } else if (cmd == "weaplast") {
    weaplast(s, slot);
  } else if (cmd == "wave") {
    wave(s, slot, w.size() > 1 ? w[1] : std::string());
  } else if (cmd == "playerlist") {
    playerlist(s, slot);
  } else {
    say(s, slot, line, false);  // an unknown word is said
  }
  return true;
}

}  // namespace shooters_edge::server
