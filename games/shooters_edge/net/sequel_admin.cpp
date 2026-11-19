// The sequel server's administrator words and remote console (sequel_admin.hpp).
//
// ge:summary The sequel server's administrator words and its remote console
// ge:keywords sequel admin rcon status kick dumpuser bans
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_admin_test.cpp
#include "shooters_edge/net/sequel_admin.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "classic/protocol/info.hpp"
#include "server/bans.hpp"
#include "shooters_edge/net/sequel_net.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;

// A line in the server's log (kept to its first 512, as the endpoint's own).
void note(SequelNetServer& s, std::string line) {
  if (s.log.size() < 512) s.log.push_back(std::move(line));
}

int64_t unix_now(const SequelNetServer& s) { return s.cfg.unix_time ? s.cfg.unix_time() : 0; }

// Every client on the server whose address the list now refuses, dropped.
void drop_refused(SequelNetServer& s) {
  for (int i = 0; i < sequel_net_slots(s); i++) {
    const SequelNetClient& c = s.clients[i];
    if (!sequel_held(c)) continue;
    if (gs::ban_refuses(s.bans, s.cfg.ban_mode, c.address, unix_now(s)))
      sequel_net_drop(s, i, "banned");
  }
}

}  // namespace

std::string sequel_status_table(const SequelNetServer& s) {
  std::string t = "map              : " + (*s.rules)->map_name + "\n";
  t += "num score ping name            lastmsg address               qport \n";
  t += "--- ----- ---- --------------- ------- --------------------- ------\n";
  char row[160];
  for (int i = 0; i < sequel_net_slots(s); i++) {
    const SequelNetClient& c = s.clients[i];
    if (c.stage == SequelSlot::free) continue;
    const int frags = static_cast<int>((*s.rules)->world.clients[i].numbers.frags);
    const std::string ping = c.stage == SequelSlot::signon   ? "CNCT"
                             : c.stage == SequelSlot::zombie ? "ZMBI"
                                                             : "0";
    const int last = static_cast<int>((s.now - c.heard) * 1000.0);
    std::snprintf(row, sizeof row, "%3d %5d %4s %-15s %7d %-21s %5d\n", i, frags, ping.c_str(),
                  c.name.c_str(), last, gn::to_string(c.address).c_str(), int(c.qport));
    t += row;
  }
  return t;
}

int sequel_find_slot(const SequelNetServer& s, const std::string& word, std::string& error) {
  if (!word.empty() && word[0] >= '0' && word[0] <= '9') {
    const int n = std::atoi(word.c_str());
    if (n < 0 || n >= sequel_net_slots(s)) {
      error = "Bad client slot: " + std::to_string(n) + "\n";
      return -1;
    }
    if (!sequel_held(s.clients[n])) {
      error = "Client " + std::to_string(n) + " is not active\n";
      return -1;
    }
    return n;
  }
  for (int i = 0; i < sequel_net_slots(s); i++)
    if (sequel_held(s.clients[i]) && s.clients[i].name == word) return i;
  error = "Userid " + word + " is not on the server\n";
  return -1;
}

bool sequel_admin(SequelNetServer& s, const std::string& line, std::string& out) {
  const std::vector<std::string> w = sequel_words(line);
  if (w.empty()) return false;
  const std::string& cmd = w[0];
  if (cmd == "status") {
    out = sequel_status_table(s);
    return true;
  }
  if (cmd == "kick" || cmd == "dumpuser") {
    if (w.size() < 2) {
      out = "Usage: " + cmd + " <userid>\n";
      return true;
    }
    std::string error;
    const int slot = sequel_find_slot(s, w[1], error);
    if (slot < 0) {
      out = error;
      return true;
    }
    SequelNetClient& c = s.clients[slot];
    if (cmd == "dumpuser") {
      out = "userinfo\n--------\n";
      std::vector<std::pair<std::string, std::string>> kv;
      ge::classic::protocol::info_pairs(c.userinfo, kv);
      for (auto& [key, value] : kv) {
        std::string k = key;
        k.resize(std::max<size_t>(k.size(), 20), ' ');
        out += k + value + "\n";
      }
      return true;
    }
    const std::string name = c.name;
    if (c.stage == SequelSlot::active)
      for (int i = 0; i < sequel_net_slots(s); i++)
        if (i != slot && sequel_held(s.clients[i]))
          sequel_net_print(s, i, name + " was kicked\n");
    sequel_net_print(s, slot, "You were kicked from the game\n");
    sequel_net_drop(s, slot, "kicked");
    out = name + " was kicked\n";
    return true;
  }
  if (gs::ban_command(s.bans, line, unix_now(s), out)) {
    if (cmd == "writeip") out = gs::ban_file_write(s.bans, s.cfg.ban_file, out);
    if (cmd == "addip") drop_refused(s);
    return true;
  }
  return false;
}

void sequel_rcon(SequelNetServer& s, const gn::Address& from, const std::string& text) {
  if (gs::ban_refuses(s.bans, s.cfg.ban_mode, from, unix_now(s))) {
    s.rcon_refused++;  // a banned address is not answered
    return;
  }
  const std::vector<std::string> w = sequel_words(text);
  if (s.cfg.rcon_password.empty() || w.size() < 2 || w[1] != s.cfg.rcon_password) {
    s.rcon_refused++;
    note(s, "bad rcon from " + gn::to_string(from));
    sequel_net_oob(s, from, "print\nBad rcon_password.\n");
    return;
  }
  // The line: the words after the password joined by single spaces.
  std::string line;
  for (size_t i = 2; i < w.size(); i++) line += (i > 2 ? " " : "") + w[i];
  s.rcon_runs++;
  note(s, "rcon from " + gn::to_string(from) + ": " + line);
  std::string answer;
  if (!line.empty() && !sequel_admin(s, line, answer)) {
    const sv::RulesCommon& game = **s.rules;
    const size_t before = game.console.size();
    if (!s.rules->console(line) && game.console.size() == before)
      answer = "Unknown command \"" + w[2] + "\"\n";
    for (size_t i = before; i < game.console.size(); i++) answer += game.console[i] + "\n";
  }
  sequel_net_oob(s, from, "print\n" + answer);
}

}  // namespace shooters_edge::net
