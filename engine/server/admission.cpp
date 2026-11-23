// The connections one address may hold (admission.hpp states the rule).
//
// ge:summary sv_iplimit: seats counted per IPv4 host or IPv6 /48, the refusal's words
// ge:keywords sv_iplimit ip-limit admission admit_refusal same_admit_host
// ge:skill net
// ge:test tests/server_admission_test.cpp
#include "server/admission.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ge::server {

namespace {

// Never counted: the server's own machine, and IPv6 link-local (fe80::/10).
bool exempt(const net::Address& a) {
  if (a.family == net::Family::ipv4) return a.bytes[0] == 127;
  if (a.family == net::Family::ipv6) {
    const bool loop =
        std::all_of(a.bytes, a.bytes + 15, [](uint8_t b) { return b == 0; }) && a.bytes[15] == 1;
    return loop || (a.bytes[0] == 0xfe && (a.bytes[1] & 0xc0) == 0x80);
  }
  return a.family == net::Family::none;
}

}  // namespace

bool same_admit_host(const net::Address& a, const net::Address& b) {
  if (a.family != b.family || exempt(a) || exempt(b)) return false;
  const int bytes = a.family == net::Family::ipv6 ? 6 : 4;
  return std::equal(a.bytes, a.bytes + bytes, b.bytes);
}

bool lock_command(const std::string& line, bool& locked, std::string& out) {
  std::istringstream in(line);
  std::string cmd, value, extra;
  in >> cmd >> value >> extra;
  if (cmd != "sv_locked") return false;
  if (value.empty()) {
    out = std::string("\"sv_locked\" is \"") + (locked ? "1" : "0") + "\"\n";
  } else if ((value != "0" && value != "1") || !extra.empty()) {
    out = "Usage: sv_locked [0|1]\n";
  } else {
    locked = value == "1";
    out = locked ? "Server locked: no new clients.\n" : "Server unlocked.\n";
  }
  return true;
}

std::string admit_refusal(int limit, const net::Address& from, std::span<const HeldSeat> seats) {
  if (limit <= 0) return {};
  int halves = 0;  // seats held, counted in halves
  for (const HeldSeat& s : seats)
    if (same_admit_host(s.address, from)) halves += s.freeing ? 1 : 2;
  if (halves < 2 * limit) return {};
  return from.family == net::Family::ipv6 ? "Too many connections from your IPv6 network."
                                          : "Too many connections from your IP address.";
}

std::string ruleset_refusal(std::string_view required, std::string_view theirs, bool spectator) {
  const auto low = [](std::string_view t) {
    std::string out(t);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
  };
  const std::string want = low(required);
  if (spectator || want.empty()) return {};
  const std::string have = theirs.empty() ? std::string("default") : low(theirs);
  if (have == want) return {};
  return "This server requires the " + want + " ruleset. Pick it in OPTIONS or type ruleset " +
         want + ", then join again.";
}

}  // namespace ge::server
