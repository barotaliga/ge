// A SHOOTER'S TWO LOOKS (docs/shooters-edge.md § The picture): one switch over the world's renderer
// (brush/app/bsp_draw.hpp) and the post pass (render/post.hpp), and every picture setting a shooter
// on classic maps shares, whatever the game.
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
// A GAME'S DEFAULTS: `add_cvars` takes the values the Picture holds at the call as the cvars'
// defaults, so a game sets its own (a classic look, other strengths) before registering; the
// member initializers below are the shared defaults, the modern look's tuning.
//
// ge:summary A shooter's classic and modern looks: the picture's cvars with a game's defaults, the
// world's options, the post's, the palette grade
// ge:keywords look classic modern r_look r_fx_lights r_fx_glow r_bloom r_lava_glow ce-switches
// ge:keywords stops quality-stop ce_shadows_quality ce_water_quality ce_skies_quality custom_stops
// r_slime_glow cvars palette grade quantize gamma brightness gamma_grade
// crisp bloom shooter picture picture-settings game-defaults r_waterwarp warped_fov
// ge:skill classic shooter render
// ge:test tests/classic_look_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/app/bsp_weather.hpp"
#include "brush/map.hpp"
#include "classic/q2_fx_switches.hpp"
#include "classic/sky_typed.hpp"
#include "classicenhance/switches.hpp"
#include "console/console.hpp"
#include "render/post.hpp"

