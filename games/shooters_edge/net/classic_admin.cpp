// The administrator's words on the classic endpoint (classic_admin.hpp states them).
//
// ge:summary users, status, speedjump grants and rcon on the classic endpoint
// ge:keywords classic_admin users status speedjump rcon grant
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_admin_test.cpp
#include "shooters_edge/net/classic_admin.hpp"

#include <algorithm>
#include <cstdlib>

#include "console/console.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "progs/text.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_remote.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;

// A share as the key and the tables show it: at most two decimals, no trailing zeros.
std::string speedjump_share_text(float share) {
  std::string t = ge::core::num::to_fixed(static_cast<double>(share), 2);
  while (!t.empty() && t.back() == '0') t.pop_back();
  if (!t.empty() && t.back() == '.') t.pop_back();
  return t;
}

// `text` read whole as a share 0..1, rounded to hundredths; false when it is not one.
bool speedjump_parse_share(const std::string& text, float& out) {
  if (text.empty()) return false;
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !(v >= 0 && v <= 1)) return false;
  out = static_cast<float>(ge::core::num::round(v * 100)) / 100.0f;
  return true;
}

namespace {

constexpr const char* kGrantKey = "*speedjump";

std::string pad(std::string s, size_t width) {
  if (s.size() < width) s.append(width - s.size(), ' ');
  return s;
}

std::string pad_left(const std::string& s, size_t width) {
  return s.size() < width ? std::string(width - s.size(), ' ') + s : s;
}

int frags_of(const ClassicServer& s, int slot) {
  return s.clients[slot].rules_active ? static_cast<int>(s.game->world.clients[slot].numbers.frags)
                                      : 0;
}

float server_share(const ClassicServer& s) { return s.game->sv.set.move.speedjump; }

// The easy jump's column: `-`, `asks`, the share pm_speedjump allows, or `<share> granted`.
std::string easy_jump_column(const ClassicServer& s, int slot) {
  const ClassicClient& c = s.clients[slot];
  const std::string granted = info_get(c.userinfo, kGrantKey);
  if (!granted.empty()) return granted + " granted";
  if (info_get(c.userinfo, "cl_speedjump") != "1") return "-";
  return server_share(s) > 0 ? speedjump_share_text(server_share(s)) : "asks";
}

std::string address_column(const ClassicClient& c) {
  if (c.local) return info_get(c.userinfo, "*bot").empty() ? "local" : "bot";
  return gn::to_string(c.address);
}

std::string speedjump_list(const ClassicServer& s) {
  std::string t;
  int n = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClassicClient& c = s.clients[i];
    if (c.state == ConnState::free) continue;
    const std::string granted = info_get(c.userinfo, kGrantKey);
    if (granted.empty()) continue;
    t += "  " + std::to_string(c.userid) + " " + c.name + ": " + granted + "\n";
    n++;
  }
  const std::string head = "easy jump: pm_speedjump " + speedjump_share_text(server_share(s)) +
                           " for everyone who asks; ";
  if (n == 0) return head + "granted to nobody; next: `users`, then `speedjump <id> on`\n";
  return head + "granted to " + std::to_string(n) + ":\n" + t;
}

}  // namespace

std::string classic_users(const ClassicServer& s) {
  std::string t = "userid frags name\n------ ----- ----\n";
  int n = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClassicClient& c = s.clients[i];
    if (c.state == ConnState::free) continue;
    t += pad_left(std::to_string(c.userid), 6) + " " + pad_left(std::to_string(frags_of(s, i)), 5) +
         " " + c.name + "\n";
    n++;
  }
  return t + std::to_string(n) + " total users\n";
}

