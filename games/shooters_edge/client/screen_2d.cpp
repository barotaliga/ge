// The 2D switch and the graph along the bottom, every classic game's (screen_2d.hpp).
//
// ge:summary The classic layer's scr_draw2d and frame-time graph cvars, the graph fed and drawn
// ge:keywords scr_draw2d r_drawhud timegraph r_timegraph debuggraph graphheight graphscale graphshift
// ge:skill ui console
// ge:test tests/shooters_edge_screen_2d_test.cpp
#include "shooters_edge/client/screen_2d.hpp"

#include <vector>

namespace shooters_edge::client {

namespace con = ge::console;

void add_screen_2d_cvars(con::Cvars& v, Screen2D& s) {
  v.bind({.name = "scr_draw2d",
          .fallback = "2",
          .flags = con::kArchive,
          .help = "What is drawn over the view: the status bar, the scoreboard, the messages.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 3,
          .values = "0: nothing (for screenshots); 1: all but the status bar; 2: everything (the "
                    "default); 3: everything but the scoreboard and the help computer"},
         &s.draw2d);
  v.bind({.name = "timegraph",
          .fallback = "0",
          .help = "Whether each frame's time is drawn as a column along the bottom of the screen.",
          .kind = con::CvarKind::boolean,
          .values = "0: not drawn (the default); 1: a column a frame, a unit tall a millisecond"},
         &s.time_graph);
  v.bind({.name = "debuggraph",
          .fallback = "0",
          .help = "Whether the graph along the bottom of the screen is drawn.",
          .kind = con::CvarKind::boolean,
          .values = "0: only with timegraph (the default); 1: always"},
         &s.debug_graph);
  v.bind({.name = "graphheight",
          .fallback = "32",
          .help = "The graph's height along the bottom of the screen.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 1000,
          .values = "0..1000: that many units of the screen (32 the default)"},
         &s.graph_height);
  v.bind({.name = "graphscale",
          .fallback = "1",
          .help = "The graph's units a unit of value.",
          .min = 0,
          .max = 100,
          .values = "0..100: 1 the default"},
         &s.graph_scale);
  v.bind({.name = "graphshift",
          .fallback = "0",
          .help = "The units added to every column of the graph.",
          .min = -1000,
          .max = 1000,
          .values = "-1000..1000: 0 the default"},
         &s.graph_shift);
  v.add_synonym("r_timegraph", "timegraph");  // Quake's spelling of the same graph
  // QuakeWorld's switch for the whole 2D: its 1 (the default) is everything, our 2.
  v.add_mapped_synonym("r_drawhud", "scr_draw2d", {{"0", "0"}, {"1", "2"}});
}

bool draw_screen_graph(ge::ui::DrawList& dl, ScreenGraph& g, const Screen2D* s, double now,
                       int width, int height, int scale, float alpha) {
  const bool on = s != nullptr && (s->time_graph || s->debug_graph) && s->draw2d != 0;
  if (s != nullptr && s->time_graph && g.at >= 0 && now > g.at)
    ge::hud::bottom_graph_add(*g.graph, static_cast<float>((now - g.at) * 1000),
                              ge::ui::hex(0xe0e0e0));
  g.at = s != nullptr && s->time_graph ? now : -1;
  if (!on || scale <= 0) return false;
  ge::hud::BottomGraphLook look;
  look.height = static_cast<float>(s->graph_height);
  look.scale = static_cast<float>(s->graph_scale);
  look.shift = static_cast<float>(s->graph_shift);
  look.alpha = alpha;
  const int whole_w = width / scale * scale, whole_h = height / scale * scale;
  ge::hud::draw_bottom_graph(dl, *g.graph, static_cast<float>(whole_w), static_cast<float>(whole_h),
                             static_cast<float>(scale), look);
  return true;
}

std::vector<ge::hud::Placed> draw_hud_2d(ge::ui::DrawList& dl, ge::hud::Layout& layout,
                                         const ge::hud::Frame& f, const ge::hud::Style& st,
                                         const Screen2D& s) {
  if (s.draw2d == 0) return {};
  if (s.draw2d != 1) return layout.draw(dl, f, st);
  std::vector<bool> shown(layout.size());  // all but the crosshair hidden for this draw
  for (size_t i = 0; i < layout.size(); ++i) {
    shown[i] = layout[i].place.show;
    if (layout[i].id != "crosshair") layout[i].place.show = false;
  }
  std::vector<ge::hud::Placed> placed = layout.draw(dl, f, st);
  for (size_t i = 0; i < layout.size(); ++i) layout[i].place.show = shown[i];
  return placed;
}

}  // namespace shooters_edge::client
