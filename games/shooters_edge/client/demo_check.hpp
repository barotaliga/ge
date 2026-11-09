// A DEMO FILE CHECKED BEFORE IT PLAYS: what the main menu's DEMOS page (menu/demos_page.hpp) asks
// of a file the player picked anywhere on the machine, so a file that only looks like a demo by
// its name is refused with the reason instead of starting a playback that breaks.
//
// `check_demo_file` reads the file (refusing one past `vfs::kUnpackLimit`), opens it with the
// game's own reader (the adapter's `open_demo`: unpacked first, so `.mvd.gz`, `.dem.zip` and
// `.mvd.7z` open as the recording inside, the archive's notes passed over by `demo_name`), and
// reads frames until a level's map is named, at most `kCheckFrames`: a file that ends or breaks
// before that is no demo the window can draw; `kRosterFrames` more are read for the players'
// names (a break there is the playback's to report, not the check's). What it found (`DemoFacts`)
// is the page's line for it: the format, the map, the players, what was peeled.
//
// ge:summary A picked demo file checked before it plays: read, unpacked, its frames read to a map
// ge:keywords demo-check check_demo_file validate valid-demo demo-facts demos-page browse
// ge:keywords packed archive mvd dem
// ge:skill shooter vfs
// ge:test tests/shooters_edge_demo_check_test.cpp
#pragma once

#include <string>
#include <vector>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client {

// The most frames read looking for the level's map (a recording names it in its first few).
inline constexpr int kCheckFrames = 4096;
// The frames read on past it for the players' names (they follow the level's start).
inline constexpr int kRosterFrames = 128;

struct DemoFacts {
  DemoFormat format = DemoFormat::single;
  bool voice = false;
  std::string map;
  int players = 0;                        // names known by the map's frame (client, multiview)
  std::vector<ge::vfs::Wrapping> layers;  // peeled, outermost first
  std::string inner_name;                 // the recording's name in an archive
  bool truncated = false;
};

// `path` checked as a demo of `game` (above). False with the reason and the next step.
bool check_demo_file(const std::string& path, const GameAdapter& game, DemoFacts& out,
                     std::string& error);
// The same on bytes in hand (a test's).
bool check_demo_bytes(std::vector<uint8_t> bytes, const GameAdapter& game, DemoFacts& out,
                      std::string& error);

// The facts as one line: "MULTIVIEW  ·  E1M2  ·  8 PLAYERS  ·  GZIP + TAR".
std::string facts_line(const DemoFacts& f);

}  // namespace shooters_edge::client
