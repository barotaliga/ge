// A picked demo file checked (demo_check.hpp).
//
// ge:summary The demo check's read, open and frame walk, and the facts line
// ge:keywords check_demo_file check_demo_bytes facts_line demo-check
// ge:skill shooter
// ge:test tests/shooters_edge_demo_check_test.cpp
#include "shooters_edge/client/demo_check.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace shooters_edge::client {

namespace {

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
  return s;
}

}  // namespace

bool check_demo_bytes(std::vector<uint8_t> bytes, const GameAdapter& game, DemoFacts& out,
                      std::string& error) {
  out = DemoFacts{};
  Demo d;
  if (!game.open_demo(game, std::move(bytes), d, error)) {
    error = "not a demo: " + error;
    return false;
  }
  out.layers = d.layers;
  out.inner_name = d.inner_name;
  out.truncated = d.truncated;
  for (int i = 0; i < kCheckFrames && d.state.map.empty(); i++) {
    std::string why;
    const DemoRead r = next_frame(d, why);
    if (r == DemoRead::error) {
      error = "the demo breaks before its level starts: " + why;
      return false;
    }
    if (r == DemoRead::end) break;
  }
  if (d.state.map.empty()) {
    error = "the demo names no level in its first " + std::to_string(kCheckFrames) +
            " frames, so there is nothing to show. Pick another file.";
    return false;
  }
  // The players come in the frames just after the level's start: a second's worth read on.
  for (int i = 0; i < kRosterFrames; i++) {
    std::string why;
    if (next_frame(d, why) != DemoRead::frame) break;
  }
  out.format = d.state.format;
  out.voice = d.state.voice_format;
  out.map = d.state.map;
  for (const std::string& n : d.state.names) out.players += n.empty() ? 0 : 1;
  return true;
}

bool check_demo_file(const std::string& path, const GameAdapter& game, DemoFacts& out,
                     std::string& error) {
  std::error_code ec;
  const uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    error = "Can't read " + path + ": " + ec.message();
    return false;
  }
  if (size > ge::vfs::kUnpackLimit) {
    error = "the file is " + std::to_string(size >> 20) + " MiB, past the " +
            std::to_string(ge::vfs::kUnpackLimit >> 20) + " MiB a demo may be";
    return false;
  }
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = "Can't open " + path + ". Check that the game may read it.";
    return false;
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  if (static_cast<uintmax_t>(f.gcount()) != size) {
    error = path + " ended early; the file may be damaged. Try copying it again.";
    return false;
  }
  return check_demo_bytes(std::move(bytes), game, out, error);
}

std::string facts_line(const DemoFacts& f) {
  static constexpr std::string_view kSep = "  \xc2\xb7  ";
  std::string s = f.format == DemoFormat::single      ? "SINGLE PLAYER"
                  : f.format == DemoFormat::multiview ? "MULTIVIEW"
                                                      : "CLIENT";
  if (f.voice) s += " WITH VOICE";
  s += std::string(kSep) + upper(f.map);
  if (f.players > 0)
    s += std::string(kSep) + std::to_string(f.players) + (f.players == 1 ? " PLAYER" : " PLAYERS");
  if (!f.layers.empty()) {
    std::string packed;
    for (const ge::vfs::Wrapping w : f.layers) {  // outermost first: "GZIP + TAR"
      if (!packed.empty()) packed += " + ";
      packed += upper(std::string(ge::vfs::wrapping_name(w)));
    }
    s += std::string(kSep) + packed;
  }
  if (f.truncated) s += std::string(kSep) + "CUT SHORT";
  return s;
}

}  // namespace shooters_edge::client
