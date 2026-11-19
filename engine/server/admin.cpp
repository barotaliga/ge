// The administrator's features (admin.hpp).
//
// ge:summary The administrator's features defined: status, kick, bans, rcon, the log
// ge:keywords server admin status kick ban rcon log
// ge:skill net
// ge:test tests/server_feature_test.cpp
#include "server/admin.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>

#include "core/num_text.hpp"

namespace ge::server {

namespace gn = ge::net;
namespace cn = ge::console;

namespace {

AdminFeatures& of(void* self) { return *static_cast<AdminFeatures*>(self); }

int64_t now_s(const Features& f) {
  const Server* s = f.server;
  return s && s->config.unix_us ? s->config.unix_us() / 1000000 : 0;
}

bool has_session(const Features& f) {
  return f.server && f.server->networked && !f.server->own_protocol;
}

std::string name_of(const Features& f, int seat) {
  if (!has_session(f) || seat < 0 || seat >= gn::kMaxPlayers) return "?";
  return f.server->session.players[seat].name;
}

std::string address_of(const Features& f, int seat) {
  gn::Address a;
  if (!has_session(f) || !gn::session_host_address(f.server->session, seat, a)) return "-";
  return gn::to_string(a);
}

// The address as a ban pattern names it: the IPv4 address a simulated one maps to, no port.
std::string ban_pattern_of(const Features& f, int seat) {
  gn::Address a;
  if (!has_session(f) || !gn::session_host_address(f.server->session, seat, a)) return "";
  uint8_t b[4] = {};
  if (a.family == gn::Family::sim)
    gn::sim_to_ipv4(a, b);
  else if (a.family == gn::Family::ipv4)
    std::copy(a.bytes, a.bytes + 4, b);
  else
    return "";
  char out[20];
  std::snprintf(out, sizeof out, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
  return out;
}

std::string pad(std::string s, size_t n, bool right = false) {
  if (s.size() >= n) return s;
  return right ? std::string(n - s.size(), ' ') + s : s + std::string(n - s.size(), ' ');
}

// THE STATUS: the header and a row a seat; `addresses` for the console.
std::string status_text(const Features& f, bool addresses) {
  const Server& s = *f.server;
  std::string out = s.config.hostname + "  map " + s.config.map;
  if (!has_session(f)) return out + "  (its players are the game's own)\n";
  const gn::SessionHost& h = s.session;
  const int n = gn::session_player_count(h, false);
  out += "  " + std::to_string(n) + " of " + std::to_string(s.config.maxclients) + " players\n";
  out += "seat  ping  team  name" + std::string(addresses ? "                      address" : "") +
         "\n";
  for (int i = 0; i < h.config.max_players; i++) {
    const gn::Player& p = h.players[i];
    if (!p.used) continue;
    std::string row = pad(std::to_string(i), 4, true) + "  ";
    row += pad(p.present ? std::to_string(p.ping_ms) : "away", 4, true) + "  ";
    row += pad(p.team == gn::kNoTeam ? "-" : std::to_string(p.team), 4, true) + "  ";
    row += addresses ? pad(p.name, 26) + address_of(f, i) : std::string(p.name);
    out += row + "\n";
  }
  return out;
}

// FEATURE: status.
void status_attach(void*, Features& f) {
  Features* fs = &f;
  f.console->add_command("status",
                         "status: the server's name, map and players, with their addresses",
                         [fs](cn::Console& c, const cn::Args&) {
                           if (fs->server == nullptr) {
                             c.print("status: the server is not running");
                             return;
                           }
                           std::string text = status_text(*fs, true);
                           if (!text.empty() && text.back() == '\n') text.pop_back();
                           c.print(text);
                         });
}

bool status_command(void*, Features& f, int player, const std::vector<std::string>& w,
                    std::string_view) {
  if (w[0] != "status") return false;
  feature_answer(f, player, status_text(f, false));
  return true;
}

// FEATURE: kick.
void kick_attach(void* self, Features& f) {
  Features* fs = &f;
  AdminFeatures* a = &of(self);
  f.console->add_command(
      "kick", "kick <seat|name>: remove a player from the server; they may come back",
      [fs, a](cn::Console& c, const cn::Args& args) {
        if (args.size() < 2) {
          c.print("usage: kick <seat|name>");
          return;
        }
        const int seat = feature_player(*fs, args[1]);
        if (seat == gn::kNoPlayer || !has_session(*fs)) {
          c.print("kick: no player " + args[1] + " on the server; status lists them");
          return;
        }
        const std::string name = name_of(*fs, seat);
        if (!gn::session_host_remove(fs->server->session, seat, gn::SessionWhy::kicked, false,
                                     fs->server->now_us)) {
          c.print("kick: " + name + " cannot be removed (the host's own player)");
          return;
        }
        a->kicks++;
        admin_log(*a, *fs, "kick " + std::to_string(seat) + " " + name);
        c.print(name + " was kicked.");
      });
}

// FEATURE: bans.
void write_bans(AdminFeatures& a, Features& f) {
  if (a.ban_file.empty()) return;
  std::string lines;
  ban_command(a.bans, "writeip", now_s(f), lines);
  if (!f.console->write_file(a.ban_file, lines))
    f.console->print("bans: " + a.ban_file + " could not be written; check sv_banfile's folder");
}

// Every seated player the list now refuses, removed.
void remove_refused(AdminFeatures& a, Features& f) {
  if (!has_session(f)) return;
  gn::SessionHost& h = f.server->session;
  for (int i = 0; i < h.config.max_players; i++) {
    gn::Address addr;
    if (!h.players[i].used || !gn::session_host_address(h, i, addr)) continue;
    if (!ban_refuses(a.bans, a.ban_mode, addr, now_s(f))) continue;
    const std::string name = name_of(f, i);
    if (gn::session_host_remove(h, i, gn::SessionWhy::banned, true, f.server->now_us)) {
      admin_log(a, f, "banned " + std::to_string(i) + " " + name);
      f.console->print(name + " was banned.");
    }
  }
}

void bans_attach(void* self, Features& f) {
  AdminFeatures* a = &of(self);
  Features* fs = &f;
  cn::Cvars& cv = f.console->cvars();
  constexpr double kAny = std::numeric_limits<double>::infinity();
  const std::string file = a->ban_file;
  cv.bind({.name = "sv_banfile",
           .fallback = file,
           .flags = cn::kServer,
           .help = "The file the ban list is kept in, in the game's folder.",
           .kind = cn::CvarKind::text,
           .min = -kAny,
           .max = kAny,
           .values = "\"\": the list is not kept; FILE: that file, as addip lines"},
          &a->ban_file);
  if (cv.find("sv_banmode") == nullptr) {
    cv.bind({.name = "sv_banmode",
             .fallback = "1",
             .flags = cn::kServer,
             .help = "How the ban list is read.",
             .kind = cn::CvarKind::integer,
             .min = 0,
             .max = 1,
             .values = "1: the listed addresses are refused (the default); 0: only they may join"},
            &a->ban_mode);
  }
  for (const char* word : {"addip", "removeip", "listip", "writeip"}) {
    const std::string w = word;
    f.console->add_command(
        w,
        w == "addip"      ? "addip <address> [ban|safe] [+seconds|time]: ban an address pattern"
        : w == "removeip" ? "removeip <address>: take a pattern off the ban list"
        : w == "listip"   ? "listip: the ban list"
                          : "writeip: write the ban list to sv_banfile now",
        [a, fs, w](cn::Console& c, const cn::Args& args) {
          std::string out;
          ban_command(a->bans, w + (args.size() > 1 ? " " + args.rest(0) : ""), now_s(*fs), out);
          if (w != "listip") write_bans(*a, *fs);
          if (w == "writeip") out = "the ban list is written to " + a->ban_file + "\n";
          if (!out.empty() && out.back() == '\n') out.pop_back();
          c.print(out);
          if (w == "addip") {
            admin_log(*a, *fs, "addip " + args.rest(0));
            remove_refused(*a, *fs);
          }
        });
  }
  f.console->add_command(
      "ban", "ban <seat|name> [minutes]: ban a player's address and remove them",
      [a, fs](cn::Console& c, const cn::Args& args) {
        if (args.size() < 2) {
          c.print("usage: ban <seat|name> [minutes]");
          return;
        }
        const int seat = feature_player(*fs, args[1]);
        const std::string pattern = seat == gn::kNoPlayer ? "" : ban_pattern_of(*fs, seat);
        if (pattern.empty()) {
          c.print("ban: no player " + args[1] + " connected to the server; status lists them");
          return;
        }
        std::string line = "addip " + pattern + " ban";
        if (args.size() > 2) {
          char* end = nullptr;
          const double minutes = std::strtod(args[2].c_str(), &end);
          if (end == args[2].c_str() || *end != 0 || !(minutes > 0)) {
            c.print("ban: " + args[2] + " is not a number of minutes");
            return;
          }
          line += " +" + std::to_string(static_cast<int64_t>(minutes * 60));
        }
        std::string out;
        ban_command(a->bans, line, now_s(*fs), out);
        write_bans(*a, *fs);
        admin_log(*a, *fs,
                  "ban " + std::to_string(seat) + " " + name_of(*fs, seat) + " " + pattern);
        remove_refused(*a, *fs);
      });
}

bool bans_admit(void* self, Features& f, const gn::Address& from) {
  AdminFeatures& a = of(self);
  if (!ban_refuses(a.bans, a.ban_mode, from, now_s(f))) return true;
  admin_log(a, f, "refused " + gn::to_string(from) + " (banned)");
  return false;
}

void bans_frame(void* self, Features& f) {
  AdminFeatures& a = of(self);
  if (a.bans_read) return;
  a.bans_read = true;
  std::string text;
  if (a.ban_file.empty() || !f.console->read_file(a.ban_file, text)) return;
  size_t at = 0;
  int read = 0;
  while (at < text.size()) {
    size_t nl = text.find('\n', at);
    if (nl == std::string::npos) nl = text.size();
    std::string out;
    if (ban_command(a.bans, std::string_view(text).substr(at, nl - at), now_s(f), out)) read++;
    at = nl + 1;
  }
  admin_log(a, f, "bans: " + std::to_string(read) + " read from " + a.ban_file);
}

// FEATURE: rcon.
bool same_password(std::string_view a, std::string_view b) {
  // Every byte compared, whatever differs first, so the answer's time tells nothing.
  uint8_t diff = a.size() == b.size() ? 0 : 1;
  for (size_t i = 0; i < a.size(); i++)
    diff |= static_cast<uint8_t>(a[i] ^ (i < b.size() ? b[i] : 0));
  return diff == 0;
}

bool rcon_command(void* self, Features& f, int player, const std::vector<std::string>& w,
                  std::string_view line) {
  if (w[0] != "rcon") return false;
  AdminFeatures& a = of(self);
  const Server& s = *f.server;
  if (s.config.rcon_password.empty()) {
    a.rcon_refused++;
    feature_answer(f, player, "This server has no remote console.");
    return true;
  }
  const bool seat_ok = player >= 0 && player < kMaxClients;
  if (seat_ok && s.now_us < a.rcon_until_us[player]) {
    a.rcon_refused++;
    admin_log(a, f, "rcon refused for " + name_of(f, player) + ": tries waiting");
    const double wait = static_cast<double>(a.rcon_until_us[player] - s.now_us) / 1e6;
    feature_answer(
        f, player,
        "Too many wrong passwords. Try again in " + core::num::to_fixed(wait, 0) + " seconds.");
    return true;
  }
  if (w.size() < 3 || !same_password(w[1], s.config.rcon_password)) {
    a.rcon_refused++;
    admin_log(a, f, "rcon refused for " + name_of(f, player) + ": bad password");
    if (seat_ok && ++a.rcon_wrong[player] >= kRconTries) {
      a.rcon_wrong[player] = 0;
      a.rcon_until_us[player] = s.now_us + static_cast<int64_t>(kRconLockSeconds * 1e6);
    }
    feature_answer(f, player,
                   w.size() < 3 ? "Usage: rcon <password> <command>" : "Bad rcon password.");
    return true;
  }
  if (seat_ok) a.rcon_wrong[player] = 0;
  // The line after the password, as the player wrote it.
  size_t at = line.find(w[1]);
  at = at == std::string_view::npos ? line.size() : at + w[1].size();
  while (at < line.size() && line[at] == ' ') at++;
  const std::string run(line.substr(at));
  a.rcon_runs++;
  admin_log(a, f, "rcon by " + name_of(f, player) + ": " + run);
  // What the console prints while it runs goes back to the player (and still to the console).
  cn::Console& c = *f.console;
  const std::function<void(std::string_view)> before = c.sink();
  std::string said;
  c.set_sink([&](std::string_view l) {
    said.append(l).push_back('\n');
    if (before) before(l);
  });
  c.execute(run);
  c.set_sink(before);
  feature_answer(f, player, said.empty() ? std::string("Done.") : said);
  return true;
}

// FEATURE: the log.
void log_attach(void* self, Features& f) {
  AdminFeatures* a = &of(self);
  constexpr double kAny = std::numeric_limits<double>::infinity();
  f.console->cvars().bind({.name = "sv_logfile",
                           .fallback = a->log_file,
                           .flags = cn::kServer,
                           .help = "The file the server log is added to, in the game's folder.",
                           .kind = cn::CvarKind::text,
                           .min = -kAny,
                           .max = kAny,
                           .values = "\"\": no file (the default); FILE: that file"},
                          &a->log_file);
}

void log_connect(void* self, Features& f, int player) {
  admin_log(
      of(self), f,
      "connect " + std::to_string(player) + " " + name_of(f, player) + " " + address_of(f, player));
}

void log_disconnect(void* self, Features& f, int player, gn::SessionWhy why) {
  admin_log(of(self), f, "disconnect " + std::to_string(player) + " " + gn::why_name(why));
}

bool log_chat(void* self, Features& f, int player, gn::ChatScope scope, std::string_view line) {
  admin_log(of(self), f,
            "chat " + std::string(gn::scope_name(scope)) + " " + name_of(f, player) + ": " +
                std::string(line));
  return false;
}

bool log_command(void* self, Features& f, int player, const std::vector<std::string>& w,
                 std::string_view line) {
  // Runs first: every line logged by its first word alone (an rcon line holds a password), then
  // left for the features after it.
  (void)line;
  admin_log(of(self), f, "command by " + name_of(f, player) + ": " + w[0]);
  return false;
}

void log_phase(void* self, Features& f, gn::SessionPhase phase) {
  admin_log(of(self), f, std::string("phase ") + gn::phase_name(phase));
}

}  // namespace

void admin_log(AdminFeatures& a, const Features& f, std::string_view line) {
  const double t = f.server ? static_cast<double>(f.server->now_us) / 1e6 : 0;
  std::string stamped = core::num::to_fixed(t, 2) + " " + std::string(line);
  if (!a.log_file.empty() && f.console && !f.console->search_path().empty()) {
    std::ofstream out(f.console->search_path().front() / a.log_file, std::ios::app);
    if (out) out << stamped << '\n';
  }
  a.log.push_back(std::move(stamped));
  if (a.log.size() > kLogKept) a.log.pop_front();
}

Feature admin_log_feature(AdminFeatures& a) {
  Feature log;
  log.name = "log";
  log.self = &a;
  log.attach = log_attach;
  log.connect = log_connect;
  log.disconnect = log_disconnect;
  log.chat = log_chat;
  log.command = log_command;
  log.phase = log_phase;
  return log;
}

void features_log(Features& f, std::string_view line) {
  for (const Feature& feature : f.list)
    if (feature.attach == log_attach) admin_log(of(feature.self), f, line);
}

std::vector<Feature> admin_features(AdminFeatures& a) {
  std::vector<Feature> out;
  out.push_back(admin_log_feature(a));
  Feature status;
  status.name = "status";
  status.self = &a;
  status.attach = status_attach;
  status.command = status_command;
  out.push_back(status);
  Feature kick;
  kick.name = "kick";
  kick.self = &a;
  kick.attach = kick_attach;
  out.push_back(kick);
  Feature bans;
  bans.name = "bans";
  bans.self = &a;
  bans.attach = bans_attach;
  bans.admit = bans_admit;
  bans.frame = bans_frame;
  out.push_back(bans);
  Feature rcon;
  rcon.name = "rcon";
  rcon.self = &a;
  rcon.command = rcon_command;
  out.push_back(rcon);
  return out;
}

}  // namespace ge::server
