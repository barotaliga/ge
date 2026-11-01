// A DEMO HEARD AND READ IN THE WINDOW (`playdemo`): what the demo frames read since the last draw
// carried (timedemo.hpp's `DemoHeard`) played and shown. Its sounds and the players' voices play
// on the window's synth through demo_sound.hpp and demo_voice.hpp, the same players `-demovideo`
// renders offline; its printed lines (the deaths, the players' chat and team chat, the pickups)
// become console and feed lines, and its centre message the HUD's centre.
//
// THE FILES: the sounds are read through the search path the demo's level loaded from (the mod's
// own folder when the demo names one: timedemo.hpp's `demo_folder`), so a new folder makes the
// sounds again.
//
// A PRINTED LINE as the protocol sends it may hold several lines, a chat line's leading marker and
// the charset's coloured letters (the high bit set: a name or a word in the second colour).
// `print_lines` splits it into lines a font without those letters can show: the high bit dropped,
// the charset's bracket and digit glyphs (16..27) as their plain letters, other control codes and
// empty lines left out, and the colour marks players' clients write into chat (`&c` and three hex
// digits, `&r`, braces round a word) taken out, leaving the words.
//
// ge:summary The window hears and reads a played demo: its sounds, voices, prints, centre message
// ge:keywords demo-heard demo playdemo sound voice print chat team-chat obituary death centerprint
// ge:keywords print_lines feed console demo-hearing
// ge:skill shooter audio
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "audio/synth.hpp"
#include "brush/map.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_sound.hpp"
#include "shooters_edge/client/demo_voice.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/timedemo.hpp"

namespace shooters_edge::client {

// The lines a printed text shows (see above).
std::vector<std::string> print_lines(const std::string& text);

class DemoHearing {
 public:
  explicit DemoHearing(ge::audio::Synth& synth);

  // A level of the demo starts: its sounds read through `files` (made again when the folder
  // changed), the level's loops and the ambience over `map`.
  void level(const DemoState& s, const SearchPath& files, const ge::brush::Map* map);
  // One drawn frame: what `h` gathered played, its lines appended to `lines` and its centre
  // message (if any, its lines on one line) in `centre`; `h` emptied. `dt` moves the ambience, s.
  // True when a centre message came (the caller shows it for a while).
  bool frame(const DemoState& s, DemoHeard& h, float dt, std::vector<std::string>& lines,
             std::string& centre);
  // The demo over: everything it plays stopped.
  void stop();

 private:
  ge::audio::Synth& synth_;
  const SearchPath* files_ = nullptr;
  std::unique_ptr<DemoSounds> sounds_;
  std::unique_ptr<DemoVoices> voices_;
};

}  // namespace shooters_edge::client
