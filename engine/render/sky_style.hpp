// THE SKY'S STYLE: what kind of sky a world has, beside where its sun stands. The default is the
// Earth's (every number neutral, so a picture under it is the picture without it); a named SKY
// TYPE (`sky_types`) is a set of these numbers for a world that is not the Earth: a dusty desert
// planet, a green alien air under two suns, a red hellish one glowing from below, an airless void
// with a nebula and a ringed planet. A game picks a type by name (`sky_style_named`, a cvar's
// word) and turns its KNOBS (`SkyKnobs`, each negative to keep the type's own) on top.
//
// WHAT EACH PART DOES:
// - THE AIR (physical): the scale on the Earth's Rayleigh scattering per colour channel, on the
//   dust's (Mie) scattering and on its absorption per channel, and on the ozone. The atmosphere's
//   tables are built from them (render/wgsl/atmosphere.wgsl's `medium_at`), so the sky's colour,
//   the sun's colour through it, the sky's light on the ground, the haze and the cloud's light all
//   follow: a redder Rayleigh is a red sky, a dusty air a butterscotch one with a blue sunset, no
//   air a black sky round a white sun. The tables are rebuilt when the air changes.
// - THE SUN'S DISC: its size against the real one and its tint (the disc alone; its light is the
//   air's to colour).
// - A SECOND SUN: a disc beside the sun (an offset in heading and elevation from it), its size,
//   tint and brightness against the sun's disc, with a glow round it. It is seen, not a light.
// - A PLANET: a large disc off the sun's heading by an angle and at an elevation, so its phase is
//   the type's whatever the hour, lit by the sun as a ball is,
//   night side faintly lit, its tint, and a ring about it (its outer radius in the planet's,
//   tilted toward the eye by an angle; the near half crosses the planet, the far half is hidden).
// - A NEBULA: a coloured cloud on the star sphere (two tints over a mottled field), seen by day
//   when it is bright enough.
// - THE STARS BY DAY: the share of the night's stars seen whatever the sky's brightness.
// - A GLOW from under the horizon (a world on fire, a city below): its tint and strength, and how
//   high it climbs.
// Only the air is physical; the rest is drawn over the sky in its pass (render/sky.wgsl), costs
// nothing when its strength is 0, and lights nothing.
//
// ge:summary Sky types: a world's air, extra suns, a planet, a nebula, a glow; named and tunable
// ge:keywords render sky sky-type sky-style alien-sky other-world rayleigh mie dust air-colour
// ge:keywords second-sun twin-suns planet ring nebula stars-by-day horizon-glow hellish void
// ge:keywords sky_types sky_style_named with_knobs style_rows sky-knobs
// ge:skill render
// ge:test tests/render_sky_style_test.cpp
#pragma once

#include <span>
#include <string_view>

namespace ge::render {

struct SkyStyle {
  // THE AIR: scales on the Earth's air (1: the Earth's).
  double rayleigh[3] = {1, 1, 1};     // Rayleigh scattering, red, green, blue
  double dust = 1;                    // the dust's (Mie) scattering
  double dust_absorb[3] = {1, 1, 1};  // the dust's absorption, per channel
  double dust_g = 0.8;                // the dust's asymmetry: 0.8 the Earth's haze, lower for a
                                      // coarse dust that lights the whole sky
  double dust_height = 1.2;           // the dust's scale height, km: 1.2 the Earth's haze, near
                                      // the air's own (8) for a dust that fills the sky
  double ozone = 1;
  // THE SUN'S DISC: its radius against the real one's and its tint.
  double sun_size = 1;
  double sun_tint[3] = {1, 1, 1};
  // A SECOND SUN (0: none): its disc's brightness against the sun's, its offset from the sun
  // (heading and elevation, rad), its radius against the real sun's and its tint.
  double sun2 = 0;
  double sun2_heading = 0;
  double sun2_elevation = 0;
  double sun2_size = 1;
  double sun2_tint[3] = {1, 1, 1};
  // A PLANET (0: none): its angular radius (rad), where it stands (its heading's offset from the
  // sun's and its elevation, rad), its albedo's tint, its ring's outer radius in the planet's (0:
  // none) and the ring's tilt toward the eye (rad; 0 edge on).
  double planet = 0;
  double planet_heading = 0;
  double planet_elevation = 0.5;
  double planet_tint[3] = {1, 1, 1};
  double ring = 0;
  double ring_tilt = 0.3;
  // A NEBULA (0: none): its strength (its brightest against the sunlit snow's white: 1 as bright),
  // its two tints and the size of its mottle (1: about a tenth of the sky across).
  double nebula = 0;
  double nebula_a[3] = {1, 0.3, 0.6};
  double nebula_b[3] = {0.3, 0.5, 1};
  double nebula_size = 1;
  // THE STARS BY DAY: the share of the stars seen whatever the sky's brightness (0..1).
  double stars = 0;
  // A GLOW FROM BELOW (0: none): its strength at the horizon against the sunlit snow's white (1:
  // as bright), its tint and the elevation it climbs to (rad, an e-fold).
  double glow = 0;
  double glow_tint[3] = {1, 0.35, 0.1};
  double glow_height = 0.12;
};

// A named sky type: the word a cvar takes, a line for a sheet's caption, and its style.
struct SkyType {
  std::string_view name;
  std::string_view caption;
  SkyStyle style;
};

// Every sky type, the Earth's first.
std::span<const SkyType> sky_types();
// The style of the type named `name` (any case); false, `out` untouched, for an unknown word.
bool sky_style_named(std::string_view name, SkyStyle& out);
// The type names as one line ("earth, dust, ..."), for a diagnostic's next step.
std::string_view sky_type_list();

// THE KNOBS over a type: each negative keeps the type's own.
struct SkyKnobs {
  double air = -1;   // a scale on the type's whole air (Rayleigh and dust): 0 none, 2 twice
  double dust = -1;  // the dust's scattering, as SkyStyle's (a scale on the Earth's)
  double sun_size = -1;
  double sun2 = -1;              // the second sun's brightness (0 hides it)
  double planet = -1;            // the planet's angular radius, DEGREES (0 hides it)
  double planet_heading = -1;    // degrees from the sun's heading, 0..360
  double planet_elevation = -1;  // degrees, 0..90
  double ring = -1;
  double nebula = -1;
  double stars = -1;
  double glow = -1;
};
// `style` with the knobs that are set turned.
void with_knobs(SkyStyle& style, const SkyKnobs& knobs);

// The style as the frame block's rows (render/wgsl/frame_types.wgsl's `style`, kSkyStyleRows
// rows of four): `sun_dir` is the unit direction toward the real sun (the second sun stands off
// it).
inline constexpr int kSkyStyleRows = 11;
void style_rows(const SkyStyle& style, const double sun_dir[3], float out[kSkyStyleRows][4]);

// Whether two styles' AIR differs (the atmosphere's tables then rebuild).
bool same_air(const SkyStyle& a, const SkyStyle& b);

}  // namespace ge::render
