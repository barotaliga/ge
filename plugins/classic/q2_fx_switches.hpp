// QUAKE II'S EFFECT SWITCHES: the player's own say over which of the game's effects are drawn and
// how its lights behave, as its own cvars set them while it is the game loaded (the window's
// cvar layers register them; another game keeps the defaults below, which change nothing).
//
// - `cl_disable_particles`, a bit sum of the specks left out: 1 a grenade's explosion, 2 a
//   grenade's trail, 4 a rocket's explosion, 8 a rocket's trail, 16 blood, 32 a blaster bolt's
//   trail.
// - `cl_disable_explosions`, a bit sum of the explosions' balls of fire left out (their light and
//   specks stay): 1 a grenade's, 2 a rocket's. `kQ2Grenade` and `kQ2Rocket` are those bits, and
//   name an explosion's kind where the effects keep it.
// - `cl_dlight_hacks`, a bit sum: 2 every explosion's light half as far, 4 no light from a machine
//   gun's or a chain gun's flash (1, a rocket's light in another colour, is not taken).
// - `cl_muzzlelight_time`: how long a gun's flash lights the room, milliseconds (16 as drawn; the
//   light's whole fade scales with it, 0 none).
// - `cl_muzzleflashes` 0: no fire drawn at another player's or a monster's gun (the light stays).
// - `cl_footsteps` 0: no footstep heard (1 and 2 the same: the game's own four steps).
// - `s_ambient`, the loops an entity carries that are heard: 0 none, 1 all, 2 only those of an
//   entity with a model (a speaker on its own is silent), 3 only the listener's own.
// - `al_merge_looping`: the entities carrying one looped sound heard as one voice
//   (q2_loop_merge.hpp): 0 never, 1 while the output is plain stereo (the default), 2 always.
// - The rail's trail, `cl_railtrail_type`: 0 the game's own specks, 1 a core beam, 2 the core and
//   a line spiralling round it, fading over `cl_railtrail_time` seconds; the core's colour and
//   width in units (`cl_railcore_color`, `cl_railcore_width`), the spiral's colour and radius
//   (`cl_railspiral_color`, `cl_railspiral_radius`). A colour is a name (black, red, green,
//   yellow, blue, cyan, magenta, white) or #rgb or #rrggbb (`q2_colour`).
// - The hit markers, on the damage the server reports the player dealt: `cl_hit_markers` 0 none,
//   1 the pack's `marker` picture in red at the screen's centre, 2 the picture and
//   `weapons/marker.wav`; the picture fades over `scr_hit_marker_time` ms (500) by the square of
//   the time gone (`q2_hit_marker_alpha`), a new hit starting it over.
// - THE PICTURE (none saved in the config): `gl_partscale` the specks' size, grown with depth
//   (`q2_speck_scale`; another game's 0: as made), `gl_partstyle` 1 added to what is behind,
//   `gl_partshape` a soft dot, a square or a sharp dot (-1: the engine's capsule); `gl_beamstyle`
//   1 a laser or the rail's core without its glow; `gl_drawsky` 0 a black sky, `gl_bilerp_skies`
//   0 a sharp one; `gl_saturation` and `gl_invert` on the walls' and floors' textures alone;
//   `gl_waterwarp` the view rippled under a liquid; `gl_fog` 0 no fog at all (`gl_anisotropy`
//   is every game's: the classic look's `anisotropy`).
// - THE LIGHT: `gl_modulate_world` and `gl_modulate_entities` times `gl_modulate` on the walls and
//   on the models, `gl_doublelight_entities` the models' share twice, `gl_dotshading` 0 a model
//   lit evenly, `gl_coloredlightmaps` the walls' light's colour, `gl_dlight_falloff` a flash's
//   edge on the walls (1 soft, 0 hard; -1 the renderer's own), `cl_lerp_lightstyles` 1 the styles
//   blended between letters; `cl_rollhack` 1 the other players lean the game's own way.
// - `gl_celshading` an ink line round each solid model, pixels (`q2_outline_px`);
//   `cl_smooth_explosions` 0 an explosion moved on by whole frames (`q2_blast_age`).
//
// ge:summary Quake II's effect switches: specks, explosions, lights and muzzle flashes left out
// ge:keywords q2 cl_disable_particles cl_disable_explosions cl_dlight_hacks cl_muzzlelight_time
// ge:keywords cl_muzzleflashes cl_footsteps s_ambient effect-switches q2_fx_switches
// ge:keywords cl_railtrail_type cl_railtrail_time cl_railcore_color cl_railspiral_color q2_colour
// ge:keywords cl_hit_markers scr_hit_marker_time hit-marker q2_hit_marker_alpha q2_speck_scale
// ge:keywords gl_partscale gl_partstyle gl_partshape gl_beamstyle gl_drawsky gl_bilerp_skies
// ge:keywords gl_saturation gl_invert gl_anisotropy gl_waterwarp gl_fog gl_modulate_world
// ge:keywords gl_modulate_entities gl_doublelight_entities gl_coloredlightmaps gl_dotshading
// ge:keywords cl_lerp_lightstyles cl_rollhack gl_celshading q2_outline_px cl_smooth_explosions
// ge:keywords q2_blast_age al_merge_looping
// ge:skill classic shooter
// ge:test tests/shooters_edge_cvar_layers_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace ge::classic {

