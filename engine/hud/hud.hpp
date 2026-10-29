// THE HUD KIT: what any game draws over its running picture, in any genre, as plain functions
// over the 2D layer's draw list (ui/draw.hpp). A racer's speed and its edge bar, a shooter's
// health and ammunition, a wanted level's stars, a kill feed and a racing game's news column, a
// "GET UP" prompt: each is one of the few widgets below, placed by the frame's anchors and
// drawn in the game's `Style`. The kit holds no game's words or state; a game builds its HUD
// view from them (Fall Line's is games/fall_line/hud/view.hpp).
//
// THE FRAME scales everything to the screen: sizes in a `Style` are pixels at a 720 px short
// side (`Style::reference`), multiplied by `Frame::scale`, so a HUD reads the same on a desktop,
// a phone held upright and a phone on its side. The SAFE AREA is the screen less the platform's
// insets (a notch, rounded corners, a home bar) and the style's margin; nothing is placed out of
// it. `place` anchors a box to one of nine points of the safe area, and a `Stack` lays boxes one
// under another (or above, from a bottom anchor) with the style's gap.
//
// THE WIDGETS are measured, then drawn into the box their measure was placed at:
//   - a READOUT: a value over its caption (the seed, the score, 75 HEALTH);
//   - a BIG readout: a large number with its unit beside it (72 km/h, 24 / 90);
//   - a METER: a bar 0..1 from its left end, or -1..1 from its centre (an edge bar, a health
//     bar, a search running out), with optional ticks;
//   - PIPS: a row of charset symbols, some lit (wanted stars, hearts, rounds left);
//   - a BANNER: a title and a line under it on a soft plate, centred (STUCK, YOU DIED);
//   - the FEED (feed.hpp): lines that arrive, hold and fade, newest nearest the corner.
// Every widget draws its words with a drop shadow, so it reads over bright snow and dark woods
// alike without a plate behind it.
//
// THE CHECK: `Placed` names each box a view drew; `layout_problems` says which two overlap or
// which left the screen, the way the labs fail a HUD before a player sees it.
//
// Blinking is the caller's clock: `blink(t, hz)` is on for the first half of each period, with
// no transcendental, so a recorded run blinks on the same frames.
//
// ge:summary The HUD kit: the frame and anchors, readouts, big numbers, meters, pips, banners
// ge:keywords hud heads-up-display overlay frame scale safe-area insets anchor place stack
// ge:keywords readout value caption big-number unit meter bar gauge health ammo armour pips
// ge:keywords stars hearts banner prompt plate blink layout-problems placed overlap genre
// ge:skill ui
// ge:test tests/hud_hud_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hud/face.hpp"
#include "ui/draw.hpp"
#include "ui/paint.hpp"

namespace ge::hud {

// A game's HUD look: its faces, its sizes (px at the reference short side) and its colours.
struct Style {
  float reference = 720;  // px: the short side at which the sizes below are drawn as they are
  float min_scale = 0.66f, max_scale = 2.5f;
  float margin = 20;  // from the safe area's edge
  float gap = 10;     // between stacked boxes
  Face value_face = Face::bitmap(charset::FontId::rounded);
  Face caption_face = Face::bitmap(charset::FontId::rounded);
  Face big_face = Face::bitmap(charset::FontId::rounded);
  float value_size = 34;
  float caption_size = 17;
  float big_size = 84;
  float unit_size = 24;
  float banner_size = 40;
  float caption_tracking = 1;
  float shadow_drop = 2;
  ui::Color ink{1, 1, 1, 1};
  ui::Color caption = ui::hex(0xcfe6f7);
  ui::Color shadow = ui::hex(0x0d2233, 0.9f);
  ui::Color plate = ui::hex(0x0d2233, 0.62f);
  ui::Color track = ui::hex(0xffffff, 0.3f);
  ui::Color good = ui::hex(0x4fdc7c);  // a signal: ahead, gained, safe
  ui::Color bad = ui::hex(0xff5a4e);   // a signal: behind, lost, danger
  ui::Color accent = ui::hex(0xe8412c);
  ui::Color dim = ui::hex(0x0d2233, 0.55f);  // an unlit pip: a dark shape, lit ones white
};

// The platform's insets, px: what of the screen a notch, a corner or a home bar takes.
struct Insets {
  float left = 0, top = 0, right = 0, bottom = 0;
};

struct Frame {
  int width = 0, height = 0;
  float scale = 1;  // reference px to screen px
  ui::Rect safe;    // the screen less the insets and the margin
  bool portrait = false;
  float px(float ref) const { return ref * scale; }
};

Frame frame_for(int width, int height, const Style& style, Insets insets = {});

// The nine points a box is anchored to.
enum class Anchor : uint8_t {
  top_left,
  top,
  top_right,
  left,
  center,
  right,
  bottom_left,
  bottom,
  bottom_right
};

// A `w` x `h` box at `anchor` of the safe area, moved by (dx, dy) px (y down).
ui::Rect place(const Frame& frame, Anchor anchor, float w, float h, float dx = 0, float dy = 0);

// Boxes stacked from an anchor: down from a top or middle anchor, up from a bottom one, each
// aligned to the anchor's side, `gap` px apart.
class Stack {
 public:
  Stack(const Frame& frame, Anchor anchor, float gap, float dx = 0, float dy = 0);
  ui::Rect next(float w, float h);
  float used() const { return used_; }  // px of the stack's height taken so far

