// engine/hud/instruments: the drawn widgets. A dial at known values puts its needle at known
// angles (the knee squeezing the low end); the needle settles like a critically damped spring and
// never overshoots; a world vector turned heading-up lands where a player would point; a skier at
// speed in still air feels an arrow pointing back at him; a calm draws no arrow and a gale draws
// it red; the compass centres the heading under its notch.
//
// ge:summary Tests the HUD instruments: dial angles, the needle's spring, heading-up, wind, compass
// ge:keywords test hud instruments dial needle knee ease heading-up wind felt gale calm compass
// ge:skill ui testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/math.hpp"
#include "hud/instruments.hpp"
#include "hud/layout.hpp"

using namespace ge::hud;
namespace ui = ge::ui;
using ge::core::kPi;

namespace {

bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

Frame frame_720() { return frame_for(1280, 720, Style{}); }

// The polygons a draw list holds, each as its points.
std::vector<std::vector<ui::Vec2>> polygons(const ui::DrawList& dl) {
  std::vector<std::vector<ui::Vec2>> out;
  for (const ui::Cmd& c : dl.cmds()) {
    if (c.kind != ui::CmdKind::polygon) continue;
    std::vector<ui::Vec2> pts;
    for (uint32_t i = 0; i < c.polygon.count; ++i)
      pts.push_back(dl.points()[c.polygon.first + i].at);
    out.push_back(pts);
  }
  return out;
}

// The draw list's angle (0 up, clockwise) from `c` to `p`.
double angle_to(ui::Vec2 c, ui::Vec2 p) { return std::atan2(p.x - c.x, c.y - p.y); }

// The point of a triangle furthest from `c`: its tip.
ui::Vec2 tip_of(const std::vector<ui::Vec2>& tri, ui::Vec2 c) {
  ui::Vec2 best = tri[0];
  float far = -1;
  for (const ui::Vec2& p : tri) {
    const float d = std::hypot(p.x - c.x, p.y - c.y);
    if (d > far) far = d, best = p;
  }
  return best;
}

}  // namespace

TEST(a_dial_sweeps_from_its_start_to_its_end_and_clamps) {
  Dial d;
  d.min = 0, d.max = 100;
  CHECK(near(dial_angle(d, 0), d.from));
  CHECK(near(dial_angle(d, 100), d.to));
  CHECK(near(dial_angle(d, 50), (static_cast<double>(d.from) + d.to) / 2, 1e-6));
  CHECK(near(dial_angle(d, -20), d.from));
  CHECK(near(dial_angle(d, 400), d.to));
}

TEST(a_knee_gives_the_low_end_its_share_of_the_sweep) {
  Dial d;
  d.min = 0, d.max = 140, d.knee = 30, d.knee_share = 0.25;
  const double sweep = static_cast<double>(d.to) - d.from;
  CHECK(near(dial_angle(d, 30), d.from + 0.25 * sweep, 1e-6));
  CHECK(near(dial_angle(d, 15), d.from + 0.125 * sweep, 1e-6));
  CHECK(near(dial_angle(d, 85), d.from + (0.25 + 0.75 * 0.5) * sweep, 1e-6));
  // The 30..140 band, the run's, takes three quarters of the dial.
  CHECK(dial_angle(d, 140) - dial_angle(d, 30) > 2 * (dial_angle(d, 30) - dial_angle(d, 0)));
}

TEST(the_needle_jumps_first_then_settles_without_overshoot) {
  Needle n;
  CHECK(near(ease_needle(n, 40, 0, 0.1), 40));
  double last = 40;
  bool monotone = true;
  for (int i = 1; i <= 30; ++i) {
    const double at = ease_needle(n, 100, i / 120.0, 0.1);
    monotone = monotone && at >= last && at <= 100;
    last = at;
  }
  CHECK(monotone);
  // A quarter of a second on, within half a percent of the step.
  CHECK(std::fabs(last - 100) < 0.3);
  // At 0.1 s it is most of the way, not all of it.
  Needle m;
  ease_needle(m, 0, 0, 0.1);
  const double tenth = ease_needle(m, 100, 0.1, 0.1);
  CHECK(tenth > 95 && tenth < 100);
}

TEST(the_needle_is_the_same_however_the_frames_cut_the_time) {
  Needle a, b;
  ease_needle(a, 0, 0, 0.1);
  ease_needle(b, 0, 0, 0.1);
  ease_needle(a, 80, 0.06, 0.1);
  for (int i = 1; i <= 6; ++i) ease_needle(b, 80, i * 0.01, 0.1);
  CHECK(near(a.at, b.at, 1e-9));
  CHECK(near(a.rate, b.rate, 1e-7));
}

TEST(a_seek_or_a_pause_jumps_the_needle) {
  Needle n;
  ease_needle(n, 10, 1, 0.1);
  CHECK(near(ease_needle(n, 90, 0.5, 0.1), 90));  // the clock went back
  CHECK(near(ease_needle(n, 20, 2.5, 0.1), 20));  // a gap of two seconds
  CHECK(near(ease_needle(n, 70, 2.51, 0), 70));   // no settling asked for
}

