// LAB `hud-instruments`: the HUD's drawn widgets (engine/hud/instruments.hpp) over a sweep of
// values, and Fall Line's HUD with them in its frame:
//
//   - the SPEEDOMETER, Fall Line's dial, at seven speeds from a stand to its end: the needle's
//     angle measured off the drawn polygon against `dial_angle`, the knee giving the crawl a
//     quarter of the sweep, the red band past a racer's pace;
//   - the NEEDLE's spring: a step from 20 to 100 km/h stepped at the frame rate, its settling
//     time to 2 % and its overshoot (a critically damped spring has none);
//   - the WIND METER at eight headings for a skier at 15 m/s in still air (the arrow must point
//     straight back at him every time), then a head wind, a cross gale (red) and a calm (no arrow,
//     the weather's mark alone);
//   - the COMPASS at three headings with two marks;
//   - Fall Line's HUD at the three reference viewports with the dial and the wind meter up, laid
//     out clean (an overlap or a box off the screen fails the lab), and once with
//     `hud_speedo_style number` for the figure the dial replaces.
//
// The backdrops are painted, so the lab needs no GPU and its pictures are the same bits on every
// row.
//
// ge:summary Lab: the HUD instruments (dial, needle, wind meter, compass) and Fall Line's HUD
// ge:keywords lab hud hud-instruments dial speedometer needle settle overshoot wind-meter felt
// ge:keywords gale calm compass viewports screenshot
// ge:skill ui labs
// ge:test tests/hud_instruments_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "fall_line/hud/view.hpp"
#include "fall_line/hud/words.hpp"
#include "hud/instruments.hpp"
#include "hud/layout.hpp"
#include "lab/lab.hpp"
#include "ui/raster.hpp"

