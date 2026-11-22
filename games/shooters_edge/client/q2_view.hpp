// QUAKE II'S VIEW AND 2D SETTINGS: what its own cvars change in the window while it is the game
// loaded (client/cvar_layers.cpp registers them in its layer; another game's window keeps the
// defaults below).
//
// THE FIELD OF VIEW is the one the server sends in the player's state each frame (the rules set it
// from the player's `fov` user info, 1 to 160; 90 under the fixed field of view deathmatch flag;
// or a mod's zoom), drawn between the last two frames. In a recording the player's own `fov`
// takes the place of a recorded one of 90 or more, so a recorded zoom still shows; `uf` 4 always
// shows the player's own, `uf` 32 always the recorded one. It is an angle across a 4:3 screen:
// `cl_adjustfov` 1 keeps that view's height on a wider screen (which sees more at the sides), 0
// keeps the angle across on the screen as it is.
//
// THE 2D (q2_bar.hpp): `scr_scale` (0 by the window's size), `scr_draw2d` (0 nothing, 1 no status
// bar, 2 everything, 3 no layouts), `scr_alpha` (every element's opacity but the crosshair's),
// and the lag graph `scr_lag_draw`, `scr_lag_x`, `scr_lag_y`, `scr_lag_min`, `scr_lag_max`.
// `cl_kickangles` 0 drops the server's kick on the view (a cheat: only while sv_cheats is 1).
//
// ge:summary Quake II's view and 2D settings: the server's field of view drawn, scale, HUD switches
// ge:keywords q2 fov cl_adjustfov uf hor-plus field-of-view zoom scr_scale scr_draw2d scr_alpha
// ge:keywords scr_lag_draw scr_lag_x scr_lag_y scr_lag_min scr_lag_max cl_kickangles q2_shown_fov
// ge:keywords gl_bilerp_pics gl_gamma_scale_pics
// ge:skill shooter ui camera
// ge:test tests/shooters_edge_q2_view_test.cpp
#pragma once

#include <string>

#include "console/console.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

struct Q2Screen {
  int scale = 0;        // scr_scale: 0 by the window's size (q2_bar_scale), else 1..10
  int draw2d = 2;       // scr_draw2d: 0 nothing, 1 all but the status bar, 2 all, 3 no layouts
  double alpha = 1;     // scr_alpha: every element's opacity but the crosshair's
  int lag_draw = 0;     // scr_lag_draw: 0 off, 1 the lag graph, 2 on a dark box
  int turtle = 1;       // scr_showturtle: 0 off, 1 a frame's troubles, 2 also the packets held back
  int font_shadow = 0;  // gl_fontshadow: the letters' shadow, units down and right (0 none)
  bool bilerp_chars = false;  // gl_bilerp_chars: the letters smoothed
  // gl_bilerp_pics: the status bar's and the layouts' pictures smoothed when drawn larger: 0 none
  // (every other game's), 1 those of 64 x 64 and up (Quake II's default), 2 all of them.
  int bilerp_pics = 0;
  // gl_gamma_scale_pics: those pictures brightened by `gamma` (vid_gamma) as the world is.
  bool gamma_pics = false;
  int lag_x = -1, lag_y = -1;  // scr_lag_x, scr_lag_y: its corner; negative from the far edge
  double lag_min = 0, lag_max = 200;  // scr_lag_min, scr_lag_max: its bottom and top, ms
  int adjust_fov = 1;                 // cl_adjustfov: 1 the 4:3 view's height kept, 0 its width
  std::string uf;                     // uf: the user info flags (4 own fov, 32 recorded fov)
  bool kick_angles = true;            // cl_kickangles: the server's kick on the view
  // The graph along the bottom (hud/bottom_graph.hpp): timegraph each frame's time in ms,
  // debuggraph the graph drawn though nothing feeds it; graphheight, graphscale, graphshift.
  bool time_graph = false, debug_graph = false;
  int graph_height = 32;
  double graph_scale = 1, graph_shift = 0;
};

// The field of view shown, degrees: the server's `sent` (0: none sent, the player's `own`); in a
// `recording` the player's own in place of one of 90 or more, as `uf_flags` asks. `own` under 1
// is 90 and over 160 is 160.
double q2_shown_fov(double sent, double own, bool recording, int uf_flags);
// The vertical angle (rad) of an angle across of `fov_deg`: at 4:3 (`adjust`) or at `aspect`.
double q2_fov_y(double fov_deg, bool adjust, double aspect);
// The window's field of view (rad, vertical) for the recording or joined game `ds` on a `width` x
// `height` window: Quake II's by the rules above (`recording` false: a joined server's own); any
// other game's the camera's default.
double q2_demo_fov_y(const DemoState& ds, double own, bool recording, const Q2Screen& s, int width,
                     int height);

// Quake II's view and 2D cvars, bound to `s`, in its layer.
void add_q2_screen_cvars(ge::console::Cvars& v, Q2Screen& s);

}  // namespace shooters_edge::client
