// The web viewer's arguments, read (viewer_args.hpp).
//
// ge:summary The web viewer's arguments read: --demo, --tv, --stream, --play, --bots, --pack
// ge:summary and --speed
// ge:keywords web viewer arguments parse_viewer_args
// ge:skill net
// ge:test tests/web_viewer_args_test.cpp
#include "web/viewer_args.hpp"

#include <cstdlib>

namespace ge::web {

namespace {

constexpr const char* kTakes =
    "the viewer takes --demo=<url>, --tv=ws://<host>:<port>/ with --stream=<n>, --play=<map> with "
    "--bots=<0..15>, --pack=<url> and --speed=<0..20>";

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

}  // namespace

bool parse_viewer_args(std::span<const std::string_view> args, ViewerArgs& out,
                       std::string& error) {
  out = ViewerArgs{};
  bool have_stream = false, have_bots = false;
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
  const int modes =
      (out.demo.empty() ? 0 : 1) + (out.tv.empty() ? 0 : 1) + (out.play.empty() ? 0 : 1);
  if (modes != 1) {
    error = std::string(modes == 0 ? "none of --demo, --tv and --play was given"
                                   : "more than one of --demo, --tv and --play was given") +
            "; " + kTakes;
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
