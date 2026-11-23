// WHAT A PLAYER DOES WITH A DEMO, in any shooter: the transport's state (speed, pause, the
// tracked player, the skips, the play bar, drawing between frames) and the console's words that set
// it: `cl_demospeed`, `demo_track`, `demo_pause`, `demo_seek`, `demo_jump` (a time, or `+`/`-` and
// a time on or back), `demo_jump_end` (two seconds before the end, paused at an intermission met on
// the way), `demo_skip`, `demo_next`, `demo_prev`, `demo_highlights` and the cvars
// `demo_skip_forward`, `demo_skip_back`, `demo_bar`, `demo_lerp`, `demo_jump_skip_messages` (the
// lines a jump reads past left unshown, 1, or shown). Quake II spells `demo_jump` as `seek`, and
// there its time is `S`, `S.F`, `M:S` or `M:S.F` with F a count of tenths (`1.25` is 3.5 s), or a
// share of the demo with `%` (`seek 50%`, `seek +10%`). A game's demo format, its scan for
// highlights and how it seeks stay the game's: it carries out what the words ask (`seek_ask`,
// `skip_ask`, `jump_ask`) and answers the two calls.
//
// ge:summary Any shooter's demo transport: speed, pause, seek, skips, highlights, the console words
// ge:keywords demo playback transport cl_demospeed demo_track demo_pause demo_seek demo_skip
// ge:keywords demo_next demo_prev demo_highlights demo_skip_forward demo_skip_back demo_bar
// ge:keywords demo_lerp parse_clock add_demo_transport kdemospeeds demo_jump seek demo_jump_end
// ge:keywords demo_jump_skip_messages parse_tenths_clock seek-percent
// ge:skill shooter replay console
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

#include "console/console.hpp"

namespace ge::shooter {

// The speeds a player steps along (faster, slower).
inline constexpr std::array<double, 6> kDemoSpeeds{0.25, 0.5, 1, 2, 4, 8};

struct DemoTransport {
  // Settings (cvars and commands):
  double speed = 1;            // cl_demospeed
  double skip_forward_s = 15;  // demo_skip_forward
  double skip_back_s = 5;      // demo_skip_back
  int bar = 1;                 // demo_bar
  int track = -1;              // demo_track
  int lerp = 1;                // demo_lerp: drawn between frames
  int skip_messages = 1;       // demo_jump_skip_messages: what a jump reads past is not printed
  bool paused = false;
  // What a key, the bar or the console asked of the next step:
  double seek_ask = -1;       // a timeline time
  double skip_ask = 0;        // s, either way
  int jump_ask = 0;           // highlights on (+1) or back (-1)
  bool end_ask = false;       // demo_jump_end
  double share_ask = -1;      // a share of the demo, 0..1 (Quake II's `seek 50%`)
  double share_skip_ask = 0;  // a share of the demo on or back (`seek +10%`)
};

// What the words ask of the game: `tracked` hears a new `demo_track`, `highlights` lists the
// demo's highlights (everyone's with `all`), `playing` says whether a demo plays (Quake II's
// `seek` without one answers "Not playing a demo."). Any may be empty.
struct DemoCalls {
  std::function<void(int track)> tracked;
  std::function<std::vector<std::string>(bool all)> highlights;
  std::function<bool()> playing;
};

// The words and cvars over `t` (both must outlive `console`).
void add_demo_transport(console::Console& console, DemoTransport& t, const DemoCalls& calls);

// "83", "1:23" or "1:02:03" as seconds; -1 when it is not a time.
double parse_clock(const std::string& text);

// Quake II's demo time: `S`, `S.F`, `M:S` or `M:S.F`, digits only, F a whole count of tenths of a
// second (`1.25` is 3.5 s, `1:05.3` is 65.3 s); -1 when it is not one.
double parse_tenths_clock(const std::string& text);

}  // namespace ge::shooter