namespace ge::classic::look {

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
struct Picture {  // NOLINT(clang-analyzer-optin.performance.Padding): one a window, read by name
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
  // The maps' light (Quake II's `intensity` and `gl_brightness`, Quake leaving them at these, and
  // every game's `gl_modulate`): a face shows its painted colour times its luxel / 255 plus
  // `light_lift`, times `light_modulate` times `light_intensity` (`light_scale`). 2, 1 and 0: a
  // luxel of 128 shows the colour as painted, 255 twice as bright.
  double light_intensity = 2, light_modulate = 1, light_lift = 0;
  // gl_anisotropy: the filtered textures' taps (0: the renderer's own 4; 1 none; up to 16).
  double anisotropy = 0;
  // r_fill_shadows: 1 lets every map light throw a figure's shadow and dims the bounce round him,
  // 0 (the default) only the lights whose source shows (brush/app/bsp_draw.hpp's fill_shadows)
  double fill_shadows = 0;
  // r_reflect: how strongly shiny floors mirror the room under ce_reflections, 1 the whole of what
  // the gloss and the angle give; 0.1 by default, a hint of the room (Niclas's pick, 2026-11-19)
  double reflect = 0.1;
  // ce_blood_time: how long blood stays on the walls and floors under ce_blood, s; its drying and
  // its fading shortened to fit a short one; 0 until its place is needed for a newer mark.
  double blood_time = 120;
  // ce_blood_reflect: how strongly a wet pool of blood mirrors the room under ce_blood and
  // ce_reflections, 1 the whole of what its gloss and the angle give; 0 none.
  double blood_reflect = 1;
  // ce_blood_amount: how much a body bleeds under ce_blood, 1 as drawn: the drops thrown, the
  // splashes and pools, the wounds' stains and runs and their drips; 0 none.
  double blood_amount = 1;
  // The ce_ switches: each enhancement the modern look draws over the pack, on or off; with
  // ce_enabled 0 every one reads off (`apply_ce_enabled`), the ce_ cvars keeping the player's.
  ge::classicenhance::Switches ce;
  // ce_enabled: classic enhance as a whole, 0 the pack as it is whatever each ce_ switch says.
  bool ce_enabled = true;
  bool ce_applied = true;  // ce_enabled as `apply_ce_enabled` last applied it
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
  // `sky <name> [rotate] [axis x y z]`: the sky box typed for one map (classic/sky_typed.hpp).
  SkyTyped sky_typed;
  // gl_texturemode: how the map's textures are sampled, either look: empty the look's own (the
  // classic crisp, the modern filtered); a mode whose first filter is nearest (GL_NEAREST,
  // GL_NEAREST_MIPMAP_NEAREST, ...) crisp, each texel a square; one that starts linear filtered
  // (`crisp_textures`).
  std::string texture_mode;
  // r_loadlit: 1 a map's coloured light read from its .lit file (client/files.hpp's `map_lit`) and
  // fetched from a server joined when the files lack it; 0 every map in its own grey light.
  bool load_lit = true;
  // r_dynamic: the moment's lights (a muzzle's flash, a rocket's glow, an explosion) light the
  // walls and the models; off, none do.
  bool dynamic = true;
  // r_rocketlight, r_explosionlight, r_powerupglow: each kind of the moment's lights on its own (a
  // rocket's glow in flight, an explosion's, a power's round its carrier); off, that kind lights
  // nothing (`kept_lights`). Either look.
  bool rocket_light = true, explosion_light = true, power_glow = true;
  // gl_flashblend (Quake's own, classic/flash_blend.hpp): each of the moment's lights drawn as a
  // glowing disc turned to the eye, the walls, doors and lifts left unlit by it (the figures still
  // take it). Either look.
  bool flash_blend = false;
  // r_powershells: a power's carrier wears a coloured shell (quad damage blue, the pentagram or
  // invulnerability red); r_itempulse: an item's light pulses as it lies. Quake II's own look:
  // on in it, off in Quake, which never drew them (each game's default its own: the
  // window's cvar layers). Either look.
  bool power_shells = false, item_pulse = false;
  // cl_muzzleflash: the light a gun throws on the walls as it fires: 1 every gun's (the default),
  // 2 the others' alone (the own gun's flash still shows in the hand, unlit), 0 none. Either look.
  int muzzle_flash = 1;
  // r_waterwarp: the view breathes while the eye is in a liquid (`warped_fov`); 0 it holds still.
  bool water_warp = true;
  // r_drawentities: the players, monsters, items and missiles drawn; 0 none of them, the map's
  // walls, doors and lifts alone (`drawn_models`). Either look.
  bool draw_entities = true;
  // cl_gibfilter: the pieces a body bursts into (each model flagged to bleed as it flies) not
  // drawn; cl_deadbodyfilter: a player's body not drawn once it lies still on the last frame of a
  // death. The game decides which models those are; the switches are here. Either look.
  bool gib_filter = false, dead_body_filter = false;
  // cl_gibs (Quake II's own, the other way round): 0 no piece drawn either. Either look.
  bool gibs = true;
  // cl_noglow (Quake II's own, over r_itempulse): 1 the items' light never pulses. Either look.
  bool no_glow = false;
  // Quake II's own effect switches (classic/q2_fx_switches.hpp). Either look.
  ge::classic::Q2FxSwitches q2fx;
  // The 3D view's share of the screen, percent, centred (Quake II's viewsize under 100: the
  // border is the 2D layer's to tile); 100 all of it.
  int view_size = 100;
  // r_drawflame: the torches' and braziers' flames drawn; 0 none, nor their fire. Either look.
  bool draw_flame = true;
  // r_drawvweps (Quake II's spelling cl_vwep): a player drawn with the weapon he carries, where
  // the server names them (classic/vweps.hpp; Quake II's carried weapons); 0 each figure with
  // its own plain gun. Either look, on in both games (the server decides whether there are any).
  bool draw_vweps = true;
  // r_particles: the specks the effects throw (sparks, blood, smoke, trails, splashes); 0 none
  // thrown, the explosions' fire and the beams still drawn. Either look.
  bool particles = true;
  // scr_centertime: how long a centre message stays on the screen, s.
  double centre_time = 2;
  // gl_polyblend: 0 the view never tinted (below).
  bool poly_blend = true;
  // r_fastsky: the sky in one colour, r_skycolor ("R G B", 0..255). Either look.
  bool fast_sky = false;
  // r_lightmap: 1 every solid face drawn a plain grey under its light, the light alone to see (over
  // r_drawflat). Either look.
  bool lightmap_only = false;
  // r_fullbright (a cheat: set only while sv_cheats is 1, never saved): every wall and floor drawn
  // as its texture is painted, with no light or shade. Either look.
  bool fullbright = false;
  // r_drawflat: the walls and floors in one colour each in place of their textures, under their
  // light (0 off, 1 both, 2 the walls alone, 3 the floors alone); r_wallcolor and r_floorcolor
  // their colours ("R G B", 0..255). Either look.
  int draw_flat = 0;
  // gl_picmip: the map's textures drawn at a half (1), a quarter (2), an eighth (3) or a
  // sixteenth (4) of their detail, nearest; 0 their own. Either look.
  int picmip = 0;
  std::string wall_colour = "150 140 125", floor_colour = "85 95 110", sky_colour = "70 90 120";
  // r_fullbrightskins: how far the players are lifted toward full light wherever they stand (0 lit
  // as the floor under them, 1 fully lit and unshaded).
  double fullbright_skins = 0;
  // The screen's tint: v_contentblend (the water, slime or lava round the eye), v_damagecshift (a
  // hit's flash), v_quadcshift, v_pentcshift, v_suitcshift, v_ringcshift (each power's), 0..1
  // each, how much of it shows; gl_cshiftpercent (0..100) all of them together. Either look.
  double content_blend = 1, damage_blend = 1, quad_blend = 1, pent_blend = 1, suit_blend = 1,
         ring_blend = 1, cshift_percent = 100;
  // cl_bonusflash: whether an item taken flashes the view gold. Either look.
  bool bonus_flash = true;
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
  // one skin on every player ("": each their own); all_figures: the folder of the one figure
  // every player is drawn as (classic remix's `cr_models`, "": each their own).
  int noskins = 0;
  // Quake II's `cl_noskins`: 0 each their own figure, 1 every player the default figure, 2 the
  // default figure of each one's sex (a female figure's, else the male).
  int stock_figures = 0;
  // Quake II's `cl_nobob`: items the game marks to float stay still (no 5-unit rise and fall).
  bool items_still = false;
  // The models' ground shadows (Quake II's `gl_shadows`): 0 none, 1 under each animated model, 2
  // fading as it rises (the client's model_shadow.hpp).
  int model_shadows = 0;
  int fog_serial = 0;  // how many times the `fog` command was given (above)
  std::string base_skin = "base", all_skins, all_figures;
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
// r_fastsky, r_skycolor, r_lightmap, r_fullbright, r_dynamic, r_rocketlight, r_explosionlight,
// r_powerupglow, r_waterwarp, r_fullbrightskins, r_lerpmodels, r_lerpmove, r_arrival, cl_gibfilter,
// cl_deadbodyfilter, ce_bubbles_share, ce_water_specks, r_quality, r_budget, every ce_ switch, the
// three ce_<id>_quality levels, r_wateralpha and its kin, noskins, baseskin and the weather's cvars
// over `picture` (it must outlive `con`), and the `fog` and `allskins` commands. Each cvar's
// default is the value `picture` holds now (a game's own; above).
void add_cvars(ge::console::Console& con, Picture& picture);
// CE_ENABLED, once a frame: off, every ce_ switch in `picture.ce` reads off; turned on again,
// each reads its ce_ cvar in `cvars` again (what the player set meanwhile too). The ce_ cvars
// themselves never change, so a saved config keeps every switch as the player set it.
void apply_ce_enabled(Picture& picture, const ge::console::Cvars& cvars);
// The models a frame draws: `placed` itself, or none with r_drawentities 0.
template <class T>
std::span<const T> drawn_models(const Picture& picture, std::span<const T> placed) {
  return picture.draw_entities ? placed : std::span<const T>{};
}
// Whether the map's textures are drawn crisp in `look` under gl_texturemode: its first filter
// (NEAREST or LINEAR, any case, the GL_ prefix optional), else the look's own (classic crisp).
bool crisp_textures(const Picture& picture, Look look);
// r_waterwarp's breath: the view's height swells and shrinks by kWaterWarp of itself, kWaterWarpHz
// times a second, while the eye is in water, slime or lava, so the liquid reads as a lens round it.
inline constexpr double kWaterWarp = 0.035, kWaterWarpHz = 0.45;
// The field of view `fov_y` (rad) `t` s after the eye went into a liquid (`in_liquid`): as it is
// out of one or with r_waterwarp 0, breathing by kWaterWarp at kWaterWarpHz in one, from rest at
// t = 0.
double warped_fov(const Picture& picture, bool in_liquid, double fov_y, double t);
// r_lightmap's faces: a middle grey (sRGB), so the light reads as it falls, neither burnt white
// nor lost in the dark.
inline constexpr double kLightmapGrey = 0.5;
// A colour cvar's words ("R G B", each 0..255; one number a grey) into `out` (0..1 each); false,
// and `out` kept, for anything else.
bool rgb_words(std::string_view text, double out[3]);
// The rows of the palette a player's colour can name (0..13).
inline constexpr int kColourRows = 14;
// A teamcolor or enemycolor command's words: "off" (-1, -1), one colour for both, or top and
// bottom, each clamped to 0..kColourRows - 1; false (nothing changed) for anything else.
bool parse_forced_colours(std::string_view top, std::string_view bottom, int* out_top,
                          int* out_bottom);
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
// The maps' light's scale over the default (`light_intensity` x `light_modulate` / 2): 1 at the
// defaults.
double light_scale(const Picture& picture);
// The walls' light's scale: `light_scale` times Quake II's gl_modulate_world (1 in every other
// game).
double world_light_scale(const Picture& picture);
// The models' light over the brightness the world's options give them in `look`: Quake II's
// gl_modulate times gl_modulate_entities, laid on twice under gl_doublelight_entities; in the
// modern look, whose models already take the world's brightness, over the world's modulate. 1 at
// the defaults and in every other game.
double model_light(const Picture& picture, Look look);
// The classic grade: every entry the nearest (in sRGB) of `palette`'s colours (768 bytes, RGB), in
// render/post.hpp's `identity_grade` layout. An empty palette gives the identity.
std::vector<float> palette_grade(std::span<const uint8_t> palette);
// The grade `table` brightened by `gamma` (each entry raised to it: 1 unchanged, 0.5 brighter),
// after the palette's pick, so the classic look keeps its 256 colours' choice and is lit like the
// classic game's own brightness slider.
std::vector<float> gamma_grade(std::vector<float> table, double gamma);

}  // namespace ge::classic::look
