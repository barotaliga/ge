// Fall Line's cvars, registered: each name, default, range, flags and help.
//
// ge:summary Fall Line's cvar table: names, defaults, ranges, flags and help, bound to Settings
// ge:keywords fall-line cvars add_cvars register_cvars
// ge:skill console sim
// ge:test tests/fall_line_cvars_test.cpp
#include "fall_line/cfg/cvars.hpp"

#include <memory>
#include <string>

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
          .help = "The window's width in logical pixels.",
          .kind = CvarKind::integer,
          .min = 320,
          .max = 7680,
          .values = "320..7680: that many pixels (1280 the default)"},
         &s.width);
  v.bind({.name = "vid_height",
          .fallback = "720",
          .flags = kStartup,
          .help = "The window's height in logical pixels.",
          .kind = CvarKind::integer,
          .min = 240,
          .max = 4320,
          .values = "240..4320: that many pixels (720 the default)"},
         &s.height);
  v.bind({.name = "vid_vsync",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether a frame waits for the display's refresh before it is shown.",
          .kind = CvarKind::boolean,
          .values = "0: shown at once, which may tear; 1: it waits (the default)"},
         &s.vsync);

  v.bind({.name = "r_quality",
          .fallback = "custom",
          .flags = kStartup,
          .help = "The picture's quality.",
          .kind = CvarKind::text,
          .values = "custom: each r_ setting as set (the default); low: the cheapest look; medium: "
                    "between; high: the whole look; top: everything at its best; auto: the best "
                    "that fits r_budget; 0..1: cheapest to best, by measured cost"},
         &s.quality);
  v.bind({.name = "r_budget",
          .fallback = "8.3",
          .flags = kStartup,
          .help = "The GPU time a frame may take when r_quality is auto, in milliseconds.",
          .min = 1,
          .max = 100,
          .values = "1..100: that many milliseconds, 8.3 for 120 frames a second (the default)"},
         &s.budget_ms);
  v.bind({.name = "r_taa",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the picture is smoothed over time and upscaled.",
          .kind = CvarKind::boolean,
          .values = "0: no jitter and no resolve; 1: on (the default)"},
         &s.taa);
  v.bind({.name = "r_scale",
          .fallback = "1",
          .flags = kStartup,
          .help = "The size the picture is drawn at against the window's, with r_taa on.",
          .min = 0.25,
          .max = 2,
          .values = "0.25..2: that share a side, 0.5 a quarter of the pixels (1 the default)"},
         &s.scale);
  v.bind({.name = "r_occlusion",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether trees and figures hidden behind the terrain are left undrawn.",
          .kind = CvarKind::boolean,
          .values = "0: only what is out of view is left out; 1: the hidden too (the default)"},
         &s.occlusion);
  v.bind({.name = "r_shadowsize",
          .fallback = "2048",
          .flags = kStartup,
          .help = "How many texels a side the sun's shadow maps have.",
          .kind = CvarKind::integer,
          .min = 256,
          .max = 8192,
          .values = "256..8192: that many (2048 the default)"},
         &s.shadow_size);
  v.bind({.name = "r_bloom",
          .fallback = "0.04",
          .flags = kStartup,
          .help = "How much glow spreads round bright light.",
          .min = 0,
          .max = 1,
          .values = "0: none; 0..1: that share of the picture (0.04 the default)"},
         &s.bloom);
  v.bind({.name = "r_snow",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the snow is drawn with its sparkle, sheen and grooming.",
          .kind = CvarKind::boolean,
          .values = "0: plain lit white; 1: the whole material (the default)"},
         &s.snow);
  v.bind({.name = "r_trails",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the skis cut tracks in the snow, with r_snow on.",
          .kind = CvarKind::boolean,
          .values = "0: no tracks; 1: tracks (the default)"},
         &s.trails);
  v.bind({.name = "r_spray",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the skis throw snow.",
          .kind = CvarKind::boolean,
          .values = "0: none; 1: thrown (the default)"},
         &s.spray);
  v.bind({.name = "r_precip",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether snow and rain fall round the eye when the sky gives any.",
          .kind = CvarKind::boolean,
          .values = "0: none drawn; 1: drawn (the default)"},
         &s.precip);
  v.bind({.name = "r_precip_density",
          .fallback = "1",
          .flags = kStartup,
          .help = "How many flakes and drops fall, against the usual number.",
          .min = 0.25,
          .max = 2,
          .values = "0.25..2: that share (1 the default)"},
         &s.precip_density);
  v.bind({.name = "r_splashes",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the rain splashes on the ground, with r_precip on.",
          .kind = CvarKind::boolean,
          .values = "0: no splashes; 1: splashes (the default)"},
         &s.splashes);
  v.bind(
      {.name = "r_wet",
       .fallback = "1",
       .flags = kStartup,
       .help = "Whether rain wets the ground, darker, mirroring the sky and pooled on the flats.",
       .kind = CvarKind::boolean,
       .values = "0: it stays dry; 1: wet (the default)"},
      &s.wet);
  v.bind({.name = "r_lights",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the lamps shine after dark: the piste's floodlights, the street lamps, "
                  "the lit windows and every skier's headlamp.",
          .kind = CvarKind::boolean,
          .values = "0: dark; 1: lit (the default)"},
         &s.lights);
  v.bind({.name = "r_light_shadows",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the nearest, brightest lamps cast shadows, with r_lights on.",
          .kind = CvarKind::boolean,
          .values = "0: none; 1: they do (the default)"},
         &s.light_shadows);
  v.bind(
      {.name = "r_lightning",
       .fallback = "1",
       .flags = kStartup,
       .help = "Whether a thunderstorm's flashes light the cloud and the ground, its bolts drawn.",
       .kind = CvarKind::boolean,
       .values = "0: neither; 1: both (the default)"},
      &s.lightning);
  v.bind({.name = "r_fog",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether fog is drawn: the valleys' sea of fog, the ground mist, the cloud on "
                  "the summit and a scene's own.",
          .kind = CvarKind::boolean,
          .values = "0: no fog; 1: fog (the default)"},
         &s.fog_drawn);
  v.bind({.name = "r_fog_quality",
          .fallback = "2",
          .flags = kStartup,
          .help = "How finely the fog is drawn.",
          .kind = CvarKind::integer,
          .min = 0,
          .max = 3,
          .values = "0: low, 80 x 45 x 32 cells; 1: medium; 2: high, 160 x 90 x 64 cells (the "
                    "default); 3: top, 240 x 135 x 96 cells"},
         &s.fog_quality);
  v.bind({.name = "r_reflections",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the scene is mirrored in the wet ground, the trees and the sky upside "
                  "down in a puddle.",
          .kind = CvarKind::boolean,
          .values = "0: only the sky's light; 1: mirrored (the default)"},
         &s.reflections);
  v.bind({.name = "r_weather",
          .fallback = "1",
          .flags = kStartup,
          .help = "Whether the map's own sky is drawn.",
          .kind = CvarKind::boolean,
          .values = "0: always clear; 1: the map's sky (the default)"},
         &s.weather);
  v.bind({.name = "r_clouds",
          .fallback = "1",
          .flags = kStartup,
          .help = "How many of the cloud march's steps are taken, as a share.",
          .min = 0,
          .max = 1,
          .values = "0: no cloud layer drawn, the sky keeping its mean cloud; 0.25..1: that share "
                    "(1 the default)"},
         &s.clouds);
  v.bind({.name = "r_sunsize",
          .fallback = "1",
          .flags = kStartup,
          .help = "How much larger the sun's disc is drawn, its light the same.",
          .min = 0.5,
          .max = 20,
          .values = "1: as it is (the default); 0.5..20: its radius times that"},
         &s.disc_sun);
  v.bind(
      {.name = "r_moonsize",
       .fallback = "1",
       .flags = kStartup,
       .help =
           "How much larger the moon's disc is drawn, its face as bright and its light the same.",
       .min = 0.5,
       .max = 20,
       .values = "1: as it is (the default); 0.5..20: its radius times that"},
      &s.disc_moon);

  // The lens and the film (render/post.hpp, render/lens.hpp) and the grade's knobs
  // (render/grade.hpp): each strength 0..1, 0 compiling it out of the frame.
  const auto strength = [&](const char* name, const char* fallback, const char* help,
                            double* field) {
    const std::string values =
        std::string("0: off; 0..1: that strength (") + fallback + " the default)";
    v.bind({.name = name,
            .fallback = fallback,
            .flags = kStartup,
            .help = help,
            .min = 0,
            .max = 1,
            .values = values},
           field);
  };
  strength("r_sharpen", "0.3", "How much the upscaled frame is sharpened, by its contrast.",
           &s.sharpen);
  strength("r_sunrays", "0.5",
           "How strongly the sun streams through the trees and over the ridges in front of it.",
           &s.sunrays);
  strength("r_flare", "0", "How strong the lens flare's ghosts and ring round the sun are.",
           &s.flare);
  strength("r_vignette", "0.25", "How much darker the picture's corners are.", &s.vignette);
  strength("r_chromatic", "0", "How strong the lens's colour fringes toward the corners are.",
           &s.chromatic);
  strength("r_grain", "0", "How strong the film grain is.", &s.grain);
  strength("r_lensdirt", "0", "How much dirt on the lens the sun and the glow light up.",
           &s.lensdirt);
  strength("r_motionblur", "0",
           "How long the shutter stays open as a share of a frame, 0.5 a film camera's.",
           &s.motionblur);
  const auto look = [&](const char* name, const char* fallback, const char* help,
                        const char* values, double lo, double hi, double* field) {
    v.bind({.name = name,
            .fallback = fallback,
            .flags = kStartup,
            .help = help,
            .min = lo,
            .max = hi,
            .values = values},
           field);
  };
  strength("r_ao", "0.6", "How much of the sky's light is kept out of creases and corners.", &s.ao);
  look("r_ao_radius", "1.5", "How far the ambient occlusion reaches from a surface, in metres.",
       "0.25..4: that many metres (1.5 the default)", 0.25, 4, &s.ao_radius);
  strength("r_dof", "0",
           "How strong the blur away from the focus is, 1 being 1.5 % of the picture's height.",
           &s.dof);
  look("r_dof_focus", "0", "How far away the depth of field is focused, in metres.",
       "0: on the camera's subject (the default); 0..10000: that many metres", 0, 10000,
       &s.dof_focus);
  look("r_contrast", "1", "The picture's contrast.",
       "1: as graded (the default); 0.5..2: flatter below 1, steeper above", 0.5, 2, &s.contrast);
  look("r_saturation", "1", "How colourful the picture is.",
       "0: grey; 1: as graded (the default); 2: doubled; 0..2: between", 0, 2, &s.saturation);
  look("r_temperature", "0", "The white balance.",
       "-1..1: colder below 0, warmer above (0 the default)", -1, 1, &s.temperature);
  look("r_tint", "0", "The white balance's tint.",
       "-1..1: greener below 0, more magenta above (0 the default)", -1, 1, &s.tint);
  look("r_gamma", "1", "The middle tones' brightness.",
       "1: as graded (the default); 0.5..2: darker below 1, brighter above", 0.5, 2, &s.gamma);

  v.bind({.name = "cl_view",
          .fallback = "chase",
          .flags = kPlayer,
          .help = "The camera the run is seen through.",
          .kind = CvarKind::text,
          .values = "tips: the ski tips' rung; helmet: the helmet's rung; chase: the chase rung "
                    "(the default); far: the far chase rung; high: the high rung; orbit: the orbit "
                    "rung; seat: a machine's seat; pilot: a machine's pilot rung; wing: the wing's "
                    "rung; down: looking down from a machine; start: the still view at the start; "
                    "piste: the still view along the piste; lookback: the still view looking back; "
                    "vista: the still view over the mountain; aerial: the still view from the air"},
         &s.view);

  // The camera's settings at the engine's defaults: a world in metres, the ladders' own rows.
  ge::camera::add_camera_cvars(v, s.camera, {});

  v.bind({.name = "volume",
          .fallback = "0.8",
          .flags = kPlayer,
          .help = "The master volume.",
          .min = 0,
          .max = 1,
          .values = "0: silent; 0..1: that share, 1 full (0.8 the default)"},
         &s.volume);
  v.bind({.name = "s_wind",
          .fallback = "1",
          .flags = kPlayer,
          .help = "How loud the wind and the snow's hiss under the skis are.",
          .min = 0,
          .max = 1,
          .values = "0: silent; 0..1: that share (1 the default)"},
         &s.wind);
  v.bind({.name = "s_effects",
          .fallback = "1",
          .flags = kPlayer,
          .help = "How loud every other sound is: edges, landings, gates and crashes.",
          .min = 0,
          .max = 1,
          .values = "0: silent; 0..1: that share (1 the default)"},
         &s.effects);
  v.bind({.name = "s_mute",
          .fallback = "0",
          .flags = kPlayer,
          .help = "Whether all sound is off, with no audio device opened.",
          .kind = CvarKind::boolean,
          .values = "0: sound (the default); 1: silent"},
         &s.mute);
  v.bind(
      {.name = "s_thunder",
       .fallback = "1",
       .flags = kPlayer,
       .help = "Whether a thunderstorm's thunder is heard, each clap when its sound reaches you.",
       .kind = CvarKind::boolean,
       .values = "0: not heard; 1: heard (the default)"},
      &s.thunder);

  v.bind({.name = "sv_seed",
          .fallback = "38",
          .flags = kRun,
          .help = "The seed the mountain is generated from.",
          .values = "N: that mountain (38 the default)"},
         &s.seed);
  v.bind({.name = "sv_hour",
          .fallback = "-1",
          .flags = kRun,
          .help = "The hour of the day the run starts at.",
          .min = -1,
          .max = 24,
          .values = "-1: the map's own (the default); 0..24: that hour"},
         &s.hour);
  v.bind({.name = "sv_skyrate",
          .fallback = "0",
          .flags = kRun,
          .help = "How many of the sky's hours pass in an hour of the run.",
          .min = 0,
          .max = 3600,
          .values = "0: the hour stands (the default); 1: real time; 60: an hour a minute; "
                    "0..3600: that many"},
         &s.sky_rate);
  v.bind({.name = "sv_weather",
          .fallback = "map",
          .flags = kRun,
          .help = "The sky.",
          .kind = CvarKind::text,
          .values = "map: the map's own (the default); clear: a clear sky; fair: fair weather; "
                    "flurries: snow flurries; high: high cloud; overcast: overcast; snow: snowing; "
                    "storm: a snowstorm; fog: fog"},
         &s.sky);
  v.bind({.name = "sv_snowfall",
          .fallback = "-1",
          .flags = kRun,
          .help = "How hard snow or rain falls on a sky that gives any.",
          .min = -1,
          .max = 1,
          .values = "-1: the sky's own (the default); 0: none; 1: a blizzard or a cloudburst; "
                    "0..1: between"},
         &s.fall);
  v.bind(
      {.name = "sv_snowline",
       .fallback = "-1",
       .flags = kRun,
       .help = "The snow line as a share of the mountain from its base to its summit, rain falling "
               "below it and sleet about it.",
       .min = -1,
       .max = 2,
       .values =
           "-1: snow to the valley (the default); 0..2: that share, 0 the base and 1 the summit"},
      &s.snowline);
  v.bind({.name = "sv_storm",
          .fallback = "-1",
          .flags = kRun,
          .help = "How strong a summer thunderstorm passing over the mountain is, raining to the "
                  "summit unless sv_snowline is set.",
          .min = -1,
          .max = 1,
          .values = "-1: the map's (the default); 0: none; 0.5: an ordinary storm; 1: a severe "
                    "one; 0..1: between"},
         &s.storm);
  v.bind({.name = "sv_fog",
          .fallback = "-1",
          .flags = kRun,
          .help = "How thick the valley fog lies under a fog.",
          .min = -1,
          .max = 1,
          .values = "-1: the sky's (the default); 0..1: that thickness"},
         &s.fog);
  v.bind({.name = "sv_wind",
          .fallback = "-1",
          .flags = kRun,
          .help = "The mean wind 10 metres up, in metres a second.",
          .min = -1,
          .max = 40,
          .values = "-1: the sky's (the default); 0..40: that speed"},
         &s.air);
  v.bind({.name = "sv_winddir",
          .fallback = "-1",
          .flags = kRun,
          .help = "The heading the wind blows from, in degrees clockwise from the map's +z.",
          .min = -1,
          .max = 360,
          .values = "-1: the map's (the default); 0..360: that heading"},
         &s.air_from);
  v.bind({.name = "sv_cover",
          .fallback = "-1",
          .flags = kRun,
          .help = "How much of a broken sky the cloud covers, under fair, flurries or high.",
          .min = -1,
          .max = 1,
          .values = "-1: the map's (the default); 0..1: that share of the sky"},
         &s.cover);
  v.bind(
      {.name = "sv_clouddensity",
       .fallback = "-1",
       .flags = kRun,
       .help = "How dense the clouds are, the sun and the moon shining through their thin patches.",
       .min = -1,
       .max = 1,
       .values = "-1: the map's (the default); 0: thin; 1: dense; 0..1: between"},
      &s.thick);
  v.bind({.name = "sv_cloudheight",
          .fallback = "-1",
          .flags = kRun,
          .help = "How high the lowest cloud layer's base is over the valley floor in metres, a "
                  "cloud below the skier skied through.",
          .min = -1,
          .max = 12000,
          .values = "-1: the map's (the default); 0..12000: that many metres, the layers above "
                    "moved with it"},
         &s.cloud_height);
  v.bind({.name = "sv_cloudthickness",
          .fallback = "-1",
          .flags = kRun,
          .help = "How tall the lowest cloud layer stands from its base to its top in metres, a "
                  "taller layer darker under it.",
          .min = -1,
          .max = 12000,
          .values = "-1: the map's (the default); 0..12000: that many metres"},
         &s.cloud_thickness);
  v.bind({.name = "sv_cloudspeed",
          .fallback = "-1",
          .flags = kRun,
          .help = "How fast the clouds drift, as a share of the wind aloft at their height.",
          .min = -1,
          .max = 10,
          .values = "-1: the true wind (the default); 0..10: that share, 1 the true wind"},
         &s.cloud_speed);
  v.bind({.name = "sv_clouds",
          .fallback = "map",
          .flags = kRun,
          .help = "The kinds of cloud drawn, lowest first, joined by +, such as cumulus+cirrus.",
          .kind = CvarKind::text,
          .values = "map: the map's own (the default); TEXT: kinds from inversion, stratus, "
                    "stratocumulus, cumulus, congestus, cumulonimbus, nimbostratus, altocumulus, "
                    "altostratus, lenticular, cirrocumulus, veil, cirrostratus, cirrus and wisps"},
         &s.cloud_kinds);
  v.bind({.name = "sv_skytype",
          .fallback = "earth",
          .flags = kRun,
          .help = "The kind of sky.",
          .kind = CvarKind::text,
          .values = "earth: the Earth's (the default); dust: a dusty sky; alien: an alien sky; "
                    "hell: a hellish sky; void: a void; nebula: a nebula's sky; violet: a violet "
                    "sky; ice: an icy sky; ash: an ashen sky; toxic: a toxic sky"},
         &s.sky_type);
  v.bind({.name = "sv_moons",
          .fallback = "type",
          .flags = kRun,
          .help = "The moons over the sky, up to four besides the Earth's.",
          .kind = CvarKind::text,
          .values = "type: the sky type's own (the default); none: no moon; TEXT: moons by name "
                    "(moon, phobos, deimos, io, europa, ganymede, callisto, titan, enceladus, "
                    "miranda, triton), each name, name:size or name:size:spin hours"},
         &s.moons);
  // The sky type's knobs (render/sky_style.hpp's SkyKnobs): -1 keeps the type's own.
  const auto knob = [&](const char* name, const char* help, double max, double* field) {
    const std::string values = "-1: the sky type's own (the default); 0.." +
                               std::to_string(static_cast<int>(max)) + ": that much";
    v.bind({.name = name,
            .fallback = "-1",
            .flags = kRun,
            .help = help,
            .min = -1,
            .max = max,
            .values = values},
           field);
  };
  knob("sv_skyair", "How much air the sky type has, scaled, 1 its own.", 4, &s.sky_air);
  knob("sv_skydust", "How much dust the air carries against the Earth's.", 200, &s.sky_dust);
  knob("sv_sunsize", "How large the sun's disc is against the real one's.", 10, &s.sun_size);
  knob("sv_sun2", "How bright a second sun is against the sun, 0 none.", 2, &s.sun2);
  knob("sv_planet", "How large a planet looks, its angular radius in degrees, 0 none.", 60,
       &s.planet);
  knob("sv_planetdir", "The planet's heading in degrees.", 360, &s.planet_dir);
  knob("sv_planetel", "The planet's elevation in degrees.", 90, &s.planet_el);
  knob("sv_ring", "How far the planet's ring reaches, its outer radius in the planet's, 0 none.", 4,
       &s.ring);
  knob("sv_nebula", "How bright a nebula is against the snow's white, 0 none.", 2, &s.nebula);
  knob("sv_skystars", "The share of the stars seen by day.", 1, &s.sky_stars);
  knob("sv_skyglow", "How bright a fire's glow from under the horizon is, 0 none.", 4, &s.sky_glow);

  v.bind(
      {.name = "sv_lethal",
       .fallback = "1",
       .flags = kRun,
       .help = "Whether the army shoots to kill.",
       .kind = CvarKind::boolean,
       .values =
           "0: it chases the skier down and makes an arrest; 1: it shoots to kill (the default)"},
      &s.lethal);
  v.bind({.name = "sv_armyfire",
          .fallback = "1",
          .flags = kRun,
          .help = "Whether the army's guns fire at all.",
          .kind = CvarKind::boolean,
          .values = "0: it hunts with its guns silent; 1: they fire (the default)"},
         &s.army_fire);
  v.bind({.name = "sv_helis",
          .fallback = "1",
          .flags = kRun,
          .help = "Whether the hunt sends the army's helicopters and the soldiers they put down.",
          .kind = CvarKind::boolean,
          .values = "0: not sent; 1: sent (the default)"},
         &s.helis);
  v.bind({.name = "sv_paras",
          .fallback = "1",
          .flags = kRun,
          .help = "Whether the hunt sends the army's paramotor riflemen, from four stars.",
          .kind = CvarKind::boolean,
          .values = "0: not sent; 1: sent (the default)"},
         &s.paras);
  ge::capture::add_screenshot_cvars(v, s.screenshot);
}

void register_cvars(con::Console& c) {
  auto s = std::make_shared<Settings>();
  add_cvars(c, *s);
  c.keep(s);
}

}  // namespace fall_line::cfg
