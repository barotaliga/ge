// Quake II's view and 2D settings (q2_view.hpp).
//
// ge:summary Quake II's field of view as shown and its view and 2D cvars
// ge:keywords q2 fov cl_adjustfov uf scr_scale scr_draw2d scr_alpha scr_lag_draw cl_kickangles
// ge:keywords timegraph debuggraph graphheight graphscale graphshift
// ge:skill shooter ui camera
// ge:test tests/shooters_edge_q2_view_test.cpp
#include "shooters_edge/client/q2_view.hpp"

#include <algorithm>
#include <cstdlib>
#include <tuple>

#include "camera/first_person.hpp"
#include "camera/pose.hpp"

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180;
constexpr double kClassicAspect = 4.0 / 3.0;
constexpr int kOwnFov = 4;        // uf: the player's own field of view in a recording
constexpr int kRecordedFov = 32;  // uf: the recorded one, always

}  // namespace

double q2_shown_fov(double sent, double own, bool recording, int uf_flags) {
  const double mine = own < 1 ? 90 : std::min(own, 160.0);
  if (sent <= 0) return mine;
  if (!recording) return sent;
  if ((uf_flags & kOwnFov) != 0) return mine;
  if ((uf_flags & kRecordedFov) != 0) return sent;
  return sent >= 90 ? mine : sent;
}

double q2_fov_y(double fov_deg, bool adjust, double aspect) {
  return ge::camera::fov_y_of(fov_deg * kDeg, adjust || !(aspect > 0) ? kClassicAspect : aspect);
}

double q2_demo_fov_y(const DemoState& ds, double own, bool recording, const Q2Screen& s, int width,
                     int height) {
  if (ds.format != DemoFormat::q2) return ge::camera::FirstPersonRow{}.fov_y;
  const int uf = static_cast<int>(std::strtol(s.uf.c_str(), nullptr, 10));
  const double aspect = height > 0 ? static_cast<double>(width) / height : kClassicAspect;
  return q2_fov_y(q2_shown_fov(ds.fov, own, recording, uf), s.adjust_fov != 0, aspect);
}

Q2ViewRect q2_view_rect(int width, int height, int percent) {
  if (percent >= 100) return {0, 0, width, height};
  const int p = std::max(percent, 1);
  const int w = width * p / 100, h = height * p / 100;
  return {(width - w) / 2, (height - h) / 2, w, h};
}

void add_q2_screen_cvars(con::Cvars& v, Q2Screen& s) {
  v.bind(
      {.name = "scr_scale",
       .fallback = "0",
       .flags = con::kArchive,
       .help = "How many times larger the status bar, the scoreboard and the messages are drawn.",
       .kind = con::CvarKind::integer,
       .min = 0,
       .max = 10,
       .values = "0: by the window's size (the default); 1..10: that many times"},
      &s.scale);
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
  v.bind({.name = "scr_alpha",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "How solid everything drawn over the view is, but the crosshair.",
          .min = 0,
          .max = 1,
          .values = "0..1: 1 solid (the default); 0.5 half see-through"},
         &s.alpha);
  v.bind({.name = "scr_lag_draw",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "A small graph of the server's last packets: how long each took, and the lost.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: none (the default); 1: the graph; 2: the graph on a dark box"},
         &s.lag_draw);
  v.bind({.name = "viewsize",
          .fallback = "100",
          .flags = con::kArchive,
          .help = "How much of the screen the view of the world fills, the border round it "
                  "tiled.",
          .kind = con::CvarKind::integer,
          .min = 40,
          .max = 100,
          .values = "40..100: that share of the screen's width and height, centred (100 the "
                    "default: all of it)",
          .overlap = con::Overlap::own},
         &s.viewsize);
  v.bind({.name = "scr_font",
          .fallback = "conchars",
          .help = "The picture of letters the status bar, the scoreboard and the inventory are "
                  "written in.",
          .kind = con::CvarKind::text,
          .values = "NAME: pics/NAME.pcx, a sheet of 16 by 16 letters (conchars the default)"},
         &s.font);
  v.bind({.name = "gl_fontshadow",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "A black shadow under the screen's letters, this many units down and right.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: none (the default); 1..2: that far"},
         &s.font_shadow);
  v.bind({.name = "gl_bilerp_chars",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Whether the screen's letters are smoothed when drawn larger.",
          .kind = con::CvarKind::boolean,
          .values = "0: sharp pixels (the default); 1: smoothed"},
         &s.bilerp_chars);
  v.bind({.name = "gl_bilerp_pics",
          .fallback = "1",
          .help = "Whether the status bar's pictures are smoothed when drawn larger.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "1: the large ones, the small icons and digits sharp (the default); 0: none, "
                    "sharp pixels; 2: all of them"},
         &s.bilerp_pics);
  v.bind({.name = "gl_gamma_scale_pics",
          .fallback = "0",
          .help = "Whether the brightness setting brightens the status bar's pictures too.",
          .kind = con::CvarKind::boolean,
          .values = "0: they keep their own brightness (the default); 1: brightened with the "
                    "world"},
         &s.gamma_pics);
  v.bind({.name = "scr_showturtle",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Words at the lower left when a frame from the server came with a problem.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "1: the frame's own problems (the default); 2: also the packets held back; "
                    "0: none"},
         &s.turtle);
  for (const auto& [name, field, across] :
       {std::tuple{"scr_lag_x", &s.lag_x, true}, std::tuple{"scr_lag_y", &s.lag_y, false}})
    v.bind({.name = name,
            .fallback = "-1",
            .flags = con::kArchive,
            .help = across ? "Where the lag graph stands across the screen."
                           : "Where the lag graph stands down the screen.",
            .kind = con::CvarKind::integer,
            .min = -10000,
            .max = 10000,
            .values = across ? "-1: at the right edge (the default); 0 and up: from the left; "
                               "below -1: further in from the right"
                             : "-1: at the bottom (the default); 0 and up: from the top; below "
                               "-1: further up from the bottom"},
           field);
  v.bind({.name = "scr_lag_min",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "The time a packet took at the bottom of the lag graph, in milliseconds.",
          .min = 0,
          .max = 10000,
          .values = "0 and up: 0 the default"},
         &s.lag_min);
  v.bind({.name = "scr_lag_max",
          .fallback = "200",
          .flags = con::kArchive,
          .help = "The time a packet took at the top of the lag graph, in milliseconds.",
          .min = 1,
          .max = 10000,
          .values = "1 and up: 200 the default"},
         &s.lag_max);
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
  v.bind(
      {.name = "cl_adjustfov",
       .fallback = "1",
       .flags = con::kArchive,
       .help = "Whether a wide screen keeps the view's height and sees more at the sides.",
       .kind = con::CvarKind::integer,
       .min = 0,
       .max = 1,
       .values = "1: the same height, more at the sides (the default); 0: the same angle across"},
      &s.adjust_fov);
  v.bind(
      {.name = "uf",
       .fallback = "",
       .flags = con::kArchive,
       .help = "Flags: 4 shows your own field of view in recordings, 32 always the recorded one.",
       .kind = con::CvarKind::text,
       .values = "\"\": your own unless the recording zooms (the default); 4: always your "
                 "own; 32: always the recorded one"},
      &s.uf);
  v.bind({.name = "cl_kickangles",
          .fallback = "1",
          .flags = con::kArchive | con::kCheat,
          .help = "Whether the view is kicked when you fire or are hit.",
          .kind = con::CvarKind::boolean,
          .values = "1: kicked (the default); 0: steady, only while the server allows cheats"},
         &s.kick_angles);
}

}  // namespace shooters_edge::client
