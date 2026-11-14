// SHOOTER'S EDGE'S TWO LOOKS (docs/shooters-edge.md § The picture): one switch over the world's
// renderer (brush/app/bsp_draw.hpp) and the post pass (render/post.hpp).
//
// - CLASSIC: the maps as they were made to look: each texel a crisp square, the lightmaps' light
//   and its styles, the liquids warped, the map's own sky; no bloom; and the picture's colours
//   pulled to the nearest of the game's 256 by the post pass's grade (`palette_grade`).
// - MODERN: the same faces under the engine's world: filtered textures, real-time lights with
//   shadows, our sky, water, lava and slime, and bloom.
//
// THE STOPS (`Stops`): the three dearest enhancements, the shadows, the water and the skies, each
// drawn at one of four: OFF (its ce_ switch off: the classic original) or on at LOW, MEDIUM or
// HIGH (its `ce_<id>_quality`, high the default and the whole of it). A stop sets the passes'
// parameters or leaves a pass out, never a code path of its own (`world_options`,
// client/world_extras.hpp, the sky's `cloud_at` and `far_land_at` in client/world_draw.cpp).
// `r_quality` picks all three off the game's quality ladder instead (app/ladder.hpp's
// `picture_stops`).
//
// The player tunes the picture with the r_ cvars `add_cvars` registers over a `Picture`: r_look
// picks the look, and r_fx_lights, r_fx_glow and r_bloom set how much light the effects throw, how
// bright a spark glows and how much bloom the modern look mixes in, r_lava_glow and r_slime_glow
// how much the pools light the walls round them (the classic keeps its own: full lights, full glow,
// no bloom, and the pools' light baked in the lightmaps); `gamma` brightens either look through
// the post pass's grade (`gamma_grade`).
//
// ge:summary Shooter's Edge's classic and modern looks: the world's options, the post's, the
// palette grade
// ge:keywords look classic modern r_look r_fx_lights r_fx_glow r_bloom r_lava_glow ce-switches
// ge:keywords stops quality-stop ce_shadows_quality ce_water_quality ce_skies_quality custom_stops
// r_slime_glow cvars palette grade quantize gamma brightness gamma_grade
// crisp bloom shooter picture
// ge:skill render shooter
// ge:test tests/shooters_edge_effects_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_weather.hpp"
#include "brush/map.hpp"
#include "classicenhance/switches.hpp"
#include "console/console.hpp"
#include "render/post.hpp"
#include "shooters_edge/app/effects.hpp"