std::string classic_status_table(const ClassicServer& s) {
  int players = 0;
  for (const ClassicClient& c : s.clients)
    if (c.state != ConnState::free) players++;
  std::string t = s.cfg.hostname + " on " + s.game->map_name + ", " + std::to_string(players) +
                  " player" + (players == 1 ? "" : "s") + ", pm_speedjump " +
                  speedjump_share_text(server_share(s)) + "\n";
  t += "slot id  name             frags ping loss min address               easy jump\n";
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
         easy_jump_column(s, i) + "\n";
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

float classic_speedjump_granted(const ClassicServer& s, int slot) {
  const std::string v = info_get(s.clients[slot].userinfo, kGrantKey);
  return v.empty() ? -1.0f : static_cast<float>(std::strtod(v.c_str(), nullptr));
}

bool classic_speedjump(ClassicServer& s, int userid, float share, std::string& error) {
  const int slot = classic_find_seat(s, std::to_string(userid));
  if (slot < 0) {
    error = "speedjump: no player has user id " + std::to_string(userid) +
            "; next: `users` lists the ids";
    return false;
  }
  if (share > 1) share = 1;
  const std::string value = share < 0 ? std::string() : speedjump_share_text(share);
  if (info_get(s.clients[slot].userinfo, kGrantKey) == value) return true;
  classic_set_info(s, slot, kGrantKey, value);
  const std::string& name = s.clients[slot].name;
  classic_print_all(
      s, share < 0 ? name + "'s easy jump is no longer granted\n"
                   : name + " is granted the easy jump (share " + value + ", cl_speedjump 1)\n");
  s.log.push_back("speedjump " + std::to_string(slot) + " " + name + " " +
                  (value.empty() ? "off" : value));
  return true;
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
  if (cmd != "speedjump") return false;
  if (w.count < 2) {
    out = speedjump_list(s);
    return true;
  }
  const std::string who = pg::word(w, 1);
  const int slot = classic_find_seat(s, who);
  if (slot < 0) {
    out = "speedjump: no player is " + who + "; next: `users` lists the ids and names\n";
    return true;
  }
  const std::string what = w.count >= 3 ? pg::word(w, 2) : "on";
  float share = -1;
  if (what == "on")
    share = kGrantShare;
  else if (what != "off" && !speedjump_parse_share(what, share)) {
    out =
        "speedjump: " + what + " is not on, off or a share 0..1; next: speedjump " + who + " on\n";
    return true;
  }
  std::string error;
  if (!classic_speedjump(s, s.clients[slot].userid, share, error)) {
    out = error + "\n";
    return true;
  }
  const ClassicClient& c = s.clients[slot];
  out = share < 0 ? c.name + " (id " + std::to_string(c.userid) + "): easy jump not granted\n"
                  : c.name + " (id " + std::to_string(c.userid) + "): easy jump granted at " +
                        speedjump_share_text(share) +
                        (info_get(c.userinfo, "cl_speedjump") == "1"
                             ? "\n"
                             : "; it acts once he sets cl_speedjump 1\n");
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
  con.add_command("users", "users: the players on the server, with their user ids",
                  [&r, hosted](Con& c, const Args& a) {
                    if (r.active) {
                      client_command(r.end, "users");
                      return;
                    }
                    if (!hosted(c, a))
                      c.print("users: no match is hosted or joined; next: map <name> or connect");
                  });
  con.add_command("status",
                  "status: the match you host: every player's id, ping, address and easy jump",
                  [&r, hosted](Con& c, const Args& a) {
                    if (hosted(c, a)) return;
                    c.print(r.active ? "status: answers for the match you host; on a joined server "
                                       "next: users, or rcon <password> status"
                                     : "status: no match is hosted; next: map <name>");
                  });
  con.add_command("speedjump",
                  "speedjump <id|name> on|off|<share 0..1>: grant one player the easy jump on "
                  "the match you host (alone: who has it)",
                  [&r, hosted](Con& c, const Args& a) {
                    if (hosted(c, a)) return;
                    c.print(r.active ? "speedjump: the server's administrator grants it; next: "
                                       "rcon <password> speedjump <id> on"
                                     : "speedjump: no match is hosted; next: map <name>");
                  });
  con.add_command("rcon", "rcon <password> <line>: a line for the console of the server joined",
                  [&r](Con& c, const Args& a) {
                    if (!r.active) {
                      c.print("rcon: not connected; next: connect <host>, then rcon again");
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
