// The 2D draw list, defined: commands appended with the clip in force, text aligned by its
// measured advance.
//
// ge:summary The 2D draw list defined: boxes and text runs appended under the clip stack
// ge:keywords ui draw draw-list clip push-clip text align measure
// ge:skill ui
// ge:test tests/ui_raster_test.cpp
#include "ui/draw.hpp"

#include "ui/font.hpp"

namespace ge::ui {

void DrawList::begin(int width, int height) {
  width_ = width;
  height_ = height;
  cmds_.clear();
  clips_.clear();
}

void DrawList::box(const Box& b) {
  Cmd c{CmdKind::box, clip(), b, {}};
  cmds_.push_back(std::move(c));
}

void DrawList::rect(Rect r, Color c, float radius) {
  Box b;
  b.rect = r;
  b.radius = radius;
  b.fill = solid(c);
  box(b);
}

float DrawList::text(const Font& font, float size, float x, float baseline, std::string_view text,
                     Color c, Align align, float tracking) {
  const float w = font.advance(text, size, tracking);
  if (align == Align::center) x -= w / 2;
  if (align == Align::right) x -= w;
  Cmd cmd{CmdKind::text, clip(), {}, {}};
  cmd.text = {&font, size, x, baseline, std::string(text), c, tracking};
  cmds_.push_back(std::move(cmd));
  return w;
}

void DrawList::push_clip(Rect r) { clips_.push_back(intersect(clip(), r)); }

void DrawList::pop_clip() {
  if (!clips_.empty()) clips_.pop_back();
}

}  // namespace ge::ui
