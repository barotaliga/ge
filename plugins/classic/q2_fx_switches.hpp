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
// - The rail's trail, `cl_railtrail_type`: 0 the game's own specks, 1 a core beam, 2 the core and
//   a line spiralling round it, fading over `cl_railtrail_time` seconds; the core's colour and
//   width in units (`cl_railcore_color`, `cl_railcore_width`), the spiral's colour and radius
//   (`cl_railspiral_color`, `cl_railspiral_radius`). A colour is a name (black, red, green,
//   yellow, blue, cyan, magenta, white) or #rgb or #rrggbb (`q2_colour`).
// - The hit markers, on the damage the server reports the player dealt: `cl_hit_markers` 0 none,
//   1 the pack's `marker` picture in red at the screen's centre, 2 the picture and
//   `weapons/marker.wav`; the picture fades over `scr_hit_marker_time` ms (500) by the square of
//   the time gone (`q2_hit_marker_alpha`), a new hit starting it over.
//
// ge:summary Quake II's effect switches: specks, explosions, lights and muzzle flashes left out
// ge:keywords q2 cl_disable_particles cl_disable_explosions cl_dlight_hacks cl_muzzlelight_time
// ge:keywords cl_muzzleflashes cl_footsteps s_ambient effect-switches q2_fx_switches
// ge:keywords cl_railtrail_type cl_railtrail_time cl_railcore_color cl_railspiral_color q2_colour
// ge:keywords cl_hit_markers scr_hit_marker_time hit-marker q2_hit_marker_alpha
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
  int no_particles = 0;                                     // cl_disable_particles
  int no_explosions = 0;                                    // cl_disable_explosions
  int light_hacks = 0;                                      // cl_dlight_hacks
  double muzzle_light_ms = 16;                              // cl_muzzlelight_time
  bool muzzle_models = true;                                // cl_muzzleflashes
  int footsteps = 1;                                        // cl_footsteps
  int ambient = 1;                                          // s_ambient
  int rail_type = 0;                                        // cl_railtrail_type
  double rail_time = 1;                                     // cl_railtrail_time, seconds
  std::string core_colour = "red", spiral_colour = "blue";  // cl_railcore_color, _spiral_
  double core_width = 2, spiral_radius = 3;  // cl_railcore_width, cl_railspiral_radius: units
  int hit_markers = 2;                       // cl_hit_markers
  double hit_marker_ms = 500;                // scr_hit_marker_time
};

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