inline constexpr uint8_t kQ2Grenade = 1, kQ2Rocket = 2;  // an explosion's kind (0: any other)

struct Q2FxSwitches {
  int no_particles = 0;         // cl_disable_particles
  int no_explosions = 0;        // cl_disable_explosions
  int light_hacks = 0;          // cl_dlight_hacks
  double muzzle_light_ms = 16;  // cl_muzzlelight_time
  bool muzzle_models = true;    // cl_muzzleflashes
  int footsteps = 1;            // cl_footsteps
  int ambient = 1;              // s_ambient
  int merge_looping = 1;  // al_merge_looping: 0 never, 1 in plain stereo, 2 always (q2_loop_merge)
  bool plain_stereo = true;  // not a setting: the output's (false while s_hrtf 2 gives two ears)
  int rail_type = 0;         // cl_railtrail_type
  double rail_time = 1;      // cl_railtrail_time, seconds
  std::string core_colour = "red", spiral_colour = "blue";  // cl_railcore_color, _spiral_
  double core_width = 2, spiral_radius = 3;  // cl_railcore_width, cl_railspiral_radius: units
  int hit_markers = 2;                       // cl_hit_markers
  double hit_marker_ms = 500;                // scr_hit_marker_time
  double part_scale = 0;                     // gl_partscale (0: each speck its own size)
  bool additive_parts = false;               // gl_partstyle
  int part_shape = -1;                       // gl_partshape (-1: the engine's own)
  bool round_beams = false;                  // gl_beamstyle
  bool draw_sky = true;                      // gl_drawsky
  bool smooth_sky = true;                    // gl_bilerp_skies
  double saturation = 1;                     // gl_saturation
  bool invert = false;                       // gl_invert
  bool water_warp = false;                   // gl_waterwarp
  bool fog = true;                           // gl_fog
  double modulate_world = 1;                 // gl_modulate_world
  double modulate_entities = 1;              // gl_modulate_entities
  bool double_light = false;                 // gl_doublelight_entities (Quake II's 1)
  bool dot_shading = true;                   // gl_dotshading
  double lightmap_saturation = 1;            // gl_coloredlightmaps
  bool lerp_styles = false;                  // cl_lerp_lightstyles
  bool roll_hack = false;                    // cl_rollhack (Quake II's 1)
  int dlight_falloff = -1;                   // gl_dlight_falloff (-1: the renderer's own)
  double cel_shading = 0;                    // gl_celshading, pixels (0: none)
  bool smooth_explosions = true;             // cl_smooth_explosions
  double glowmap_intensity = 0.75;           // gl_glowmap_intensity
};

// A glow map's light on a model (gl_glowmap_intensity): its colour times twice the setting, added
// whatever the light round it.
inline float q2_glow_map_gain(double intensity) { return static_cast<float>(2 * intensity); }

// gl_partscale's share of a speck's own size `depth_units` in front of the eye: 1 at the default
// 2 within 20 units, past them 1 and 0.004 a unit of the whole depth (5 at 1000 units), times
// `part_scale` / 2; 1 with it 0 (another game's: the speck as made).
inline double q2_speck_scale(const Q2FxSwitches& s, double depth_units) {
  if (s.part_scale <= 0) return 1;
  const double grown = depth_units > 20 ? 1 + 0.004 * depth_units : 1;
  return grown * s.part_scale / 2;
}

