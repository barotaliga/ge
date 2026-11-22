// The window's cvar layers and each classic game's own settings (cvar_layers.hpp).
//
// ge:summary The window's cvar layers: each game's own settings, the switch, the clashes printed
// ge:keywords cvar-layer game-cvars per-game-cvar same own clash game-switch crosshair
// ge:keywords vid_gamma s_volume gl_dynamic gl_lightmap cl_gun scr_centertime
// ge:keywords intensity gl_modulate gl_brightness cl_blend cl_entities cl_particles cl_lights
// ge:keywords cl_gibs cl_noglow logfile cl_disable_particles cl_disable_explosions
// ge:keywords cl_dlight_hacks cl_muzzlelight_time cl_muzzleflashes
// ge:keywords sshot_format sshot_dir image_jpeg_quality_level gl_screenshot_format
// ge:keywords gl_screenshot_quality gl_screenshot_template gl_screenshot_compression
// ge:keywords gl_partscale gl_partstyle gl_partshape gl_beamstyle gl_drawsky gl_bilerp_skies
// ge:keywords gl_saturation gl_invert gl_anisotropy gl_waterwarp gl_fog gl_bilerp_pics
// ge:keywords gl_gamma_scale_pics gl_modulate_world gl_modulate_entities gl_doublelight_entities
// ge:keywords gl_dotshading gl_coloredlightmaps cl_lerp_lightstyles cl_rollhack
// ge:skill console shooter
// ge:test tests/shooters_edge_cvar_layers_test.cpp
#include "shooters_edge/client/cvar_layers.hpp"

#include "classicenhance/profile.hpp"
#include "core/num_text.hpp"
#include "shooters_edge/client/picture_files.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/sound/ambience.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <tuple>
#include <utility>
#include <vector>

