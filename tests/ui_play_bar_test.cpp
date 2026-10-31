// The play bar (engine/ui/play_bar.hpp): its layout scales with the target, the time under a
// point on the track, the marker under the pointer, the clock's text, the pointer's drag and
// release and the button, and the bar drawn by the CPU raster with the played part, a marker and
// the playhead where they belong.
//
// ge:summary Tests: the play bar's layout, time, markers, clock, drag and its pixels
// ge:keywords test ui play-bar seek drag marker clock raster
// ge:skill ui testing
// ge:test tests/ui_play_bar_test.cpp
#include <vector>

#include "check.hpp"
#include "ui/play_bar.hpp"
#include "ui/raster.hpp"

namespace gu = ge::ui;

TEST(the_layout_scales_and_maps_x_to_time) {
  const gu::PlayBarLayout a = gu::play_bar_layout(1280, 720);
  const gu::PlayBarLayout b = gu::play_bar_layout(2560, 1440);
  CHECK(b.scale == 2 * a.scale && b.track.h == 2 * a.track.h);
  CHECK(a.track.bottom() < 720 && a.panel.bottom() == 720);
  CHECK(gu::play_bar_time(a, a.track.x, 100) == 0);
  CHECK(gu::play_bar_time(a, a.track.right(), 100) == 100);
  CHECK(gu::play_bar_time(a, a.track.x + a.track.w / 4, 100) == 25);
  CHECK(gu::play_bar_time(a, -50, 100) == 0 && gu::play_bar_time(a, 5000, 100) == 100);
}

TEST(markers_under_the_pointer_and_the_clock) {
  const gu::PlayBarLayout l = gu::play_bar_layout(1280, 720);
  const std::vector<gu::PlayBarMarker> m{{25, 20, 27, {1, 0, 0, 1}, "a"},
                                         {50, 45, 52, {0, 1, 0, 1}, "b"}};
  const float y = l.track.y;
  const float x50 = l.track.x + l.track.w / 2;
  CHECK(gu::play_bar_marker_at(l, m, 100, x50 + 2, y) == 1);
  CHECK(gu::play_bar_marker_at(l, m, 100, x50 + 40, y) == -1);
  CHECK(gu::play_bar_marker_at(l, m, 100, x50, y - 100) == -1);  // off the bar's row
  CHECK(gu::play_clock(0) == "0:00" && gu::play_clock(83.9) == "1:23");
  CHECK(gu::play_clock(3723) == "1:02:03");
}

TEST(a_press_drags_and_the_release_seeks) {
  const gu::PlayBarLayout l = gu::play_bar_layout(1280, 720);
  gu::PlayBarPointer p;
  const float y = l.track.y + 1;
  CHECK(gu::play_bar_pointer(p, l, 100, 10, 10, true) == gu::PlayBarAct::none);  // not the bar
  CHECK(gu::play_bar_pointer(p, l, 100, 10, 10, false) == gu::PlayBarAct::none);
  CHECK(gu::play_bar_pointer(p, l, 100, l.track.x + l.track.w / 4, y, true) ==
        gu::PlayBarAct::drag);
  CHECK(p.at_s == 25);
  CHECK(gu::play_bar_pointer(p, l, 100, l.track.x + l.track.w / 2, y - 200, true) ==
        gu::PlayBarAct::drag);  // a drag follows the pointer off the bar
  CHECK(p.at_s == 50);
  CHECK(gu::play_bar_pointer(p, l, 100, l.track.x + l.track.w / 2, y - 200, false) ==
        gu::PlayBarAct::seek);
  CHECK(p.at_s == 50 && !p.dragging);
  CHECK(gu::play_bar_pointer(p, l, 100, l.button.x + 5, l.button.y + 5, true) ==
        gu::PlayBarAct::toggle);
}

TEST(the_bar_is_drawn_where_it_says) {
  const int w = 640, h = 360;
  const gu::PlayBarLayout l = gu::play_bar_layout(w, h);
  const std::vector<gu::PlayBarMarker> m{{75, 70, 76, gu::hex(0x00ff00), "late"}};
  gu::PlayBarView v;
  v.length_s = 100;
  v.position_s = 50;
  v.markers = m;
  gu::DrawList list;
  list.begin(w, h);
  gu::draw_play_bar(list, l, v);
  gu::Image c = gu::make_image(w, h, gu::hex(0x000000));
  gu::render(list, c);
  const auto px = [&](float x, float y) {
    return gu::sample(c, static_cast<float>(static_cast<int>(x)) + 0.5f,
                      static_cast<float>(static_cast<int>(y)) + 0.5f);
  };
  const float cy = l.track.y + l.track.h / 2;
  const gu::Color played = px(l.track.x + l.track.w * 0.25f, cy);
  CHECK(played.r > 0.8f && played.g < 0.5f);  // the played part is red
  const gu::Color ahead = px(l.track.x + l.track.w * 0.9f, cy);
  CHECK(ahead.r < 0.5f && ahead.r == ahead.g);  // the rest grey
  const gu::Color marker = px(l.track.x + l.track.w * 0.75f, cy);
  CHECK(marker.g > 0.8f && marker.r < 0.3f);  // the marker green
  const gu::Color head = px(l.track.x + l.track.w * 0.5f, cy);
  CHECK(head.r > 0.9f && head.g > 0.9f && head.b > 0.9f);  // the playhead white
  CHECK(px(w / 2.0f, 20).r == 0);                          // nothing over the picture above the bar
}
