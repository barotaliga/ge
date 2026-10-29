// THE 2D LAYER'S DRAW LIST: what a menu, a HUD, a console or a backdrop asks to be drawn this
// frame, in order, back to front. It is immediate mode: the caller rebuilds the list every frame
// from its own state, and the list holds no state between frames. Five kinds of command cover
// everything the UI draws:
//
//   - a BOX: a rectangle with rounded corners, filled with a paint (paint.hpp), optionally
//     sampled from an image (a backdrop, a picture, the live game frame; whole or nine-sliced)
//     and tinted, with a border, an edge feather (1 px is a crisp antialiased edge; 20 px is a
//     soft drop shadow or a glow) and an opacity, turned by `rotation` about its `pivot`;
//   - a TEXT run: a string in a font (font.hpp) at a pixel size, at a baseline, in a colour;
//   - a SEGMENT: a line `width` px thick with round caps (a polyline is its segments, so its
//     joins are round);
//   - an ARC: a ring sector, `width` px thick across its radius, from one angle to another, its
//     ends flat or round, filled with a paint;
//   - a POLYGON: a convex one, up to `kMaxPolygon` points in either winding, filled with a paint.
//
// ANGLES are radians, 0 at twelve o'clock and clockwise positive on the screen, the way a dial
// reads: a box at `rotation` pi/2 lies on its right side; an arc from 0 to pi/2 is the top-right
// quarter. The sines and cosines are worked once a command (`rotation_of`, the arc's ends) with
// the engine's own libm, so the CPU raster and the GPU pass read the same numbers.
//
// Every command carries the clip rectangle in force when it was added (`push_clip`); the clip is
// always axis-aligned, even round a rotated box. The list is read by the CPU raster (raster.hpp:
// the labs' pictures and the tests) and by the GPU pass (render/ui_pass.hpp), which draw the same
// commands; nothing here knows which.
//
// Coordinates are pixels of the target, y down. A box's paint is in the box's own frame (0,0 at
// its top-left, turning with it), so one gradient reads the same wherever the box is placed; an
// arc's and a polygon's are in their bounding box's frame.
//
// ge:summary The 2D draw list: rounded boxes (paint, image, nine-slice, rotation), text, lines,
// ge:summary arcs and convex polygons
// ge:keywords ui draw draw-list immediate-mode box rect rounded radius border feather shadow
// ge:keywords glow image tint clip text run align rotation pivot nine-slice slice line segment
// ge:keywords polyline capsule arc ring gauge dial polygon convex needle
// ge:skill ui
// ge:test tests/ui_raster_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ui/image.hpp"
#include "ui/paint.hpp"