namespace shooters_edge::client {

namespace con = ge::console;

namespace {

// Quake II's effect switches (classic/q2_fx_switches.hpp) on `v`, bound to `x`.
void add_q2_fx_cvars(con::Cvars& v, ge::classic::Q2FxSwitches& x) {
  v.bind({.name = "cl_disable_particles",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Which specks are left out, as a sum.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 63,
          .values = "0: none (the default); add 1 a grenade's explosion, 2 a grenade's trail, 4 a "
                    "rocket's explosion, 8 a rocket's trail, 16 blood, 32 a blaster bolt's trail"},
         &x.no_particles);
  v.bind({.name = "cl_disable_explosions",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Which explosions' balls of fire are left out, as a sum (their light stays).",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 3,
          .values = "0: none (the default); add 1 a grenade's, 2 a rocket's"},
         &x.no_explosions);
  v.bind({.name = "cl_dlight_hacks",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "Changes to the lights the shots and explosions give, as a sum.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 7,
          .values = "0: none (the default); add 2 for explosions lighting half as far, 4 for no "
                    "light from a machine gun's or a chain gun's flash"},
         &x.light_hacks);
  v.bind({.name = "cl_muzzlelight_time",
          .fallback = "16",
          .flags = con::kArchive,
          .help = "How long a gun's flash lights the room, in milliseconds.",
          .min = 0,
          .max = 1000,
          .values = "16: the default; 0: no light; 1..1000: that long, the light fading over it"},
         &x.muzzle_light_ms);
  v.bind({.name = "cl_muzzleflashes",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether fire shows at the guns of other players and monsters as they shoot.",
          .kind = con::CvarKind::boolean,
          .values = "1: it shows (the default); 0: only their light"},
         &x.muzzle_models);
  v.bind({.name = "cl_footsteps",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Whether you hear the players' footsteps.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "1: you do (the default); 0: none; 2: the same as 1, the game's own four "
                    "steps"},
         &x.footsteps);
  v.bind({.name = "s_ambient",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Which of the sounds that go round and round as things move are heard.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 3,
          .values = "1: all of them (the default); 0: none; 2: only those of a thing you can "
                    "see; 3: only your own"},
         &x.ambient);
  v.bind({.name = "cl_railtrail_type",
          .fallback = "0",
          .flags = con::kArchive,
          .help = "How a railgun's shot is drawn.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: the game's own trail of specks (the default); 1: a beam; 2: a beam with a "
                    "line spiralling round it"},
         &x.rail_type);
  v.bind({.name = "cl_railtrail_time",
          .fallback = "1",
          .flags = con::kArchive,
          .help = "Seconds a railgun's beam takes to fade.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 10,
          .values = "0..10: seconds (1 the default)"},
         &x.rail_time);
  v.bind({.name = "cl_railcore_color",
          .fallback = "red",
          .flags = con::kArchive,
          .help = "The colour of a railgun's beam.",
          .kind = con::CvarKind::text,
          .values = "a colour's name (red the default; black, green, yellow, blue, cyan, "
                    "magenta, white) or #rgb or #rrggbb"},
         &x.core_colour);
  v.bind({.name = "cl_railcore_width",
          .fallback = "2",
          .flags = con::kArchive,
          .help = "How wide a railgun's beam is.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 64,
          .values = "0..64: the game's units (2 the default)"},
         &x.core_width);
  v.bind({.name = "cl_railspiral_color",
          .fallback = "blue",
          .flags = con::kArchive,
          .help = "The colour of the line spiralling round a railgun's beam.",
          .kind = con::CvarKind::text,
          .values = "a colour's name (blue the default; black, red, green, yellow, cyan, "
                    "magenta, white) or #rgb or #rrggbb"},
         &x.spiral_colour);
  v.bind({.name = "cl_railspiral_radius",
          .fallback = "3",
          .flags = con::kArchive,
          .help = "How far the spiralling line keeps from a railgun's beam.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 64,
          .values = "0..64: the game's units (3 the default)"},
         &x.spiral_radius);
  v.bind({.name = "cl_hit_markers",
          .fallback = "2",
          .flags = con::kArchive,
          .help = "What shows when your shot hurts someone, on servers that tell.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "2: a red mark at the centre and a sound (the default); 1: the mark alone; "
                    "0: nothing"},
         &x.hit_markers);
  v.bind({.name = "scr_hit_marker_time",
          .fallback = "500",
          .flags = con::kArchive,
          .help = "Milliseconds the hit mark takes to fade.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 10000,
          .values = "500: the default; 0: no mark; 1..10000: that many milliseconds"},
         &x.hit_marker_ms);
  // The picture's switches: none saved in the config, as the game keeps them.
  v.bind({.name = "gl_partscale",
          .fallback = "2",
          .help = "How big the specks are; the far ones are drawn larger, so they still show.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 100,
          .values = "2: the default; a number: that size, twice the default at 4"},
         &x.part_scale);
  v.bind({.name = "gl_partstyle",
          .fallback = "0",
          .help = "How the specks lie over what is behind them.",
          .kind = con::CvarKind::boolean,
          .values = "0: laid over it (the default); 1: added to it, so they glow"},
         &x.additive_parts);
  v.bind({.name = "gl_partshape",
          .fallback = "0",
          .help = "The specks' shape.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 2,
          .values = "0: a soft round dot (the default); 1: a hard square; 2: a sharp round dot"},
         &x.part_shape);
  v.bind({.name = "gl_beamstyle",
          .fallback = "0",
          .help = "How the lasers and the railgun's beam are drawn.",
          .kind = con::CvarKind::boolean,
          .values = "0: a soft glowing strip (the default); 1: a plain solid tube"},
         &x.round_beams);
  v.bind({.name = "gl_drawsky",
          .fallback = "1",
          .help = "Whether the sky is drawn.",
          .kind = con::CvarKind::boolean,
          .values = "1: the sky (the default); 0: solid black"},
         &x.draw_sky);
  v.bind({.name = "gl_bilerp_skies",
          .fallback = "1",
          .help = "Whether the sky is smoothed.",
          .kind = con::CvarKind::boolean,
          .values = "1: smoothed (the default); 0: sharp pixels"},
         &x.smooth_sky);
  v.bind({.name = "gl_saturation",
          .fallback = "1",
          .help = "How much colour the walls and floors keep.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 1,
          .values = "1: as painted (the default); 0..1: that share of it; 0: grey"},
         &x.saturation);
  v.bind({.name = "gl_invert",
          .fallback = "0",
          .help = "Whether the walls and floors are drawn as a negative.",
          .kind = con::CvarKind::boolean,
          .values = "0: as painted (the default); 1: their colours inverted"},
         &x.invert);
  v.bind(
      {.name = "gl_anisotropy",
       .fallback = "1",
       .help = "How sharp the textures stay on surfaces seen at a slant, when they are smoothed.",
       .kind = con::CvarKind::number,
       .min = 1,
       .max = 16,
       .values = "1: off (the default); 2..16: that many samples, sharper"},
      &x.anisotropy);
  v.bind({.name = "gl_waterwarp",
          .fallback = "0",
          .help = "Whether the view ripples while you are under water, slime or lava.",
          .kind = con::CvarKind::boolean,
          .values = "0: still (the default); 1: it ripples"},
         &x.water_warp);
  v.bind({.name = "gl_fog",
          .fallback = "1",
          .help = "Whether fog is drawn.",
          .kind = con::CvarKind::boolean,
          .values = "1: the fog the map or the fog command sets (the default); 0: none at all"},
         &x.fog);
  // The light on the walls and the models.
  for (const auto& [name, help, field] :
       {std::tuple{"gl_modulate_world", "How bright the walls' light is, times gl_modulate.",
                   &x.modulate_world},
        std::tuple{"gl_modulate_entities",
                   "How bright the light on the players, monsters and items is, times gl_modulate.",
                   &x.modulate_entities}})
    v.bind({.name = name,
            .fallback = "1",
            .help = help,
            .kind = con::CvarKind::number,
            .min = 0,
            .max = 1000000,
            .values = "1: the default; a number: that many times as bright"},
           field);
  v.bind({.name = "gl_doublelight_entities",
          .fallback = "1",
          .help = "Whether the players, monsters and items take gl_modulate's light twice.",
          .kind = con::CvarKind::boolean,
          .values = "1: twice, as the game always drew them (the default); 0: once"},
         &x.double_light);
  v.bind({.name = "gl_dotshading",
          .fallback = "1",
          .help = "Whether the players, monsters and items are shaded brighter on one side.",
          .kind = con::CvarKind::boolean,
          .values = "1: shaded (the default); 0: lit evenly all round"},
         &x.dot_shading);
  v.bind({.name = "gl_coloredlightmaps",
          .fallback = "1",
          .help = "How much colour the walls' light keeps.",
          .kind = con::CvarKind::number,
          .min = 0,
          .max = 1,
          .values = "1: its colours (the default); 0..1: that share; 0: white light"},
         &x.lightmap_saturation);
  v.bind({.name = "cl_lerp_lightstyles",
          .fallback = "0",
          .help = "Whether flickering and pulsing lights change smoothly.",
          .kind = con::CvarKind::boolean,
          .values = "0: in steps, ten a second (the default); 1: smoothly"},
         &x.lerp_styles);
  v.bind({.name = "cl_rollhack",
          .fallback = "1",
          .help = "Which way the other players lean as they run sideways.",
          .kind = con::CvarKind::boolean,
          .values = "1: away from the way they move, as the game always drew it (the default); "
                    "0: into it"},
         &x.roll_hack);
  v.bind({.name = "gl_dlight_falloff",
          .fallback = "1",
          .help = "Whether the edge of a flash's or a rocket's light on the walls is soft.",
          .kind = con::CvarKind::integer,
          .min = 0,
          .max = 1,
          .values = "1: it fades out (the default); 0: a hard edge"},
         &x.dlight_falloff);
}

// THE FIRST CLASSIC GAME: every name it shares with the layers under it means there what it means
// in the game (its crosshair: 0 none, 1 its own +, 2 a dot, as the engine's marks begin), so it has
// no settings of its own yet, and its crosshair numbers the engine's marks. Its spelling of the
// lens, `fov`, is our `cl_fov` (90 in both); `cvar_reset` is `reset`, `viewalias` is `alias`.
// `cl_novweps` keeps its servers from showing the weapons players carry (the connect leaves the
// visible weapons' bit out: net/qw_remote.cpp). Its
// servers' switch rules, `w_switch` and `b_switch`, are user info a server reads on a pickup (the
// Quake II has none). Its weapon's `r_viewmodelsize` and `r_viewmodeloffset` (units, added to
// ours), and `r_viewmodellastfired` (a recording's weapon the one that last fired).
void add_first(con::Console& c, const GameBindings& b) {
  if (b.crosshair != nullptr) {  // Quake II's own shares and health colour set aside
    b.crosshair->pictures = false;
    b.crosshair->shade[0] = b.crosshair->shade[1] = b.crosshair->shade[2] = 1;
    b.crosshair->by_health = false;
  }
  if (b.mouse != nullptr) b.mouse->per_frame = false;  // m_accel: counts a millisecond
  if (b.predict != nullptr) *b.predict = true;  // only Quake II turns it off (its cl_predict)
  if (b.show_misses != nullptr) *b.show_misses = false;  // Quake II's cl_showmiss
  if (b.shownet != nullptr) *b.shownet = 0;              // and cl_shownet
  if (b.showpackets != nullptr) *b.showpackets = false;  // and cl_showpackets
  if (b.items_still != nullptr) *b.items_still = false;  // and cl_nobob
  if (b.menu != nullptr) {  // and ui_scale, ui_sortservers, ui_pingrate: the menus by the window
    b.menu->menu_scale() = 0;
    b.menu->servers().game_sort = shooters_edge::menu::ServersPage::kNoGameSort;
    b.menu->servers().game_ping_rate = -1;
  }
  if (b.console != nullptr) {  // and con_background: the game's own picture
    b.console->back_name = "conback";
    b.console->read_back = nullptr;
  }
  if (b.light_intensity != nullptr) *b.light_intensity = 2;  // Quake II's light settings set aside
  if (b.light_modulate != nullptr) *b.light_modulate = 1;
  if (b.light_lift != nullptr) *b.light_lift = 0;
  if (b.gibs != nullptr) *b.gibs = true;
  if (b.no_glow != nullptr) *b.no_glow = false;
  if (b.q2fx != nullptr) *b.q2fx = {};  // Quake II's effect switches set aside
  if (b.music != nullptr) {             // its ogg_enable, ogg_shuffle and ogg_menu_track set aside
    b.music->enabled = true;
    b.music->shuffle = false;
    b.music->menu_track = 0;
  }
  if (b.view != nullptr) {  // Quake's weapon is always drawn, at its own place and lens
    b.view->hand = 0;
    b.view->gun = 1;
    b.view->gun_units[0] = b.view->gun_units[1] = b.view->gun_units[2] = 0;
    b.view->gun_fov_h = 0;
    b.view->alpha = 1;
  }
  // Its weapon's depth and offset, in its units; the offset adds to our cl_gunx/y/z.
  const con::CvarSpec size{
      .name = "r_viewmodelsize",
      .fallback = "1",
      .flags = con::kArchive,
      .help =
          "How long the held weapon looks: under 1 it is squashed toward "
          "you.",
      .min = 0,
      .max = 1,
      .values = "1: its full length (the default); 0..1: down to half as long"};
  const con::CvarSpec offset{
      .name = "r_viewmodeloffset",
      .fallback = "",
      .flags = con::kArchive,
      .help =
          "Moves the held weapon by up to three numbers in game units: right, down and "
          "forward; added to cl_gunx, cl_guny and cl_gunz.",
      .kind = con::CvarKind::text,
      .values =
          "\"\": where the game holds it (the default); \"x y z\": that far right, down and "
          "forward, missing numbers 0"};
  // A recording's weapon: the one that last fired, kept while the player flicks through others.
  const con::CvarSpec last_fired{
      .name = "r_viewmodellastfired",
      .fallback = "0",
      .flags = con::kArchive,
      .help =
          "Whether a recording played back shows the weapon last fired while the player "
          "looks through others.",
      .kind = con::CvarKind::boolean,
      .values =
          "0: the weapon held (the default); 1: the weapon that last fired, until another "
          "one fires"};
  if (b.view != nullptr) {
    c.cvars().bind(size, &b.view->size);
    c.cvars().bind(offset, &b.view->offset_text);
    c.cvars().bind(last_fired, &b.view->last_fired);
  } else {
    c.cvars().add(size);
    c.cvars().add(offset);
    c.cvars().add(last_fired);
  }
  // Its ambience of water and sky (sound/ambience.hpp): how loud, and how fast it swells and fades.
  c.cvars().bind({.name = "ambient_level",
                  .fallback = "0.3",
                  .help = "How loud the water and wind around you sound.",
                  .kind = con::CvarKind::number,
                  .min = 0,
                  .max = 1,
                  .values = "0: silent; 0..1: that share of full (0.3 the default)"},
                 &ambient_settings().level);
  c.cvars().bind({.name = "ambient_fade",
                  .fallback = "100",
                  .help = "How fast the water and wind swell and fade as you move, in 255ths of "
                          "full a second.",
                  .kind = con::CvarKind::number,
                  .min = 1,
                  .max = 10000,
                  .values = "1..10000: that fast (100 the default: silence to full in 2.55 s)"},
                 &ambient_settings().fade);
  if (b.noskins != nullptr) *b.noskins = 0;         // its figures are its own (choose_figure)
  if (b.screen != nullptr) *b.screen = Q2Screen{};  // Quake II's view and 2D settings set aside
  c.cvars().add_synonym("fov", "cl_fov");
  c.cvars().add_synonym("ignore_qtv", "ignore_tv");     // its spelling of the TV viewers' chat
  c.cvars().add_synonym("cl_chatmode", "con_chat");     // what a typed line is: command or chat
  c.cvars().add_synonym("sshot_format", "scr_format");  // its spellings of the screenshot's
  c.cvars().add_synonym("sshot_dir", "scr_folder");     // format, folder and JPEG quality
  c.cvars().add_synonym("image_jpeg_quality_level", "scr_quality");
  c.add_command_synonym("cvar_reset", "reset");  // its spellings of a setting's default and an
  c.add_command_synonym("viewalias", "alias");   // alias printed (`alias <name>` with no body)
  c.add_command_synonym("togglechat", "toggleconsole");  // QuakeWorld's second name for it
  c.cvars().add_synonym("chase_right", "cam_side");      // the chase camera's step to the left
  // An old Quake config's saved name and colours: `_cl_name` is the name, `_cl_color` both
  // colours as one number (shirt times 16 plus pants), so its config dresses you as it did.
  c.cvars().add_synonym("_cl_name", "name");
  c.cvars().add_synonym("port", "sv_port");  // the port the next game is served on
  c.add_command(
      "_cl_color", "_cl_color <shirt*16+pants>: both your colours as one number",
      [](con::Console& k, const con::Args& a) {
        const auto colour = [&k](const char* name) {
          const con::Cvar* v = k.cvars().find(name);
          return v != nullptr ? std::atoi(v->value.c_str()) : 0;
        };
        if (a.size() < 2) {
          k.print("\"_cl_color\" is \"" +
                  std::to_string(colour("topcolor") * 16 + colour("bottomcolor")) + "\"");
          return;
        }
        const int n = static_cast<int>(std::atof(a[1].c_str()));
        k.cvars().set("topcolor", std::to_string(std::min((n >> 4) & 15, 13)), con::Source::local);
        k.cvars().set("bottomcolor", std::to_string(std::min(n & 15, 13)), con::Source::local);
      });
  c.cvars().add({.name = "cl_novweps",
                 .fallback = "0",
                 .flags = con::kArchive,
                 .help = "Whether you tell servers not to show the weapons players carry.",
                 .kind = con::CvarKind::boolean,
                 .values = "0: they may (the default); 1: they show none, from the next connect"});
  for (const auto& [name, help] :
       {std::pair{"w_switch", "The best weapon a server may switch you to on picking one up."},
        std::pair{"b_switch",
                  "The best weapon a server may switch you to on picking up a "
                  "backpack."}})
    c.cvars().add({.name = name,
                   .fallback = "",
                   .flags = con::kArchive | con::kUserInfo,
                   .help = help,
                   .kind = con::CvarKind::text,
                   .values = "\"\": the server's own rule (the default); 1..8: up to that weapon's "
                             "number"});
}

// QUAKE II (porting/shooters-edge/cvar-meanings.md):
// - `crosshair` is its own: 0 none, 1 to 3 its own pictures (ch1, ch2, ch3), never the engine's
//   marks, so a Quake II player's config keeps its crosshair;
// - `scr_centertime` is the same setting, 2.5 seconds by default where Quake's is 2, and
//   `cl_timeout` 120 where Quake's is 60;
// - its own spellings of settings we have under Quake's names, the same thing in each:
//   `vid_gamma` (`gamma`: higher darker in both), `s_volume` (`volume`), `con_alpha`
//   (`con_bgalpha`), `gl_dynamic` (`r_dynamic`), `gl_lightmap` (`r_lightmap`), `fov` (`cl_fov`),
//   `cl_vwep` (`r_drawvweps`: the weapon a figure carries, 1 in both), and its crosshair's `ch_x`,
//   `ch_y` (`cl_crossx`, `cl_crossy`), `ch_scale` (`crosshairsize`) and `ch_alpha`
//   (`crosshairalpha`); its own `ch_red`, `ch_green`, `ch_blue` and `ch_health` colour it;
//   `scr_showpause` (`showpause`), `netgraph` (`r_netgraph`) and `ogg_volume` (`bgmvolume`); its
//   `ogg_enable` is the music's switch (`cd on|off`); `ogg` its spelling of `music`;
// - the move keys' speeds are the same settings, 200 walking by default where Quake's
//   are 400 (Quake II has no back speed of its own: `cl_backspeed` keeps the forward's 200);
// - its player's user info: `hand`, `gender` and `gender_auto`; its own `m_accel` (per frame);
//   `cl_predict` (our `cl_nopred` the other way round) and `cl_predict_crouch` (the eye's height
//   predicted with the moves; Quake never ducks);
// - its weapon's place and lens in its units: `cl_gun_x/y/z` (added to ours) and `cl_gunfov`
//   (across at 4:3; our `r_viewmodel_fov` wins when set); its own `cl_gun`, the weapon drawn and
//   its side (`hand` 1 or `cl_gun` 3 draw it mirrored on the left);
// - its own `cl_maxfps`, the moves a second (ours, the frame cap, set aside while it is loaded),
//   and `r_maxfps`, the frame cap, kept with its own settings;
// - the maps' light: `intensity`, `gl_modulate` and `gl_brightness` (Quake's stay 2, 1 and 0);
// - its chat: its own `cl_chat_sound` (one sound by number or name) and `con_auto_chat` (2 says a
//   typed line to the team, where our `con_chat` 2 tells a command by its first word);
// - its view and 2D (q2_view.hpp): `scr_scale`, `scr_draw2d`, `scr_alpha`, the lag graph's
//   `scr_lag_*`, `cl_adjustfov`, `uf`, `cl_kickangles`; and `s_underwater` the same setting as
//   Quake's (room_ear.hpp), on by default where Quake's is off;
// - its browsers: `ui_sortdemos` (`menu_demo_sort`, by name by default where ours lists the newest
//   first) and `ui_colorservers` (`sb_colorservers`); its own `ui_sortservers` (one column either
//   way, in place of `sb_sortservers`) and `ui_pingrate` (from `rate`, in place of
//   `sb_infospersec`); `ui_scale`, the menus' size; `al_device` its spelling of `s_device`.
void add_q2(con::Console& c, const GameBindings& b) {
  con::Cvars& v = c.cvars();
  // Its whole set of configs run again on a game change (client/install_configs.cpp's
  // run_game_settings), autoexec.cfg after config.cfg.
  // The client's own port (net/qw_remote.cpp's `own_port`), its socket opened again when it moves.
  v.add({.name = "net_clientport",
         .fallback = "-1",
         .help = "The network port your client sends from; a change applies at once.",
         .kind = con::CvarKind::integer,
         .min = -1,
         .max = 65535,
         .values = "-1: any free port (the default); 1..65535: that port"});
  // Joining an IPv6 server (net/qw_remote.cpp's `own_v6`), the socket on `net_ip6`'s address.
  v.add({.name = "net_enable_ipv6",
         .fallback = "1",
         .help = "Whether the client may join a server by an IPv6 address.",
         .kind = con::CvarKind::integer,
         .min = 0,
         .max = 2,
         .values = "0: IPv4 only; 1: IPv6 too (the default); 2: the same"});
  v.add({.name = "net_ip6",
         .fallback = "",
         .help = "The IPv6 address on this computer the client sends from when it joins by IPv6.",
         .kind = con::CvarKind::text,
         .values = "\"\": any of them (the default); ADDRESS: that one"});
  v.add({.name = "fs_autoexec",
         .fallback = "1",
         .help = "Whether autoexec.cfg runs again after config.cfg when another game is loaded.",
         .kind = con::CvarKind::boolean,
         .values = "0: config.cfg alone; 1: autoexec.cfg and postexec.cfg after it (the default)"});
  // Its `m_accel` adds the frame's counts times the setting (held within 0..1), no time in it.
  if (b.mouse != nullptr) b.mouse->per_frame = true;
  if (b.crosshair != nullptr) {
    b.crosshair->pictures = true;
    v.bind({.name = "crosshair",
            .fallback = "1",
            .flags = con::kArchive,
            .help = "The crosshair: one of the game's own pictures.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 3,
            .values = "0: none; 1, 2, 3: the game's crosshair pictures (1 the default)",
            .overlap = con::Overlap::own},
           &b.crosshair->mark);
  }
  // The maps' light, as its own renderer works it out: a wall shows its painted colour times its
  // light / 255 plus gl_brightness, times gl_modulate, times intensity (2: a light of 128 shows the
  // colour as painted).
  if (b.light_intensity != nullptr)
    v.bind({.name = "intensity",
            .fallback = "2",
            .flags = con::kArchive,
            .help = "How bright the walls are lit: their light times this.",
            .min = 1,
            .max = 5,
            .values = "1..5: 2 the default, as the maps were lit; 3 a third brighter again"},
           b.light_intensity);
  if (b.light_modulate != nullptr)
    v.bind({.name = "gl_modulate",
            .fallback = "1",
            .flags = con::kArchive,
            .help = "How strongly the maps' light shows: their light times this.",
            .min = 0,
            .max = 1000000,
            .values = "0 and up: 1 the default, as the maps were lit; 2 twice as bright"},
           b.light_modulate);
  if (b.light_lift != nullptr)
    v.bind({.name = "gl_brightness",
            .fallback = "0",
            .flags = con::kArchive,
            .help = "Light added to every wall, so the darkest corners show.",
            .min = -1,
            .max = 1,
            .values = "-1..1: 0 the default; 0.1 to 0.3 lifts the dark corners"},
           b.light_lift);
  v.add({.name = "rate",  // the bytes a second its servers may send: more than Quake's
         .fallback = "15000",
         .flags = con::kArchive | con::kUserInfo,
         .help = "How many bytes a second a joined server may send you.",
         .kind = con::CvarKind::integer,
         .min = 500,
         .max = 1000000,
         .values = "500..1000000: that many bytes a second, held to the server's own bounds "
                   "(15000 the default)",
         .overlap = con::Overlap::same});
  v.add({.name = "cl_timeout",  // Quake II's servers are waited for longer
         .fallback = "120",
         .flags = con::kArchive,
         .help = "How long a joined server may send nothing before you leave it, in seconds.",
         .min = 0,
         .max = 86400,
         .values = "0: never; 1..86400: that many seconds (120 the default)",
         .overlap = con::Overlap::same});
  v.add({.name = "m_side",  // the same sidestep as Quake's, a whole unit a count
         .fallback = "1",
         .flags = con::kArchive,
         .help = "How far the mouse's sideways moves sidestep you when they do (+strafe held, or "
                 "lookstrafe with +mlook).",
         .min = -10,
         .max = 10,
         .values = "-10..10: units a count at sensitivity 1 (1 the default)",
         .overlap = con::Overlap::same});
  v.add({.name = "s_underwater",  // its own client muffles the world under water; Quake's never
         .fallback = "1",
         .flags = con::kArchive,
         .help = "Whether the world sounds muffled while your head is under water.",
         .kind = con::CvarKind::boolean,
         .values = "1: muffled (the default); 0: as in the air",
         .overlap = con::Overlap::same});
  if (b.screen != nullptr) add_q2_screen_cvars(v, *b.screen);
  if (b.gibs != nullptr)  // our cl_gibfilter, the other way round
    v.bind({.name = "cl_gibs",
            .fallback = "1",
            .flags = con::kArchive,
            .help = "Whether the pieces a body bursts into are drawn.",
            .kind = con::CvarKind::boolean,
            .values = "1: they fly and lie about (the default); 0: none is drawn"},
           b.gibs);
  if (b.no_glow != nullptr)  // over r_itempulse
    v.bind({.name = "cl_noglow",
            .fallback = "0",
            .flags = con::kArchive,
            .help = "Whether the items lying about never pulse with light.",
            .kind = con::CvarKind::boolean,
            .values = "0: they pulse as r_itempulse says (the default); 1: never"},
           b.no_glow);
  if (b.q2fx != nullptr) add_q2_fx_cvars(v, *b.q2fx);
  const con::CvarSpec music_on{.name = "ogg_enable",
                               .fallback = "1",
                               .flags = con::kArchive,
                               .help = "Whether the game's music plays.",
                               .kind = con::CvarKind::boolean,
                               .values = "1: it plays (the default); 0: no music"};
  const con::CvarSpec shuffle{.name = "ogg_shuffle",
                              .fallback = "0",
                              .flags = con::kArchive,
                              .help =
                                  "Whether the game's music plays every track in a random "
                                  "order instead of each level's own.",
                              .kind = con::CvarKind::boolean,
                              .values = "0: each level's own track (the default); 1: shuffled"};
  const con::CvarSpec menu_track{.name = "ogg_menu_track",
                                 .fallback = "0",
                                 .flags = con::kArchive,
                                 .help = "The track played while no game is running.",
                                 .kind = con::CvarKind::integer,
                                 .min = 0,
                                 .max = 99,
                                 .values = "0: none (the default); 1..99: that track"};
  if (b.music != nullptr) {
    v.bind(music_on, &b.music->enabled);
    v.bind(shuffle, &b.music->shuffle);
    v.bind(menu_track, &b.music->menu_track);
  } else {
    v.add(music_on), v.add(shuffle), v.add(menu_track);
  }
  c.add_command_synonym("ogg", "music");       // its music player (engine/audio/music_console.hpp)
  c.add_command_synonym("seek", "demo_jump");  // its spelling: a time, or + or - and one
  v.add({.name = "menu_demo_sort",             // its demo browser lists by name (demos_page.hpp)
         .fallback = "1",
         .flags = con::kArchive,
         .help = "The order DEMOS lists the recordings in.",
         .kind = con::CvarKind::integer,
         .min = -3,
         .max = 3,
         .values = "1: by name, A to Z (the default); -1: Z to A; 2: oldest first; -2: newest "
                   "first; 3: smallest first; -3: largest first; 0: by name",
         .overlap = con::Overlap::same});
  v.add_synonym("ui_sortdemos", "menu_demo_sort");
  if (v.find("s_device") != nullptr)  // the window's (platform/window_cvars.hpp)
    v.add_synonym("al_device", "s_device");
  for (int k = 0; k < 16; k++)  // its address book, asked by JOIN A GAME (servers_page.hpp)
    v.add({.name = "adr" + std::to_string(k),
           .fallback = "",
           .flags = con::kArchive,
           .help = "A server in your address book, asked whenever the server list refreshes.",
           .kind = con::CvarKind::text,
           .values = "\"\": none (the default); TEXT: a server's address, host:port"});
  v.add({.name = "scr_centertime",
         .fallback = "2.5",
         .flags = con::kArchive,
         .help = "How long a message in the middle of the screen stays, in seconds.",
         .min = 0,
         .max = 30,
         .values = "0..30: that many seconds (2.5 the default)",
         .overlap = con::Overlap::same});
  v.add({.name = "scr_template",  // its screenshots are numbered, quake000 up
         .fallback = "quakeXXX",
         .flags = con::kArchive,
         .help = "The screenshot's file name, its X's numbered.",
         .kind = con::CvarKind::text,
         .values = "quakeXXX: quake000, quake001 ... (the default); \"\": named by the time; "
                   "NAME: that name, its last run of X's the first free number",
         .overlap = con::Overlap::same});
  for (const auto& [name, ours] : {std::pair{"vid_gamma", "gamma"},
                                   std::pair{"s_volume", "volume"},
                                   std::pair{"gl_dynamic", "r_dynamic"},
                                   std::pair{"gl_lightmap", "r_lightmap"},
                                   std::pair{"fov", "cl_fov"},
                                   std::pair{"ch_x", "cl_crossx"},
                                   std::pair{"ch_y", "cl_crossy"},
                                   std::pair{"ch_scale", "crosshairsize"},
                                   std::pair{"ch_alpha", "crosshairalpha"},
                                   std::pair{"cl_vwep", "r_drawvweps"},
                                   std::pair{"con_alpha", "con_bgalpha"},
                                   std::pair{"cl_entities", "r_drawentities"},
                                   std::pair{"cl_particles", "r_particles"},
                                   std::pair{"cl_lights", "r_dynamic"},
                                   std::pair{"cl_blend", "gl_polyblend"},
                                   std::pair{"gl_screenshot_format", "scr_format"},
                                   std::pair{"gl_screenshot_quality", "scr_quality"},
                                   std::pair{"gl_screenshot_template", "scr_template"},
                                   std::pair{"gl_screenshot_compression", "scr_compression"},
                                   std::pair{"scr_showpause", "showpause"},
                                   std::pair{"netgraph", "r_netgraph"},
                                   std::pair{"ogg_volume", "bgmvolume"},
                                   std::pair{"ui_colorservers", "sb_colorservers"}})
    v.add_synonym(name, ours);
  // Its chat sound (0 none, 1 talk, 2 talk1, or a name) over s_chat_custom (chat_lines.hpp), and
  // what a line typed in the console is (0 a command, 1 said, 2 said to the team) over con_chat.
  if (b.chat != nullptr) add_q2_chat_sound(c, *b.chat);
  if (b.console_view != nullptr) {
    v.bind({.name = "con_auto_chat",
            .fallback = "0",
            .flags = con::kArchive,
            .help = "What a line typed in the console without a slash is.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 2,
            .values = "0: a command (the default); 1: said to everyone; 2: said to your team"},
           &b.console_view->game_chat);
    // Its stamps on every console line, not only chat's (the classic layer's `con_timestamps`).
    v.bind({.name = "con_timestamps",
            .fallback = "0",
            .help = "Whether every console line begins with the time it came.",
            .kind = con::CvarKind::boolean,
            .values = "0: no time (the default); 1: the time, as con_timestampsformat writes it",
            .overlap = con::Overlap::own},
           &b.console_view->stamps);
    if (b.chat != nullptr) b.chat->timestamps = 0;  // its chat lines stamped as every line is
    v.bind({.name = "con_timestampsformat",
            .fallback = "%H:%M:%S ",
            .help = "How the time before each console line is written, in strftime's words.",
            .kind = con::CvarKind::text,
            .values = "\"%H:%M:%S \": hours, minutes and seconds (the default); TEXT: that form"},
           &b.console_view->stamp_format);
    v.bind({.name = "con_timestampscolor",
            .fallback = "#aaa",
            .help = "The colour of the time before each console line.",
            .kind = con::CvarKind::text,
            .values = "#aaa: light grey (the default); #rgb, #rrggbb, #rrggbbaa, a colour's name "
                      "or 0..7"},
           &b.console_view->stamp_colour);
    // Its Tab: the word completed to what the matches share, the matches listed.
    v.bind({.name = "com_completion_mode",
            .fallback = "1",
            .help = "How Tab lists the commands and settings a word could be.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 2,
            .values = "0: one a line; 1: in columns (the default); 2: in columns, commands, "
                      "settings and aliases apart"},
           &b.console_view->list_mode);
    v.bind({.name = "com_completion_treshold",
            .fallback = "50",
            .help = "How many matches Tab lists before it asks first.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .values = "50: fifty (the default); a number: that many"},
           &b.console_view->list_threshold);
  }
  // Its own spelling of our `cl_nopred` the other way round: 0 draws the server's place.
  if (b.predict != nullptr)
    v.bind({.name = "cl_predict",
            .fallback = "1",
            .help = "Whether your own moves are predicted on a server joined.",
            .kind = con::CvarKind::boolean,
            .values = "1: predicted, drawn where you will be (the default); 0: drawn where the "
                      "server last put you, a round trip late"},
           b.predict);
  if (b.show_misses != nullptr)
    v.bind({.name = "cl_showmiss",
            .fallback = "0",
            .help = "Whether each miss of your moves' prediction is printed, with its size.",
            .kind = con::CvarKind::boolean,
            .values = "0: not printed (the default); 1: the frame and how far off, in units"},
           b.show_misses);
  if (b.shownet != nullptr)
    v.bind({.name = "cl_shownet",
            .fallback = "0",
            .help = "Whether each message from the server you joined is printed, as its size.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 3,
            .values = "0: none (the default); 1..3: each message's size in bytes"},
           b.shownet);
  if (b.showpackets != nullptr)
    v.bind({.name = "cl_showpackets",
            .fallback = "0",
            .help = "Whether each packet you send to the server you joined is printed.",
            .kind = con::CvarKind::boolean,
            .values = "0: none (the default); 1: each one, its sequences and size"},
           b.showpackets);
  if (b.console != nullptr) {
    if (b.files != nullptr)
      b.console->read_back = [files = b.files](std::string_view name, ge::ui::Image* out) {
        return read_console_back(*files, name, out);
      };
    v.bind({.name = "con_background",
            .fallback = "conback",
            .help = "The picture behind the console, one of the game's under pics/.",
            .kind = con::CvarKind::text,
            .values = "conback: the game's own (the default); NAME: that picture, the game's own "
                      "when it is not there"},
           &b.console->back_name);
  }
  if (b.menu != nullptr) {
    v.bind({.name = "ui_sortservers",
            .fallback = "0",
            .help = "How JOIN A GAME sorts its servers: by one column, either way.",
            .kind = con::CvarKind::integer,
            .min = -5,
            .max = 5,
            .values = "0: as they are found (the default); 1 name, 2 game, 3 map, 4 players (the "
                      "busiest first), 5 ping; -1..-5: the same the other way"},
           &b.menu->servers().game_sort);
    v.bind({.name = "ui_pingrate",
            .fallback = "0",
            .help = "How many servers JOIN A GAME asks a second.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 100,
            .values = "0: from your rate (the default; 33 at 15000); 1..100: that many a second"},
           &b.menu->servers().game_ping_rate);
  }
  if (b.menu != nullptr)
    v.bind({.name = "ui_scale",
            .fallback = "0",
            .help = "How large the menus are drawn.",
            .min = 0,
            .max = 10,
            .values = "0: as large as fits the window (the default); 1..10: that many times the "
                      "game's own size"},
           &b.menu->menu_scale());
  if (b.items_still != nullptr)
    v.bind({.name = "cl_nobob",
            .fallback = "0",
            .help = "Whether the items the game makes float rise and sink as they lie.",
            .kind = con::CvarKind::boolean,
            .values = "0: they float (the default); 1: they lie still"},
           b.items_still);
  if (b.crouch != nullptr)
    v.bind({.name = "cl_predict_crouch",
            .fallback = "1",
            .help = "Whether the eye's height is predicted with your moves on a server joined, so "
                    "ducking lowers the view at once.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 2,
            .values = "0: the view follows the server; 1: on our servers, which say how they move "
                      "you (the default); 2: on every server"},
           b.crouch);
  // Its own `cl_noskins`, beside the shared `noskins` (whose 2 keeps skins but fetches none).
  if (b.noskins != nullptr)
    v.bind({.name = "cl_noskins",
            .fallback = "0",
            .help = "Whether players are drawn in their own figures and skins.",
            .kind = con::CvarKind::integer,
            .min = 0,
            .max = 2,
            .values = "0: each in their own (the default); 1: everyone as the stock male; 2: "
                      "women as the stock female, everyone else as the stock male"},
           b.noskins);
  // Its powers' shells and its items' pulse are its own look, on in it (Quake's default
  // is off, the shells and the pulse never in it).
  for (const auto& [name, help] :
       {std::pair{"r_powershells", "Whether a player with a power wears a coloured shell."},
        std::pair{"r_itempulse", "Whether the items lying about pulse with light."}})
    v.add({.name = name,
           .fallback = "1",
           .flags = con::kArchive,
           .help = help,
           .kind = con::CvarKind::boolean,
           .values = "0: none; 1: on, as the game draws them (the default)",
           .overlap = con::Overlap::same});
  // The player as its servers read him: the hand his weapon is held in (the shot leaves from that
  // side), and the words his own deaths are told in (gender_auto: picked from his skin's figure).
  const con::CvarSpec hand{
      .name = "hand",
      .fallback = "0",
      .flags = con::kArchive | con::kUserInfo,
      .help = "The hand you hold your weapon in; the server fires from that side.",
      .kind = con::CvarKind::integer,
      .min = 0,
      .max = 2,
      .values =
          "0: right (the default); 1: left, the weapon drawn on the left; 2: in the middle, "
          "no weapon drawn"};
  if (b.view != nullptr)
    v.bind(hand, &b.view->hand);
  else
    v.add(hand);
  // Its crosshair's own colour: shares of red, green and blue over the picture, or the player's
  // health (white at full, yellow at two thirds, red at a third).
  if (b.crosshair != nullptr) {
    const char* names[3] = {"ch_red", "ch_green", "ch_blue"};
    const char* words[3] = {"red", "green", "blue"};
    for (int k = 0; k < 3; ++k)
      v.bind({.name = names[k],
              .fallback = "1",
              .flags = con::kArchive,
              .help = std::string("How much ") + words[k] + " the crosshair shows.",
              .min = 0,
              .max = 1,
              .values = "0..1: that share (1 the default)"},
             &b.crosshair->shade[k]);
    v.bind({.name = "ch_health",
            .fallback = "0",
            .flags = con::kArchive,
            .help = "Whether the crosshair takes the colour of your health.",
            .kind = con::CvarKind::boolean,
            .values = "0: its own colour (the default); 1: white at full health, yellow at two "
                      "thirds, red at a third and below"},
           &b.crosshair->by_health);
  }
  // Its own `cl_gun`, not our `r_drawviewmodel`: its 2 and 3 force the side, where Quake's 2 hides
  // the weapon at a wide lens. Both have to allow the weapon for it to be drawn.
  const con::CvarSpec gun_drawn{
      .name = "cl_gun",
      .fallback = "1",
      .flags = con::kArchive,
      .help = "Whether the held weapon is drawn, and on which side.",
      .kind = con::CvarKind::integer,
      .min = -1,
      .max = 3,
      .values =
          "1: on the side of your hand (the default); 2: always on the right; 3: always on "
          "the left; 0 or -1: not drawn"};
  if (b.view != nullptr)
    v.bind(gun_drawn, &b.view->gun);
  else
    v.add(gun_drawn);
  if (b.view != nullptr) {  // Quake's place and size for the weapon set aside
    b.view->size = 1;
    b.view->offset_text.clear();
    b.view->last_fired = false;
  }
  // Its own place and lens for the weapon, in its units.
  const char* gun_names[3] = {"cl_gun_x", "cl_gun_y", "cl_gun_z"};
  const char* gun_helps[3] = {"Moves the held weapon right, in game units; added to cl_gunx.",
                              "Moves the held weapon forward, in game units; added to cl_gunz.",
                              "Moves the held weapon up, in game units; added to cl_guny."};
  for (int k = 0; k < 3; ++k) {
    const con::CvarSpec gun{
        .name = gun_names[k],
        .fallback = "0",
        .flags = con::kArchive,
        .help = gun_helps[k],
        .min = -20,
        .max = 20,
        .values = "-20..20: that many units, negative the other way (0 the default)"};
    if (b.view != nullptr)
      v.bind(gun, &b.view->gun_units[k]);
    else
      v.add(gun);
  }
  const con::CvarSpec gun_fov{
      .name = "cl_gunfov",
      .fallback = "90",
      .flags = con::kArchive,
      .help = "The held weapon's own lens, in degrees across; r_viewmodel_fov wins when set.",
      .min = 0,
      .max = 160,
      .values =
          "0: the view's; 30..160: that many degrees across at 4:3 (90 the default), less "
          "held at 30"};
  if (b.view != nullptr)
    v.bind(gun_fov, &b.view->gun_fov_h);
  else
    v.add(gun_fov);
  const con::CvarSpec gun_alpha{
      .name = "cl_gunalpha",
      .fallback = "1",
      .flags = con::kArchive,
      .help = "How solid the held weapon is drawn.",
      .min = 0,
      .max = 1,
      .values = "1: solid (the default); 0.1..0.9: see-through; 0: not seen"};
  if (b.view != nullptr)
    v.bind(gun_alpha, &b.view->alpha);
  else
    v.add(gun_alpha);
  // Its move rate and its frame cap: its `cl_maxfps` is the moves a second (ours, the frame cap, is
  // set aside while it is loaded and comes back with its value after), its `r_maxfps` the frame
  // cap, starting at the cap in force when the game loads and kept with the game's own settings.
  const con::CvarSpec moves{
      .name = "cl_maxfps",
      .fallback = "62",
      .flags = con::kArchive,
      .help = "How many moves a second you send the server.",
      .kind = con::CvarKind::integer,
      .min = 0,
      .max = 125,
      .values = "62: the default; 10..125: that many, whole milliseconds apart; 0: 125",
      .overlap = con::Overlap::own};
  if (b.moves != nullptr)
    v.bind(moves, b.moves);
  else
    v.add(moves);
  const con::CvarSpec stuff{
      .name = "cl_ignore_stufftext",
      .fallback = "0",
      .flags = con::kArchive,
      .help = "Which console lines a server you join may run on your client.",
      .kind = con::CvarKind::integer,
      .min = 0,
      .max = 2,
      .values =
          "0: every one (the default); 1: only those stufftext-whitelist.txt names; 2: the "
          "same, each one refused said"};
  if (b.stuff_level != nullptr)
    v.bind(stuff, b.stuff_level);
  else
    v.add(stuff);
  const std::string cap = b.frame_cap != nullptr ? ge::core::num::to_string(*b.frame_cap) : "0";
  const con::CvarSpec frames{
      .name = "r_maxfps",
      .fallback = cap,
      .flags = con::kArchive,
      .help =
          "The most frames a second drawn; kept with this game's own settings (cl_maxfps in "
          "the other games).",
      .min = 0,
      .max = 10000,
      .values =
          "0: no cap; 0..10000: that many (the cap in force when the game loaded, the "
          "default)"};
  if (b.frame_cap != nullptr)
    v.bind(frames, b.frame_cap);
  else
    v.add(frames);
  v.add({.name = "gender",
         .fallback = "male",
         .flags = con::kArchive | con::kUserInfo,
         .help = "How the server words your own deaths: his, her or its.",
         .kind = con::CvarKind::text,
         .values = "male (the default), female or none"});
  v.add({.name = "gender_auto",
         .fallback = "1",
         .flags = con::kArchive,
         .help = "Whether your gender follows your skin's figure when you join.",
         .kind = con::CvarKind::boolean,
         .values = "1: male for the male and cyborg figures, female for the female ones, else "
                   "none (the default); 0: as you set it"});
  for (const char* name : {"cl_forwardspeed", "cl_backspeed", "cl_sidespeed", "cl_upspeed"})
    v.add({.name = name,
           .fallback = "200",
           .flags = con::kArchive,
           .help = "The speed a move key asks for, walking; running asks cl_movespeedkey times it.",
           .kind = con::CvarKind::integer,
           .min = 0,
           .max = 2000,
           .values = "0..2000: units a second, the server capping it (200 the default)",
           .overlap = con::Overlap::same});
}

struct GameRows {
  std::string_view game;  // the game profile's id
  void (*add)(con::Console&, const GameBindings&);
};
constexpr GameRows kGameCvars[] = {{"q1", add_first}, {"q2", add_q2}};

}  // namespace

std::string auto_gender(std::string_view skin) {
  std::string figure(skin.substr(0, skin.find('/')));
  for (char& ch : figure)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  if (figure == "male" || figure == "cyborg") return "male";
  if (figure == "female" || figure == "crackhor") return "female";
  return "none";
}

std::string game_layer(std::string_view game_id) { return "game:" + std::string(game_id); }

bool is_game_layer(std::string_view layer) { return layer.starts_with("game:"); }

std::string game_scope(const profiles::GameProfile& game) {
  const ge::classicenhance::Profile* row = ge::classicenhance::find_profile(game.id);
  return con::lower(row != nullptr ? row->base_dir : game.id);
}

void without_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& game,
                        const GameBindings& b, const std::function<void()>& run) {
  // The game's own settings' values (not a `same` one's: that is the series' setting).
  std::vector<std::pair<std::string, std::string>> kept;
  for (const con::Cvar* cv : c.cvars().sorted())
    if (cv->layer == v.layer &&
        (cv->earlier.empty() || cv->earlier.back().how != con::Overlap::same))
      kept.emplace_back(cv->name, cv->value);
  if (!v.layer.empty()) c.drop_layer(v.layer);
  run();
  add_game_cvars(c, v, game, b);
  for (const auto& [name, value] : kept) c.cvars().set(name, value, con::Source::local);
}

void add_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& game,
                    const GameBindings& b) {
  v.layer = game_layer(game.id);
  c.cvars().set_scope(game_scope(game));  // its own values over the common ones (cvar.hpp)
  if (c.cvars().find("cl_beginmapcmd") == nullptr) {  // every game's map commands, once
    con::CvarLayer classic(c.cvars(), kLayerClassic);
    add_map_command_cvars(c.cvars(), v.q2);
  }
  con::CvarLayer layer(c.cvars(), v.layer);
  if (b.chat != nullptr) b.chat->named_sound.clear();  // only Quake II's cl_chat_sound sets it
  if (b.console_view != nullptr) {  // only Quake II's con_auto_chat and stamps set them
    b.console_view->game_chat = -1;
    b.console_view->stamps = false;
    b.console_view->list_mode = -1;  // our own Tab
  }
  {  // only Quake II's cvars set them; what the frames saw is kept
    Q2Session fresh;
    fresh.joined = v.q2.joined;
    fresh.map = v.q2.map;
    fresh.begin_cmd = v.q2.begin_cmd;  // the map commands are every game's (classic layer)
    fresh.change_cmd = v.q2.change_cmd;
    fresh.disconnect_cmd = v.q2.disconnect_cmd;
    v.q2 = fresh;
  }
  if (game.id == "q2") add_q2_session_cvars(c.cvars(), v.q2);
  if (b.view != nullptr) {  // gun_next, gun_prev and gun_model over the game's own weapons
    b.view->gun_frame = 0;
    b.view->gun_model = -1;
    shooters_edge::view::add_gun_commands(c, *b.view, game.held_weapons);
  }
  if (!v.log) v.log = std::make_shared<ConsoleLog>();
  v.log->mode = 0;  // Quake II's logfile and the rest; its file closed by the next line printed
  v.log->flush = 0;
  if (game.id == "q2") add_console_log_cvars(c.cvars(), *v.log);
  if (!v.log_hooked && c.sink()) {
    v.log_hooked = true;
    c.set_sink([&c, log = v.log, before = c.sink()](std::string_view line) {
      if (log->mode > 0 || log->file->file != nullptr)  // its opening or closing shown first
        console_log_line(*log, c.search_path().empty() ? "." : c.search_path().front(), line,
                         std::time(nullptr), before);
      before(line);
    });
  }
  GameBindings bound = b;
  if (bound.screen == nullptr) bound.screen = &v.screen;
  for (const GameRows& row : kGameCvars)
    if (row.game == game.id) row.add(c, bound);
  if (b.crosshair != nullptr) {  // crosshairimage's picture read again from this game's files
    if (b.files != nullptr) add_crosshair_reader(*b.crosshair, *b.files);
    b.crosshair->picture_of.clear();
  }
}

int switch_game_cvars(con::Console& c, GameCvarValues& v, const profiles::GameProfile& now,
                      const GameBindings& b) {
  const int moved = v.layer.empty() ? 0 : c.drop_layer(v.layer);
  const size_t before = c.cvars().size() + c.cvars().synonyms().size();
  add_game_cvars(c, v, now, b);
  return moved + static_cast<int>(c.cvars().size() + c.cvars().synonyms().size() - before);
}

}  // namespace shooters_edge::client
