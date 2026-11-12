// The administrator's words on the classic endpoint (classic_admin.hpp states them).
//
// ge:summary users, status, the player aids' grants and rcon on the classic endpoint
// ge:keywords classic_admin users status speedjump aimhelp rjump player-aids rcon grant
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_admin_test.cpp
#include "shooters_edge/net/classic_admin.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>
#include <vector>

#include "console/console.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "progs/text.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_remote.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;

const ge::server::AidKind kAids[] = {
    {"speedjump", "easy jump", "*speedjump", "0.8", true},
    {"aimhelp", "aim help", "*aimhelp", "1", false},
    {"rjump", "rocket jump helper", "*rjump", "1", false},
};

std::span<const ge::server::AidKind> classic_aids() { return kAids; }

namespace {

namespace gsv = ge::server;

const gsv::AidKind& speedjump_kind() { return kAids[0]; }
const gsv::AidKind& aimhelp_kind() { return kAids[1]; }

int frags_of(const ClassicServer& s, int slot) {
  return s.clients[slot].rules_active ? static_cast<int>(s.game->world.clients[slot].numbers.frags)
                                      : 0;
}

float server_share(const ClassicServer& s) { return s.game->sv.set.move.speedjump; }

bool cvar_on(const ClassicServer& s, const char* name) {
  const ge::console::Cvar* v = s.game->cvars.find(name);
  return v != nullptr && v->on();
}

std::string leeway_text(const ClassicServer& s) {
  const sh::ServerSettings& set = s.game->sv.set;
  return gsv::aid_share_text(set.aimhelp_x) + " across, " + gsv::aid_share_text(set.aimhelp_y) +
         " up and down";
}

// What seat `slot` has of `kind`: how it is allowed and how it is used.
gsv::AidSeat seat_aid(const ClassicServer& s, int slot, const gsv::AidKind& kind) {
  const ClassicClient& c = s.clients[slot];
  gsv::AidSeat a;
  a.kind = &kind;
  const std::string granted = info_get(c.userinfo, kind.key);
  a.granted = !granted.empty();
  const std::string name = kind.name;
  if (name == "speedjump") {
    if (a.granted)
      a.allowed = "granted " + granted;
    else if (server_share(s) > 0)
      a.allowed = "on for all at " + gsv::aid_share_text(server_share(s));
    if (info_get(c.userinfo, "cl_speedjump") == "1") a.used = "cl_speedjump 1";
  } else if (name == "aimhelp") {
    if (a.granted) a.allowed = "granted, " + leeway_text(s);
    const int helped = static_cast<size_t>(slot) < s.game->sv.clients.size()
                           ? s.game->sv.clients[slot].aim_helped
                           : 0;
    if (helped > 0)
      a.used = std::to_string(helped) + " shot" + (helped == 1 ? "" : "s") + " helped";
  } else if (name == "rjump") {
    if (a.granted)
      a.allowed = "granted";
    else if (cvar_on(s, "sv_rjump") || cvar_on(s, "sv_rrjump"))
      a.allowed = "on for all";
  } else if (a.granted) {
    a.allowed = "granted " + granted;
  }
  return a;
}

gsv::UserRow user_row(const ClassicServer& s, int slot) {
  const ClassicClient& c = s.clients[slot];
  gsv::UserRow r;
  r.userid = c.userid;
  r.frags = frags_of(s, slot);
  r.name = c.name;
  r.spectator = c.spectator;
  r.bot = c.local && !info_get(c.userinfo, "*bot").empty();
  for (const gsv::AidKind& k : kAids) {
    gsv::AidSeat a = seat_aid(s, slot, k);
    if (!a.allowed.empty() || !a.used.empty()) r.aids.push_back(std::move(a));
  }
  return r;
}

std::vector<gsv::UserRow> user_rows(const ClassicServer& s) {
  std::vector<gsv::UserRow> rows;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].state != ConnState::free) rows.push_back(user_row(s, i));
  return rows;
}

