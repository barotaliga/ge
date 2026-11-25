// Client commands, chat, info changes, the console and pausing (commands.hpp states the routing).
//
// ge:summary The routing of client commands and console lines to the mod's hooks and the server
// ge:keywords client_command server_command set_user_info set_local_info request_pause call_hook
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_hooks_test.cpp
#include "shooters_edge/server/commands.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "progs/text.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/server/builtins.hpp"
#include "shooters_edge/server/chat_rules.hpp"
#include "shooters_edge/server/shot_help.hpp"

namespace shooters_edge::server {

namespace {

constexpr int kFlagGodmode = 64, kFlagNotarget = 128;  // the program's `flags` bits

constexpr const char* kFixed[] = {
    "new",     "modellist",  "soundlist", "prespawn", "spawn",  "begin", "drop",         "pings",
    "setinfo", "serverinfo", "download",  "nextdl",   "ptrack", "snap",  "stopdownload", "upload"};
constexpr const char* kOverridable[] = {"setpos",   "rate",    "kill",    "pause",   "say",
                                        "say_team", "minping", "airstep", "maps",    "ban",
                                        "banip",    "banrem",  "join",    "observe", "qtvusers",
                                        "god",      "give",    "noclip",  "fly",     "notarget"};

template <size_t N>
bool listed(const char* const (&list)[N], const char* w) {
  for (const char* x : list)
    if (std::strcmp(x, w) == 0) return true;
  return false;
}

void print_to(QcServer& s, uint32_t to, int level, const std::string& text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = to;
  ev.reliable = true;
  ev.time = s.sv.time;
  ev.level = level;
  ev.text = text;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

bool players_left(const QcServer& s) {
  for (const ClientInfo& c : s.clients)
    if (c.active && !c.spectator) return true;
  return false;
}

// The text after "say": the arguments line, its surrounding quotes removed.
std::string chat_text(const pg::Words& w) {
  std::string t = w.args;
  if (t.size() >= 2 && t.front() == '"' && t.back() == '"') t = t.substr(1, t.size() - 2);
  return t;
}

float field_f(QcServer& s, int e, int f) { return pg::as_float(*pg::entity_field(s.q.vm, e, f)); }
void set_field_f(QcServer& s, int e, int f, float v) {
  *pg::entity_field(s.q.vm, e, f) = pg::as_word(v);
}

// The cheats with the single-player game's words: `god`, `notarget`, `noclip`, `fly` toggled
// ("godmode ON"), `give` a weapon (2..8), ammunition (s n r c) or health (h) with a count. The
// program's own fields and flags carry them; false: not one of them. A single-player program's
// player has them alone or in co-op; a multiplayer program's with sv_cheats on (the operator's
// `god <id>` and `give <id> ...` on the classic endpoint come here too).
bool run_cheat(QcServer& s, int slot, const pg::Words& w) {
  const char* cmd = pg::word(w, 0);
  const bool god = std::strcmp(cmd, "god") == 0, notarget = std::strcmp(cmd, "notarget") == 0;
  const bool noclip = std::strcmp(cmd, "noclip") == 0, fly = std::strcmp(cmd, "fly") == 0;
  const bool give = std::strcmp(cmd, "give") == 0;
  if (!god && !notarget && !noclip && !fly && !give) return false;
  const int e = slot + 1;
  const uint32_t me = 1u << slot;
  const pg::LayoutFields& f = s.q.layout->f;
  sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
  if (god || notarget) {
    const int bit = god ? kFlagGodmode : kFlagNotarget;
    const int flags = static_cast<int>(field_f(s, e, f.flags)) ^ bit;
    set_field_f(s, e, f.flags, static_cast<float>(flags));
    ed.flags = flags;
    print_to(s, me, 2,
             std::string(god ? "godmode" : "notarget") + ((flags & bit) ? " ON\n" : " OFF\n"));
  } else if (noclip || fly) {
    const int kind = noclip ? sh::kMovetypeNoclip : sh::kMovetypeFly;
    const int now = ed.movetype == kind ? sh::kMovetypeWalk : kind;
    ed.movetype = now;
    set_field_f(s, e, f.movetype, static_cast<float>(now));
    print_to(s, me, 2,
             std::string(noclip ? "noclip" : "flymode") + (now == kind ? " ON\n" : " OFF\n"));
  } else if (w.count >= 2) {
    const char* what = pg::word(w, 1);
    const float n = w.count >= 3 ? static_cast<float>(std::atoi(pg::word(w, 2))) : 0.0f;
    const char t = what[0];
    if (t >= '2' && t <= '8') {
      const int items = static_cast<int>(field_f(s, e, f.items)) | (1 << (t - '2'));
      set_field_f(s, e, f.items, static_cast<float>(items));
    } else if (t == 's') {
      set_field_f(s, e, f.ammo_shells, n);
    } else if (t == 'n') {
      set_field_f(s, e, f.ammo_nails, n);
    } else if (t == 'r') {
      set_field_f(s, e, f.ammo_rockets, n);
    } else if (t == 'c') {
      set_field_f(s, e, f.ammo_cells, n);
    } else if (t == 'h') {
      set_field_f(s, e, f.health, n);
      ed.health = n;
    }
  }
  return true;
}

// The same refused (done, nothing changed) in deathmatch: a single-player program's.
bool single_player_cheat(QcServer& s, int slot, const pg::Words& w) {
  const char* cmd = pg::word(w, 0);
  for (const char* c : {"god", "notarget", "noclip", "fly", "give"})
    if (std::strcmp(cmd, c) == 0) {
      const ge::console::Cvar* dm = s.cvars.find("deathmatch");
      return dm != nullptr && dm->on() ? true : run_cheat(s, slot, w);
    }
  return false;
}

// Two names the same over their whole length, a-z and A-Z folded.
bool same_name(const std::string& a, const char* b) {
  const size_t n = std::strlen(b);
  if (a.size() != n) return false;
  for (size_t i = 0; i < n; i++) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
    if (x != y) return false;
  }
  return true;
}

// Quake's `tell <name> <text>`: the first player in slot order whose whole name matches,
// ignoring case, reads "<sender>: <the whole arguments>" (the target's name still at the
// front; quotes round the arguments taken off), cut to 62 bytes, in the plain font. The sender
// sees nothing; no match, or no text after the name, does nothing.
void quake_tell(QcServer& s, int slot, const pg::Words& w) {
  if (w.count < 3) return;
  std::string text = w.args;
  if (!text.empty() && text.front() == '"')
    text = text.size() >= 2 ? text.substr(1, text.size() - 2) : "";
  std::string line = info_value(s.clients[slot].userinfo, "name") + ": " + text;
  if (line.size() > 62) line.resize(62);
  const char* target = pg::word(w, 1);
  for (size_t i = 0; i < std::size(s.clients); i++) {
    const ClientInfo& c = s.clients[i];
    if (!c.active || !same_name(info_value(c.userinfo, "name"), target)) continue;
    print_to(s, 1u << i, 2, line + "\n");
    return;
  }
}

// Quake's `ping`: the asker reads "Client ping times:" and each connected player's ping in
// milliseconds, right-aligned in four columns, then the name, in slot order.
void quake_ping(QcServer& s, int slot) {
  std::string out = "Client ping times:\n";
  for (const ClientInfo& c : s.clients) {
    if (!c.active) continue;
    char head[24];
    std::snprintf(head, sizeof head, "%4d ", c.ping_ms);
    out += head + info_value(c.userinfo, "name") + "\n";
  }
  print_to(s, 1u << slot, 2, out);
}

void run_server_command(QcServer& s, int slot, const pg::Words& w) {
  const char* cmd = pg::word(w, 0);
  const uint32_t me = 1u << slot;
  const int e = slot + 1;
  ClientInfo& c = s.clients[slot];
  if (std::strcmp(cmd, "kill") == 0) {
    // Both games refuse the dead (spectators too, whose health is never set); the misspelling
    // is theirs.
    if (c.spectator || s.sv.edicts[static_cast<size_t>(e)].health <= 0)
      print_to(s, me, 2, "Can't suicide -- allready dead!\n");
    else if (!c.spectator)
      call_hook(s, HookId::client_kill, s.q.hooks.client_kill, e, {});
  } else if (std::strcmp(cmd, "say") == 0 || std::strcmp(cmd, "say_team") == 0) {
    const bool team = std::strcmp(cmd, "say_team") == 0;
    const std::string text = chat_text(w);
    if (s.named.chat_message > 0 && call_hook(s, HookId::chat_message, s.named.chat_message, e,
                                              {text, team ? 1.0f : 0.0f}) != 0)
      return;
    // The server's own route (chat_rules.hpp): a team's words to its own, a spectator's to his.
    const ChatLine line = chat_route(s, slot, team, text);
    print_to(s, line.to, 3, line.text);
  } else if (std::strcmp(cmd, "pause") == 0) {
    request_pause(s, slot, !s.paused);
  } else if (std::strcmp(cmd, "setinfo") == 0) {
    if (w.count >= 3) set_user_info(s, slot, pg::word(w, 1), pg::word(w, 2));
  } else if (std::strcmp(cmd, "rate") == 0) {
    if (w.count >= 2) set_user_info(s, slot, "rate", pg::word(w, 1));
  } else if (std::strcmp(cmd, "drop") == 0) {
    c.drop = true;
  } else if (s.sv.set.single_player_clock && std::strcmp(cmd, "tell") == 0) {
    quake_tell(s, slot, w);
  } else if (s.sv.set.single_player_clock && std::strcmp(cmd, "ping") == 0) {
    quake_ping(s, slot);
  } else if (s.sv.set.single_player_clock && single_player_cheat(s, slot, w)) {
    // done: a single-player program's cheats (no sv_cheats; refused in deathmatch)
  } else if (std::strcmp(cmd, "setpos") == 0) {
    // setpos X Y Z: the player stood with his eye there, in the metres (y up) `viewfrom` prints;
    // a cheat (sv_cheats 1), for a picture taken where a player stood.
    if (!s.cvars.cheats()) {
      print_to(s, me, 2, "Cheats are not enabled on this server\n");
      return;
    }
    if (w.count < 4) {
      print_to(s, me, 2, "setpos X Y Z: metres, as viewfrom prints them\n");
      return;
    }
    constexpr float kUnitM = 0.025f;   // a map unit in metres (brush/map.hpp)
    constexpr float kEyeOver = 22.0f;  // units from a player's origin up to his eye
    sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    const auto metres = [&](int i) { return static_cast<float>(std::atof(pg::word(w, i))); };
    ed.origin = {metres(1) / kUnitM, metres(3) / kUnitM, metres(2) / kUnitM - kEyeOver};
    ed.velocity = {0, 0, 0};
    sh::link_edict(s.sv, e, false);
  } else if (std::strcmp(cmd, "god") == 0 || std::strcmp(cmd, "give") == 0 ||
             std::strcmp(cmd, "noclip") == 0 || std::strcmp(cmd, "fly") == 0) {
    if (!s.cvars.cheats()) {
      print_to(s, me, 2, "Cheats are not enabled on this server\n");
      return;
    }
    run_cheat(s, slot, w);  // with the single-player game's words, in deathmatch too
  } else {
    // The rest of the server's own commands are the protocol's and the admin tools' (M18g).
    print_to(s, me, 2, std::string(cmd) + ": not available on this server yet\n");
  }
}

}  // namespace

float call_hook(QcServer& s, HookId h, int32_t fn, int self, std::initializer_list<HookArg> args,
                bool* ok) {
  if (ok != nullptr) *ok = false;
  if (fn <= 0) return 0;
  sh::Server& sv = s.sv;
  const int old_self = sv.g.self, old_other = sv.g.other;
  sv.g.self = self;
  sv.g.other = 0;
  sv.g.time = static_cast<float>(sv.time);
  sim::enter_qc(s.q, sv);
  int i = 0;
  for (const HookArg& a : args) {
    if (a.is_text)
      pg::set_global_int(s.q.vm, pg::parm(i), pg::temp_string(s.q.vm, a.text.c_str()));
    else
      pg::set_global_float(s.q.vm, pg::parm(i), a.number);
    i++;
  }
  note_hook(s, h, self);
  const bool ran = sim::run_qc(s.q, fn);
  const float r = ran ? pg::global_float(s.q.vm, pg::kGlobalReturn) : 0.0f;
  sim::leave_qc(s.q);
  sv.g.self = old_self;
  sv.g.other = old_other;
  if (ok != nullptr) *ok = ran;
  return r;
}

static CommandResult run_client_command(QcServer& s, int slot, const std::string& line) {
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].active) return CommandResult::bad;
  pg::Words& w = s.q.vm.words;
  pg::tokenize_words(line.c_str(), w);
  if (w.count == 0) return CommandResult::bad;
  const std::string cmd = pg::word(w, 0);
  const std::string args = w.args;
  if (listed(kFixed, cmd.c_str())) {
    run_server_command(s, slot, w);
    return CommandResult::server;
  }
  const int e = slot + 1;
  float handled = 0;
  if (s.named.client_command > 0)
    handled = call_hook(s, HookId::client_command, s.named.client_command, e, {cmd, args});
  else if (s.named.user_cmd > 0)
    handled = call_hook(s, HookId::user_cmd, s.named.user_cmd, e, {cmd});
  if (handled != 0) return CommandResult::mod;
  // Quake's `tell` and `ping` (a mod may still take them first); QuakeWorld has neither.
  const bool quake_word = s.sv.set.single_player_clock && (cmd == "tell" || cmd == "ping");
  if (quake_word || listed(kOverridable, cmd.c_str())) {
    pg::tokenize_words(line.c_str(), w);  // the hook may have tokenized something else
    run_server_command(s, slot, w);
    return CommandResult::server;
  }
  print_to(s, 1u << slot, 2, "Bad user command: " + cmd + "\n");
  return CommandResult::bad;
}