 private:
  const Frame* frame_;
  Anchor anchor_;
  float gap_, dx_, dy_;
  float used_ = 0;
};

// The text's horizontal alignment that matches an anchor's side.
ui::Align align_of(Anchor anchor);

struct Size {
  float w = 0, h = 0;
};

// A value over its caption.
struct Readout {
  std::string value;
  std::string caption;
  ui::Color color{1, 1, 1, 1};
};
Size measure(const Frame& f, const Style& st, const Readout& r);
void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Readout& r, ui::Rect at,
          ui::Align align);

// A big number, its unit beside it on its baseline, an optional caption under both.
struct Big {
  std::string value;
  std::string unit;
  std::string caption;
  ui::Color color{1, 1, 1, 1};
};
Size measure(const Frame& f, const Style& st, const Big& b);
void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Big& b, ui::Rect at,
          ui::Align align);

// A bar. `centred`: `value` -1..1 fills from the middle (an edge bar); else 0..1 from the left.
struct Meter {
  float value = 0;
  bool centred = false;
  int ticks = 0;  // marks dividing the track into this many parts (0: none)
  ui::Color fill{1, 1, 1, 1};
  float w = 200, h = 10;  // reference px
};
Size measure(const Frame& f, const Meter& m);
void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Meter& m, ui::Rect at);

// A row of symbols, the first `lit` drawn in `color` (in the dim colour while `blink_off`, so the
// lit ones flash), the rest in the style's dim colour.
struct Pips {
  int count = 5;
  int lit = 0;
  bool blink_off = false;
  uint8_t symbol = charset::sym(charset::Sym::star);
  charset::FontId font = charset::FontId::rounded;
  ui::Color color{1, 1, 1, 1};
  float size = 36;    // reference px: a symbol's cell height (its width is half)
  float spacing = 0;  // reference px between cells
};
Size measure(const Frame& f, const Pips& p);
void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Pips& p, ui::Rect at);

// A title and a line under it, centred on a soft plate.
struct Banner {
  std::string title;
  std::string line;
  ui::Color color{1, 1, 1, 1};
};
Size measure(const Frame& f, const Style& st, const Banner& b);
void draw(ui::DrawList& dl, const Frame& f, const Style& st, const Banner& b, ui::Rect at);

// Whether a blink of `hz` is on at `t` s: the first half of every period.
bool blink(double t, double hz);

// A box a view drew, by name, for the layout check.
struct Placed {
  std::string name;
  ui::Rect rect;
};

// What a screenshot would show wrong: a box out of the screen, or two boxes overlapping (by
// more than a pixel each way). Each problem is one line naming the boxes.
std::vector<std::string> layout_problems(const Frame& f, const std::vector<Placed>& placed);

}  // namespace ge::hud
