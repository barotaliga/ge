// Tests: the classic trouble icons (games/shooters_edge/client/trouble_icons.hpp): the turtle only
// with showturtle on and after three slow frames in a row, the turtle and the net picture drawn in
// the top-left corner at the classic screen's whole scale (the net picture 64 classic pixels to
// the right), nothing for a picture the set lacks; and the net icon's rule
// (net/qw_remote.hpp's remote_net_trouble): 63 unacknowledged packets on QuakeWorld.
//
// ge:summary Tests: the turtle after three slow frames, the icons' places, the net icon's rule
// ge:keywords test showturtle turtle net-icon trouble_icons remote_net_trouble
// ge:skill ui testing
#include "check.hpp"
#include "shooters_edge/client/trouble_icons.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace cl = shooters_edge::client;
namespace sn = shooters_edge::net;

TEST(the_turtle_shows_after_three_slow_frames_with_showturtle_on) {
  ge::loop::FrameTimes t;
  t.add(0.2);
  t.add(0.15);
  CHECK(!cl::turtle_due(t, true));  // two slow frames
  t.add(0.1);
  CHECK(cl::turtle_due(t, true));
  CHECK(!cl::turtle_due(t, false));  // showturtle 0
  t.add(0.05);
  CHECK(!cl::turtle_due(t, true));  // a fast frame resets it
}

TEST(the_icons_sit_in_the_top_left_corner_at_the_classic_scale) {
  shooters_edge::hud::HudArt art;
  art.turtle = ge::ui::make_image(32, 32, {1, 1, 1, 1});
  art.net = ge::ui::make_image(32, 32, {1, 1, 1, 1});
  ge::loop::FrameTimes slow;
  for (int i = 0; i < 3; i++) slow.add(0.2);
  ge::ui::DrawList dl;
  dl.begin(640, 400);  // scale 2
  cl::draw_trouble_icons(dl, 640, 400, art, slow, true, true);
  CHECK(dl.size() == 2);
  const ge::ui::Rect a = dl.cmds()[0].box.rect, b = dl.cmds()[1].box.rect;
  CHECK(a.x == 0 && a.y == 0 && a.w == 64 && a.h == 64);
  CHECK(b.x == 128 && b.y == 0 && b.w == 64);
  // A set without the pictures (the free set) draws nothing.
  ge::ui::DrawList none;
  none.begin(640, 400);
  cl::draw_trouble_icons(none, 640, 400, shooters_edge::hud::HudArt{}, slow, true, true);
  CHECK(none.size() == 0);
}

TEST(the_net_icon_shows_with_63_packets_unacknowledged) {
  sn::Remote r;
  CHECK(!sn::remote_net_trouble(r));  // not joined
  r.active = true;
  r.end.stage = sn::ClientStage::active;
  r.end.ch.outgoing_sequence = 64;
  r.end.ch.incoming_acknowledged = 1;
  CHECK(!sn::remote_net_trouble(r));  // 62
  r.end.ch.outgoing_sequence = 65;
  CHECK(sn::remote_net_trouble(r));  // 63
}