std::string pad(std::string s, size_t width) {
  if (s.size() < width) s.append(width - s.size(), ' ');
  return s;
}

std::string pad_left(const std::string& s, size_t width) {
  return s.size() < width ? std::string(width - s.size(), ' ') + s : s;
}

std::string address_column(const ClassicClient& c) {
  if (c.local) return info_get(c.userinfo, "*bot").empty() ? "local" : "bot";
  return gn::to_string(c.address);
}

// An aid's grants: who has it, and how to give it.
std::string aid_list(const ClassicServer& s, const gsv::AidKind& kind) {
  std::string t;
  int n = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClassicClient& c = s.clients[i];
    if (c.state == ConnState::free) continue;
    const std::string granted = info_get(c.userinfo, kind.key);
    if (granted.empty()) continue;
    t += "  " + std::to_string(c.userid) + " " + c.name + ": " + granted + "\n";
    n++;
  }
  std::string head = std::string(kind.words) + ": ";
  if (&kind == &speedjump_kind())
    head += "pm_speedjump " + gsv::aid_share_text(server_share(s)) + " for everyone who asks; ";
  if (&kind == &aimhelp_kind()) head += leeway_text(s) + "; ";
  if (n == 0)
    return head + "granted to nobody (type \"users\", then \"" + kind.name + " <id> on\")\n";
  return head + "granted to " + std::to_string(n) + ":\n" + t;
}

std::string join_detail(const gsv::AidKind& kind, const void* ctx) {
  const ClassicServer& s = *static_cast<const ClassicServer*>(ctx);
  if (&kind == &aimhelp_kind()) return leeway_text(s);
  return {};
}

}  // namespace

std::string classic_users(const ClassicServer& s) { return gsv::users_text(user_rows(s)); }

std::string classic_status_table(const ClassicServer& s) {
  int players = 0;
  for (const ClassicClient& c : s.clients)
    if (c.state != ConnState::free) players++;
  std::string t = s.cfg.hostname + " on " + s.game->map_name + ", " + std::to_string(players) +
                  " player" + (players == 1 ? "" : "s") + ", pm_speedjump " +
                  gsv::aid_share_text(server_share(s)) + "\n";
  t += "slot id  name             frags ping loss min address               aids\n";
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClassicClient& c = s.clients[i];
    if (c.state == ConnState::free) continue;
    const int ping = c.local                         ? 0
                     : c.state != ConnState::spawned ? 999
                     : c.ping_samples == 0           ? 0
                                                     : static_cast<int>(c.ping_s * 1000);
    const int minutes = static_cast<int>((s.game->sv.time - c.connect_time) / 60);
    std::string name = c.name;
    if (c.spectator) name += " (spec)";
    t += pad_left(std::to_string(i), 4) + " " + pad_left(std::to_string(c.userid), 3) + " " +
         pad(name, 16) + " " + pad_left(std::to_string(frags_of(s, i)), 5) + " " +
         pad_left(std::to_string(ping), 4) + " " + pad_left(std::to_string(c.loss_pct), 4) + " " +
         pad_left(std::to_string(minutes), 3) + " " + pad(address_column(c), 21) + " " +
         gsv::aids_column(user_row(s, i)) + "\n";
  }
  return t;
}

int classic_find_seat(const ClassicServer& s, const std::string& id_or_name) {
  char* end = nullptr;
  const long id = std::strtol(id_or_name.c_str(), &end, 10);
  if (!id_or_name.empty() && *end == '\0')
    for (int i = 0; i < sh::kMaxClients; i++)
      if (s.clients[i].state != ConnState::free && s.clients[i].userid == id) return i;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].state != ConnState::free && s.clients[i].name == id_or_name) return i;
  return -1;
}

std::string classic_aid_granted(const ClassicServer& s, int slot, const gsv::AidKind& kind) {
  return info_get(s.clients[slot].userinfo, kind.key);
}

