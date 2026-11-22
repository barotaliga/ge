// The graph along the bottom (bottom_graph.hpp).
//
// ge:summary The graph along the bottom: samples kept, columns sized and drawn
// ge:keywords bottom_graph_add bottom_graph_column draw_bottom_graph
// ge:skill ui
// ge:test tests/hud_bottom_graph_test.cpp
#include "hud/bottom_graph.hpp"

#include <algorithm>

namespace ge::hud {

void bottom_graph_add(BottomGraph& g, float value, ui::Color color) {
  const uint32_t at = g.count % kBottomColumns;
  g.value[at] = value;
  g.color[at] = color;
  g.count++;
}

float bottom_graph_column(float value, const BottomGraphLook& look) {
  if (look.height <= 0) return 0;
  return std::clamp(value * look.scale + look.shift, 1.0f, look.height);
}

void draw_bottom_graph(ui::DrawList& dl, const BottomGraph& g, float width, float height,
                       float unit, const BottomGraphLook& look) {
  if (unit <= 0 || look.height <= 0) return;
  const auto columns = static_cast<uint32_t>(std::max(0.0f, width / unit));
  const uint32_t n = std::min({columns, g.count, static_cast<uint32_t>(kBottomColumns)});
  for (uint32_t i = 0; i < n; i++) {
    const uint32_t at = (g.count - 1 - i) % kBottomColumns;
    const float tall = bottom_graph_column(g.value[at], look) * unit;
    ui::Color c = g.color[at];
    c.a *= look.alpha;
    dl.rect({width - unit * static_cast<float>(i + 1), height - tall, unit, tall}, c);
  }
}

}  // namespace ge::hud