// gl_celshading's ink line round a model `depth_units` from the eye, `cel` the setting: the
// share of its width and of its opacity, 1 - depth / 700; 0 (none) with the setting 0 or once the
// share is under 0.01 (past about 693 units). Its width in pixels: the setting (at most 10) times
// the share.
inline double q2_outline_share(double cel, double depth_units) {
  if (cel <= 0) return 0;
  const double share = 1 - depth_units / 700;
  return share < 0.01 ? 0 : share;
}
inline double q2_outline_px(double cel, double depth_units) {
  return (cel > 10 ? 10 : cel) * q2_outline_share(cel, depth_units);
}

// cl_smooth_explosions 0: an explosion `age_s` old as the classic stepping shows it, its ball and
// its light moved on only every tenth of a second (a frame of the game's); 1: as it is.
inline double q2_blast_age(const Q2FxSwitches& s, double age_s) {
  if (s.smooth_explosions || age_s <= 0) return age_s;
  return static_cast<double>(static_cast<long long>(age_s * 10)) / 10;
}

// The hit marker's opacity `age_s` after the last hit: 1 at the hit, 1 - f*f with f the share of
// `scr_hit_marker_time` gone, 0 past it (or with the markers off).
inline float q2_hit_marker_alpha(const Q2FxSwitches& s, double age_s) {
  if (s.hit_markers <= 0 || s.hit_marker_ms <= 0 || age_s < 0) return 0;
  const double f = age_s * 1000 / s.hit_marker_ms;
  return f > 1 ? 0.0f : static_cast<float>(1 - f * f);
}

// A colour as Quake II's settings write one, 0..1 each: a name or #rgb or #rrggbb (false and
// `rgb` untouched when it is neither).
inline bool q2_colour(std::string_view text, float rgb[3]) {
  static constexpr std::array<std::pair<std::string_view, uint32_t>, 8> kNames{
      {{"black", 0x000000},
       {"red", 0xff0000},
       {"green", 0x00ff00},
       {"yellow", 0xffff00},
       {"blue", 0x0000ff},
       {"cyan", 0x00ffff},
       {"magenta", 0xff00ff},
       {"white", 0xffffff}}};
  for (const auto& [name, v] : kNames)
    if (text == name) {
      for (int k = 0; k < 3; ++k) rgb[k] = static_cast<float>((v >> (16 - 8 * k)) & 0xff) / 255;
      return true;
    }
  if (text.size() != 4 && text.size() != 7) return false;
  if (text[0] != '#') return false;
  uint32_t v = 0;
  for (const char c : text.substr(1)) {
    const int d = c >= '0' && c <= '9'   ? c - '0'
                  : c >= 'a' && c <= 'f' ? c - 'a' + 10
                  : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                         : -1;
    if (d < 0) return false;
    v = v * 16 + static_cast<uint32_t>(d);
  }
  if (text.size() == 4)  // #rgb: each digit twice
    v = ((v >> 8) & 0xf) * 0x110000 + ((v >> 4) & 0xf) * 0x1100 + (v & 0xf) * 0x11;
  for (int k = 0; k < 3; ++k) rgb[k] = static_cast<float>((v >> (16 - 8 * k)) & 0xff) / 255;
  return true;
}

// The specks of an explosion of `kind` are left out.
inline bool q2_no_blast_specks(const Q2FxSwitches& s, uint8_t kind) {
  return (kind == kQ2Grenade && (s.no_particles & 1) != 0) ||
         (kind == kQ2Rocket && (s.no_particles & 4) != 0);
}
// Its ball of fire is left out.
inline bool q2_no_blast_ball(const Q2FxSwitches& s, uint8_t kind) {
  return (s.no_explosions & kind) != 0;
}
// A gun's flash light of `age` seconds as one of `life` seconds drawn would be at the age the
// switch makes it (past `life`: gone).
inline double q2_muzzle_age(const Q2FxSwitches& s, double age) {
  return s.muzzle_light_ms > 0 ? age * 16 / s.muzzle_light_ms : 1e9;
}

}  // namespace ge::classic
