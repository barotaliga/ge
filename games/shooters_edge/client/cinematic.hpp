// QUAKE II'S CINEMATICS IN THE WINDOW: the movie a unit's end plays (`video/eou1_.cin`), the
// intro, and the picture the game ends on (`pics/victory.pcx`), shown full screen while the
// window's own game waits behind them.
//
// THE CHAIN comes from the rules (server/q2_server.hpp's `cinematic`): one or more files joined by
// `+` ("eou1_.cin", "end.cin+victory.pcx"), shown in turn; the map the level change named loads
// once the last has played (the window holds it until then). A movie plays at 14 frames a second
// on the wall clock with its own sound, fitted to the screen with its shape kept on black; it
// waits while the menu or the console is up. Holding a button (fire, jump, use) more than a
// second after it started skips it. A movie the files lack is passed over at once, as is one
// that is damaged (the reason printed). A picture has no clock and no skip: the game is over, and
// it stays until the player picks something from the menu.
//
// ge:summary Quake II's cinematics in the window: a unit's movie and the game's last picture
// ge:keywords cinematic cin movie video eou victory pcx skip unit-end end-of-game full-screen
// ge:skill classic ui
// ge:test tests/shooters_edge_cinematic_test.cpp
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "classic/content/cin.hpp"
#include "shooters_edge/client/files.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

inline constexpr double kCinematicSkipAfter = 1.0;  // seconds before a held button skips

struct Cinematic {
  std::string left;     // the chain's files still to show, `+` joined
  std::string showing;  // the file shown now ("" none)
  std::vector<uint8_t> store;
  std::span<const uint8_t> bytes;
  ge::classic::content::Cin cin;
  ge::classic::content::CinFrame frame;
  ge::ui::Image picture;  // what is on the screen
  bool still = false;     // a picture: no clock, stays
  double started = 0;     // the wall clock the movie's frame 0 was at (moved on while it waits)
  int64_t shown = -1;     // the movie's frame on the screen
  std::unique_ptr<ge::audio::Pcm> left_sound, right_sound;
  ge::audio::SampleId left_id, right_id;
  std::vector<std::unique_ptr<ge::audio::Pcm>> retired;  // until no voice reads them
  std::vector<std::string> said;                         // lines for the console
};

// Whether anything is on the screen or still to show; whether it holds the next map back (a
// movie does, the game's last picture never: the menu's new game replaces it).
inline bool cinematic_on(const Cinematic& c) { return !c.showing.empty() || !c.left.empty(); }
inline bool cinematic_holds(const Cinematic& c) { return cinematic_on(c) && !c.still; }

// Start the chain `files` ("eou1_.cin", "end.cin+victory.pcx").
void cinematic_start(Cinematic& c, const std::string& files);

// One window frame at wall clock `t`: the next file opened, the movie moved on, a skip taken
// (`held`: a button is down), the sound started or stopped on `synth`; nothing moves while
// `paused`; a picture put away once `map_asked` (a new game). True while it shows something.
bool cinematic_frame(Cinematic& c, const SearchPath& files, ge::audio::Synth& synth, bool held,
                     bool paused, bool map_asked, double t);

// The picture over the whole screen: black, and the frame fitted with its shape kept.
void draw_cinematic(ge::ui::DrawList& list, const Cinematic& c, int width, int height);

}  // namespace shooters_edge::client
