// The web viewer's arguments, read (viewer_args.hpp).
//
// ge:summary The web viewer's arguments read: --demo, --tv, --stream, --play, --bots, --join,
// ge:summary --cert, --pack and --speed
// ge:keywords web viewer arguments parse_viewer_args
// ge:skill net
// ge:test tests/web_viewer_args_test.cpp
#include "web/viewer_args.hpp"

#include <cstdlib>

namespace ge::web {

namespace {

constexpr const char* kTakes =
    "the viewer takes --demo=<url>, --tv=ws://<host>:<port>/ with --stream=<n>, --play=<map> with "
    "--bots=<0..15>, --join=https://<host>:<port>/play?to=<server> with --cert=<64 hex digits> "
    "and or --join=ws://<host>:<port>/play?to=<server>, --pack=<url> and --speed=<0..20>";

bool value_of(std::string_view arg, std::string_view key, std::string_view& value) {
  if (arg.substr(0, key.size()) != key) return false;
  value = arg.substr(key.size());
  return true;
}

// `v` as a whole number of at most nine digits.
bool number_of(std::string_view v, uint64_t& n) {
  n = 0;
  bool digits = !v.empty() && v.size() <= 9;
  for (const char c : v) {
    digits = digits && c >= '0' && c <= '9';
    n = n * 10 + static_cast<uint64_t>(c - '0');
  }
  return digits;
}

// A map's name as the game's folder holds it: letters, digits, `_` and `-`, at most 32.
bool map_name(std::string_view v) {
  if (v.empty() || v.size() > 32) return false;
  for (const char c : v)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-'))
      return false;
  return true;
}

bool hex_hash(std::string_view v) {
  if (v.size() != 64) return false;
  for (const char c : v)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  return true;
}

// The server a join URL names: its `to=` query value, up to the next `&`.
std::string_view join_target(std::string_view url) {
  const size_t q = url.find('?');
  if (q == std::string_view::npos) return {};
  std::string_view rest = url.substr(q + 1);
  while (!rest.empty()) {
    const size_t amp = rest.find('&');
    const std::string_view part = rest.substr(0, amp);
    if (part.substr(0, 3) == "to=") return part.substr(3);
    if (amp == std::string_view::npos) break;
    rest = rest.substr(amp + 1);
  }
  return {};
}

}  // namespace

bool parse_viewer_args(std::span<const std::string_view> args, ViewerArgs& out,
                       std::string& error) {
  out = ViewerArgs{};
  bool have_stream = false, have_bots = false, have_cert = false;
  for (const std::string_view a : args) {
    std::string_view v;
    if (value_of(a, "--demo=", v)) {
      out.demo = std::string(v);
    } else if (value_of(a, "--tv=", v)) {
      if (v.substr(0, 5) != "ws://" && v.substr(0, 6) != "wss://") {
        error = "--tv=" + std::string(v) + " is not a WebSocket address; " + kTakes;
        return false;
      }
      out.tv = std::string(v);
    } else if (value_of(a, "--stream=", v)) {
      uint64_t n = 0;
      if (!number_of(v, n)) {
        error = "--stream=" + std::string(v) + " is not a stream number; " + kTakes;
        return false;
      }
      out.stream = static_cast<uint32_t>(n);
      have_stream = true;
    } else if (value_of(a, "--play=", v)) {
      if (!map_name(v)) {
        error = "--play=" + std::string(v) + " is not a map's name; " + kTakes;
        return false;
      }
      out.play = std::string(v);
    } else if (value_of(a, "--bots=", v)) {
      uint64_t n = 0;
      if (!number_of(v, n) || n > kMostBots) {
        error = "--bots=" + std::string(v) + " is not a count from 0 to 15; " + kTakes;
        return false;
      }
      out.bots = static_cast<int>(n);
      have_bots = true;
    } else if (value_of(a, "--join=", v)) {
      const bool web_transport = v.substr(0, 8) == "https://";
      const bool web_socket = v.substr(0, 5) == "ws://" || v.substr(0, 6) == "wss://";
      if (!(web_transport || web_socket) || join_target(v).empty() ||
          (!out.join_to.empty() && out.join_to != join_target(v)) ||
          !(web_transport ? out.join : out.join_ws).empty()) {
        error = "--join=" + std::string(v) +
                " is not an endpoint's address with the server it plays on (one WebTransport and "
                "one WebSocket at most, to the same server); " +
                kTakes;
        return false;
      }
      (web_transport ? out.join : out.join_ws) = std::string(v);
      out.join_to = std::string(join_target(v));
    } else if (value_of(a, "--cert=", v)) {
      if (!hex_hash(v)) {
        error = "--cert=" + std::string(v) + " is not a certificate's hash; " + kTakes;
        return false;
      }
      out.cert = std::string(v);
      have_cert = true;
    } else if (value_of(a, "--pack=", v)) {
      if (v.empty()) {
        error = std::string("--pack= names no pack; ") + kTakes;
        return false;
      }
      out.packs.emplace_back(v);
    } else if (value_of(a, "--speed=", v)) {
      const std::string s(v);
      char* end = nullptr;
      const double x = std::strtod(s.c_str(), &end);
      if (s.empty() || end != s.c_str() + s.size() || !(x >= 0 && x <= kMostSpeed)) {
        error = "--speed=" + s + " is not a pace from 0 to 20; " + kTakes;
        return false;
      }
      out.speed = x;
    } else {
      error = "the viewer does not know " + std::string(a) + "; " + kTakes;
      return false;
    }
  }
  const int modes = (out.demo.empty() ? 0 : 1) + (out.tv.empty() ? 0 : 1) +
                    (out.play.empty() ? 0 : 1) + (out.join_to.empty() ? 0 : 1);
  if (modes != 1) {
    error = std::string(modes == 0 ? "none of --demo, --tv, --play and --join was given"
                                   : "more than one of --demo, --tv, --play and --join was given") +
            "; " + kTakes;
    return false;
  }
  if (have_cert && out.join.empty()) {
    error = "--cert goes with --join=https://<url>, a server played on from the page; " +
            std::string(kTakes);
    return false;
  }
  if (have_bots && out.play.empty()) {
    error = "--bots goes with --play=<map>, a game played on the page; " + std::string(kTakes);
    return false;
  }
  if (!out.tv.empty() && !have_stream) {
    error = "--tv needs --stream=<n>, the broadcast's stream on the node; " + std::string(kTakes);
    return false;
  }
  return true;
}

}  // namespace ge::web
