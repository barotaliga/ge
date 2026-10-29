// OTHER WORLDS' MOONS in a sky: ten kinds of moon, each the numbers of a real moon of another
// planet (its size and shape, its albedo and colour, how far it orbits over its planet's
// surface, its period, tilt and eccentricity, and how it spins), and a SET of up to kMaxMoons of
// them a game hangs over a sky, with or without the Earth's own moon. render/night.hpp's
// `set_moons` places a set at a moment and lights the night by it; render/sky.wgsl draws each
// (render/wgsl/moons.wgsl).
//
// WHAT EACH KIND IS:
// - ITS SIZE as seen: its mean radius over its orbit's height above its planet's surface (its
//   planet's radius taken off the orbit's), so each looks as large as it does from its own
//   planet: Io a little larger than the Earth's moon, Phobos a third of it, Deimos a point.
// - ITS SHAPE: its three semi-axes (Phobos's and Deimos's lumpy potatoes, the round ones nearly
//   spheres) and how deeply its surface is dented (`lump`, a share of its long semi-axis).
// - ITS SURFACE: its geometric albedo (the full disc's brightness against a white Lambert disc's
//   of its size, so Enceladus, over 1, is brighter than fresh snow), its colour (luminance 1),
//   how much of its light is a dusty body's (Lommel-Seeliger's law, the Earth's moon's) and how
//   much an icy one's (Lambert's) and a haze round it (Titan's, a share of its radius); its face
//   is the real moon's map (render/moon_maps.hpp), a layer a kind in this order.
// - ITS ORBIT: its sidereal period, its inclination (to its planet's equator, which is taken as
//   the sky's celestial equator: a moon crosses the sky near it; over 90 degrees it goes round
//   backwards, as Triton does) and eccentricity, and where it stood at the epoch. A moon whose
//   period is shorter than the sky's day (Phobos) rises in the west.
// - ITS SPIN: every one of the ten is locked to its planet (it keeps one face toward it, so the
//   same face toward the eye), rocking by twice its eccentricity in longitude (its optical
//   libration); a set may spin one freely (`MoonPick::spin_hours`).
// The phase is the geometry's: the face toward the real sun is lit, the rest dark (no planet
//   lights it), so two moons in different places show different phases.
//
// A SET is a list of words (`moon_set_named`): kind names, each with an optional size factor
// and spin (`io:2`, `phobos:1:3`), and `moon` for the Earth's own; `none` is an empty sky. Each
// sky type names its own set (render/sky_style.hpp's SkyType::moons): the Earth's only its
// moon, the dusty desert planet Phobos and Deimos, the green air three of Jupiter's.
//
// ge:summary Other worlds' moons: ten real moons' sizes, shapes, surfaces, orbits and spins, sets
// ge:keywords render moons moon-kinds moon-set other-moons alien-moons phobos deimos io europa
// ge:keywords ganymede callisto titan enceladus miranda triton libration retrograde orbit
// ge:keywords moon_kinds moon_kind_named moon_set_named irregular-moon haze
// ge:skill render
// ge:test tests/render_moons_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>

namespace ge::render {

// The most moons of other worlds a sky draws at once, and the frame block's rows for each
// (render/frame.hpp's `moons`, render/wgsl/frame_types.wgsl's): toward it and its drawn angular
// radius; its north pole and its kind; its 0 N 0 E and its surface's luminance, pre-exposed;
// its tint and Lambert's share; its semi-axes against the longest and its dents; its haze.
inline constexpr int kMaxMoons = 4;
inline constexpr int kMoonRows = 6;

struct MoonKind {
  std::string_view name;     // the word a set names it by
  std::string_view caption;  // a line for a sheet's caption
  double radius_km;          // mean radius
  double axes_km[3];         // semi-axes: toward its planet, along its orbit, along its pole
  double height_km;          // its orbit's height above its planet's surface (mean)
  double albedo;             // geometric
  double tint[3];            // its colour, luminance 1
  double period_days;        // sidereal orbit
  double inclination_deg;    // to the sky's celestial equator; over 90: retrograde
  double eccentricity;       // its optical libration is twice it, rad
  double epoch_deg;          // its orbit's angle at the epoch (jd 2451545), degrees
  double lambert;            // its light's share that is Lambert's (the rest Lommel-Seeliger's)
  double lump;               // the depth of its dents, a share of its long semi-axis
  double haze;               // a haze round it, its depth a share of the radius (0: none)
};

// The ten kinds, in a fixed order (a kind's index is its number in the frame's rows).
std::span<const MoonKind> moon_kinds();
// The index of the kind named `name` (any case), or -1.
int moon_kind_named(std::string_view name);
// The kinds' names as one line, for a diagnostic's next step.
std::string moon_kind_list();
// A kind's angular radius from its planet's surface, rad (its mean radius over its height).
double moon_kind_radius(const MoonKind& kind);

// One moon of a set: its kind, its size against the kind's own, and its spin (hours a turn;
// negative: the kind's own, locked; 0 also locked).
struct MoonPick {
  int kind = -1;
  double size = 1;
  double spin_hours = -1;
};

// The moons over a sky: whether the Earth's moon is one, and up to kMaxMoons others.
struct MoonSet {
  bool earth = true;
  int count = 0;
  MoonPick moons[kMaxMoons];
};

// `out` from a line of words: kind names each with an optional `:size` and `:spin` (hours),
// `moon` for the Earth's own, `none` for no moon at all. Every word read: true and `problem`
// empty; a word that names nothing, a number that does not read, or more than kMaxMoons others:
// false, `problem` saying which and what to set, `out` holding what did read.
bool moon_set_named(std::string_view words, MoonSet& out, std::string& problem);

}  // namespace ge::render
