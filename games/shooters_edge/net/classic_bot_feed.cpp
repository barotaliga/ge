// A bot team's match as a feed, and the control file back (classic_bot_feed.hpp).
//
// ge:summary A bot team's live match as a JSON feed, and a control file whose lines reach the bots
// ge:keywords bot_feed_line bot_feed_status control_poll json-lines
// ge:skill bots net
// ge:test tests/shooters_edge_bot_team_test.cpp
#include "shooters_edge/net/classic_bot_feed.hpp"

#include <cstdio>
#include <fstream>

#include "core/num_text.hpp"
#include "shooter/locs.hpp"
#include "shooters_edge/bots/knowledge.hpp"
#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

namespace bt = shooters_edge::bots;
using ge::core::num::to_fixed;

// A JSON string: quoted, every control byte escaped, a coloured character as its plain one.
std::string str(const std::string& s) {
  std::string out = "\"";
  for (const char ch : s) {
    auto c = static_cast<unsigned char>(ch);
    if (c >= 0x80) c = static_cast<unsigned char>(c & 0x7f);
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\u%04x", c);
      out += buf;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out + "\"";
}

std::string plain_name(const std::string& s) {
  std::string out;
  for (const char ch : s) out += static_cast<char>(static_cast<unsigned char>(ch) & 0x7f);
  return out;
}

const char* weapon_name(int32_t active) {
  switch (active) {
    case 1:
      return "sg";
    case 2:
      return "ssg";
    case 4:
      return "ng";
    case 8:
      return "sng";
    case 16:
      return "gl";
    case 32:
      return "rl";
    case 64:
      return "lg";
    case 4096:
      return "axe";
    default:
      return "-";
  }
}

// The client every bot shares the server's lists through: the first.
const ClassicClientEnd* first_client(const BotTeam& t) {
  return t.seats.empty() ? nullptr : &t.seats[0]->c;
}

bool is_name(const BotTeam& t, const std::string& name) {
  const ClassicClientEnd* c = first_client(t);
  if (!c || name.empty()) return false;
  for (const std::string& u : c->userinfos)
    if (!u.empty() && plain_name(info_get(u, "name")) == name) return true;
  return false;
}

}  // namespace

std::string bot_feed_line(const BotTeam& t, double t_s, const std::string& line) {
  const std::string head = "{\"t\":" + to_fixed(t_s, 2) + ",\"type\":";
  // `(name): words` is a team's line, `name: words` a player's to all.
  if (line.size() > 4 && line[0] == '(') {
    const size_t close = line.find("): ");
    if (close != std::string::npos && is_name(t, line.substr(1, close - 1)))
      return head + "\"team_chat\",\"from\":" + str(line.substr(1, close - 1)) +
             ",\"text\":" + str(line.substr(close + 3)) + "}";
  }
  const size_t colon = line.find(": ");
  if (colon != std::string::npos && is_name(t, line.substr(0, colon)))
    return head + "\"chat\",\"from\":" + str(line.substr(0, colon)) +
           ",\"text\":" + str(line.substr(colon + 2)) + "}";
  return head + "\"log\",\"text\":" + str(line) + "}";
}

std::string bot_feed_status(const BotTeam& t, double t_s) {
  std::string j = "{\"t\":" + to_fixed(t_s, 2) + ",\"type\":\"status\",\"map\":" + str(t.map) +
                  ",\"team\":" + str(t.cfg.team);
  const ClassicClientEnd* c = first_client(t);
  int ours = 0, theirs = 0;
  std::string players;
  if (c)
    for (int i = 0; i < 32; i++) {
      const std::string& u = c->userinfos[i];
      if (u.empty() || !info_get(u, "*spectator").empty()) continue;
      const std::string team = info_get(u, "team");
      (team == t.cfg.team ? ours : theirs) += c->frags[i];
      bool bot = false;
      for (const auto& s : t.seats) bot = bot || s->c.playernum == i;
      if (!players.empty()) players += ",";
      players += "{\"slot\":" + std::to_string(i) + ",\"name\":" + str(info_get(u, "name")) +
                 ",\"team\":" + str(team) + ",\"frags\":" + std::to_string(c->frags[i]) +
                 ",\"ours\":" + (bot ? "true" : "false") + "}";
    }
  j += ",\"score\":{\"us\":" + std::to_string(ours) + ",\"them\":" + std::to_string(theirs) +
       "},\"players\":[" + players + "],\"bots\":[";
  for (size_t k = 0; k < t.seats.size(); k++) {
    const BotSeat& s = *t.seats[k];
    const ClassicClientEnd& sc = s.c;
    if (k > 0) j += ",";
    j += "{\"bot\":" + std::to_string(k + 1) + ",\"name\":" + str(s.name) +
         ",\"skill\":" + str(sh::aim_skill_name(s.skill)) +
         ",\"ping\":" + to_fixed(sc.latency_avg * 1000, 0);
    if (s.set_up && s.fighter && s.mirror && sc.playernum >= 0) {
      const bt::Fighter& f = *s.fighter;
      const sh::Vec3f at = s.mirror->s.sv.edicts[static_cast<size_t>(sc.playernum) + 1].origin;
      std::string goal = "-";
      if (f.goal_item >= 0 && static_cast<size_t>(f.goal_item) < f.items.size())
        goal = bt::item_call(f.items[static_cast<size_t>(f.goal_item)]);
      j += ",\"h\":" + std::to_string(sc.stats[0]) + ",\"a\":" + std::to_string(sc.stats[4]) +
           ",\"weapon\":" + str(weapon_name(sc.stats[10])) +
           ",\"rockets\":" + std::to_string(sc.stats[8]) +
           ",\"cells\":" + std::to_string(sc.stats[9]) +
           ",\"mode\":" + str(bt::fight_mode_name(f.mode)) +
           ",\"role\":" + str(bt::role_name(f.role)) + ",\"goal\":" + str(goal) +
           ",\"at\":" + str(f.locs ? sh::loc_name_at(*f.locs, at) : std::string("?")) +
           ",\"xyz\":[" + to_fixed(at.x, 0) + "," + to_fixed(at.y, 0) + "," + to_fixed(at.z, 0) +
           "],\"frags\":" + std::to_string(sc.frags[sc.playernum]) +
           ",\"deaths\":" + std::to_string(s.deaths);
    } else {
      j += ",\"stage\":" + str(client_stage_name(sc.stage));
    }
    j += "}";
  }
  return j + "]}";
}

std::vector<std::string> control_poll(ControlFile& c) {
  std::vector<std::string> lines;
  std::ifstream in(c.path, std::ios::binary);
  if (!in) return lines;
  in.seekg(0, std::ios::end);
  const auto size = static_cast<uint64_t>(in.tellg());
  if (size < c.offset) c.offset = 0;  // the file was started again
  if (size == c.offset) return lines;
  in.seekg(static_cast<std::streamoff>(c.offset));
  std::string got(static_cast<size_t>(size - c.offset), '\0');
  in.read(got.data(), static_cast<std::streamsize>(got.size()));
  c.offset = size;
  c.pending += got;
  size_t nl;
  while ((nl = c.pending.find('\n')) != std::string::npos) {
    std::string line = c.pending.substr(0, nl);
    c.pending.erase(0, nl + 1);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) lines.push_back(line);
  }
  return lines;
}

}  // namespace shooters_edge::net
