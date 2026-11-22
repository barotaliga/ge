// The definitions behind addr_list.hpp.
//
// ge:summary A classic server's address list: parse, match by mask, the words' answers
// ge:keywords address-list mask hits last-hit addgtvhost addgtvban
// ge:skill classic net
// ge:test tests/classic_addr_list_test.cpp
#include "classic/protocol/addr_list.hpp"

#include <cstdlib>
#include <cstring>
#include <ctime>

namespace ge::classic {

namespace {

// The address as an entry's family and bytes (a simulated node as its IPv4 form).
bool key_of(const net::Address& a, net::Family& family, uint8_t bytes[16]) {
  std::memset(bytes, 0, 16);
  if (a.family == net::Family::ipv4) {
    family = net::Family::ipv4;
    std::memcpy(bytes, a.bytes, 4);
  } else if (a.family == net::Family::sim) {
    family = net::Family::ipv4;
    net::sim_to_ipv4(a, bytes);
  } else if (a.family == net::Family::ipv6) {
    family = net::Family::ipv6;
    std::memcpy(bytes, a.bytes, 16);
  } else {
    return false;
  }
  return true;
}

bool matches(const AddrEntry& e, net::Family family, const uint8_t bytes[16]) {
  if (e.family != family) return false;
  for (int bit = 0; bit < e.bits; bit++) {
    const int k = bit / 8, m = 0x80 >> (bit % 8);
    if ((e.bytes[k] & m) != (bytes[k] & m)) return false;
  }
  return true;
}

std::string first_word(std::string_view args, std::string_view& rest) {
  size_t i = 0;
  while (i < args.size() && args[i] == ' ') i++;
  size_t j = i;
  while (j < args.size() && args[j] != ' ') j++;
  size_t k = j;
  while (k < args.size() && args[k] == ' ') k++;
  rest = args.substr(k);
  return std::string(args.substr(i, j - i));
}

// "<address>[/<bits>]" into `e`; false with the refusal in `out`.
bool parse_entry(const std::string& text, AddrEntry& e, std::string& out) {
  std::string addr = text;
  int bits = -1;
  if (const size_t slash = text.find('/'); slash != std::string::npos) {
    addr = text.substr(0, slash);
    const std::string m = text.substr(slash + 1);
    if (m.empty()) {
      out = "Please specify a mask after '/'.\n";
      return false;
    }
    char* end = nullptr;
    const long v = std::strtol(m.c_str(), &end, 10);
    bits = *end == '\0' ? static_cast<int>(v) : -2;
    if (bits == -2 || v < 0 || v > 128) {
      out = "Bad mask: " + m + " bits\n";
      return false;
    }
  }
  net::Address a;
  if (!net::parse_address(addr + ":0", a) && !net::parse_address("[" + addr + "]:0", a)) {
    out = "Bad address: " + addr + "\n";
    return false;
  }
  if (!key_of(a, e.family, e.bytes)) {
    out = "Bad address: " + addr + "\n";
    return false;
  }
  const int most = e.family == net::Family::ipv4 ? 32 : 128;
  if (bits > most) {
    out = "Bad mask: " + std::to_string(bits) + " bits\n";
    return false;
  }
  e.bits = bits < 0 ? most : bits;
  // The bits past the mask cleared, so an entry reads as its network.
  for (int bit = e.bits; bit < 128; bit++)
    e.bytes[bit / 8] &= static_cast<uint8_t>(~(0x80 >> (bit % 8)));
  return true;
}

std::string address_text(const AddrEntry& e) {
  net::Address a;
  a.family = e.family;
  std::memcpy(a.bytes, e.bytes, 16);
  std::string t = net::to_string(a);
  // The port the address form carries (":0") dropped, and IPv6's brackets.
  if (const size_t colon = t.rfind(':'); colon != std::string::npos) t.resize(colon);
  if (!t.empty() && t.front() == '[') t = t.substr(1, t.size() - 2);
  return t + "/" + std::to_string(e.bits);
}

std::string padded(std::string s, size_t width) {
  if (s.size() < width) s.append(width - s.size(), ' ');
  return s;
}

}  // namespace

const AddrEntry* addr_list_hit(AddrList& list, const net::Address& a, int64_t now) {
  net::Family family{};
  uint8_t bytes[16];
  if (!key_of(a, family, bytes)) return nullptr;
  for (AddrEntry& e : list.entries)
    if (matches(e, family, bytes)) {
      e.hits++;
      e.last_hit = now;
      return &e;
    }
  return nullptr;
}

void addr_list_add(AddrList& list, std::string_view cmd, std::string_view args, std::string& out) {
  out.clear();
  std::string_view rest;
  const std::string word = first_word(args, rest);
  if (word.empty()) {
    out = "Usage: " + std::string(cmd) + " <address[/mask]> [comment]\n";
    return;
  }
  AddrEntry e;
  if (!parse_entry(word, e, out)) return;
  for (const AddrEntry& was : list.entries)
    if (was.family == e.family && was.bits == e.bits && std::memcmp(was.bytes, e.bytes, 16) == 0) {
      out = "Entry " + address_text(e) + " already exists.\n";
      return;
    }
  e.comment = std::string(rest);
  list.entries.push_back(e);
}

void addr_list_del(AddrList& list, std::string_view cmd, std::string_view args, std::string& out) {
  out.clear();
  std::string_view rest;
  const std::string word = first_word(args, rest);
  if (word.empty()) {
    out = "Usage: " + std::string(cmd) + " <address[/mask]|id|all>\n";
    return;
  }
  if (list.entries.empty()) {
    out = "Address list is empty.\n";
    return;
  }
  if (word == "all") {
    list.entries.clear();
    return;
  }
  if (word.find_first_not_of("0123456789") == std::string::npos) {
    const long id = std::strtol(word.c_str(), nullptr, 10);
    if (id < 1 || static_cast<size_t>(id) > list.entries.size()) {
      out = "No such index: " + word + "\n";
      return;
    }
    list.entries.erase(list.entries.begin() + (id - 1));
    return;
  }
  AddrEntry e;
  if (!parse_entry(word, e, out)) return;
  for (size_t i = 0; i < list.entries.size(); i++) {
    const AddrEntry& was = list.entries[i];
    if (was.family == e.family && was.bits == e.bits && std::memcmp(was.bytes, e.bytes, 16) == 0) {
      list.entries.erase(list.entries.begin() + static_cast<std::ptrdiff_t>(i));
      return;
    }
  }
  out = "No such entry: " + word + "\n";
}

std::string addr_list_text(const AddrList& list) {
  if (list.entries.empty()) return "Address list is empty.\n";
  std::string t = "id address/mask       hits last hit     comment\n";
  t += "-- ------------------ ---- ------------ -------\n";
  int id = 1;
  for (const AddrEntry& e : list.entries) {
    std::string when = "never";
    if (e.last_hit != 0) {
      const auto tt = static_cast<std::time_t>(e.last_hit);
      std::tm tm{};
#if defined(_WIN32)
      localtime_s(&tm, &tt);
#else
      localtime_r(&tt, &tm);
#endif
      char buf[32];
      std::strftime(buf, sizeof buf, "%d %b %H:%M", &tm);
      when = buf;
    }
    t += padded(std::to_string(id++), 2) + " " + padded(address_text(e), 18) + " " +
         padded(std::to_string(e.hits), 4) + " " + padded(when, 12) + " " + e.comment + "\n";
  }
  return t;
}

}  // namespace ge::classic
