// The classic turtle and net icons (trouble_icons.hpp).
//
// ge:summary When the turtle and the net icon show, and the two drawn in the view's corner
// ge:keywords showturtle turtle net-icon draw_trouble_icons
// ge:skill ui
// ge:test tests/shooters_edge_trouble_icons_test.cpp
#include "shooters_edge/client/trouble_icons.hpp"

#include <algorithm>
#include <cmath>

namespace shooters_edge::client {

bool turtle_due(const ge::loop::FrameTimes& times, bool on) {
  if (!on || times.size() < kTurtleFrames) return false;
  for (size_t i = 0; i < kTurtleFrames; i++)
    if (times.recent(i) < kTurtleFrameS) return false;
  return true;
}

void draw_trouble_icons(ge::ui::DrawList& dl, int w, int h, const hud::HudArt& art,
                        const ge::loop::FrameTimes& times, bool showturtle, bool net) {
  // The classic screen's whole scale, as its menus are drawn (320 x 200 pixels).
  const float scale = std::max(
      1.0f, std::floor(std::min(static_cast<float>(w) / 320.0f, static_cast<float>(h) / 200.0f)));
  const auto draw = [&](const ge::ui::Image& img, float x) {
    if (img.width <= 0 || img.height <= 0) return;
    const float k = scale / std::max(1.0f, art.density);
    ge::ui::Box b;
    b.rect = {x * scale, 0, static_cast<float>(img.width) * k, static_cast<float>(img.height) * k};
    b.image = &img;
    b.uv = {0, 0, static_cast<float>(img.width), static_cast<float>(img.height)};
    b.nearest = art.density <= 1;
    b.feather = 0;
    dl.box(b);
  };
  if (turtle_due(times, showturtle)) draw(art.turtle, 0);
  if (net) draw(art.net, kNetIconX);
}

}  // namespace shooters_edge::client
