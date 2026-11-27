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
// ge:keywords scr_lag_draw scr_lag_x scr_lag_y scr_lag_min scr_lag_max cl_kickangles q2_shown_fov q2_own_fov
// ge:keywords gl_bilerp_pics gl_gamma_scale_pics scr_demobar
// ge:skill shooter ui camera
// ge:test tests/shooters_edge_q2_view_test.cpp
#pragma once

#include <string>

#include "console/console.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/screen_2d.hpp"

namespace shooters_edge::client {

// The 2D switch and the graph (scr_draw2d, timegraph...) are every game's: Screen2D, registered in
// the classic layer, which a game change keeps.
struct Q2Screen : Screen2D {
  int scale = 0;        // scr_scale: 0 by the window's size (q2_bar_scale), else 1..10
  double alpha = 1;     // scr_alpha: every element's opacity but the crosshair's
  int lag_draw = 0;     // scr_lag_draw: 0 off, 1 the lag graph, 2 on a dark box
  int turtle = 1;       // scr_showturtle: 0 off, 1 a frame's troubles, 2 also the packets held back
  int font_shadow = 0;  // gl_fontshadow: the letters' shadow, units down and right (0 none)
  bool bilerp_chars = false;  // gl_bilerp_chars: the letters smoothed
  // scr_font: the letters' picture of the status bar's and the layouts' text and the inventory
  // (pics/<name>, .pcx when it has no extension); one missing falls back to conchars.
  std::string font = "conchars";
  // viewsize: the 3D view's share of the screen, 40..100 percent, centred; the border round it
  // tiled with pics/backtile (the bar's `draw_q2_bar`).
  int viewsize = 100;
  // scr_demobar: the play bar while a demo plays (demo_playback.cpp reads it): 0 none, 1 always
  // with the percentage played (the default), 2 also the clock. -1, every other game's: `demo_bar`.
  int demo_bar = -1;
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
};

// viewsize's rectangle of a `width` x `height` screen: `percent` of each, centred (100 and over:
// all of it). The world is drawn in it and the border tiled round it.
struct Q2ViewRect {
  int x = 0, y = 0, w = 0, h = 0;
};
Q2ViewRect q2_view_rect(int width, int height, int percent);

// The player's own field of view, degrees: `cl_fov` under 1 (0, the rig's own) is 90, over 160 is
// 160. The rules are told this, never the 0, which they would hold to one degree.
double q2_own_fov(double own);

// The field of view shown, degrees: the server's `sent` (0: none sent, the player's own,
// q2_own_fov); in a `recording` the player's own in place of one of 90 or more, as `uf_flags` asks.
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
