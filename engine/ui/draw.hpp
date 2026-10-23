// THE 2D LAYER'S DRAW LIST: what a menu, a HUD, a console or a backdrop asks to be drawn this
// frame, in order, back to front. It is immediate mode: the caller rebuilds the list every frame
// from its own state, and the list holds no state between frames. Two kinds of command cover
// everything the UI draws:
//
//   - a BOX: a rectangle with rounded corners, filled with a paint (paint.hpp), optionally
//     sampled from an image (a backdrop, a picture, the live game frame) and tinted, with a
//     border, an edge feather (1 px is a crisp antialiased edge; 20 px is a soft drop shadow or
//     a glow) and an opacity;
//   - a TEXT run: a string in a font (font.hpp) at a pixel size, at a baseline, in a colour.
//
// Every command carries the clip rectangle in force when it was added (`push_clip`). The list is
// read by the CPU raster (raster.hpp: the labs' pictures and the tests) and by the GPU pass
// (M22b), which draw the same commands; nothing here knows which.
//
// Coordinates are pixels of the target, y down. A box's paint is in the box's own frame (0,0 at
// its top-left), so one gradient reads the same wherever the box is placed.
//
// ge:summary The 2D draw list: rounded boxes (paint, image, border, feather) and text runs
// ge:keywords ui draw draw-list immediate-mode box rect rounded radius border feather shadow
// ge:keywords glow image tint clip text run align
// ge:skill ui
// ge:test tests/ui_raster_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ui/image.hpp"
#include "ui/paint.hpp"

namespace ge::ui {

class Font;

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

enum class CmdKind : uint8_t { box, text };

struct Cmd {
  CmdKind kind;
  Rect clip;
  Box box;       // kind == box
  TextRun text;  // kind == text
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

  // Everything added until the matching pop is clipped to `r` (intersected with the clip in
  // force).
  void push_clip(Rect r);
  void pop_clip();
  Rect clip() const {
    return clips_.empty() ? Rect{0, 0, float(width_), float(height_)} : clips_.back();
  }

  const std::vector<Cmd>& cmds() const { return cmds_; }
  size_t size() const { return cmds_.size(); }

 private:
  int width_ = 0, height_ = 0;
  std::vector<Cmd> cmds_;
  std::vector<Rect> clips_;
};

}  // namespace ge::ui
