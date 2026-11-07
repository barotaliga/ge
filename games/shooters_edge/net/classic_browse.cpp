// The classic status query and the master's list, read for the engine's server list
// (classic_browse.hpp).
//
// ge:summary The classic status reply and master reply parsed into the server list's words
// ge:keywords classic status-query status-reply serverinfo player-line master-reply sources
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_browse_test.cpp
#include "shooters_edge/net/classic_browse.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

namespace gn = ge::net;

const uint8_t kHeader[4] = {0xff, 0xff, 0xff, 0xff};

size_t put(uint8_t* out, size_t cap, std::string_view bytes) {
  const size_t n = bytes.size() < cap ? bytes.size() : cap;
  std::memcpy(out, bytes.data(), n);
  return n;
}

const std::string* find(const std::vector<std::pair<std::string, std::string>>& keys,
                        std::string_view key) {
  for (const auto& [k, v] : keys)
    if (k == key) return &v;
  return nullptr;
}

int number(const std::vector<std::pair<std::string, std::string>>& keys, std::string_view key,
           int otherwise) {
  const std::string* v = find(keys, key);
  return v == nullptr || v->empty() ? otherwise : std::atoi(v->c_str());
}

// A player line's words: bare, or in double quotes (which may hold spaces).
std::vector<std::string> words(std::string_view line) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && (line[at] == ' ' || line[at] == '\t' || line[at] == '\r')) at++;
    if (at >= line.size()) break;
    if (line[at] == '"') {
      const size_t end = line.find('"', at + 1);
      const size_t stop = end == std::string_view::npos ? line.size() : end;
      out.emplace_back(line.substr(at + 1, stop - at - 1));
      at = stop + 1;
    } else {
      size_t end = at;
      while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '\r') end++;
      out.emplace_back(line.substr(at, end - at));
      at = end;
    }
  }
  return out;
}

bool read_player(std::string_view line, gn::ServerPlayer& p) {
  const std::vector<std::string> w = words(line);
  if (w.size() < 5) return false;
  p.frags = std::atoi(w[1].c_str());
  const int ping = std::atoi(w[3].c_str());
  p.name = w[4];
  p.spectator = w[1] == "S" || p.frags == -9999 || ping < 0;
  if (p.name.rfind("\\s\\", 0) == 0) {
    p.spectator = true;
    p.name.erase(0, 3);
  }
  if (p.spectator) p.frags = 0;
  p.ping_ms = ping < 0 ? -ping : ping;
  if (w.size() >= 9) p.team = w[8];
  return true;
}

}  // namespace

size_t classic_status_query(uint8_t* out, size_t cap) {
  return put(out, cap, std::string_view("\xff\xff\xff\xffstatus 23\n", 14));
}

size_t classic_master_query(uint8_t* out, size_t cap) {
  return put(out, cap, std::string_view("c\n\0", 3));
}

std::string classic_mode(const std::vector<std::pair<std::string, std::string>>& keys) {
  if (const std::string* m = find(keys, "mode"); m != nullptr && !m->empty()) return *m;
  if (number(keys, "coop", 0) != 0) return "coop";
  if (number(keys, "deathmatch", 0) != 0) return number(keys, "teamplay", 0) != 0 ? "team" : "ffa";
  return "single";
}

bool classic_read_status(const uint8_t* data, size_t size, gn::ServerInfo& out) {
  if (size < 6 || std::memcmp(data, kHeader, 4) != 0 || data[4] != 'n') return false;
  const std::string_view text(reinterpret_cast<const char*>(data) + 5,
                              strnlen(reinterpret_cast<const char*>(data) + 5, size - 5));
  const size_t eol = text.find('\n');
  const std::string info(text.substr(0, eol));
  if (info.empty() || info[0] != '\\') return false;
  out = gn::ServerInfo{};
  if (!info_pairs(info, out.keys)) return false;
  const auto& k = out.keys;
  if (const std::string* v = find(k, "hostname")) out.name = *v;
  if (const std::string* v = find(k, "map")) out.map = *v;
  if (const std::string* v = find(k, "*gamedir")) out.game = *v;
  if (const std::string* v = find(k, "status")) out.state = *v;
  out.mode = classic_mode(k);
  out.max_players = number(k, "maxclients", 0);
  out.max_spectators = number(k, "maxspectators", 0);
  out.skill = number(k, "skill", -1);
  out.locked = (number(k, "needpass", 0) & 1) != 0;
  size_t at = eol == std::string_view::npos ? text.size() : eol + 1;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    gn::ServerPlayer p;
    if (read_player(text.substr(at, end - at), p)) {
      (p.spectator ? out.spectators : out.players)++;
      out.roster.push_back(std::move(p));
    }
    at = end + 1;
  }
  return true;
}

bool classic_read_master(const uint8_t* data, size_t size, std::vector<gn::Address>& out) {
  if (size < 6 || std::memcmp(data, kHeader, 4) != 0 || data[4] != 'd' || data[5] != '\n')
    return false;
  for (size_t at = 6; at + 6 <= size; at += 6) {
    const uint16_t port = static_cast<uint16_t>(data[at + 4] << 8 | data[at + 5]);
    if (port == 0) continue;
    out.push_back(gn::ipv4_or_sim(data + at, port));
  }
  return true;
}

gn::ServerQuery classic_query() {
  gn::ServerQuery q;
  q.status = classic_status_query;
  q.read_status = classic_read_status;
  q.master = classic_master_query;
  q.read_master = classic_read_master;
  return q;
}

std::vector<std::string> classic_masters_in(const std::string& text) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    const std::vector<std::string> w = words(std::string_view(text).substr(at, end - at));
    if (w.size() >= 3 && w[0] == "master") out.push_back(w[2]);
    at = end + 1;
  }
  return out;
}

std::vector<std::string> classic_install_masters(const std::string& basedir) {
  namespace fs = std::filesystem;
  std::vector<std::string> out;
  std::error_code ec;
  if (basedir.empty() || !fs::is_directory(basedir, ec)) return out;
  std::vector<fs::path> lists;
  for (const fs::directory_entry& d : fs::directory_iterator(basedir, ec)) {
    const fs::path p = d.path() / "sb" / "sources.txt";
    if (d.is_directory(ec) && fs::is_regular_file(p, ec)) lists.push_back(p);
  }
  std::sort(lists.begin(), lists.end());  // the same order on every OS
  for (const fs::path& p : lists) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    for (std::string& m : classic_masters_in(text.str()))
      if (std::find(out.begin(), out.end(), m) == out.end()) out.push_back(std::move(m));
  }
  return out;
}

}  // namespace shooters_edge::net