bool classic_grant(ClassicServer& s, const gsv::AidKind& kind, int userid, const std::string& value,
                   std::string& error) {
  const int slot = classic_find_seat(s, std::to_string(userid));
  if (slot < 0) {
    error = "No player has user id " + std::to_string(userid) + ". Type \"users\" to see the ids.";
    return false;
  }
  if (info_get(s.clients[slot].userinfo, kind.key) == value) return true;
  classic_set_info(s, slot, kind.key, value);
  const std::string& name = s.clients[slot].name;
  std::string told;
  if (value.empty())
    told = name + "'s " + kind.words + " is no longer granted\n";
  else if (&kind == &speedjump_kind())
    told = name + " is granted the easy jump (share " + value + ", cl_speedjump 1)\n";
  else if (&kind == &aimhelp_kind())
    told = name + " is granted the aim help (" + leeway_text(s) + ")\n";
  else
    told = name + " is granted the " + kind.words + "\n";
  classic_print_all(s, told);
  s.log.push_back(std::string(kind.name) + " " + std::to_string(slot) + " " + name + " " +
                  (value.empty() ? "off" : value));
  return true;
}

float classic_speedjump_granted(const ClassicServer& s, int slot) {
  const std::string v = classic_aid_granted(s, slot, speedjump_kind());
  return v.empty() ? -1.0f : static_cast<float>(std::strtod(v.c_str(), nullptr));
}

bool classic_speedjump(ClassicServer& s, int userid, float share, std::string& error) {
  if (share > 1) share = 1;
  return classic_grant(s, speedjump_kind(), userid,
                       share < 0 ? std::string() : gsv::aid_share_text(share), error);
}

std::string classic_aids_join_line(const ClassicServer& s) {
  return gsv::aids_join_line(user_rows(s), &join_detail, &s);
}

void classic_tell_aids(ClassicServer& s, int slot) {
  const std::string line = classic_aids_join_line(s);
  if (!line.empty()) client_print(s, slot, 2, line);
}

bool classic_admin(ClassicServer& s, const std::string& line, std::string& out) {
  pg::Words w;
  pg::tokenize_words(line.c_str(), w);
  const std::string cmd = pg::word(w, 0);
  if (cmd == "users") {
    out = classic_users(s);
    return true;
  }
  if (cmd == "status") {
    out = classic_status_table(s);
    return true;
  }
  gsv::AidOrder order;
  std::string error;
  if (!gsv::aid_parse(kAids, line, order, error)) return false;
  if (!error.empty()) {
    out = error + "\n";
    return true;
  }
  if (order.who.empty()) {
    out = aid_list(s, *order.kind);
    return true;
  }
  const int slot = classic_find_seat(s, order.who);
  if (slot < 0) {
    out = "No player is called " + order.who + ". Type \"users\" to see the ids and names.\n";
    return true;
  }
  if (!classic_grant(s, *order.kind, s.clients[slot].userid, order.value, error)) {
    out = error + "\n";
    return true;
  }
  const ClassicClient& c = s.clients[slot];
  const std::string who = c.name + " (id " + std::to_string(c.userid) + "): ";
  if (order.value.empty())
    out = who + order.kind->words + " not granted\n";
  else if (order.kind == &speedjump_kind())
    out = who + "easy jump granted at " + order.value +
          (info_get(c.userinfo, "cl_speedjump") == "1" ? "\n"
                                                       : "; it acts once he sets cl_speedjump 1\n");
  else if (order.kind == &aimhelp_kind())
    out = who + "aim help granted (" + leeway_text(s) + ")\n";
  else
    out = who + order.kind->words + " granted\n";
  return true;
}