static void change_user_info(QcServer& s, int slot, const std::string& key,
                             const std::string& value) {
  std::string old;
  if (!set_info_value(s.clients[slot].userinfo, key, value, old)) return;
  if (key == "name") set_netname(s, slot + 1, value);
  if (key == "cl_speedjump") s.sv.clients[slot].speedjump = value == "1";  // pm_speedjump's ask
  note_aim_help_info(s, slot, key, value);
  call_hook(s, HookId::user_info_changed, s.named.user_info_changed, slot + 1, {key, old, value});
}

void set_local_info(QcServer& s, const std::string& key, const std::string& value) {
  std::string old;
  if (!set_info_value(s.localinfo, key, value, old)) return;
  call_hook(s, HookId::localinfo_changed, s.named.localinfo_changed, 0, {key, old, value});
}

static bool run_console_line(QcServer& s, const std::string& line) {
  pg::Words& w = s.q.vm.words;
  pg::tokenize_words(line.c_str(), w);
  if (w.count == 0) return true;
  const std::string cmd = pg::word(w, 0);
  if (cmd == "set" && w.count >= 3) {
    s.cvars.set(pg::word(w, 1), pg::word(w, 2), ge::console::Source::local, true);
    apply_settings(s);
    return true;
  }
  if (cmd == "serverinfo" && w.count >= 3) {
    std::string old;
    set_info_value(s.serverinfo, pg::word(w, 1), pg::word(w, 2), old);
    return true;
  }
  if (cmd == "localinfo" && w.count >= 3) {
    set_local_info(s, pg::word(w, 1), pg::word(w, 2));
    return true;
  }
  if ((cmd == "map" || cmd == "changelevel") && w.count >= 2) {
    s.pending_map = pg::word(w, 1);
    s.pending_change = cmd == "map" ? MapChange::new_game : MapChange::change_level;
    return true;
  }
  // The operator's chat: QuakeWorld's "console: " at chat level; the single-player server's
  // game names its host instead, as "<hostname> ".
  if ((cmd == "say" || cmd == "say_team") && w.count >= 2) {
    const std::string text = chat_text(w);
    const ge::console::Cvar* host = s.cvars.find("hostname");
    const std::string from =
        s.sv.set.single_player_clock
            ? "<" + (host != nullptr ? host->value : std::string("UNNAMED")) + "> "
            : std::string("console: ");
    print_to(s, all_clients(s), 3, from + text + "\n");
    if (s.sv.set.single_player_clock) console_print(s, from + text);
    return true;
  }
  // Quake's `tell` and `ping` are a player's: the server's own console is no player.
  if (s.sv.set.single_player_clock && (cmd == "tell" || cmd == "ping")) {
    console_print(s, "Can't \"" + cmd + "\", not connected");
    return true;
  }
  if (cmd == "restart") {
    s.pending_map = s.map_name;
    s.pending_change = MapChange::restart;
    return true;
  }
  if (s.cvars.find(cmd) != nullptr) {
    if (w.count >= 2) {
      s.cvars.set(cmd, pg::word(w, 1), ge::console::Source::local);
      apply_settings(s);
    } else {
      console_print(s, cmd + " is \"" + s.cvars.find(cmd)->value + "\"");
    }
    return true;
  }
  if (s.named.console_cmd > 0 && call_hook(s, HookId::console_cmd, s.named.console_cmd, 0, {}) != 0)
    return true;
  console_print(s, "Unknown command \"" + cmd + "\"");
  return false;
}

