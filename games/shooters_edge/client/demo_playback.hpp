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
// | | `demo_lerp 0|1` | drawn between the demo's frames (1, demo_lerp.hpp) or each as read |
// | | `demo_bar 0|1|2` | the bar: hidden, shown a while after the pointer moves or a key (1),
// always |
// | | `cl_democlock 0|1|2` | a clock: none, the time since the demo began, the recording's own
// clock (`cl_democlock_x`, `cl_democlock_y` place it) |
// | | `cl_demowait 0|1` | at the end the demo ends, or holds paused on its last frame |
// | | `cl_startupdemo <demo>` | played once at start-up, in place of the title's recordings, when
// no map, demo or server was asked for |
// | | `cam_thirdperson 0|1`, `cam_dist <units>` | the player watched seen from `cam_dist` behind |
// | +forward, +back (W, S) | `cam_zoomaccel`, `cam_zoomspeed` | in third person, the camera in or
// out | | `cam_lockpos 0|1` | the third-person camera stays where it is, turned to follow the
// player | | | `demo_jump <time>`, `demo_jump_end` | to a time (or on or back), two seconds before
// the end | | | `demo_jump_skip_messages 0|1` | the messages a jump passes over left unshown (1) or
// printed | | | `mvd_autotrack 0|1`, `autotrack` | a multiview demo's view follows the hottest
// player | | | `cl_hightrack 0|1` | it follows the player with the most frags |
//
// AUTOTRACK (`mvd_autotrack 1`; `autotrack` turns it on or off): the view cuts to the hottest
// player by armour, weapons, powerups, frags and his kill streak (demo_autotrack.hpp), the plan
// worked out over the whole demo the first time it is asked for. `cl_hightrack 1` follows the top
// fragger instead: the view moves only to someone with strictly more frags than the player it
// rides. Picking a player with `demo_track` turns both off; neither works on a demo of one
// player's view.
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
// ge:keywords add_demo_commands timedemo playdemo demo_lerp cl_democlock demo-clock cl_demowait
// ge:keywords cl_startupdemo startup-demo cam_thirdperson cam_dist demo_jump_end mvd_autotrack
// ge:keywords autotrack cl_hightrack top-fragger demo_jump_skip_messages cam_lockpos
// ge:keywords cam_zoomaccel cam_zoomspeed third-person-zoom mvd_info demo_playlist_track_name
// ge:keywords demo_benchmarkdumps
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
#include "shooter/controls.hpp"
#include "shooter/demo_transport.hpp"
#include "shooters_edge/client/demo_autotrack.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/client/place_names.hpp"
#include "shooters_edge/client/record_cvars.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/video_cfg.hpp"
#include "ui/play_bar.hpp"