namespace {

namespace fh = fall_line::hud;
namespace gh = ge::hud;
namespace ui = ge::ui;
namespace num = ge::core::num;
using ge::core::kPi;

void backdrop(ui::DrawList& dl, int w, int h) {
  ui::Box b;
  const ui::Stop sky[2] = {{0, ui::hex(0x2d5f9a)}, {1, ui::hex(0xbcd3ea)}};
  b.rect = {0, 0, float(w), float(h) * 0.42f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, sky, 2);
  dl.box(b);
  const ui::Stop snow[2] = {{0, ui::hex(0xdfe8f2)}, {1, ui::hex(0xf9fbfd)}};
  b.rect = {0, float(h) * 0.42f, float(w), float(h) * 0.58f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, snow, 2);
  dl.box(b);
}

ge::lab::Picture rendered(const ui::DrawList& dl) {
  ui::Image img = ui::make_image(dl.width(), dl.height(), ui::hex(0x000000));
  ui::render(dl, img);
  ge::lab::Picture pic;
  pic.width = img.width, pic.height = img.height, pic.rgba = std::move(img.rgba);
  return pic;
}

// The draw list's angle (0 up, clockwise) from `c` to the furthest point of the last polygon.
double last_polygon_angle(const ui::DrawList& dl, ui::Vec2 c) {
  const ui::Cmd* last = nullptr;
  for (const ui::Cmd& cmd : dl.cmds())
    if (cmd.kind == ui::CmdKind::polygon) last = &cmd;
  if (last == nullptr) return 0;
  ui::Vec2 tip{c.x, c.y};
  float far = -1;
  for (uint32_t i = 0; i < last->polygon.count; ++i) {
    const ui::Vec2 p = dl.points()[last->polygon.first + i].at;
    const float d = std::hypot(p.x - c.x, p.y - c.y);
    if (d > far) far = d, tip = p;
  }
  return std::atan2(tip.x - c.x, c.y - tip.y);
}

// A label under an instrument in the sheet.
void label(ui::DrawList& dl, const gh::Frame& f, const gh::Style& st, float x, float y,
           const std::string& text) {
  gh::draw_shadowed(dl, st.caption_face, f.px(st.caption_size), x, y, text, st.ink, st.shadow,
                    f.px(st.shadow_drop), ui::Align::center);
}

void dials(ge::lab::Lab& lab, const gh::Style& st, const gh::Dial& d) {
  const int w = 1280, h = 360;
  const gh::Frame f = gh::frame_for(w, h, st);
  ui::DrawList dl;
  dl.begin(w, h);
  backdrop(dl, w, h);
  const double speeds[] = {0, 15, 30, 60, 90, 110, 140};
  const gh::Size z = gh::measure(f, d);
  double worst = 0;
  const float step = static_cast<float>(w) / 7;
  for (int i = 0; i < 7; ++i) {
    const ui::Rect at{step * static_cast<float>(i) + (step - z.w) / 2, 60, z.w, z.h};
    const double v = speeds[i];
    gh::draw(dl, f, st, d, {v, num::to_fixed(v, 0), fh::words::kSpeedUnit, st.ink}, at);
    ui::DrawList one;  // the needle's polygon alone, for its angle
    one.begin(w, h);
    gh::draw(one, f, st, d, {v, "", "", st.ink}, at);
    const ui::Vec2 c{at.x + z.w / 2, at.y + z.h / 2};
    worst = std::max(
        worst, std::fabs(ge::core::angle_diff(last_polygon_angle(one, c), gh::dial_angle(d, v))));
    label(dl, f, st, c.x, at.bottom() + f.px(40),
          num::to_fixed(gh::dial_angle(d, v) * 180 / kPi, 0) + " DEG");
  }
  lab.metric("dial.angle_error_max", worst, "rad");
  if (worst > 1e-3)
    lab.fail("the dial's needle stands " + num::to_fixed(worst, 4) + " rad off dial_angle",
             "draw(Dial) in engine/hud/instruments.cpp must turn the needle by dial_angle; "
             "rerun `make lab LAB=hud-instruments`. Skill: ui.");
  lab.shot({"speedometer",
            "THE SPEEDOMETER FROM A STAND TO ITS END",
            "Fall Line's dial at 0, 15, 30, 60, 90, 110 and 140 km/h, each with the angle its "
            "needle stands at. Under 30 km/h (the knee) the crawl takes the sweep's first "
            "quarter; the red band runs from 110 to the end.",
            "the needle on its value; the figure in the face; the red band at the top end",
            {{"max", num::to_fixed(d.max, 0)},
             {"knee", num::to_fixed(d.knee, 0)},
             {"red", num::to_fixed(d.red, 0)},
             {"angle error max", num::to_fixed(worst, 6)}}},
           rendered(dl));
}

void needle(ge::lab::Lab& lab, const gh::Dial& d) {
  gh::Needle n;
  gh::ease_needle(n, 20, 0, d.settle);
  double settle = -1, peak = 20;
  for (int i = 1; i <= 120; ++i) {
    const double t = i / 120.0;
    const double at = gh::ease_needle(n, 100, t, d.settle);
    peak = std::max(peak, at);
    if (settle < 0 && std::fabs(at - 100) <= 0.02 * 80) settle = t;
  }
  lab.metric("needle.settle_s", settle, "s");
  lab.metric("needle.overshoot", peak - 100, "km/h");
  if (peak > 100)
    lab.fail("the needle overshot by " + num::to_fixed(peak - 100, 4) + " km/h",
             "ease_needle must stay critically damped (engine/hud/instruments.cpp). Skill: ui.");
}

void winds(ge::lab::Lab& lab, const gh::Style& st) {
  const int w = 1280, h = 520;
  const gh::Frame f = gh::frame_for(w, h, st);
  ui::DrawList dl;
  dl.begin(w, h);
  backdrop(dl, w, h);
  const gh::WindGauge g;
  const gh::Size z = gh::measure(f, st, g);
  const float step = static_cast<float>(w) / 8;
  double worst = 0;
  for (int i = 0; i < 8; ++i) {
    const double heading = i * kPi / 4;
    gh::WindReading r;
    r.heading = heading;
    r.felt_x = -15 * std::sin(heading), r.felt_z = -15 * std::cos(heading);
    r.figure = "15", r.unit = fh::words::kWindUnit;
    const ui::Rect at{step * static_cast<float>(i) + (step - z.w) / 2, 30, z.w, z.h};
    gh::draw(dl, f, st, g, r, at);
    ui::DrawList one;
    one.begin(w, h);
    gh::draw(one, f, st, g, r, at);
    const ui::Vec2 c{at.x + z.w / 2, at.y + f.px(g.size) / 2};
    worst = std::max(worst, std::fabs(std::fabs(last_polygon_angle(one, c)) - kPi));
    label(dl, f, st, c.x, at.bottom() + f.px(26),
          "FACING " + num::to_fixed(heading * 180 / kPi, 0));
  }
  struct Case {
    const char* name;
    double fx, fz, wx, wz;
  };
  const Case cases[] = {{"HEAD WIND", 0, -9, 0, -6},
                        {"CROSS GALE", 22, 0, 20, 0},
                        {"CALM", 0.2, 0, 4, 3},
                        {"TAIL WIND", 0, 4, 0, 12}};
  const float step2 = static_cast<float>(w) / 4;
  for (int i = 0; i < 4; ++i) {
    const Case& k = cases[i];
    gh::WindReading r;
    r.felt_x = k.fx, r.felt_z = k.fz;
    r.weather = true, r.weather_x = k.wx, r.weather_z = k.wz;
    r.figure = num::to_fixed(std::round(ge::core::hypot(k.fx, k.fz)), 0);
    r.unit = fh::words::kWindUnit;
    const ui::Rect at{step2 * static_cast<float>(i) + (step2 - z.w) / 2, 290, z.w, z.h};
    gh::draw(dl, f, st, g, r, at);
    label(dl, f, st, at.x + z.w / 2, at.bottom() + f.px(26), k.name);
  }
  lab.metric("wind.back_error_max", worst, "rad");
  if (worst > 1e-3)
    lab.fail("at speed in still air the arrow points " + num::to_fixed(worst, 4) +
                 " rad off straight back",
             "heading_up or draw(WindGauge) in engine/hud/instruments.cpp. Skill: ui.");
  lab.shot({"wind",
            "THE WIND METER, HEADING-UP",
            "Top row: a skier at 15 m/s in still air facing eight ways; the air he feels is his "
            "own speed, so the arrow points straight back every time. Bottom row: a head wind, "
            "a cross gale past 17 m/s (red), a calm (no arrow) and a tail wind he outruns, each "
            "with the weather's mark on the rim where its wind comes from.",
            "every top-row arrow pointing down; the gale red; the calm arrowless",
            {{"back error max", num::to_fixed(worst, 6)}, {"gale", num::to_fixed(g.gale, 1)}}},
           rendered(dl));
}

void compasses(ge::lab::Lab& lab, const gh::Style& st) {
  const int w = 1280, h = 300;
  const gh::Frame f = gh::frame_for(w, h, st);
  ui::DrawList dl;
  dl.begin(w, h);
  backdrop(dl, w, h);
  const gh::Compass c;
  const gh::Size z = gh::measure(f, c);
  const std::vector<gh::CompassMark> marks{{0.5, st.good}, {3.6, st.bad}};
  const double headings[] = {0, kPi / 4, 3.5};
  for (int i = 0; i < 3; ++i) {
    const ui::Rect at{(static_cast<float>(w) - z.w) / 2,
                      30 + static_cast<float>(i) * (z.h + f.px(44)), z.w, z.h};
    gh::draw(dl, f, st, c, {headings[i], marks, st.ink}, at);
    label(dl, f, st, at.x + z.w / 2, at.bottom() + f.px(24),
          "FACING " + num::to_fixed(headings[i] * 180 / kPi, 0));
  }
  lab.shot({"compass",
            "THE COMPASS STRIP",
            "Facing north, north-east and 200 degrees, with two marks: a green one at 29 degrees "
            "and a red one at 206. A mark off the strip waits at its edge.",
            "the heading under the notch; N, E, S, W where they belong; the marks on their "
            "bearings or at the edge",
            {}},
           rendered(dl));
}

// A free ride at speed with a hunt on and wind blowing: every element up but the prompt.
fh::HudSnapshot staged(const gh::Style& st, fh::HudRun& run) {
  fh::HudSnapshot s;
  s.t = 30;
  s.seed = 38, s.x = -1412, s.z = -2290;
  s.speed_kmh = 87, s.edge = 0.45;
  s.heading = 0.6;
  s.felt_x = -15, s.felt_z = -18;
  s.weather_x = 7, s.weather_z = -4;
  s.skied_m = 12480, s.run = "B12 BLACK", s.best_air_s = 1.92;
  fh::WantedTile w;
  w.stars = 2, w.survived = 154;
  s.wanted = w;
  run.news.push(fh::words::landed(1.92, 2.6), st.good, s.t - 2);
  run.news.push(fh::words::lift_top(252), st.ink, s.t - 0.5);
  return s;
}

void game(ge::lab::Lab& lab, const gh::Style& st) {
  fh::HudRun news;
  const fh::HudSnapshot snap = staged(st, news);
  fh::HudLayout hud = fh::hud_layout(st);
  ge::console::Console console;
  fh::add_hud_cvars(console, hud);
  struct View {
    int w, h;
    const char* style;
  };
  int problems_total = 0;
  for (const View v : {View{1280, 720, "dial"}, View{390, 844, "dial"}, View{844, 390, "dial"},
                       View{1280, 720, "number"}}) {
    console.execute(std::string("set hud_speedo_style ") + v.style);
    fh::fill_hud(hud, st, snap, news);
    const gh::Frame f = hud.layout.frame(v.w, v.h, st);
    ui::DrawList dl;
    dl.begin(v.w, v.h);
    backdrop(dl, v.w, v.h);
    const std::vector<gh::Placed> placed = hud.layout.draw(dl, f, st);
    const std::vector<std::string> problems = gh::layout_problems(f, placed);
    problems_total += static_cast<int>(problems.size());
    const std::string name = std::to_string(v.w) + "x" + std::to_string(v.h);
    for (const std::string& p : problems)
      lab.fail("Fall Line's HUD at " + name + " (" + v.style + "): " + p,
               "move or shrink the element in games/fall_line/hud/view.cpp (hud_layout); "
               "rerun `make lab LAB=hud-instruments`. Skill: ui.");
    lab.shot({std::string("game-") + v.style + "-" + name,
              "FALL LINE'S HUD WITH ITS INSTRUMENTS AT " + name,
              std::string("The free ride at 87 km/h, hunted, with the wind blowing: the ") +
                  (std::string(v.style) == "dial" ? "speedometer dial" : "speed as a figure") +
                  " bottom left over the edge bar, the wind meter top right under the hunt.",
              "nothing overlaps or leaves the screen; the wind's arrow back and to the left of "
              "him",
              {{"viewport", name},
               {"hud_speedo_style", v.style},
               {"problems", std::to_string(problems.size())}}},
             rendered(dl));
  }
  lab.metric("game.problems", problems_total, "count");
}

void run(ge::lab::Lab& lab) {
  const gh::Style st = fh::hud_style();
  const fh::HudLayout hud = fh::hud_layout(st);
  const gh::Dial& d = hud.layout[static_cast<size_t>(hud.speedo)].spec.dial;
  dials(lab, st, d);
  needle(lab, d);
  winds(lab, st);
  compasses(lab, st);
  game(lab, st);
}

const ge::lab::LabSpec kSpec{
    "hud-instruments",
    ge::lab::LabKind::picture,
    "game:fall_line",
    "engine/hud/instruments.hpp (dial, needle, wind meter, compass) under "
    "games/fall_line/hud/view.hpp",
    "ui",
    "Does each instrument point where its numbers say (a dial on its value, the felt wind back at "
    "a skier in still air), does the needle settle without overshoot, and does Fall Line's HUD "
    "with them lay out clean?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