bool request_pause(QcServer& s, int slot, bool pause) {
  if (pause == s.paused) return false;
  int self = 0;
  if (slot >= 0) {
    if (s.clients[slot].spectator) {
      print_to(s, 1u << slot, 2, "Spectators can not pause.\n");
      return false;
    }
    if (pause && !s.cvars.find("pausable")->on()) {
      print_to(s, 1u << slot, 2, "Pause not allowed.\n");
      return false;
    }
    self = slot + 1;
  } else if (!pause && slot != kPauseByServer && players_left(s)) {
    return false;  // the server unpauses on its own only when no players are left
  }
  if (s.named.should_pause > 0) {
    bool ran = false;
    const float r =
        call_hook(s, HookId::should_pause, s.named.should_pause, self, {pause ? 1.0f : 0.0f}, &ran);
    if (ran && r == 0) return false;
  }
  s.paused = pause;
  if (pause) s.paused_seconds = 0;
  // Everyone is told who paused. A single-player program's game words it as it happened; a
  // multiplayer one, as QuakeWorld does, by the state before the toggle, so the words come out
  // the other way round ("unpaused" for a pause), and the console hears it too.
  if (slot >= 0) {
    const bool says_paused = s.sv.set.single_player_clock ? pause : !pause;
    const std::string line = info_value(s.clients[slot].userinfo, "name") +
                             (says_paused ? " paused the game\n" : " unpaused the game\n");
    print_to(s, all_clients(s), 2, line);
    if (!s.sv.set.single_player_clock) console_print(s, line.substr(0, line.size() - 1));
  } else if (!pause && slot != kPauseByServer && !s.sv.set.single_player_clock) {
    const std::string line = "Pause released since no players are left.\n";
    print_to(s, all_clients(s), 2, line);
    console_print(s, line.substr(0, line.size() - 1));
  }
  NetEvent ev;
  ev.kind = EventKind::pause;
  ev.to = all_clients(s);
  ev.reliable = true;
  ev.time = s.sv.time;
  ev.level = pause ? 1 : 0;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
  return true;
}

void tell_paused(QcServer& s, int slot) {
  if (!s.paused || s.sv.set.single_player_clock) return;
  NetEvent ev;
  ev.kind = EventKind::pause;
  ev.to = 1u << slot;
  ev.reliable = true;
  ev.time = s.sv.time;
  ev.level = 1;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
  print_to(s, 1u << slot, 2, "Server is paused.\n");
}

// The entry points: each leaves the rules' world as the command left the server.
CommandResult client_command(QcServer& s, int slot, const std::string& line) {
  const CommandResult r = run_client_command(s, slot, line);
  fill_world(s, s.world, false);
  return r;
}

void set_user_info(QcServer& s, int slot, const std::string& key, const std::string& value) {
  change_user_info(s, slot, key, value);
  fill_world(s, s.world, false);
}

bool server_command(QcServer& s, const std::string& line) {
  const bool ok = run_console_line(s, line);
  fill_world(s, s.world, false);
  return ok;
}

}  // namespace shooters_edge::server
