// A DEMO WATCHED IN THE WINDOW LIKE A VIDEO: the controls round `playdemo` (timedemo.hpp's paced
// playback) and the play bar over it (engine/ui/play_bar.hpp) with a marker for each of the
// tracked player's highlights (demo_highlights.hpp), found by scanning the whole demo once when it
// starts (demo_scan.hpp).
//
// THE CONTROLS, on the keys, the pointer and the console:
//
// | Key | Console | What |
// | --- | --- | --- |
// | Space, K | `demo_pause` | pause or play |
// | Right, L | `demo_skip 15` | forward `demo_skip_forward` s (15) |
// | Left, J | `demo_skip -5` | back `demo_skip_back` s (5) |
// | Up / Down | `cl_demospeed <x>` | faster / slower along `kDemoSpeeds` (0.25 .. 8); 0..20 by hand
// | | Page Down / Page Up, N / P | `demo_next`, `demo_prev` | the next / last highlight's window |
// | Home, End | `demo_seek <s>` | the start / a second before the end; any time by hand ("1:23") |
// | the bar | | a press on the track drags the playhead, the release seeks; the button plays or
// pauses | | | `demo_highlights` | list the highlights (everyone's, the tracked player's marked) |
// | | `demo_bar 0|1|2` | the bar: hidden, shown a while after the pointer moves or a key (1),
// always |
//
// While a demo plays the pointer is free (no raw mouse), so the bar can be pointed at. A seek
// stops where the timeline reaches its time (timedemo.hpp's `demo_seek`); the step after it loads
// the level when the seek crossed into another.
//
// ge:summary A demo watched like a video: seek bar with highlight markers, pause, speed, skips,
// ge:summary next and previous highlight, the console's demo commands
// ge:keywords demo playback playdemo play-bar seek scrub pause resume speed cl_demospeed skip
// ge:keywords demo_skip demo_skip_forward demo_skip_back demo_next demo_prev demo_seek
// ge:keywords demo_pause demo_highlights demo_bar demo_track highlights markers video-player
// ge:skill shooter replay ui
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#pragma once

#include <array>
#include <span>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "hud/hud.hpp"
#include "replay/highlights.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/client/record_cvars.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "ui/play_bar.hpp"

namespace shooters_edge::client {

// The speeds Up and Down step along.
inline constexpr std::array<double, 6> kDemoSpeeds{0.25, 0.5, 1, 2, 4, 8};
inline constexpr double kBarShowS = 3;  // the bar stays this long after the pointer or a key, s

struct DemoPlayback {
  // Settings (cvars and commands):
  double speed = 1;            // cl_demospeed
  double skip_forward_s = 15;  // demo_skip_forward
  double skip_back_s = 5;      // demo_skip_back
  int bar = 1;                 // demo_bar
  int track = -1;              // demo_track
  RecordSettings record;       // demo_pack, demo_pack_level (record_cvars.hpp)
  // The demo playing:
  bool paused = false;
  DemoScan scan;
  ge::replay::Highlights highlights;  // the tracked player's
  std::vector<ge::ui::PlayBarMarker> markers;
  int marked_for = -2;  // the slot `highlights` was finished for
  // The pointer and the bar:
  ge::ui::PlayBarPointer pointer;
  float pointer_x = -1, pointer_y = -1;  // px
  double shown_until = 0;                // the app's clock, s
  float bar_bottom_px = 0;               // the bar sits this far up, over the HUD (`hud_inset`)
  // What a key, the bar or the console asked of the next step:
  double seek_ask = -1;  // a timeline time
  double skip_ask = 0;   // s, either way
  int jump_ask = 0;      // highlights on (+1) or back (-1)
  bool list_ask = false;
};

// A frame's keys and pointer, as the window has them: the keys by scancode, the pointer in px.
struct PlaybackInput {
  std::span<const bool> pressed;  // went down this frame, by scancode (empty: none)
  float x = -1, y = -1;           // px
  bool button = false;            // the left button held
  bool tapped(int scancode) const {
    return scancode >= 0 && static_cast<size_t>(scancode) < pressed.size() &&
           pressed[static_cast<size_t>(scancode)];
  }
};

// The console's demo commands and cvars (the table above), bound to `p`; `demo_track` also sets
// `bench`'s tracked player; the recording's `demo_pack` cvars bind to `p.record`.
void add_playback_commands(ge::console::Console& console, DemoPlayback& p, Timedemo& bench);

// The demo `t` just started: scanned for its highlights. False with the reason (the demo still
// plays, with no markers).
bool playback_begin(DemoPlayback& p, const Timedemo& t, std::string& error);

// One frame of playback: the keys and the pointer read (on a `w` x `h` px target), a seek, skip or
// jump asked carried out, then the demo moved on by `wall_dt_s` at the speed (none while paused).
// `now_s` is the app's clock. Lines for the console in `say`.
TimedemoStep playback_step(DemoPlayback& p, Timedemo& t, const PlaybackInput& in, int w, int h,
                           double wall_dt_s, double now_s, std::vector<std::string>& say,
                           std::string& error);

// The play bar into `overlay` when it shows.
void playback_draw(ge::ui::DrawList& overlay, const DemoPlayback& p, const Timedemo& t, int w,
                   int h, double now_s);

// How far up the bar sits to clear the HUD: the height from the screen's bottom to the top of
// the HUD's boxes in its lower third (0 when there are none), px.
float hud_inset(std::span<const ge::hud::Placed> placed, int height);

// A time as the console takes it: seconds ("83.5"), "m:ss" or "h:mm:ss"; -1 when it is none.
double parse_clock(const std::string& text);

// The highlights as console lines: time, who, what (everyone's when `all`).
std::vector<std::string> highlight_lines(const DemoPlayback& p, bool all);

}  // namespace shooters_edge::client
