// THE HUD AS DATA: a game declares its HUD once as a table of ELEMENTS (its speed, its health
// bar, its wanted stars, its news column), writes each element's CONTENT once a frame, and the
// layout places and draws them with the kit's widgets (hud.hpp). Where an element sits and how
// big it is drawn are the element's PLACE, and every place is a set of cvars (`add_hud_cvars`),
// so a player moves, resizes, hides and fades any element from a cfg or the console without the
// game's code knowing (docs/hud.md).
//
// AN ELEMENT is a widget `Kind`, a default place (one of the nine anchors, an offset as a share
// of the safe area, a scale and an opacity) and the kind's shape (a bar's length, the pips' size,
// the feed's column). Elements that share an anchor STACK in table order (hud.hpp's `Stack`):
// hiding one closes the gap, and an element's own offset moves it alone. Two rules keep the
// centre clear of the corners, each an element's option:
//   - `under_corners`: an element on the top or bottom row's middle anchor that would meet a box
//     of its row's corner stacks drops past both (the taller one's height and a gap), so a narrow
//     screen's air clock goes under the corners' columns instead of over them;
//   - `under`: an element kept at least a gap below another element's box while that one shows.
//
// THE CONTENT is what the game writes once a frame, by the element's index: whether it shows
// now, its words (value, caption, unit), its level (a bar's 0..1, or -1..1 centred), its pips
// (count, lit, blinking off), its colour, and for a feed the feed and the clock. The layout
// never reads the game: a snapshot in, pixels out, so a lab, a test or a replay draws the HUD
// with no game running.
//
// THE CVARS, per element `<id>` (no archive flag until the player's own `seta` gives one, so
// `cfg_save` keeps only what the player set):
//   hud_<id>_show     0 hides it
//   hud_<id>_anchor   top_left, top, top_right, left, center, right, bottom_left, bottom,
//                     bottom_right (a word it does not know keeps the game's)
//   hud_<id>_x/_y     its offset, a share of the safe area's width and height (y down)
//   hud_<id>_scale    its size, times the frame's scale
//   hud_<id>_alpha    its opacity, 0..1
//   hud_<id>_parent   an element's id to hang from ("" none), declared before it
//   hud_<id>_attach   the point of its own box set on the parent's anchor point
// and for the whole HUD `hud_scale`, `hud_alpha` and `hud_margin` (reference px from the screen's
// safe edge). `hud_reset [<id>]` puts one element (or all of them) back to the game's place.
//
// ge:summary The HUD as data: elements, their places as hud_ cvars, per-frame content, drawing
// ge:keywords hud layout element elements content place anchor offset scale alpha show cvars
// ge:keywords hud_scale hud_alpha hud_margin hud_reset under-corners stack customizable
// ge:keywords player-layout add_hud_cvars parent attach child picture digits strip ring unit
// ge:keywords snap fit pixel-art status-bar
// ge:skill ui console
// ge:test tests/hud_layout_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "hud/feed.hpp"
#include "hud/hud.hpp"
#include "hud/instruments.hpp"
#include "hud/pictures.hpp"

