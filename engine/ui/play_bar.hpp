// THE PLAY BAR: a video player's seek bar over anything played back on a timeline (a demo, a
// replay), the instrument everyone already knows. A track along the bottom of the screen with the
// played part filled, the playhead, a marker for each highlight in its colour (the window of the
// one under the pointer shaded on the track), and under it the play or pause button, the clock
// ("1:23 / 10:05"), the speed and a title. The pointer over a marker shows its label and time above
// it; over the track, the time under the pointer.
//
// It is a model and a view, no state of its own between frames but the pointer's drag
// (`PlayBarPointer`), and no clock: the app hands it the length, the playhead, the markers and the
// pointer each frame, draws it into its overlay and acts on what `play_bar_pointer` answers (a
// press on the track starts a drag that shows where it will land; the release seeks there; a
// press on the button toggles play). So a test and a lab drive it with numbers and draw it with the
// CPU raster.
//
// SIZES scale with the target's height (720 px is 1), so the bar reads the same in a lab's small
// picture and on a 4K display.
//
// ge:summary The play bar: a seek bar with highlight markers, the playhead, clock, speed, drag
// ge:keywords ui play-bar seek-bar scrubber timeline progress playhead markers highlights tooltip
// ge:keywords clock play pause speed drag seek video-player draw_play_bar play_bar_layout
// ge:keywords play_bar_time play_bar_marker_at play_bar_pointer play_clock demo replay
// ge:skill ui replay
// ge:test tests/ui_play_bar_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "ui/draw.hpp"

namespace ge::ui {

struct PlayBarMarker {
  double at_s = 0;              // the moment
  double from_s = 0, to_s = 0;  // its window, shaded on the track (from == to: none)
  Color color{1, 1, 1, 1};
  std::string label;  // shown while the pointer is over it
};

struct PlayBarView {
  double length_s = 0;    // the timeline's length
  double position_s = 0;  // the playhead
  bool playing = true;
  double speed = 1;
  std::span<const PlayBarMarker> markers;
  float pointer_x = -1, pointer_y = -1;  // the pointer, px (negative: none)
  double drag_s = -1;  // a drag's landing, drawn as the playhead until the release (-1: none)
  std::string title;   // at the right of the row under the track
};

struct PlayBarLayout {
  Rect panel;   // the shaded band the bar sits on
  Rect track;   // the track itself
  Rect hit;     // where a press grabs the track (taller than it)
  Rect button;  // play / pause
  float scale = 1;
  float text_y = 0;  // the row under the track's baseline
};

// Where the bar sits on a `width` x `height` target: at the bottom, or `bottom_px` above it (over
// a HUD that keeps the bottom of the screen).
PlayBarLayout play_bar_layout(int width, int height, float bottom_px = 0);

// The time under `x` px on the track, held inside 0..length.
double play_bar_time(const PlayBarLayout& l, float x, double length_s);

// The marker under the pointer (within a few px of it on the track's row), nearest first; -1 none.
int play_bar_marker_at(const PlayBarLayout& l, std::span<const PlayBarMarker> markers,
                       double length_s, float x, float y);

// "m:ss" (and "h:mm:ss" from an hour), whole seconds rounded down (a thousandth short of one is
// it: a timeline summed from frames lands a hair under a whole second).
std::string play_clock(double s);

// The bar into `list` (after the frame's other overlay, so it is on top).
void draw_play_bar(DrawList& list, const PlayBarLayout& l, const PlayBarView& v);

// THE POINTER: the left button's press on the track starts a drag, its moves keep `at_s` under
// the pointer, its release seeks; a press on the button toggles play.
struct PlayBarPointer {
  bool dragging = false;
  double at_s = 0;  // the drag's landing
  bool was_down = false;
};
enum class PlayBarAct : uint8_t {
  none,    // nothing for the app
  drag,    // a drag is under way: draw `at_s` as the playhead, seek on release
  seek,    // the drag ended: seek to `at_s`
  toggle,  // the button was pressed: play or pause
};
// This frame's pointer (`x`, `y` px, the left button `down`) against the bar.
PlayBarAct play_bar_pointer(PlayBarPointer& p, const PlayBarLayout& l, double length_s, float x,
                            float y, bool down);

}  // namespace ge::ui