TEST(a_dial_draws_its_needle_at_the_values_angle) {
  const Style st;
  const Frame f = frame_720();
  Dial d;
  d.min = 0, d.max = 140, d.knee = 30, d.red = 110;
  for (const double v : {0.0, 30.0, 72.0, 140.0}) {
    ui::DrawList dl;
    dl.begin(1280, 720);
    const Size z = measure(f, d);
    const ui::Rect at{100, 100, z.w, z.h};
    draw(dl, f, st, d, {v, "72", "KM/H", st.ink}, at);
    const auto polys = polygons(dl);
    CHECK(polys.size() == 1);
    const ui::Vec2 c{at.x + z.w / 2, at.y + z.h / 2};
    const double a = angle_to(c, tip_of(polys[0], c));
    CHECK(std::fabs(ge::core::angle_diff(a, dial_angle(d, v))) < 1e-3);
  }
}

TEST(a_world_vector_turned_heading_up) {
  CHECK(near(heading_up(0, 0, 1), 0));
  CHECK(near(heading_up(0, 1, 0), kPi / 2));
  CHECK(near(heading_up(kPi / 2, 1, 0), 0));
  CHECK(near(std::fabs(heading_up(kPi / 2, -1, 0)), kPi));
  CHECK(near(heading_up(kPi / 2, 0, 1), -kPi / 2));
}

TEST(at_speed_in_still_air_the_arrow_points_back_at_him) {
  const Style st;
  const Frame f = frame_720();
  const WindGauge g;
  for (const double h : {0.0, 0.7, -2.1}) {
    // Skiing along `h` at 15 m/s with no wind: he feels -v.
    WindReading r;
    r.heading = h;
    r.felt_x = -15 * std::sin(h), r.felt_z = -15 * std::cos(h);
    ui::DrawList dl;
    dl.begin(1280, 720);
    const Size z = measure(f, st, g);
    const ui::Rect at{200, 100, z.w, z.h};
    draw(dl, f, st, g, r, at);
    const auto polys = polygons(dl);
    CHECK(polys.size() == 2);  // the notch ahead and the arrowhead
    const ui::Vec2 c{at.x + z.w / 2, at.y + f.px(g.size) / 2};
    const double a = angle_to(c, tip_of(polys.back(), c));
    CHECK(std::fabs(std::fabs(a) - kPi) < 1e-3);
  }
}

TEST(the_weather_mark_stands_where_the_wind_comes_from) {
  const Style st;
  const Frame f = frame_720();
  const WindGauge g;
  WindReading r;
  r.heading = 0;
  r.weather = true;
  r.weather_x = 8, r.weather_z = 0;  // blowing toward the east: it comes from the west, on his left
  ui::DrawList dl;
  dl.begin(1280, 720);
  const Size z = measure(f, st, g);
  const ui::Rect at{200, 100, z.w, z.h};
  draw(dl, f, st, g, r, at);
  const auto polys = polygons(dl);
  CHECK(polys.size() == 2);  // the notch and the mark; no arrow in the calm
  const ui::Vec2 c{at.x + z.w / 2, at.y + f.px(g.size) / 2};
  // The mark's base sits on the rim on the left, its tip pointing in.
  ui::Vec2 mid{0, 0};
  for (const ui::Vec2& p : polys.back()) mid.x += p.x / 3, mid.y += p.y / 3;
  CHECK(std::fabs(ge::core::angle_diff(angle_to(c, mid), -kPi / 2)) < 0.05);
}

TEST(a_gale_turns_the_arrow_the_alarm_colour) {
  const Style st;
  const Frame f = frame_720();
  const WindGauge g;
  WindReading r;
  r.felt_z = -25;
  ui::DrawList dl;
  dl.begin(1280, 720);
  draw(dl, f, st, g, r, {0, 0, 200, 200});
  const ui::Cmd* head = nullptr;
  for (const ui::Cmd& c : dl.cmds())
    if (c.kind == ui::CmdKind::polygon) head = &c;
  CHECK(head != nullptr);
  if (head != nullptr) CHECK(head->polygon.fill.stops[0].color.r == st.bad.r);
}

TEST(the_compass_centres_the_heading_under_its_notch) {
  const Style st;
  const Frame f = frame_720();
  const Compass c;
  const Size z = measure(f, c);
  for (const double h : {0.0, kPi / 2, 0.1}) {
    ui::DrawList dl;
    dl.begin(1280, 720);
    const ui::Rect at{300, 20, z.w, z.h};
    draw(dl, f, st, c, {h, {}, st.ink}, at);
    // A cardinal's long tick: under the notch when he faces it, a tenth of a radian off when not.
    const float mid = at.x + at.w / 2;
    const float off = static_cast<float>((h == 0.1 ? -0.1 : 0) / (c.span / 2)) * at.w / 2;
    bool found = false;
    for (const ui::Cmd& cmd : dl.cmds())
      if (cmd.kind == ui::CmdKind::segment && cmd.segment.half > f.px(1)) {
        found |= std::fabs(cmd.segment.a.x - (mid + off)) < 0.01f;
      }
    CHECK(found);
  }
}

TEST(the_layout_draws_the_instruments_from_their_content) {
  Layout l;
  const int dial = l.add({.id = "dial", .kind = Kind::dial});
  const int wind = l.add({.id = "wind", .kind = Kind::wind, .anchor = Anchor::top_right});
  const int compass = l.add({.id = "compass", .kind = Kind::compass, .anchor = Anchor::top});
  for (const int i : {dial, wind, compass}) l.content(i).on = true;
  l.content(dial).number = 60;
  l.content(dial).value = "60";
  const Style st;
  const Frame f = l.frame(1280, 720, st);
  ui::DrawList dl;
  dl.begin(1280, 720);
  const auto placed = l.draw(dl, f, st);
  CHECK(placed.size() == 3);
  CHECK(layout_problems(f, placed).empty());
  CHECK(near(l[static_cast<size_t>(dial)].needle.at, 60));
}
