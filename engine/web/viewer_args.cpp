// The web viewer's arguments, read (viewer_args.hpp).
//
// ge:summary The web viewer's arguments read: --demo, --tv, --stream, --pack, --speed
// ge:keywords web viewer arguments parse_viewer_args
// ge:skill net
// ge:test tests/web_viewer_args_test.cpp
#include "web/viewer_args.hpp"

#include <cstdlib>

namespace ge::web {

namespace {

constexpr const char* kTakes =
    "the viewer takes --demo=<url>, --tv=ws://<host>:<port>/ with --stream=<n>, --pack=<url> and "
    "--speed=<0..20>";

bool value_of(std::string_view arg, std::string_view key, std::string_view& value) {
  if (arg.substr(0, key.size()) != key) return false;
  value = arg.substr(key.size());
  return true;
}

}  // namespace

bool parse_viewer_args(std::span<const std::string_view> args, ViewerArgs& out,
                       std::string& error) {
  out = ViewerArgs{};
  bool have_stream = false;
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
      bool digits = !v.empty() && v.size() <= 9;
      for (const char c : v) {
        digits = digits && c >= '0' && c <= '9';
        n = n * 10 + static_cast<uint64_t>(c - '0');
      }
      if (!digits) {
        error = "--stream=" + std::string(v) + " is not a stream number; " + kTakes;
        return false;
      }
      out.stream = static_cast<uint32_t>(n);
      have_stream = true;
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
  if (out.demo.empty() == out.tv.empty()) {
    error = std::string(out.demo.empty() ? "neither --demo nor --tv was given"
                                         : "both --demo and --tv were given") +
            "; " + kTakes;
    return false;
  }
  if (!out.tv.empty() && !have_stream) {
    error = "--tv needs --stream=<n>, the broadcast's stream on the node; " + std::string(kTakes);
    return false;
  }
  return true;
}

}  // namespace ge::web