namespace ge::hud {

// The widget that draws an element.
enum class Kind : uint8_t {
  readout,  // a value over its caption
  big,      // a big number, its unit, a caption under them
  bar,      // a meter; with a caption, the caption under it
  pips,     // a row of symbols, some lit
  banner,   // a title and a line on a plate
  feed,     // lines that arrive, hold and fade
  picture,  // a picture (pictures.hpp): a panel, an icon, a portrait, a crosshair
  digits,   // a number drawn from a digit sheet, right-aligned in its cells
  strip,    // a row of picture cells: weapons, keys, powerups, counts, frag plates
  ring,     // a meter bent round a centre
  dial,     // a needle gauge, its reading in digits in the face (instruments.hpp)
  wind,     // the felt wind, heading-up, and the weather's mark on the rim
  compass,  // a strip of headings, the current one centred
};

// An anchor's cvar word, and back (nullopt for a word that is none of the nine).
std::string_view anchor_name(Anchor a);
std::optional<Anchor> anchor_from(std::string_view word);

// What the game declares once: an element's id (its cvars' middle word: lower case, digits and
// `_`), kind, default place and shape.
struct ElementSpec {
  std::string_view id;
  Kind kind = Kind::readout;
  Anchor anchor = Anchor::top_left;
  double x = 0, y = 0;  // the offset, a share of the safe area's width and height
  double scale = 1, alpha = 1;
  bool show = true;
  std::string_view help = {};  // what it shows, for its cvars' help
  Meter bar = {};              // kind bar: its length, height, ticks, centred (the fill is content)
  Pips pips = {};              // kind pips: the symbol, its font, size and spacing
  float feed_size = 21;        // kind feed: the type, reference px
  float feed_w = 330;          // kind feed: the column's width at most, reference px
  float feed_share = 1;        // kind feed: and at most this share of the safe area's width
  Dial dial = {};              // kind dial: its range, knee, red band, ticks, size, settling
  WindGauge wind = {};         // kind wind: its size, the arrow's full length, gale and calm
  Compass compass = {};        // kind compass: its size, span and tick step
  bool under_corners = false;
  std::string_view under = {};  // an element's id
  // PICTURE KINDS (picture, digits, strip): reference px per art pixel, so a 24 px icon at
  // unit 3 is 72 px tall at the reference size. `snap` floors the screen's px per art pixel
  // to a whole number (at least 1) so pixel art stays crisp; `fit` (0: no limit) shrinks the
  // element until it takes at most that share of the safe area's width.
  float unit = 1;
  float fit = 0;
  int digit_cells = 3;  // kind digits: the field's width in cells
  bool snap = false;
  // A PARENT: an element declared before this one. The child is drawn at the parent's scale
  // times its own, with its `attach` point (its own box's nine points) on the parent's box's
  // `anchor` point, moved by (x, y) in the parent's units (its picture px, or reference px for
  // a printed kind) instead of shares of the safe area; it never stacks. A status bar's pieces
  // are children of its panel: each sits on its spot and each is still moved or hidden alone.
  Anchor attach = Anchor::top_left;
  std::string_view parent = {};
  StripShape strip = {};  // kind strip
  RingShape ring = {};    // kind ring
};

// What the player may change: bound to the element's cvars.
struct ElementPlace {
  bool show = true;
  std::string anchor;
  double x = 0, y = 0, scale = 1, alpha = 1;
  std::string parent, attach;  // the parent's id ("" none) and the attach point's word
};

// What the game writes once a frame.
struct Content {
  bool on = false;  // shown this frame (and the player has not hidden it)
  std::string value, caption, unit;
  float level = 0;
  int count = 0, lit = 0;
  bool blink_off = false;
  ui::Color color{1, 1, 1, 1};
  const Feed* feed = nullptr;
  // The picture kinds' art, which a game may swap (its own pictures or a player's): the picture,
  // the digit sheet (`alt`: its alternate figures), the strip's cells and its counts' face (the
  // style's value face when null), the ring's second-lap colour.
  const ui::Image* image = nullptr;
  const DigitSheet* sheet = nullptr;
  bool alt = false;
  std::vector<Cell> cells;
  const Face* face = nullptr;
  float density = 1;  // the pictures' pixels per art pixel (pictures.hpp)
  ui::Color over{1, 1, 1, 1};
  double t = 0;  // s, the frame's clock: the feed's, and the dial's needle eases by it
  // The instruments' numbers (instruments.hpp): a dial's value (`value` is its figure), the
  // heading (rad) a wind meter and a compass turn by, the felt and the weather's wind (world,
  // m/s, the way the air blows to), and a compass's marks (must outlive the draw).
  double number = 0;
  double heading = 0;
  double felt_x = 0, felt_z = 0;
  bool weather = false;
  double weather_x = 0, weather_z = 0;
  std::span<const CompassMark> marks;
};

struct Element {
  std::string id, help, parent;
  ElementSpec spec;  // its `id`, `help` and `parent` views point at the strings above; `under`
                     // is cleared
  ElementPlace place;
  Content content;
  int under = -1;  // the index of `spec.under`, or -1
  Needle needle;   // kind dial: where its needle was drawn last
  // Where it was drawn last, for `under`, its children and the edit mode: its box, its scale
  // over the frame's reference size and its unit (screen px per picture pixel, or per reference
  // px for a printed kind).
  ui::Rect drawn;
  float drawn_scale = 1, drawn_unit = 1;
  bool shown = false;
};

class Layout {
 public:
  Layout() = default;
  Layout(const Layout&) = delete;
  Layout& operator=(const Layout&) = delete;
  Layout(Layout&&) noexcept = default;
  Layout& operator=(Layout&&) noexcept = default;

  // Declares an element; returns its index. Every `under` names an element declared before it.
  int add(const ElementSpec& spec);
  int find(std::string_view id) const;  // -1 when none
  size_t size() const { return elements_.size(); }
  Element& operator[](size_t i) { return elements_[i]; }
  const Element& operator[](size_t i) const { return elements_[i]; }
  Content& content(int i) { return elements_[static_cast<size_t>(i)].content; }

  // The whole HUD: its scale over the frame's, its opacity, and the margin (reference px) its
  // frame keeps from the screen's safe edge. Bound to hud_scale, hud_alpha, hud_margin.
  double scale = 1, alpha = 1;
  double margin = 20;

  // The frame the layout draws in: `style`'s, with this layout's margin and scale.
  Frame frame(int width, int height, const Style& style, Insets insets = {}) const;

  // Every element whose content is on and whose place shows, in table order; returns each box
  // by the element's id, for the layout check.
  std::vector<Placed> draw(ui::DrawList& dl, const Frame& f, const Style& st);

 private:
  std::deque<Element> elements_;  // stable addresses: the cvars are bound into them
};

// Registers every element's cvars, the whole HUD's and `hud_reset`, bound into `layout` (which
// must outlive `con`). Call once, after the last `add`.
void add_hud_cvars(console::Console& con, Layout& layout);

}  // namespace ge::hud
