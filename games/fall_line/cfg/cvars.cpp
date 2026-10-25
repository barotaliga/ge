// Fall Line's cvars, registered: each name, default, range, flags and help.
//
// ge:summary Fall Line's cvar table: names, defaults, ranges, flags and help, bound to Settings
// ge:keywords fall-line cvars add_cvars register_cvars
// ge:skill console sim
// ge:test tests/fall_line_cvars_test.cpp
#include "fall_line/cfg/cvars.hpp"

#include <memory>

namespace fall_line::cfg {

namespace con = ge::console;

void add_cvars(con::Console& c, Settings& s) {
  using con::CvarKind;
  constexpr uint32_t kPlayer = con::kArchive;
  constexpr uint32_t kStartup = con::kArchive | con::kLatch;
  constexpr uint32_t kRun = con::kServer | con::kServerInfo;
  con::Cvars& v = c.cvars();

  v.bind({.name = "vid_width",
          .fallback = "1280",
          .flags = kStartup,
          .help = "the window's width, logical pixels",
          .kind = CvarKind::integer,
          .min = 320,
          .max = 7680},
         &s.width);
  v.bind({.name = "vid_height",
          .fallback = "720",
          .flags = kStartup,
          .help = "the window's height, logical pixels",
          .kind = CvarKind::integer,
          .min = 240,
          .max = 4320},
         &s.height);
  v.bind({.name = "vid_vsync",
          .fallback = "1",
          .flags = kStartup,
          .help = "wait for the display's refresh before showing a frame",
          .kind = CvarKind::boolean},
         &s.vsync);

  v.bind({.name = "r_taa",
          .fallback = "1",
          .flags = kStartup,
          .help = "temporal anti-aliasing and upscaling (off: no jitter, no resolve)",
          .kind = CvarKind::boolean},
         &s.taa);
  v.bind({.name = "r_scale",
          .fallback = "1",
          .flags = kStartup,
          .help = "the render size over the window's, with r_taa (0.5: a quarter of the pixels)",
          .min = 0.25,
          .max = 2},
         &s.scale);
  v.bind({.name = "r_occlusion",
          .fallback = "1",
          .flags = kStartup,
          .help = "cull trees and figures hidden behind the terrain (off: the frustum only)",
          .kind = CvarKind::boolean},
         &s.occlusion);
  v.bind({.name = "r_shadowsize",
          .fallback = "2048",
          .flags = kStartup,
          .help = "the sun's shadow maps' texels a side",
          .kind = CvarKind::integer,
          .min = 256,
          .max = 8192},
         &s.shadow_size);
  v.bind({.name = "r_bloom",
          .fallback = "0.04",
          .flags = kStartup,
          .help = "the share of the glow round bright light in the picture; 0: none",
          .min = 0,
          .max = 1},
         &s.bloom);
  v.bind({.name = "r_snow",
          .fallback = "1",
          .flags = kStartup,
          .help = "the snow's material: sparkle, sheen and grooming (off: plain lit white)",
          .kind = CvarKind::boolean},
         &s.snow);
  v.bind({.name = "r_trails",
          .fallback = "1",
          .flags = kStartup,
          .help = "the tracks skis cut in the snow (needs r_snow)",
          .kind = CvarKind::boolean},
         &s.trails);
  v.bind({.name = "r_spray",
          .fallback = "1",
          .flags = kStartup,
          .help = "the snow the skis throw",
          .kind = CvarKind::boolean},
         &s.spray);
  v.bind({.name = "r_snowfall",
          .fallback = "1",
          .flags = kStartup,
          .help = "the falling snow when the sky snows",
          .kind = CvarKind::boolean},
         &s.snowfall);
  v.bind({.name = "r_weather",
          .fallback = "1",
          .flags = kStartup,
          .help = "the map's own sky (off: always clear)",
          .kind = CvarKind::boolean},
         &s.weather);
  v.bind({.name = "r_clouds",
          .fallback = "1",
          .flags = kStartup,
          .help = "the share of the cloud march's steps, 0.25 .. 1 (0: no cloud layer drawn, the "
                  "sky keeps its mean cloud)",
          .min = 0,
          .max = 1},
         &s.clouds);

  // The lens and the film (render/post.hpp, render/lens.hpp) and the grade's knobs
  // (render/grade.hpp): each strength 0..1, 0 compiling it out of the frame.
  const auto strength = [&](const char* name, const char* fallback, const char* help,
                            double* field) {
    v.bind(
        {.name = name, .fallback = fallback, .flags = kStartup, .help = help, .min = 0, .max = 1},
        field);
  };
  strength("r_sharpen", "0.3", "contrast-adaptive sharpening of the upscaled frame, 0..1 (0: off)",
           &s.sharpen);
  strength("r_sunrays", "0.5",
           "the sun streaming through trees and over ridges in front of it, 0..1 (0: off)",
           &s.sunrays);
  strength("r_flare", "0", "the lens flare's ghosts and ring round the sun, 0..1 (0: off)",
           &s.flare);
  strength("r_vignette", "0.25", "the picture's corners darker, 0..1 (0: off)", &s.vignette);
  strength("r_chromatic", "0", "the lens's colour fringes toward the corners, 0..1 (0: off)",
           &s.chromatic);
  strength("r_grain", "0", "film grain, 0..1 (0: off)", &s.grain);
  strength("r_lensdirt", "0", "dirt on the lens, lit by the sun and the glow, 0..1 (0: off)",
           &s.lensdirt);
  const auto look = [&](const char* name, const char* fallback, const char* help, double lo,
                        double hi, double* field) {
    v.bind(
        {.name = name, .fallback = fallback, .flags = kStartup, .help = help, .min = lo, .max = hi},
        field);
  };
  look("r_contrast", "1", "the picture's contrast (1: as graded; above 1 steeper)", 0.5, 2,
       &s.contrast);
  look("r_saturation", "1", "the picture's colour (0: grey, 1: as graded, 2: doubled)", 0, 2,
       &s.saturation);
  look("r_temperature", "0", "the white balance, -1 cold .. 1 warm", -1, 1, &s.temperature);
  look("r_tint", "0", "the white balance's tint, -1 green .. 1 magenta", -1, 1, &s.tint);
  look("r_gamma", "1", "the middle tones' brightness (above 1 brighter)", 0.5, 2, &s.gamma);

  v.bind({.name = "cl_view",
          .fallback = "chase",
          .flags = kPlayer,
          .help = "the camera: a rung (tips helmet chase far high orbit; on a machine its own: "
                  "seat pilot wing down) or a still view (start piste lookback vista aerial)",
          .kind = CvarKind::text},
         &s.view);

  // The camera's settings at the engine's defaults: a world in metres, the ladders' own rows.
  ge::camera::add_camera_cvars(v, s.camera, {});

  v.bind({.name = "volume",
          .fallback = "0.8",
          .flags = kPlayer,
          .help = "the master volume, 0 silent .. 1 full",
          .min = 0,
          .max = 1},
         &s.volume);
  v.bind({.name = "s_wind",
          .fallback = "1",
          .flags = kPlayer,
          .help = "the wind and the snow's hiss under the skis, 0 .. 1",
          .min = 0,
          .max = 1},
         &s.wind);
  v.bind({.name = "s_effects",
          .fallback = "1",
          .flags = kPlayer,
          .help = "every other sound: edges, landings, gates, crashes, 0 .. 1",
          .min = 0,
          .max = 1},
         &s.effects);
  v.bind({.name = "s_mute",
          .fallback = "0",
          .flags = kPlayer,
          .help = "no sound at all, and no audio device opened",
          .kind = CvarKind::boolean},
         &s.mute);

  v.bind({.name = "sv_seed",
          .fallback = "38",
          .flags = kRun,
          .help = "the seed the mountain is generated from"},
         &s.seed);
  v.bind({.name = "sv_hour",
          .fallback = "-1",
          .flags = kRun,
          .help = "the hour of the day the run starts at, 0 .. 24 (-1: the map's own)",
          .min = -1,
          .max = 24},
         &s.hour);
  v.bind({.name = "sv_skyrate",
          .fallback = "0",
          .flags = kRun,
          .help = "the sky's hours per hour of the run: 1 real time, 60 an hour a minute (0: the "
                  "hour stands)",
          .min = 0,
          .max = 3600},
         &s.sky_rate);
  v.bind({.name = "sv_weather",
          .fallback = "map",
          .flags = kRun,
          .help = "the sky: clear fair flurries high overcast snow storm fog (map: the map's own)",
          .kind = CvarKind::text},
         &s.sky);
  v.bind({.name = "sv_snowfall",
          .fallback = "-1",
          .flags = kRun,
          .help = "how hard it snows, 0 none .. 1 a blizzard, on a sky that snows (-1: the sky's)",
          .min = -1,
          .max = 1},
         &s.fall);
  v.bind({.name = "sv_fog",
          .fallback = "-1",
          .flags = kRun,
          .help = "how thick the valley fog lies, 0 .. 1, under a fog (-1: the sky's)",
          .min = -1,
          .max = 1},
         &s.fog);
  v.bind({.name = "sv_wind",
          .fallback = "-1",
          .flags = kRun,
          .help = "the mean wind at 10 m, m/s (-1: the sky's)",
          .min = -1,
          .max = 40},
         &s.air);
  v.bind({.name = "sv_winddir",
          .fallback = "-1",
          .flags = kRun,
          .help = "the heading the wind blows from, degrees clockwise from the map's +z (-1: the "
                  "map's)",
          .min = -1,
          .max = 360},
         &s.air_from);
  v.bind({.name = "sv_cover",
          .fallback = "-1",
          .flags = kRun,
          .help = "a broken sky's cloud cover (fair, flurries, high), 0 .. 1 (-1: the map's)",
          .min = -1,
          .max = 1},
         &s.cover);
  v.bind({.name = "sv_clouds",
          .fallback = "map",
          .flags = kRun,
          .help = "the kinds of cloud drawn, lowest first, joined by '+' (cumulus+cirrus): "
                  "inversion stratus stratocumulus cumulus congestus cumulonimbus nimbostratus "
                  "altocumulus altostratus lenticular cirrocumulus veil cirrostratus cirrus wisps "
                  "(map: the map's own)",
          .kind = CvarKind::text},
         &s.cloud_kinds);
  v.bind({.name = "sv_skytype",
          .fallback = "earth",
          .flags = kRun,
          .help = "the kind of sky: earth dust alien hell void nebula violet ice ash toxic",
          .kind = CvarKind::text},
         &s.sky_type);
  // The sky type's knobs (render/sky_style.hpp's SkyKnobs): -1 keeps the type's own.
  const auto knob = [&](const char* name, const char* help, double max, double* field) {
    v.bind({.name = name, .fallback = "-1", .flags = kRun, .help = help, .min = -1, .max = max},
           field);
  };
  knob("sv_skyair", "the sky type's air, scaled: 0 none, 1 its own (-1: its own)", 4, &s.sky_air);
  knob("sv_skydust", "the air's dust against the Earth's: 0 none (-1: the type's)", 200,
       &s.sky_dust);
  knob("sv_sunsize", "the sun's disc against the real one's (-1: the type's)", 10, &s.sun_size);
  knob("sv_sun2", "a second sun's brightness against the sun's: 0 none (-1: the type's)", 2,
       &s.sun2);
  knob("sv_planet", "a planet's angular radius, degrees: 0 none (-1: the type's)", 60, &s.planet);
  knob("sv_planetdir", "the planet's heading, degrees (-1: the type's)", 360, &s.planet_dir);
  knob("sv_planetel", "the planet's elevation, degrees (-1: the type's)", 90, &s.planet_el);
  knob("sv_ring", "the planet's ring's outer radius in its own: 0 none (-1: the type's)", 4,
       &s.ring);
  knob("sv_nebula", "a nebula's brightness against the snow's white: 0 none (-1: the type's)", 2,
       &s.nebula);
  knob("sv_skystars", "the share of the stars seen by day, 0 .. 1 (-1: the type's)", 1,
       &s.sky_stars);
  knob("sv_skyglow", "a fire's glow from under the horizon: 0 none (-1: the type's)", 4,
       &s.sky_glow);
}

void register_cvars(con::Console& c) {
  auto s = std::make_shared<Settings>();
  add_cvars(c, *s);
  c.keep(s);
}

}  // namespace fall_line::cfg
