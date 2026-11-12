// THE VIEWER'S ARGUMENTS (docs/hub.md § Watching in a browser): what the hub's Watch and Demo
// pages hand a game's web viewer as `Module.arguments` (engine/hub/watch.hpp writes them), read
// into one struct. The same words on every platform, so the reading is tested natively.
//
//   --demo=<url>      a demo to play, fetched from the hub (`/d/ab/<sha256>.ger`)
//   --tv=ws://h:p/    a live broadcast's TV node, its WebSocket (with --stream)
//   --stream=<n>      the broadcast's stream on that node
//   --pack=<url>      a content pack the world is drawn from, fetched; repeated, in mount order
//   --speed=<x>       the demo's pace, 0..20 (1 by default)
//
// One of --demo and --tv is needed. A word the viewer does not know is refused with its name, so
// a hub that hands a newer viewer's words to an older one says which.
//
// ge:summary The web viewer's arguments from the hub's page: the demo, the broadcast, the packs
// ge:keywords web viewer arguments module-arguments demo tv stream pack speed parse_viewer_args
// ge:skill net
// ge:test tests/web_viewer_args_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::web {

inline constexpr double kMostSpeed = 20;

struct ViewerArgs {
  std::string demo;                // the demo's URL ("": none)
  std::string tv;                  // the TV node's WebSocket URL ("": none)
  uint32_t stream = 0;             // with `tv`
  std::vector<std::string> packs;  // the content packs' URLs, in mount order
  double speed = 1;
};

// `args` (argv without the program's name) read into `out`. False with the word at fault and what
// the viewer takes.
bool parse_viewer_args(std::span<const std::string_view> args, ViewerArgs& out, std::string& error);

}  // namespace ge::web
