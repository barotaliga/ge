// Quake II's player commands (q2_commands.hpp).
//
// ge:summary Quake II's player commands: chat and its flood guard, the cheats, give, gestures
// ge:keywords q2 client-command say chat flood cheats give god notarget noclip wave
// ge:keywords spectator-chat sv_spectalk
// ge:skill shooter
// ge:test tests/shooters_edge_q2_commands_test.cpp
#include "shooters_edge/server/q2_commands.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "progs/text.hpp"
#include "server/chat.hpp"
#include "shooters_edge/server/chat_rules.hpp"
#include "shooters_edge/server/q2_bar.hpp"
#include "shooters_edge/server/q2_campaign.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_poses.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

namespace con = ge::console;

constexpr size_t kChatCut = 150;  // a chat line's characters, its name and marks included
constexpr int kFloodKept = 10;    // the said times a player keeps

void send(Q2Server& s, NetEvent ev) {
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

std::string name_of(const Q2Server& s, int slot) {
  for (const auto& [k, v] : s.clients[slot].userinfo)
    if (k == "name") return v;
  return "someone";
}

int cvar_int(const Q2Server& s, const char* name) {
  const con::Cvar* c = s.cvars.find(name);
  return c != nullptr ? static_cast<int>(c->integer()) : 0;
}

double cvar_number(const Q2Server& s, const char* name) {
  const con::Cvar* c = s.cvars.find(name);
  return c != nullptr ? c->number : 0;
}

bool deathmatch_on(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

// The cheats are the server's to allow in deathmatch; single player and co-op always have them.
bool cheats_allowed(Q2Server& s, int slot) {
  if ((!deathmatch_on(s) && cvar_int(s, "coop") == 0) || cvar_int(s, "cheats") != 0) return true;
  fight_print(s, slot, "You must run the server with '+set cheats 1' to enable this command.\n");
  return false;
}

// The player's chat line: refused while quiet, else counted; true when it may be said. Only
// deathmatch and co-op guard against a flood; a `flood_msgs` over 10 is lowered to 10.
bool flood_ok(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  if (!deathmatch_on(s) && cvar_int(s, "coop") == 0) return true;
  if (cvar_int(s, "flood_msgs") > kFloodKept) {
    s.cvars.set("flood_msgs", std::to_string(kFloodKept), con::Source::local);
    s.console.push_back("flood_msgs lowered to max: " + std::to_string(kFloodKept));
  }
  const int most = cvar_int(s, "flood_msgs");
  if (most <= 0) return true;
  if (s.level_time < p.quiet_until) {
    const int left = static_cast<int>(p.quiet_until - s.level_time);
    fight_print(s, slot, "You can't talk for " + std::to_string(left) + " more seconds\n");
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
    ev.text = "Flood protection: You can't talk for " + std::to_string(static_cast<int>(wait)) +
              " seconds.\n";
    send(s, std::move(ev));
    return false;
  }
  p.said_at[p.said_next] = s.level_time > 0 ? s.level_time : 1e-6;
  p.said_next = (p.said_next + 1) % kFloodKept;
  return true;
}

// `text` said by `slot`, to his team when `team` and teams are on (dmflags 64 or 128),
// through the engine's chat route (server/chat.hpp): a spectator's `say_team` reaches only the
// spectators, and so does his `say` in a match while `sv_spectalk` is 0.
void say(Q2Server& s, int slot, std::string text, bool team) {
  if (text.empty()) return;
  // A text that starts with a double quote loses its first and its last character.
  if (text.front() == '"') text = text.size() >= 2 ? text.substr(1, text.size() - 2) : "";
  const bool teams = fight_anyflag(s, 64) || fight_anyflag(s, 128) || s.clients[slot].spectator;
  team = team && teams;
  if (!flood_ok(s, slot)) return;
  // The seats as the route reads them: only the speaker's team matters, so a teammate holds his
  // team's mark and everyone else one of his own.
  std::array<ge::server::ChatSeat, sh::kMaxClients> seats;
  for (int i = 0; i < sh::kMaxClients; i++) {
    ge::server::ChatSeat& d = seats[static_cast<size_t>(i)];
    d.active = s.clients[i].active;
    if (!d.active) continue;
    d.spectator = s.clients[i].spectator;
    d.name = name_of(s, i);
    d.team = i == slot || fight_skin_mates(s, slot, i) ? "=" : std::to_string(i);
  }
  ge::server::ChatLine said = ge::server::chat_route(seats, slot, team, false, chat_in_match(s),
                                                     ge::server::chat_settings(s.cvars), text);
  std::string line = said.text.substr(0, said.text.size() - 1);
  if (line.size() > kChatCut) line.resize(kChatCut);
  line += '\n';
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.reliable = true;
  ev.level = 3;
  ev.text = line;
  ev.to = said.to;
  send(s, std::move(ev));
  s.console.push_back(line.substr(0, line.size() - 1));
}

// `give`'s forms, the cheat already allowed.
void give(Q2Server& s, int slot, const std::vector<std::string>& w, const std::string& rest) {
  Q2Player& p = s.players[slot];
  SqBody& b = p.body;
  const bool all = fold_is(rest, "all");
  const std::string first = w.size() > 1 ? w[1] : std::string();
  if (all || fold_is(first, "health")) {
    if (!all && w.size() == 3) {
      p.health = static_cast<float>(std::max(1, std::atoi(w[2].c_str())));
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
    const int shield = sq_item_named("Power Shield");  // always one more, by the pickup rules
    if (shield >= 0 && p.alive) sq_give(s, slot, shield);
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
    fight_print(
        s, slot,
        sq_weapon_named(rest) == int(SqWeapon::blaster) ? "non-pickup item\n" : "unknown item\n");
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

void toggle(Q2Server& s, int slot, bool& flag, const char* name) {
  flag = !flag;
  fight_print(s, slot, std::string(name) + (flag ? " ON\n" : " OFF\n"));
}

// The weapon a bind's class name names (`weapon_shotgun`; the hand grenade `ammo_grenades`; the
// Blaster `weapon_blaster`), ignoring case; -1 none.
int weapon_of_class(const std::string& name) {
  for (int w = 0; w < kSqWeaponCount; w++) {
    const char* c = kSqWeapons[w].item != nullptr ? kSqWeapons[w].item : "weapon_blaster";
    if (q2_same_name(c, name)) return w;
  }
  return -1;
}

// `cycleweap` and `prefweap` (a bind's list of weapons by class name): cycleweap takes the next
// listed after the weapon chosen (else the one in hand), wrapping, never the one in hand; prefweap
// the first listed. The first held with a shot's ammo; else the first held short of it (refused
// with the ammo line); else the first not held (`Out of item:`). Names not of a weapon are
// skipped. cycleweap from three names or more shows the new weapon's icon and name 0.7 s.
void weapon_list(Q2Server& s, int slot, const std::vector<std::string>& w, bool cycle) {
  if (w.size() < 2) {
    fight_print(s, slot,
                std::string("Usage: ") + (cycle ? "cycleweap" : "prefweap") +
                    " classname1 classname2 .. classnameN\n");
    return;
  }
  std::vector<int> list;
  for (size_t i = 1; i < w.size(); i++)
    if (const int n = weapon_of_class(w[i]); n >= 0) list.push_back(n);
  if (list.empty()) return;
  Q2Player& p = s.players[slot];
  size_t start = 0;
  if (cycle) {
    const int now = p.hand.pending >= 0 ? p.hand.pending : p.hand.weapon;
    for (size_t i = 0; i < list.size(); i++)
      if (list[i] == now) {
        start = i + 1;
        break;
      }
  }
  int ready = -1, short_of = -1, missing = -1;
  for (size_t k = 0; k < list.size(); k++) {
    const int n = list[(start + k) % list.size()];
    if (cycle && n == p.hand.weapon) continue;
    if (sq_can_fire(p.body, n)) {
      ready = n;
      break;
    }
    if (sq_holds(p.body, n) && short_of < 0) short_of = n;
    if (!sq_holds(p.body, n) && missing < 0) missing = n;
  }
  const int n = ready >= 0 ? ready : short_of;
  if (n < 0) {
    if (missing >= 0)
      fight_print(s, slot, std::string("Out of item: ") + kSqWeapons[missing].name + "\n");
    return;
  }
  if (n == p.hand.weapon) return;  // prefweap: the one in hand already
  fight_command(s, slot, "use", kSqWeapons[n].name);
  if (cycle && list.size() > 2 && p.hand.pending == n && kSqWeapons[n].item != nullptr) {
    p.picked = sq_item_number(sq_item(kSqWeapons[n].item));
    p.picked_at = s.level_time - (3.0 - 0.7);  // the bar's pickup line shows 3 s
  }
}

// `teleport x y z`: the player put there (10 units up), stopped, his view angles cleared.
void teleport(Q2Server& s, int slot, const std::vector<std::string>& w) {
  if (w.size() != 4) {
    fight_print(s, slot, "Usage: teleport x y z\n");
    return;
  }
  const sh::Vec3f at{std::strtof(w[1].c_str(), nullptr), std::strtof(w[2].c_str(), nullptr),
                     std::strtof(w[3].c_str(), nullptr)};
  map_place_player(s, slot, at, {});
}

// `listentities <all|ammo|items|keys|monsters|weapons> ...`: every entity of the asked kinds,
// `<classname>: x y z`, on the server's console; an unknown word prints the usage, the rest apply.
void list_entities(Q2Server& s, int slot, const std::vector<std::string>& w) {
  const char* usage = "Usage: listentities <all|ammo|items|keys|monsters|weapons>\n";
  if (w.size() < 2) {
    fight_print(s, slot, usage);
    return;
  }
  static constexpr std::pair<const char*, const char*> kKinds[] = {{"ammo", "ammo_"},
                                                                   {"items", "item_"},
                                                                   {"keys", "key_"},
                                                                   {"monsters", "monster_"},
                                                                   {"weapons", "weapon_"}};
  std::vector<std::string_view> starts;
  bool all = false;
  for (size_t i = 1; i < w.size(); i++) {
    bool known = q2_same_name(w[i], "all");
    all = all || known;
    for (const auto& [word, start] : kKinds)
      if (q2_same_name(w[i], word)) {
        starts.push_back(start);
        known = true;
      }
    if (!known) fight_print(s, slot, usage);
  }
  for (const Q2Entity& e : s.entities) {
    if (e.gone || e.classname.empty()) continue;
    bool shown = all;
    for (const std::string_view start : starts) shown = shown || e.classname.rfind(start, 0) == 0;
    if (!shown) continue;
    char line[160];
    std::snprintf(line, sizeof line, "%s: %f %f %f", e.classname.c_str(), double(e.origin.x),
                  double(e.origin.y), double(e.origin.z));
    s.console.push_back(line);
  }
}

const char* const kGestureWords[5] = {"flipoff", "salute", "taunt", "wave", "point"};

void wave(Q2Server& s, int slot, const std::string& arg) {
  Q2Player& p = s.players[slot];
  if (!p.alive || !sq_can_gesture(p.pose, p.on_ground)) return;
  int n = std::atoi(arg.c_str());
  if (n < 0 || n > 4) n = 4;
  p.pose.gesture = n;
  fight_print(s, slot, std::string(kGestureWords[n]) + "\n");
}

void weaplast(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  if (p.last_weapon < 0 || p.held_weapon < 0) return;
  if (!sq_holds(p.body, p.last_weapon)) return;
  fight_command(s, slot, "use", kSqWeapons[p.last_weapon].name);
}

void players(Q2Server& s, int slot) {
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

void playerlist(Q2Server& s, int slot) {
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

void commands_cvars(Q2Server& s) {
  add_chat_settings(s.cvars);  // sv_spectalk, sv_sayteam_to_spec (the engine's)
  s.cvars.add({.name = "g_select_empty",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether a player may switch to a weapon with no ammunition for it.",
               .values = "0: refused, and the player told (the default); 1: allowed"});
  s.cvars.add({.name = "g_quick_weap",
               .fallback = "1",
               .flags = con::kServer,
               .help = "Whether the next and previous weapon keys step on from the weapon already "
                       "chosen, so presses skip ahead, its icon and name shown a moment.",
               .values = "0: from the weapon in hand; 1: from the one chosen (the default)"});
  s.cvars.add({.name = "g_swap_speed",
               .fallback = "1",
               .flags = con::kServer,
               .help = "How many frames a weapon's lowering and raising advance each frame.",
               .values = "1: the classic pace (the default); 2..N: that many times faster"});
  s.cvars.add({.name = "aimfix",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether a player's shots fly from the gun to the point the crosshair is "
                       "on, rather than straight ahead from the gun.",
               .values = "0: straight ahead, as the game was made (the default); 1: onto the "
                         "crosshair's point"});
  s.cvars.add({.name = "g_machinegun_norecoil",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether the machine gun's view stays level while it fires outside "
                       "deathmatch (it never climbs in deathmatch).",
               .values = "0: it climbs 1.5 degrees a bullet, to 13.5 (the default); 1: level"});
  s.cvars.add({.name = "g_footsteps",
               .fallback = "1",
               .flags = con::kServer,
               .help = "When players' footsteps sound.",
               .values = "0: never; 1: running on the ground faster than 225 units a second (the "
                         "default); 2: on the ground at any pace; 3: always, in the air too"});
  s.cvars.add({.name = "map_noareas",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether the doors that close parts of the map off are ignored: every part "
                       "sees and hears every other (for finding faults in a map).",
               .values = "0: the doors close parts off (the default); 1: every part joined"});
  s.cvars.add({.name = "sv_maxvelocity",
               .fallback = "2000",
               .flags = con::kServer,
               .help = "The fastest a thrown or falling thing moves along each axis, units a "
                       "second: grenades, gibs, dropped items, bodies.",
               .values = "N: that speed (2000 the default)"});
  s.cvars.add({.name = "sv_rollspeed",
               .fallback = "200",
               .flags = con::kServer,
               .help = "The sideways speed at which a player's figure leans the most, as others "
                       "see it.",
               .values = "N: units a second (200 the default); 0: the full lean at any speed"});
  s.cvars.add({.name = "sv_rollangle",
               .fallback = "2",
               .flags = con::kServer,
               .help = "How far a player's figure leans into a strafe as others see it: four "
                       "times this many degrees at sv_rollspeed.",
               .values = "0: no lean; N: degrees (2 the default)"});
  s.cvars.add({.name = "g_fix_triggered",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether a monster set to wait for a trigger that nothing can give (it has "
                       "no name) stands from the start instead of never appearing.",
               .values = "0: it never appears, as the map was made (the default); 1: it stands"});
  s.cvars.add({.name = "g_monsterfootsteps",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether walking monsters make footstep sounds as they walk, run and "
                       "stagger.",
               .values = "0: silent feet, as the game was made (the default); 1: footsteps"});
  s.cvars.add({.name = "g_commanderbody_nogod",
               .fallback = "0",
               .flags = con::kServer,
               .help = "Whether the tank commander's body can be shot apart.",
               .values = "0: it takes no harm (the default); 1: the first blow throws it apart"});
  s.cvars.add({.name = "cheats",
               .fallback = "0",
               .flags = con::kServer | con::kServerInfo | con::kLatch,
               .help = "Whether players may use give, god, notarget and noclip in deathmatch "
                       "and co-op. A change takes hold on the next map.",
               .values = "0: not in deathmatch (the default); 1: always"});
  // The held weapon moved off its place, for whoever makes a weapon's model (a developer's tool).
  for (const auto& [name, way] :
       {std::pair{"gun_x", "right"}, std::pair{"gun_y", "forward"}, std::pair{"gun_z", "down"}})
    s.cvars.add({.name = name,
                 .fallback = "0",
                 .flags = con::kServer,
                 .help = std::string("Moves every player's held weapon ") + way +
                         " by that many units, to line a weapon's model up.",
                 .values = "0: in its place (the default); N: units"});
  // What a server browser shows of the game: its folder and the day its rules were settled (fixed,
  // so every build serves the same server info).
  s.cvars.add({.name = "gamename",
               .fallback = "baseq2",
               .flags = con::kServer | con::kServerInfo | con::kLatch,
               .help = "The game's name in the server info, for server browsers.",
               .values = "baseq2 (the default)"});
  s.cvars.add({.name = "gamedate",
               .fallback = "Nov 22 2026",
               .flags = con::kServer | con::kServerInfo | con::kLatch,
               .help = "The date of the game's rules in the server info, for server browsers.",
               .values = "a date, month day year"});
  s.cvars.add({.name = "maxentities",
               .fallback = "1024",
               .flags = con::kServer | con::kLatch,
               .help = "The entity slots a map may fill. This server makes room for every map, "
                       "so it is kept for configs that set it.",
               .values = "N: slots (1024 the default)"});
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

void commands_spawn(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  p.noclip = false;
  for (double& t : p.said_at) t = 0;
  p.said_next = 0;
  p.quiet_until = 0;
  p.held_weapon = p.last_weapon = p.hand.weapon;
}

void commands_frame(Q2Server& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2Player& p = s.players[i];
    if (!s.clients[i].active || !p.alive || p.hand.weapon == p.held_weapon) continue;
    p.last_weapon = p.held_weapon;
    p.held_weapon = p.hand.weapon;
  }
}

bool commands_in_intermission(const std::string& cmd) {
  return cmd == "players" || cmd == "say" || cmd == "say_team" || cmd == "score" || cmd == "help";
}

bool commands_refuse_item(Q2Server& s, int slot, bool use, const std::string& name) {
  const Q2Player& p = s.players[slot];
  const int w = sq_weapon_named(name);
  const int item = sq_item_named(name);
  if (w < 0 && item < 0) {
    fight_print(s, slot, "unknown item: " + name + "\n");
    return true;
  }
  const SqKind kind = w >= 0 ? SqKind::weapon : kSqItems[item].kind;
  const int row =
      w >= 0 ? (kSqWeapons[w].item != nullptr ? sq_item(kSqWeapons[w].item) : -1) : item;
  const bool able =
      use ? kind == SqKind::weapon || kind == SqKind::powerup || kind == SqKind::power_armour
          : fight_droppable(s, row);
  if (!able) {
    fight_print(s, slot, use ? "Item is not usable.\n" : "Item is not dropable.\n");
    return true;
  }
  const bool held = w >= 0                 ? sq_holds(p.body, w)
                    : kind == SqKind::ammo ? p.body.ammo[int(kSqItems[item].ammo)] > 0
                                           : p.body.counts[item] > 0;
  if (!held || !p.alive) {
    fight_print(s, slot, "Out of item: " + name + "\n");
    return true;
  }
  return false;
}

bool commands_run(Q2Server& s, int slot, const std::string& line) {
  ge::progs::Words words;
  ge::progs::tokenize_words(line.c_str(), words);
  if (words.count == 0) return false;
  std::vector<std::string> w;
  w.reserve(size_t(words.count));
  for (int i = 0; i < words.count; i++) w.emplace_back(ge::progs::word(words, i));
  const std::string cmd = w[0];
  std::string rest;
  for (size_t i = 1; i < w.size(); i++) rest.append(i > 1 ? " " : "").append(w[i]);
  Q2Player& p = s.players[slot];
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
    if (cheats_allowed(s, slot)) toggle(s, slot, p.god, "godmode");
  } else if (cmd == "notarget") {
    if (cheats_allowed(s, slot)) toggle(s, slot, p.notarget, "notarget");
  } else if (cmd == "noclip") {
    if (cheats_allowed(s, slot)) toggle(s, slot, p.noclip, "noclip");
  } else if (cmd == "teleport") {
    if (cheats_allowed(s, slot)) teleport(s, slot, w);
  } else if (cmd == "listentities") {
    if (cheats_allowed(s, slot)) list_entities(s, slot, w);
  } else if (cmd == "cycleweap" || cmd == "prefweap") {
    weapon_list(s, slot, w, cmd == "cycleweap");
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