namespace ge::ui {

class Font;

struct Vec2 {
  float x = 0, y = 0;
};

// Distances in from each edge of a rectangle, px.
struct Insets {
  float left = 0, top = 0, right = 0, bottom = 0;
  bool empty() const { return left <= 0 && top <= 0 && right <= 0 && bottom <= 0; }
};

struct Box {
  Rect rect;
  float radius = 0;  // the corners', px (clamped to half the short side)
  Paint fill = solid({1, 1, 1, 1});
  // An image sampled over the box, multiplied by `fill` (a white fill shows it as it is; a
  // tinted or graded one colours it). `uv` is the source rectangle in the image's pixels.
  const Image* image = nullptr;
  Rect uv;
  float border = 0;  // px inside the edge, drawn in border_color
  Color border_color{1, 1, 1, 1};
  float feather = 1;  // px of edge softness: 1 antialiases, more blurs it outward
  float opacity = 1;
  // NINE-SLICE: with `slice` set, the image's corners (`slice` px of the source, inside `uv`)
  // are drawn unstretched into `slice_to` px of the box, the edges stretch one way and the middle
  // both, so a framed panel is one box at any size. `slice_to` empty means the same as `slice`.
  Insets slice;
  Insets slice_to;
  // The box turned by `rotation` (radians, clockwise) about `pivot` (px in the box's frame; the
  // centre when unset), its paint, image and border turning with it.
  float rotation = 0;
  std::optional<Vec2> pivot;
};

// A box's turn as the drawers read it: the cosine and sine of its rotation and the pivot on the
// target, px. Worked with the engine's libm, so both drawers turn by the same numbers.
struct Turn {
  float cos = 1, sin = 0;
  Vec2 pivot;
  bool turned = false;
};
Turn turn_of(const Box& b);
// A target point in the turned box's own frame (px from its top-left, before the turn).
inline Vec2 unturn(const Turn& t, const Box& b, Vec2 p) {
  if (!t.turned) return {p.x - b.rect.x, p.y - b.rect.y};
  const float dx = p.x - t.pivot.x, dy = p.y - t.pivot.y;
  return {t.pivot.x + dx * t.cos + dy * t.sin - b.rect.x,
          t.pivot.y - dx * t.sin + dy * t.cos - b.rect.y};
}
// The axis-aligned rectangle the turned box covers, grown by `grow` px.
Rect turned_bounds(const Turn& t, const Box& b, float grow);
// A nine-sliced box's source x (or y) for `l` px into a box `size` px across: the image's
// `uv_from` .. `uv_from + uv_size`, its first `s0` and last `s1` px drawn into the box's first
// `d0` and last `d1` px.
float slice_map(float l, float size, float d0, float d1, float uv_from, float uv_size, float s0,
                float s1);

struct Segment {
  Vec2 a, b;
  float half = 0.5f;  // half the width, px
  Color color{1, 1, 1, 1};
  float feather = 1;
  float opacity = 1;
};

struct Arc {
  Vec2 centre;
  float radius = 0;        // to the middle of the ring, px
  float half = 0.5f;       // half the ring's width, px
  float from = 0, to = 0;  // radians, 0 at twelve o'clock, clockwise
  bool round = false;      // round ends (flat when false)
  Paint fill = solid({1, 1, 1, 1});
  float feather = 1;
  float opacity = 1;
  // Worked by DrawList::arc with the engine's libm: the unit directions of the ends and the
  // sweep's middle, and the cosine of half the sweep (a point is in the sweep when its direction
  // is at least that close to the middle).
  Vec2 dir_from, dir_to, dir_mid;
  float cos_half = 1;
};

inline constexpr int kMaxPolygon = 16;

struct Polygon {
  uint32_t first = 0, count = 0;  // points in DrawList::points(), clockwise on the screen
  Rect bounds;                    // the points' bounding box: the paint's frame
  Paint fill = solid({1, 1, 1, 1});
  float feather = 1;
  float opacity = 1;
};

// A polygon's point and the outward unit normal of the edge from it to the next.
struct PolyPoint {
  Vec2 at, normal;
};

enum class Align : uint8_t { left, center, right };

struct TextRun {
  const Font* font = nullptr;
  float size = 16;  // the font's pixel height (ascent minus descent)
  float x = 0, baseline = 0;
  std::string text;  // UTF-8
  Color color{1, 1, 1, 1};
  float tracking = 0;  // px added after every glyph (letter spacing)
};

enum class CmdKind : uint8_t { box, text, segment, arc, polygon };

struct Cmd {
  CmdKind kind = CmdKind::box;
  Rect clip;
  Box box;          // kind == box
  TextRun text;     // kind == text
  Segment segment;  // kind == segment
  Arc arc;          // kind == arc
  Polygon polygon;  // kind == polygon
};

class DrawList {
 public:
  // The target's size, which is the outermost clip.
  void begin(int width, int height);
  int width() const { return width_; }
  int height() const { return height_; }

  void box(const Box& b);
  // A plain filled rectangle.
  void rect(Rect r, Color c, float radius = 0);
  // `text` with its left edge, centre or right edge at x (by `align`) and its baseline at y.
  // Returns the run's advance width, px.
  float text(const Font& font, float size, float x, float baseline, std::string_view text, Color c,
             Align align = Align::left, float tracking = 0);

  // A line from `a` to `b`, `width` px thick, with round caps.
  void line(Vec2 a, Vec2 b, float width, Color c, float feather = 1, float opacity = 1);
  // The segments through `pts` (and back to the first when `closed`).
  void polyline(std::span<const Vec2> pts, float width, Color c, bool closed = false,
                float feather = 1, float opacity = 1);
  // A ring sector round `centre`: `radius` px to the ring's middle, `width` px thick, from angle
  // `from` clockwise to `to` (radians, 0 at twelve o'clock; a sweep of 2 pi or more is the whole
  // ring), its ends flat or `round`.
  void arc(Vec2 centre, float radius, float width, float from, float to, const Paint& fill,
           bool round = false, float feather = 1, float opacity = 1);
  // A convex polygon through `pts` in either winding: 3 to kMaxPolygon points (past that the
  // command is dropped with an error line). A concave outline draws as the part inside every
  // edge's line, not as itself, so keep it convex and split a concave shape into convex ones.
  void polygon(std::span<const Vec2> pts, const Paint& fill, float feather = 1, float opacity = 1);

  // Everything added until the matching pop is clipped to `r` (intersected with the clip in
  // force).
  void push_clip(Rect r);
  void pop_clip();
  Rect clip() const {
    return clips_.empty() ? Rect{0, 0, float(width_), float(height_)} : clips_.back();
  }

  const std::vector<Cmd>& cmds() const { return cmds_; }
  size_t size() const { return cmds_.size(); }
  // Every command from the `from`th on drawn `alpha` times as strongly (a HUD element faded as
  // a whole): its opacity, or a text run's colour, scaled.
  void fade(size_t from, float alpha);
  // Every polygon's points, each command's at its `first`.
  const std::vector<PolyPoint>& points() const { return points_; }

 private:
  Cmd make_cmd(CmdKind kind) const;  // an empty command of `kind` under the clip in force

  int width_ = 0, height_ = 0;
  std::vector<Cmd> cmds_;
  std::vector<PolyPoint> points_;
  std::vector<Rect> clips_;
};

}  // namespace ge::ui
