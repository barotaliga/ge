// The 2D switch and the graph along the bottom every classic game shares (client/screen_2d.hpp):
// the cvars and Quake's spelling, the graph fed by the real time, the HUD drawn as scr_draw2d
// asks, and the values kept when another game is loaded.
//
// ge:summary Tests the shared scr_draw2d and timegraph cvars, the graph and the HUD's 2D switch
// ge:keywords scr_draw2d timegraph r_timegraph debuggraph graphheight draw_hud_2d
// ge:skill ui
#include "shooters_edge/client/screen_2d.hpp"

#include <string>

#include "check.hpp"
#include "console/console.hpp"

namespace cl = shooters_edge::client;

TEST(the_cvars_are_registered_with_their_defaults_and_quakes_spelling) {
  ge::console::Console c;
  cl::Screen2D s;
  s.draw2d = 0;
  cl::add_screen_2d_cvars(c.cvars(), s);
  CHECK(s.draw2d == 2 && !s.time_graph && !s.debug_graph && s.graph_height == 32);
  c.execute("scr_draw2d 3; r_timegraph 1; graphheight 20; graphscale 0.5; graphshift 4");
  CHECK(s.draw2d == 3 && s.time_graph && s.graph_height == 20);
  CHECK(s.graph_scale == 0.5 && s.graph_shift == 4);
  c.execute("scr_draw2d 9");  // past its top: held to 3
  CHECK(s.draw2d == 3);
  c.execute("r_drawhud 0");  // QuakeWorld's switch: 0 nothing, 1 everything (our 2)
  CHECK(s.draw2d == 0);
  c.execute("r_drawhud 1");
  CHECK(s.draw2d == 2);
}

TEST(the_graph_takes_each_frames_time_and_draws_only_while_asked) {
  cl::Screen2D s;
  cl::ScreenGraph g;
  ge::ui::DrawList dl;
  s.time_graph = true;
  for (const double now : {1.0, 1.010, 1.050}) {  // frames of 10 and 40 ms
    dl.begin(640, 480);
    CHECK(cl::draw_screen_graph(dl, g, &s, now, 640, 480, 2));
  }
  CHECK_MSG(dl.cmds().size() == 2, std::to_string(dl.cmds().size()));
  const ge::ui::Rect newest = dl.cmds()[0].box.rect;
  CHECK(newest.x == 638 && newest.w == 2 && newest.h == 64 && newest.y == 416);  // 40 ms: 32 high
  s.draw2d = 0;  // nothing over the view: the graph too, though it is still fed
  dl.begin(640, 480);
  CHECK(!cl::draw_screen_graph(dl, g, &s, 1.060, 640, 480, 2));
  CHECK(dl.cmds().empty());
  CHECK(!cl::draw_screen_graph(dl, g, nullptr, 2, 640, 480, 2));  // no settings: off
}

TEST(the_hud_is_drawn_as_scr_draw2d_asks) {
  ge::hud::Layout l;
  for (const char* id : {"health", "crosshair"}) {
    const int i = l.add({.id = id, .kind = ge::hud::Kind::readout});
    l.content(i).on = true;
    l.content(i).value = "100";
  }
  const ge::hud::Style st;
  ge::ui::DrawList dl;
  const auto placed = [&](int draw2d) {
    dl.begin(640, 480);
    std::string names;
    for (const ge::hud::Placed& p :
         cl::draw_hud_2d(dl, l, l.frame(640, 480, st), st, cl::Screen2D{.draw2d = draw2d}))
      names += std::string(p.name) + " ";
    return names;
  };
  CHECK_MSG(placed(2) == "health crosshair ", placed(2));
  CHECK(placed(3) == "health crosshair ");
  CHECK_MSG(placed(1) == "crosshair ", placed(1));  // all but the status bar
  CHECK(placed(0).empty());
  CHECK(l[0].place.show);  // the player's own choice kept
  CHECK(cl::shows_scores({.draw2d = 2}) && cl::shows_scores({.draw2d = 1}));
  CHECK(!cl::shows_scores({.draw2d = 0}) && !cl::shows_scores({.draw2d = 3}));
}
