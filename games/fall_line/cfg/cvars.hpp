// FALL LINE'S CVARS: every setting the game lets a cfg change, and the struct each writes into.
// The app execs its cfgs (console/cfg.hpp's startup order) into a `Settings` before it opens the
// window and makes the frame, then reads the struct; the play executable registers the same cvars
// so `ge cvar` and `ge cfg` answer for the game without a screen. Everything else Fall Line reads
// stays a constant in code (docs/console.md § Tiers).
//
//   vid_width vid_height vid_vsync      the window (A, latched: read at startup)
//   r_taa r_scale r_occlusion r_shadowsize r_bloom r_snow r_trails r_spray r_snowfall r_weather
//                                       the picture (A, latched: the frame is made once)
//   cl_view                             the camera: a rung or a still view (A)
//   cl_chasedist cl_chaseheight cl_fovspeed cl_camblend cl_motion cl_shake, and the first
//   person's (cl_fov cl_viewheight cl_bob* cl_roll* v_kick* v_idlescale cl_landdip, read by no
//   rung of Fall Line's)                 the camera's settings (engine/camera/cvars.hpp, A)
//   volume s_wind s_effects s_mute      the sound's faders (A)
//   sv_seed sv_hour                     the run's map and hour (the server's, server info)
//
// ge:summary Fall Line's cvars: the window, the picture, the camera, the sound, the map
// ge:keywords fall-line cvars settings cfg vid_width r_taa r_bloom r_shadowsize cl_view volume
// ge:keywords s_mute sv_seed sv_hour add_cvars register_cvars cl_chasedist cl_motion
// ge:skill console sim
// ge:test tests/fall_line_cvars_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "camera/cvars.hpp"
#include "console/console.hpp"

namespace fall_line::cfg {

struct Settings {
  // The window.
  int32_t width = 1280, height = 720;
  bool vsync = true;
  // The picture: app/frame.hpp's FrameOptions, the player's half of it.
  bool taa = true;
  double scale = 1.0;
  bool occlusion = true;
  uint32_t shadow_size = 2048;
  double bloom = 0.04;
  bool snow = true, trails = true, spray = true, snowfall = true, weather = true;
  // The camera.
  std::string view = "chase";
  ge::camera::CameraCvars camera;  // the camera's settings, in metres (no cl_camera: cl_view)
  // The sound: sound/mix.hpp's Faders.
  float volume = 0.8f, wind = 1, effects = 1;
  bool mute = false;
  // The run.
  double seed = 38;
  double hour = -1;  // the map's own start hour when negative
};

// Every cvar above, bound to `s`'s fields (which take the defaults now).
void add_cvars(ge::console::Console& c, Settings& s);

// The play contract's hook (engine/play/game.hpp): the cvars on a Settings the console keeps.
void register_cvars(ge::console::Console& c);

}  // namespace fall_line::cfg