namespace shooters_edge::look {

enum class Look : uint8_t { classic, modern };

// A row's stop, cheapest first: the switch off, then on at its three levels.
enum class Stop : uint8_t { off, low, medium, high };
struct Stops {
  Stop shadows = Stop::high, water = Stop::high, skies = Stop::high;
};
// "low", "medium" or "high"; false for any other word.
bool parse_level(std::string_view word, Stop* out);
const char* stop_name(Stop stop);
// "classic" or "modern"; false for any other word.
bool parse_look(std::string_view word, Look* out);
const char* look_name(Look look);

// The picture as the player set it (the r_ cvars); the defaults are the modern look's tuning.
struct Picture {
  std::string look = "modern";  // r_look
  double fx_lights = 0.2;       // r_fx_lights: the dynamic lights' strength, the classic's 1
  double fx_glow = 0.15;        // r_fx_glow: the glowing particles' light, the classic's 1
  double bloom = 0.08;          // r_bloom: the bloom's share, 0 none
  double lava_glow = 0.02;      // r_lava_glow: the lava's light on the walls round it, 0 none
  double glow = 0.6;      // r_glow: the glowing texels' light (windows, lamps), 1 the classic's
  double slime_glow = 1;  // r_slime_glow: the slime's faint green light, 0 none
  double fire_glow = 1;   // r_fire_glow: the flames' own brightness, 1 as made
  double fire_pace = ge::brush::app::kFirePace;   // r_fire_speed: the flames' pace, 1 a real fire's
  double water_pace = ge::brush::app::kPoolPace;  // r_water_speed: the pools' pace, 1 real water's
  double gamma = 1;  // gamma: the picture's brightness, below 1 brighter
  // r_fill_shadows: 1 lets every map light throw a figure's shadow and dims the bounce round him,
  // 0 (the default) only the lights whose source shows (brush/app/bsp_draw.hpp's fill_shadows)
  double fill_shadows = 0;
  // The ce_ switches: each enhancement the modern look draws over the pack, on or off.
  ge::classicenhance::Switches ce;
  // ce_shadows_quality, ce_water_quality, ce_skies_quality: low, medium or high, each its
  // switch's level when on (Stops).
  std::string shadows_quality = "high", water_quality = "high", skies_quality = "high";
  // r_quality: custom (each ce_ switch and level as set), a preset of the ladder (low, medium,
  // high, top), auto (the most inside r_budget) or 0..1; r_budget: the ms a frame `auto` fits.
  std::string quality = "custom";
  double budget_ms = 8.3;
  ge::brush::app::WeatherCvars weather;  // r_weather and its kin: the rain and the storm
  // r_farland: the land round the map seen over its walls from high up (render/far_land.hpp):
  // auto (a kind dealt to suit the map's sky), off, or a kind's name; read when the world is made.
  std::string far_land = "auto";
  // r_skyname: the sky box drawn through every map's sky in place of the one its settings name
  // (client/files.hpp's `sky_box_files`); empty, each map's own. Read when the world is made.
  std::string sky_name;
  // r_loadlit: 1 a map's coloured light read from its .lit file (client/files.hpp's `map_lit`) and
  // fetched from a server joined when the files lack it; 0 every map in its own grey light.
  bool load_lit = true;
  // r_drawflat: the walls and floors in one colour each in place of their textures, under their
  // light (0 off, 1 both, 2 the walls alone, 3 the floors alone); r_wallcolor and r_floorcolor
  // their colours ("R G B", 0..255). r_fastsky: the sky in one colour, r_skycolor. Either look.
  int draw_flat = 0;
  // r_dynamic: the moment's lights (a muzzle's flash, a rocket's glow, an explosion) light the
  // walls and the models; off, none do. r_fullbrightskins: how far the players are lifted toward
  // full light wherever they stand (0 lit as the floor under them, 1 fully lit and unshaded).
  bool dynamic = true;
  // The screen's tint: v_contentblend (the water, slime or lava round the eye), v_damagecshift (a
  // hit's flash), v_quadcshift, v_pentcshift, v_suitcshift, v_ringcshift (each power's), 0..1
  // each, how much of it shows; gl_cshiftpercent (0..100) all of them together; gl_polyblend 0
  // none at all. Either look.
  double content_blend = 1, damage_blend = 1, quad_blend = 1, pent_blend = 1, suit_blend = 1,
         ring_blend = 1, cshift_percent = 100;
  bool poly_blend = true;
  double fullbright_skins = 0;
  bool fast_sky = false;
  std::string wall_colour = "150 140 125", floor_colour = "85 95 110", sky_colour = "70 90 120";
  // r_skyfog: the share of a map's fog laid over its sky, where the map's own `skyfog` does not
  // say (0..1, either look).
  double sky_fog = 0.5;
  // The `fog` command's words (density red green blue alpha bias) and how many times it was given:
  // drawn over the map's own fog when given since the world was made, so a new map is its own.
  std::string fog;
  // r_wateralpha: how opaque water is (0..1; 1 the default, solid), and r_slimealpha,
  // r_lavaalpha, r_telealpha the same for slime, lava and a teleporter's face ("": water's). A
  // map's own `wateralpha` and kin hold for that map; a server that sets `watervis` 0 keeps them
  // solid (`liquid_alphas`).
  double water_alpha = 1;
  std::string slime_alpha, lava_alpha, tele_alpha;
  // noskins: 0 each player drawn in their own skin (`skins/<name>.pcx`), 1 every player in the
  // figure's own skin (still in their colours), 2 as 0 with nothing fetched from a server;
  // baseskin: the skin of a player who names none or one the files lack; the `allskins` command's
  // one skin on every player ("": each their own).
  int noskins = 0;
  int fog_serial = 0;  // how many times the `fog` command was given (above)
  std::string base_skin = "base", all_skins;
  // teamskin, enemyskin: every teammate's or every enemy's skin ("": their own); the teamcolor and
  // enemycolor commands' shirt and pants colours (-1: their own). A teammate shares your team
  // with teamplay on; with it off everyone is an enemy.
  std::string team_skin, enemy_skin;
  int team_top = -1, team_bottom = -1, enemy_top = -1, enemy_bottom = -1;
  double far_warm = 0;  // r_farland_warm: its air warmer (or colder) than its kind's, C
  // r_arrival: how strongly your own arrival (a spawn, a teleport's far end) warps the whole view
  // in the modern look with ce_warps on (render/post.hpp's arrival), 0 none.
  double arrival = 1;
  // ce_bubbles_share: the share of the bubbles the game blows under water that is drawn, either
  // look (a map's air vents blow one a second each, a column of ten in the water at once).
  double bubbles_share = 0.3;
  // ce_water_specks: the specks floating in the water seen from under it, a share of the pool
  // look's (render/underwater.hpp's `motes`); 0 by default: round a swimmer they read as bubbles.
  double water_specks = 0;
  // r_lerpmodels, r_lerpmove: the models' animation drawn between frames, and (in the window's own
  // game) their places between the server's states (client/model_lerp.hpp); either look.
  bool lerp_models = true, lerp_move = true;
};
// Registers r_look, r_fx_lights, r_fx_glow, r_bloom, r_lava_glow, r_slime_glow, gamma,
// r_farland, r_farland_warm, r_skyname, r_skyfog, r_drawflat, r_wallcolor, r_floorcolor,
// r_fastsky, r_skycolor, r_dynamic, r_fullbrightskins, r_lerpmodels, r_lerpmove, r_arrival,
// ce_bubbles_share, ce_water_specks, r_quality, r_budget, every ce_ switch, the three
// ce_<id>_quality levels, r_wateralpha and its kin, noskins, baseskin and the weather's cvars over
// `picture` (it must outlive `con`), and the `fog` and `allskins` commands.
void add_cvars(ge::console::Console& con, Picture& picture);
// The screen's tint (effects::screen_blend) of `s` as the player tones it: each layer at its v_
// share, all of them at gl_cshiftpercent, none with gl_polyblend 0.
void toned_blend(const Picture& picture, const effects::Screen& s, double rgba[4]);
// A colour cvar's words ("R G B", each 0..255; one number a grey) into `out` (0..1 each); false,
// and `out` kept, for anything else.
bool rgb_words(std::string_view text, double out[3]);
// r_look's look; modern for a word it does not know.
Look look_of(const Picture& picture);

// The stops as the player set them, r_quality aside: each switch, then its level (a word it does
// not know: high).
Stops custom_stops(const Picture& picture);
// The world renderer's options in `look`, over `base` (the game's other settings), at `stops`
// (the shadows' and the skies' parts: the casters, the capsules, the pack's sky); the second at
// the picture's own `custom_stops`.
ge::brush::app::Options world_options(Look look, ge::brush::app::Options base,
                                      const Picture& picture, const Stops& stops);
ge::brush::app::Options world_options(Look look, ge::brush::app::Options base = {},
                                      const Picture& picture = {});
// SEE-THROUGH LIQUIDS: how opaque water, slime, lava and a teleporter's face are drawn (`out`, the
// renderer's Options::liquid_alpha): the map's own (`map`, the worldspawn's `wateralpha`,
// `slimealpha`, `lavaalpha`, `telealpha`; below 0 where it has none), else the player's cvars,
// slime, lava and teleporters falling back to water's; all solid (1) when the server does not
// allow it (`allowed`). Each is clamped to 0..1.
void liquid_alphas(const Picture& picture, const double map[4], bool allowed, double out[4]);
// The map's own liquid alphas off its worldspawn's keys (below 0 where it has none).
void map_liquid_alphas(const ge::brush::Map& map, double out[4]);
// Whether a server's info allows see-through liquids: not when it sets `watervis` to 0.
bool watervis_allowed(const std::vector<std::pair<std::string, std::string>>& serverinfo);
// THE SKIES' STOPS: the map's cloud layer marched through the sky faces from medium up (off, the
// pack's own sky fills them: nothing for a cloud to show in), the far land at high alone.
constexpr bool cloud_at(Stop skies) { return skies >= Stop::medium; }
constexpr bool far_land_at(Stop skies) { return skies == Stop::high; }
// The glowing particles' share of their light in `look` (render/particles.hpp's
// ParticleSettings::glow): the modern's exposure and bloom would blow the sparks out at full, so
// it takes r_fx_glow.
double effects_glow(Look look, const Picture& picture = {});
// The post pass's settings in `look`.
ge::render::PostSettings post_settings(Look look, const Picture& picture = {});
// The classic grade: every entry the nearest (in sRGB) of `palette`'s colours (768 bytes, RGB), in
// render/post.hpp's `identity_grade` layout. An empty palette gives the identity.
std::vector<float> palette_grade(std::span<const uint8_t> palette);
// The grade `table` brightened by `gamma` (each entry raised to it: 1 unchanged, 0.5 brighter),
// after the palette's pick, so the classic look keeps its 256 colours' choice and is lit like the
// classic game's own brightness slider.
std::vector<float> gamma_grade(std::vector<float> table, double gamma);

}  // namespace shooters_edge::look