namespace shooters_edge::client {

using ge::shooter::kDemoSpeeds;  // the speeds Up and Down step along
using ge::shooter::parse_clock;
inline constexpr double kBarShowS = 3;  // the bar stays this long after the pointer or a key, s

// The shooter's transport (shooter/demo_transport.hpp: speed, pause, track, skips, the asks) with
// this game's demo beside it.
struct DemoPlayback : ge::shooter::DemoTransport {
  RecordSettings record;  // demo_pack, demo_pack_level (record_cvars.hpp)
  VideoSettings video;    // demovideo_ (video_cfg.hpp), read by -demovideo alone
  double rate = 1;        // times `speed`: the live map's fly (0 while it dives, then catching up)
  // The demo playing:
  DemoScan scan;
  ge::replay::Highlights highlights;  // the tracked player's
  std::vector<ge::ui::PlayBarMarker> markers;
  int marked_for = -2;  // the slot `highlights` was finished for
  // The pointer and the bar:
  ge::ui::PlayBarPointer pointer;
  float pointer_x = -1, pointer_y = -1;  // px
  double shown_until = 0;                // the app's clock, s
  float bar_bottom_px = 0;               // the bar sits this far up, over the HUD (`hud_inset`)
  bool list_ask = false;
  // The demo clock (`cl_democlock`): 0 none, 1 the time since the demo began, 2 the recording's
  // own clock; at `cl_democlock_x` cells from the left (from the right when negative) and
  // `cl_democlock_y` from the top (up from the HUD's top when negative), a cell a line's height.
  int clock = 0, clock_x = 0, clock_y = -2;
  int hold_end = 0;      // `cl_demowait`: at the end, paused on the last frame rather than ended
  std::string startup;   // `cl_startupdemo`: played once at start-up when nothing else is asked
  int third_person = 0;  // `cam_thirdperson`: the watched player seen from behind
  float behind = 100;    // `cam_dist`: how far behind, units
  int lock_pos = 0;      // `cam_lockpos`: the third-person camera stays put, turned to the player
  float zoom_accel = 2000, zoom_top = 300;  // `cam_zoomaccel`, `cam_zoomspeed`: units/s², units/s
  float zoom_speed = 0;                     // the zoom's speed now, units/s
  ge::console::Cvar* dist_cvar = nullptr;   // `cam_dist`, written when a zoom ends
  ge::console::Cvars* cvars = nullptr;
  // The table of every player (mvd_info.hpp): `mvd_info`, `mvd_info_setup`, its header and place.
  int info = 0, info_header = 0;
  std::string info_setup;
  float info_x = 0, info_y = 0;
  std::string track_name;  // `demo_playlist_track_name`: picked at a multiview demo's start
  bool track_name_done = false;
  int bench_dumps = 1;  // `demo_benchmarkdumps`: each timedemo's result kept in timedemo.log
  const PlaceNames* places = nullptr;             // the table's `%l` (null: someplace)
  const ge::console::Console* console = nullptr;  // the table's words
  int autotrack = 0;    // `mvd_autotrack`: the view follows the hottest player
  int hightrack = 0;    // `cl_hightrack`: the view follows the top fragger
  AutotrackPlan plan;   // autotrack's, worked out the first time it is on
  int plan_state = 0;   // 0 not tried, 1 made, -1 refused (the reason said once)
  bool jumped = false;  // a jump was made this step: the window clears the notify lines
};

// The slot `cl_hightrack` rides in `s`: the playing player (no spectator) with the most frags, kept
// while no one has strictly more than the one ridden (`riding`); -1 with no one playing.
int high_track_slot(const DemoState& s, int riding);

// A demo clock's words: whole seconds as hh:mm:ss, the hours past 99 starting again at 0.
std::string demo_clock_text(double s);

// The third person's distance now (`cam_thirdperson` on: `cam_dist`, else 0), `watching` true for a
// demo, TV or a server joined as a spectator (playing, the view is always your own).
float third_person_units(const DemoPlayback& p, bool watching);

// One frame of the third person's zoom: `dir` 1 in (+forward), -1 out (+back), 0 none; the speed
// grows at `cam_zoomaccel` to `cam_zoomspeed`, `cam_dist` never below 0, its cvar written when the
// zoom ends. Nothing while `cam_thirdperson` is off.
void zoom_third_person(DemoPlayback& p, int dir, double dt_s);
// The zoom's way off the move keys: +forward in (1), +back out (-1), both or neither 0.
inline int zoom_of(const ge::shooter::Moves& m) { return (m.forward ? 1 : 0) - (m.back ? 1 : 0); }

// A frame's keys and pointer, as the window has them: the keys by scancode, the pointer in px.
struct PlaybackInput {
  std::span<const bool> pressed;  // went down this frame, by scancode (empty: none)
  float x = -1, y = -1;           // px
  bool button = false;            // the left button held
  int zoom = 0;                   // +forward held (1) or +back (-1): cam_dist in or out
  bool tapped(int scancode) const {
    return scancode >= 0 && static_cast<size_t>(scancode) < pressed.size() &&
           pressed[static_cast<size_t>(scancode)];
  }
};

// The console's demo commands and cvars (the table above), bound to `p`; `demo_track` also sets
// `bench`'s tracked player; the recording's `demo_pack` cvars bind to `p.record`.
void add_playback_commands(ge::console::Console& console, DemoPlayback& p, Timedemo& bench);
// `timedemo <demo>` (into `ask`) and `playdemo <demo>` (into `ask`, with `paced` set); the window
// starts what they ask for at its next frame. Both must outlive `console`.
void add_demo_commands(ge::console::Console& console, std::string& ask, bool& paced);

// The demo `t` just started: scanned for its highlights. False with the reason (the demo still
// plays, with no markers).
bool playback_begin(DemoPlayback& p, const Timedemo& t, std::string& error);

// One frame of playback: the keys and the pointer read (on a `w` x `h` px target), a seek, skip or
// jump asked carried out, then the demo moved on by `wall_dt_s` at the speed (none while paused).
// `now_s` is the app's clock. Lines for the console in `say`.
TimedemoStep playback_step(DemoPlayback& p, Timedemo& t, const PlaybackInput& in, int w, int h,
                           double wall_dt_s, double now_s, std::vector<std::string>& say,
                           std::string& error);

// The play bar into `overlay` when it shows, and the demo clock when it is on.
void playback_draw(ge::ui::DrawList& overlay, const DemoPlayback& p, const Timedemo& t, int w,
                   int h, double now_s);

// How far up the bar sits to clear the HUD: the height from the screen's bottom to the top of
// the HUD's boxes in its lower third (0 when there are none), px.
float hud_inset(std::span<const ge::hud::Placed> placed, int height);

// The highlights as console lines: time, who, what (everyone's when `all`).
std::vector<std::string> highlight_lines(const DemoPlayback& p, bool all);

}  // namespace shooters_edge::client
