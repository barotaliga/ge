// engine/hud/bottom_graph: samples kept newest last, columns sized by scale and shift and held to
// the height, drawn along the bottom newest at the right, as many as the width holds.
//
// ge:summary Tests the graph along the bottom: samples, column heights, the drawing
// ge:keywords test bottom-graph timegraph graphheight graphscale graphshift
// ge:skill ui testing
#include <memory>

#include "check.hpp"
#include "hud/bottom_graph.hpp"

namespace gh = ge::hud;

TEST(a_column_is_value_times_scale_plus_shift_held_to_the_height) {
  gh::BottomGraphLook look;
  CHECK(gh::bottom_graph_column(10, look) == 10);
  CHECK(gh::bottom_graph_column(100, look) == 32);  // graphheight 32
  CHECK(gh::bottom_graph_column(0, look) == 1);     // a unit, so the line shows
  look.scale = 2;
  look.shift = 3;
  CHECK(gh::bottom_graph_column(5, look) == 13);
  look.height = 0;
  CHECK(gh::bottom_graph_column(5, look) == 0);
}

TEST(the_graph_is_drawn_along_the_bottom_newest_at_the_right) {
  auto g = std::make_unique<gh::BottomGraph>();
  const ge::ui::Color white{1, 1, 1, 1}, red{1, 0, 0, 1};
  gh::bottom_graph_add(*g, 4, white);
  gh::bottom_graph_add(*g, 8, red);
  ge::ui::DrawList dl;
  dl.begin(320, 240);
  gh::BottomGraphLook look;
  gh::draw_bottom_graph(dl, *g, 320, 240, 2, look);
  CHECK(dl.size() == 2);
  const auto& c = dl.cmds();
  // The newest (8 units, red) at the right edge, two px a unit; the older left of it.
  CHECK(c[0].box.rect.x == 318 && c[0].box.rect.w == 2 && c[0].box.rect.h == 16 &&
        c[0].box.rect.y == 224);
  CHECK(c[1].box.rect.x == 316 && c[1].box.rect.h == 8);
  // A narrow screen draws only the samples it holds.
  for (int i = 0; i < gh::kBottomColumns + 5; i++) gh::bottom_graph_add(*g, 1, white);
  ge::ui::DrawList narrow;
  narrow.begin(10, 10);
  gh::draw_bottom_graph(narrow, *g, 10, 10, 1, look);
  CHECK(narrow.size() == 10);
}
