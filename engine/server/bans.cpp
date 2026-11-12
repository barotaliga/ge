// A server's ban list by address (bans.hpp states the rules).
//
// ge:summary Ban patterns parsed and matched, the mode, addip removeip listip writeip
// ge:keywords ban addip removeip listip writeip ban_match
// ge:skill net
// ge:test tests/server_bans_test.cpp
#include "server/bans.hpp"

#include <algorithm>
#include <charconv>

namespace ge::server {

namespace {

bool parse_int(std::string_view t, int64_t& v) {
  if (t.empty()) return false;
  const auto r = std::from_chars(t.data(), t.data() + t.size(), v);
  return r.ec == std::errc{} && r.ptr == t.data() + t.size();
}

std::vector<std::string_view> words(std::string_view line) {
  std::vector<std::string_view> out;
  size_t at = 0;
  while (at < line.size()) {
    while (at < line.size() && (line[at] == ' ' || line[at] == '\t')) at++;
    const size_t end = std::min(line.find_first_of(" \t", at), line.size());
    if (end > at) out.push_back(line.substr(at, end - at));
    at = end;
  }
  return out;
}

uint32_t ipv4_of(const net::Address& a) {
  uint8_t b[4] = {};
  if (a.family == net::Family::sim)
    net::sim_to_ipv4(a, b);
  else if (a.family == net::Family::ipv4)
    std::copy(a.bytes, a.bytes + 4, b);
  else
    return 0;
  return static_cast<uint32_t>(b[0]) << 24 | static_cast<uint32_t>(b[1]) << 16 |
         static_cast<uint32_t>(b[2]) << 8 | b[3];
}

void drop_expired(BanList& list, int64_t now) {
  std::erase_if(list.entries, [now](const BanEntry& e) { return e.until != 0 && e.until <= now; });
}

std::string entry_line(const BanEntry& e) {
  std::string s = e.text;
  if (e.safe) s += " safe";
  if (e.until != 0) s += " " + std::to_string(e.until);
  return s;
}

}  // namespace

bool parse_ban_pattern(std::string_view text, BanEntry& out) {
  BanEntry e;
  e.text = std::string(text);
  int bits = -1;
  if (const size_t slash = text.find('/'); slash != std::string_view::npos) {
    int64_t b = 0;
    if (!parse_int(text.substr(slash + 1), b) || b < 0 || b > 32) return false;
    bits = static_cast<int>(b);
    text = text.substr(0, slash);
  }
  if (text.empty()) return false;
  if (text.back() == '.') text.remove_suffix(1);  // "10.0." is "10.0"
  int part = 0;
  for (size_t at = 0;; part++) {
    const size_t end = std::min(text.find('.', at), text.size());
    if (part == 4) return false;
    const std::string_view p = text.substr(at, end - at);
    if (p != "*") {
      int64_t v = 0;
      if (!parse_int(p, v) || v < 0 || v > 255) return false;
      e.ip |= static_cast<uint32_t>(v) << (24 - 8 * part);
      e.mask |= 0xFFu << (24 - 8 * part);
    }
    if (end == text.size()) break;
    at = end + 1;
  }
  if (bits >= 0) {
    const uint32_t m = bits == 0 ? 0 : ~uint32_t{0} << (32 - bits);
    e.mask &= m;
    e.ip &= m;
  }
  out = e;
  return true;
}

const BanEntry* ban_match(const BanList& list, const net::Address& a, int64_t now) {
  const uint32_t ip = ipv4_of(a);
  for (const BanEntry& e : list.entries)
    if ((e.until == 0 || e.until > now) && (ip & e.mask) == e.ip) return &e;
  return nullptr;
}

bool ban_refuses(const BanList& list, int ban_mode, const net::Address& a, int64_t now) {
  if (ban_mode == 0) return ban_match(list, a, now) == nullptr;
  const uint32_t ip = ipv4_of(a);
  bool banned = false;
  for (const BanEntry& e : list.entries) {
    if ((e.until != 0 && e.until <= now) || (ip & e.mask) != e.ip) continue;
    if (e.safe) return false;  // a safe entry keeps its address in whatever else matches
    banned = true;
  }
  return banned;
}

bool ban_command(BanList& list, std::string_view line, int64_t now, std::string& out) {
  const std::vector<std::string_view> w = words(line);
  if (w.empty()) return false;
  const std::string_view cmd = w[0];
  if (cmd != "addip" && cmd != "removeip" && cmd != "listip" && cmd != "writeip") return false;
  drop_expired(list, now);
  if (cmd == "listip" || cmd == "writeip") {
    const bool cfg = cmd == "writeip";
    out.clear();
    for (const BanEntry& e : list.entries) out += (cfg ? "addip " : "") + entry_line(e) + "\n";
    if (!cfg) out += std::to_string(list.entries.size()) + " addresses listed\n";
    return true;
  }
  if (w.size() < 2) {
    out = cmd == "addip" ? "usage: addip <address> [ban|safe] [+seconds|time]\n"
                         : "usage: removeip <address>\n";
    return true;
  }
  if (cmd == "removeip") {
    const auto it = std::find_if(list.entries.begin(), list.entries.end(),
                                 [&](const BanEntry& e) { return e.text == w[1]; });
    if (it == list.entries.end()) {
      out = "removeip: " + std::string(w[1]) + " is not on the list; listip shows it as written\n";
      return true;
    }
    list.entries.erase(it);
    out = "removed " + std::string(w[1]) + "\n";
    return true;
  }
  BanEntry e;
  if (!parse_ban_pattern(w[1], e)) {
    out = "addip: " + std::string(w[1]) +
          " is not an address pattern; write up to four numbers 0..255 with dots, * for any\n";
    return true;
  }
  for (size_t i = 2; i < w.size(); i++) {
    int64_t t = 0;
    if (w[i] == "safe") {
      e.safe = true;
    } else if (w[i] == "ban") {
      e.safe = false;
    } else if (w[i].front() == '+' && parse_int(w[i].substr(1), t) && t > 0) {
      e.until = now + t;
    } else if (parse_int(w[i], t) && t > 0) {
      e.until = t;
    } else {
      out = "addip: " + std::string(w[i]) +
            " is neither ban, safe, +seconds nor a time in seconds since 1970\n";
      return true;
    }
  }
  std::erase_if(list.entries, [&](const BanEntry& x) { return x.text == e.text; });
  list.entries.push_back(e);
  out = "added " + entry_line(e) + "\n";
  return true;
}

}  // namespace ge::server
