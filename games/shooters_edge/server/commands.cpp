// Client commands, chat, info changes, the console and pausing (commands.hpp states the routing).
//
// ge:summary The routing of client commands and console lines to the mod's hooks and the server
// ge:keywords client_command server_command set_user_info set_local_info request_pause call_hook
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_hooks_test.cpp
#include "shooters_edge/server/commands.hpp"

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
constexpr const char* kOverridable[] = {"rate",    "kill",    "pause",   "say",      "say_team",
                                        "minping", "airstep", "maps",    "ban",      "banip",
                                        "banrem",  "join",    "observe", "qtvusers", "god",
                                        "give",    "noclip",  "fly",     "notarget"};

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

// The single-player game's cheats as the original's server answers them, for any player alone or
// in co-op (refused only in deathmatch): `god`, `notarget`, `noclip`, `fly` toggled with the
// original's words, `give` a weapon (2..8), ammunition (s n r c) or health (h) with a count.
// The program's own fields and flags carry them; false: not one of them.
bool single_player_cheat(QcServer& s, int slot, const pg::Words& w) {
  const char* cmd = pg::word(w, 0);
  const bool god = std::strcmp(cmd, "god") == 0, notarget = std::strcmp(cmd, "notarget") == 0;
  const bool noclip = std::strcmp(cmd, "noclip") == 0, fly = std::strcmp(cmd, "fly") == 0;
  const bool give = std::strcmp(cmd, "give") == 0;
  if (!god && !notarget && !noclip && !fly && !give) return false;
  if (s.cvars.find("deathmatch") != nullptr && s.cvars.find("deathmatch")->on()) return true;
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

void run_server_command(QcServer& s, int slot, const pg::Words& w) {
  const char* cmd = pg::word(w, 0);
  const uint32_t me = 1u << slot;
  const int e = slot + 1;
  ClientInfo& c = s.clients[slot];
  if (std::strcmp(cmd, "kill") == 0) {
    if (s.sv.set.single_player_clock && s.sv.edicts[static_cast<size_t>(e)].health <= 0)
      print_to(s, me, 2, "Can't suicide -- allready dead!\n");  // the original's own words
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
  } else if (s.sv.set.single_player_clock && single_player_cheat(s, slot, w)) {
    // done: a single-player program's cheats (no sv_cheats; refused in deathmatch)
  } else if (std::strcmp(cmd, "god") == 0 || std::strcmp(cmd, "give") == 0 ||
             std::strcmp(cmd, "noclip") == 0 || std::strcmp(cmd, "fly") == 0) {
    if (!s.cvars.cheats()) {
      print_to(s, me, 2, "Cheats are not enabled on this server\n");
      return;
    }
    sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    if (std::strcmp(cmd, "noclip") == 0)
      ed.movetype = ed.movetype == sh::kMovetypeNoclip ? sh::kMovetypeWalk : sh::kMovetypeNoclip;
    if (std::strcmp(cmd, "fly") == 0)
      ed.movetype = ed.movetype == sh::kMovetypeFly ? sh::kMovetypeWalk : sh::kMovetypeFly;
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
  if (listed(kOverridable, cmd.c_str())) {
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
