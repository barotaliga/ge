// FALL LINE'S CVARS: every setting the game lets a cfg change, and the struct each writes into.
// The app execs its cfgs (console/cfg.hpp's startup order) into a `Settings` before it opens the
// window and makes the frame, then reads the struct; the play executable registers the same cvars
// so `ge cvar` and `ge cfg` answer for the game without a screen. Everything else Fall Line reads
// stays a constant in code (docs/console.md § Tiers).
//
//   vid_width vid_height vid_vsync      the window (A, latched: read at startup)
//   r_taa r_scale r_occlusion r_shadowsize r_bloom r_snow r_trails r_spray r_precip
//   r_precip_density r_splashes r_wet r_weather r_clouds
//                                       the picture (A, latched: the frame is made once)
//   cl_view                             the camera: a rung or a still view (A)
//   cl_chasedist cl_chaseheight cl_fovspeed cl_camblend cl_motion cl_shake, and the first
//   person's (cl_fov cl_viewheight cl_bob* cl_roll* v_kick* v_idlescale cl_landdip, read by no
//   rung of Fall Line's)                 the camera's settings (engine/camera/cvars.hpp, A)
//   volume s_wind s_effects s_mute      the sound's faders (A)
//   sv_seed sv_hour                     the run's map and hour (the server's, server info)
//   sv_weather sv_snowfall sv_snowline sv_fog sv_wind sv_winddir sv_cover sv_thick sv_skyrate
//                                       the sky over the run, picked by hand, and how fast its
//                                       hour runs (the server's, server info; sim/sky.hpp)
//   sv_skytype sv_skyair sv_skydust sv_sunsize sv_sun2 sv_planet sv_planetdir sv_planetel
//   sv_ring sv_nebula sv_skystars sv_skyglow
//                                       the kind of sky (render/sky_style.hpp's types) and its
//                                       knobs (the server's, server info; app/sky_type.hpp)
//   sv_lethal sv_armyfire sv_helis sv_paras
//                                       the hunt: whether the army shoots to kill, fires at all,
//                                       and sends its helicopters and paramotors (the server's)
//
// ge:summary Fall Line's cvars: the window, the picture, the camera, the sound, the map, the hunt
// ge:keywords fall-line cvars settings cfg vid_width r_taa r_bloom r_shadowsize cl_view volume
// ge:keywords s_mute sv_seed sv_hour add_cvars register_cvars sv_weather sv_snowfall sv_fog
// ge:keywords sv_wind sv_winddir sv_cover sv_thick sv_skyrate r_clouds sv_skytype sky-type
// ge:keywords sky-knobs cl_chasedist cl_motion r_precip r_precip_density r_splashes r_wet
// ge:keywords sv_snowline rain
// ge:keywords sv_lethal sv_armyfire sv_helis sv_paras hunt army
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
  bool snow = true, trails = true, spray = true, weather = true;
  bool precip = true, splashes = true, wet = true;  // what falls, the rain's splashes, wet ground
  double precip_density = 1.0;                      // the share of the falling particles, 0.25..2
  double clouds = 1.0;  // the share of the cloud march's steps; 0: no layer drawn
  // The camera.
  std::string view = "chase";
  ge::camera::CameraCvars camera;  // the camera's settings, in metres (no cl_camera: cl_view)
  // The sound: sound/mix.hpp's Faders.
  float volume = 0.8f, wind = 1, effects = 1;
  bool mute = false;
  // The run.
  double seed = 38;
  double hour = -1;  // the map's own start hour when negative
  // The sky over the run (sim/sky.hpp): a weather word or "map", then each kept where negative.
  std::string sky = "map";
  double fall = -1, fog = -1;
  double snowline = -1;  // the snow line, a share of the mountain's height (rain under it)
  double air = -1, air_from = -1;  // the mean wind, m/s at 10 m; the heading it blows from, deg
  double cover = -1;               // the broken sky's cloud cover, 0..1
  double thick = -1;               // how thick the clouds are, 0 thin .. 1 thick
  double sky_rate = 0;             // the sky's hours per hour of the run (0: the hour stands)
  // The kind of sky (render/sky_style.hpp's type names) and its knobs, each kept where negative
  // (render/sky_style.hpp's SkyKnobs; angles in degrees).
  std::string cloud_kinds = "map";  // sv_clouds: the kinds of cloud drawn (map: the map's)
  std::string sky_type = "earth";
  double sky_air = -1, sky_dust = -1, sun_size = -1, sun2 = -1;
  double planet = -1, planet_dir = -1, planet_el = -1, ring = -1;
  double nebula = -1, sky_stars = -1, sky_glow = -1;
  // The hunt (sim/hunt_cfg.hpp hands them to a run's wanted level and army).
  bool lethal = true;  // the army shoots to kill; off, it chases him down and arrests him
  bool army_fire = true;
  bool helis = true;
  bool paras = true;
};

// Every cvar above, bound to `s`'s fields (which take the defaults now).
void add_cvars(ge::console::Console& c, Settings& s);

// The play contract's hook (engine/play/game.hpp): the cvars on a Settings the console keeps.
void register_cvars(ge::console::Console& c);

}  // namespace fall_line::cfg
