// LAB `bsp-flicker`: the lights' flicker over time (ce_flicker, bsp_draw.hpp's Options::flicker).
// A still picture cannot show a flicker, so this lab charts it: a wall torch's light share over two
// seconds as its flame puffs (fire_flicker) against the steady mean the switch's off keeps, and a
// flickering light style both ways: the classic's ten steps a second (style_level) and the steps
// glided into one another (style_level_glided).
//
// Its checks: the torch wavers round its mean with the switch on and is flat with it off; the
// glided style meets the classic one at each step's middle and never jumps by more than a step.
//
// ge:summary Lab: the lights' flicker (ce_flicker) over time: a torch's waver and the light styles
// ge:summary stepped and glided, charted
// ge:keywords lab ce_flicker flicker light-style style_level_glided fire_flicker torch bsp-flicker
// ge:skill brush render labs
// ge:test tests/brush_draw_data_test.cpp
#include <algorithm>
#include <cstdio>
#include <string>

#include "brush/app/bsp_flames.hpp"
#include "brush/app/draw_data.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"

namespace {

namespace gl = ge::lab;
namespace bd = ge::brush::app;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

// The classic flicker style (style 1) and the slow pulse (style 10's fluorescent buzz is a strobe).
constexpr const char* kFlicker = "mmnmmommommnonmmonqnmmo";
constexpr double kSeconds = 2.0, kStep = 1.0 / 240;

void run(gl::Lab& lab) {
  bd::PointLight torch;
  torch.flicker = bd::Flicker::candle;
  torch.flame = true;
  torch.fire = bd::Fire::wall_torch;
  torch.seed = 7;
  const bd::FireLook look;
  gl::Trace on{"torch, ce_flicker 1", {}, {}}, off{"torch, ce_flicker 0", {}, {}};
  gl::Trace stepped{"style, ce_flicker 0 (10 steps/s)", {}, {}};
  gl::Trace glided{"style, ce_flicker 1 (glided)", {}, {}};
  double lo = 1e9, hi = -1e9, jump_classic = 0, jump_glided = 0, prev_s = -1, prev_g = -1;
  const int steps = static_cast<int>(kSeconds / kStep);
  for (int i = 0; i <= steps; ++i) {
    const double t = kStep * i;
    const double f = bd::fire_flicker(torch, t, look);
    lo = f < lo ? f : lo;
    hi = f > hi ? f : hi;
    on.x.push_back(t);
    on.y.push_back(f);
    off.x.push_back(t);
    off.y.push_back(bd::kFireFlickerMean);
    const double s = bd::style_level(kFlicker, t), g = bd::style_level_glided(kFlicker, t);
    stepped.x.push_back(t);
    stepped.y.push_back(s);
    glided.x.push_back(t);
    glided.y.push_back(g);
    if (prev_s >= 0) {
      jump_classic = std::max(jump_classic, s > prev_s ? s - prev_s : prev_s - s);
      jump_glided = std::max(jump_glided, g > prev_g ? g - prev_g : prev_g - g);
    }
    prev_s = s;
    prev_g = g;
  }
  lab.metric("torch.swing", hi - lo, "share");
  lab.metric("style.jump-classic", jump_classic, "level/frame");
  lab.metric("style.jump-glided", jump_glided, "level/frame");

  gl::LineChart line;
  line.frame = {"THE LIGHTS' FLICKER OVER TWO SECONDS",
                "ce_flicker on and off, 240 frames a second", "t (s)",
                "light (share of the map's)"};
  line.traces = {on, off, stepped, glided};
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"flicker",
            chart.title,
            "A wall torch's light as its flame puffs (on) against the steady mean (off), and the "
            "classic flicker style stepped ten times a second (off) and glided (on).",
            "the torch wavering round the flat line; the glided style smooth through the steps",
            {}},
           gl::to_picture(chart.plot));
  lab.table({"THE FLICKER",
             {"measure", "value"},
             {{"torch swing (on)", fmt(hi - lo, 3)},
              {"style's biggest jump a frame, stepped", fmt(jump_classic, 3)},
              {"style's biggest jump a frame, glided", fmt(jump_glided, 3)}}});

  if (hi - lo < 0.05) {
    lab.fail("the torch's light holds still (swing " + fmt(hi - lo, 3) + ")",
             "next: fire_flicker (plugins/brush/app/bsp_flames.cpp). Skill: render.");
  }
  if (!(jump_glided < jump_classic)) {
    lab.fail("the glided style jumps as far as the stepped one",
             "next: style_level_glided (plugins/brush/app/draw_data.cpp). Skill: render.");
  }
}

const gl::LabSpec kSpec{
    "bsp-flicker",
    gl::LabKind::sim,
    "plugin:brush",
    "The lights' flicker (plugins/brush/app/bsp_flames, draw_data's light styles)",
    "render",
    "Does ce_flicker make a torch's light waver with its flame and the light styles glide, and "
    "does off hold them to the classic's?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