void classic_rcon(ClassicServer& s, const gn::Address& from, const std::string& text) {
  if (s.cfg.rcon_password.empty()) {
    send_oob_print(s, from, "Remote console is not offered by this server.\n");
    return;
  }
  const int64_t second = static_cast<int64_t>(s.now);
  if (second != s.rcon_second) {
    s.rcon_second = second;
    s.rcon_tries = 0;
  }
  if (++s.rcon_tries > kRconTriesPerSecond) {
    s.counts.rcon_refused++;
    return;
  }
  pg::Words w;
  pg::tokenize_words(text.c_str(), w);
  if (w.count < 3 || pg::word(w, 1) != s.cfg.rcon_password) {
    s.counts.rcon_refused++;
    s.log.push_back("rcon refused " + gn::to_string(from));
    send_oob_print(s, from, "Bad rcon_password.\n");
    return;
  }
  // The line: what follows the password, as it was sent.
  const size_t pass_at = text.find(s.cfg.rcon_password, text.find("rcon") + 4);
  size_t at = pass_at + s.cfg.rcon_password.size();
  if (at < text.size() && text[at] == '"') at++;
  while (at < text.size() && text[at] == ' ') at++;
  std::string line = text.substr(at);
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
  s.counts.rcon_run++;
  s.log.push_back("rcon " + gn::to_string(from) + " " + line);
  std::string answer;
  if (!classic_admin(s, line, answer)) {
    sv::RulesCommon& game = *s.game;
    const size_t before = game.console.size();
    if (!s.rules->console(line) && game.console.size() == before)
      answer = "Unknown command \"" + std::string(pg::word(w, 2)) + "\"\n";
    for (size_t i = before; i < game.console.size(); i++) answer += game.console[i] + "\n";
  }
  send_oob_print(s, from, answer.empty() ? std::string("\n") : answer);
}

namespace {

void print_lines(ge::console::Console& con, const std::string& text) {
  for (size_t at = 0; at < text.size();) {
    const size_t end = std::min(text.find('\n', at), text.size());
    con.print(text.substr(at, end - at));
    at = end + 1;
  }
}

}  // namespace

void add_admin_commands(ge::console::Console& con, client::Listen& l, Remote& r) {
  using Con = ge::console::Console;
  using Args = ge::console::Args;
  const auto hosted = [&l](Con& c, const Args& a) {
    std::string line = a[0];
    const std::string rest = a.rest(0);
    if (!rest.empty()) line += " " + rest;
    std::string out;
    if (listen_admin(l, line, out)) {
      print_lines(c, out);
      return true;
    }
    return false;
  };
  con.add_command("users", "users: the players on the server, with their user ids and aids",
                  [&r, hosted](Con& c, const Args& a) {
                    if (r.active) {
                      client_command(r.end, "users");
                      return;
                    }
                    if (!hosted(c, a))
                      c.print("users: you are not in a match. Start one with map, or connect.");
                  });
  con.add_command("status", "status: the match you host: every player's id, ping, address and aids",
                  [&r, hosted](Con& c, const Args& a) {
                    if (hosted(c, a)) return;
                    c.print(r.active ? "status: answers for the match you host; on a joined server "
                                       "type users, or rcon <password> status"
                                     : "status: you are not hosting a match");
                  });
  for (const ge::server::AidKind& k : kAids) {
    const std::string name = k.name;
    std::string usage = name;
    usage += " <id|name> on|off";
    if (k.share) usage += "|<share 0..1>";
    usage += ": grant one player the ";
    usage += k.words;
    usage += " on the match you host (alone: who has it)";
    con.add_command(name, usage, [&r, hosted, name](Con& c, const Args& a) {
      if (hosted(c, a)) return;
      if (!r.active) {
        c.print("You are not hosting a match.");
        return;
      }
      std::string how = "The server's administrator grants it (rcon <password> ";
      how += name;
      how += " <id> on), or the players by a vote (vote ";
      how += name;
      how += " <player> on).";
      c.print(how);
    });
  }
  con.add_command("rcon", "rcon <password> <line>: a line for the console of the server joined",
                  [&r](Con& c, const Args& a) {
                    if (!r.active) {
                      c.print("rcon: not connected to a server");
                      return;
                    }
                    if (a.size() < 3) {
                      c.print("rcon: give the password and a line (rcon secret users)");
                      return;
                    }
                    client_rcon(r.end, a[1], a.rest(1));
                  });
}

}  // namespace shooters_edge::net
