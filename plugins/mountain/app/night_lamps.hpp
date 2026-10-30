// THE MOUNTAIN'S LAMPS AFTER DARK (docs/rendering.md § Local lights): the floodlight masts a ski
// area stands along its piste for skiing at night, the village's street lamps, and the warm spill
// of a lit building's windows on the snow before it, as the local lights a game hands its frame
// (render/lights.hpp) and the posts and masts it draws (one instance family).
//
// How a lit piste and a village street are built, and the numbers here:
// - BRIGHTNESS: a recreational night piste is lit to about 20 lx on average and never under 4 lx
//   (a race slope 100 lx and more); laid out here for some 30 lx. A residential street is lit to
//   5 to 15 lx, a shop street to 15 to 20.
// - THE MASTS: about every 50 m down the piste (never more than seven times the lamp's height
//   apart), on one side, 3 m off the piste's edge; the lamps 12 m over the snow beside a narrow
//   run, 16 m beside a broad one (over 40 m), so the far side is not lit at a graze. Two floods
//   a mast, both AIMED DOWNHILL of square across (the way the skier goes, so he never looks into
//   one) and tipped down at least 15 degrees: the NEAR flood 15 degrees downhill at the piste's
//   far half, wide (16 000 cd, 70 degrees off its axis at its last), and the THROW 55 degrees
//   downhill some 40 m out, narrower and brighter (45 000 cd, 45 degrees), into the gap before
//   the next mast; a broader piste's floods brighter by the square of its width over 40 m. Some
//   400 W of LED a mast. Neutral white, 4000 K. So laid, a 40 m piste is lit to a mean of over
//   20 lx and nowhere under 4 (tests/mountain_night_lamps_test.cpp).
// - THE STREET LAMPS: a post 5.5 m tall every 30 m along a street (40 m along the road out), on
//   its sidewalk's side (the main street's alternate), its lamp over the kerb shining down in a
//   wide cone: a 3000 lm LED of 1000 cd. Warm white, 3000 K.
// - THE WINDOWS: a lit building's rooms lay a soft warm pool on the snow before its front: one
//   wide, soft point light a metre out at the ground floor's height, brighter for a wider front.
//   Unshadowed (the building would hide it from itself).
// - ON WITH THE DARK (`lamps_lit`): off by day, coming up as the sun sinks to the horizon and
//   fully on once it is `kLightsOnElevation` under it, when the windows light (building_look.hpp).
//
// The layout is a pure function of the map and the street plan (no stream, no clock). A lamp's
// shadow key is its kind's base plus its index, the same every frame, so the lamps' shadow atlas
// keeps its tiles (render/lamp_shadows.hpp).
//
// ge:summary The mountain's lamps at night: the piste's floodlight masts, street lamps, window
// ge:summary spill, as local lights and posts
// ge:keywords night lamps floodlight flood-mast piste-lights street-lamp lamp-post window-spill
// ge:keywords lamps-lit lamp-lights night-piste village-lights piste_lamps street_lamps
// ge:keywords lamp_post_meshes lamp_instance lux candela
// ge:skill mountain render
// ge:test tests/mountain_night_lamps_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "mountain/village/buildings.hpp"
#include "mountain/village/streets.hpp"
#include "render/instances.hpp"
#include "render/lights.hpp"
#include "render/mesh.hpp"
#include "render/surface.hpp"

namespace ge::mountain::app {

enum class LampKind : uint8_t { flood, street };

// A lamp on its post: the post's foot on the snow, the lamp's height over it, the bearing the
// lamp faces (heading 0 = +z, clockwise from above) and how far it is tipped down, rad; a mast's
// throw flood's own bearing and tip, and the scale on its floods' candela.
struct Lamp {
  LampKind kind = LampKind::flood;
  double x = 0, y = 0, z = 0;
  double height = 0;
  double heading = 0;
  double tilt = 0;
  double throw_heading = 0, throw_tilt = 0;
  double bright = 1;
};

// The numbers (m, lx, cd, degrees).
inline constexpr double kMastEvery = 50;      // down the piste
inline constexpr double kMastFirst = 25;      // the first past the start line
inline constexpr double kMastOut = 3;         // off the piste's edge
inline constexpr double kMastLow = 12;        // the lamps over the snow beside a narrow piste
inline constexpr double kMastHigh = 16;       // and beside a broad one
inline constexpr double kMastBroad = 40;      // the width a piste is broad from
inline constexpr double kNearDownhill = 15;   // the near flood's degrees downhill of square across
inline constexpr double kNearAt = 0.6;        // aimed this share of the width across the piste
inline constexpr double kThrowDownhill = 55;  // the throw's
inline constexpr double kThrowAt = 40;        // aimed this far out, m
inline constexpr double kMastMinTilt = 15;    // degrees under level, at least
inline constexpr float kNearCandela = 16000;
inline constexpr float kThrowCandela = 45000;
inline constexpr float kFloodRange = 90;
inline constexpr double kStreetEvery = 30;
inline constexpr double kRoadEvery = 40;
inline constexpr double kPostHeight = 5.5;
inline constexpr float kStreetCandela = 1000;
inline constexpr float kStreetRange = 22;
inline constexpr float kWindowCandela = 30;  // a window's spill a 6 m of front
inline constexpr float kWindowRange = 11;
// The shadow keys' bases (a lamp's key is its base plus its index); a game keys its own lamps
// (a headlamp) under other bases.
inline constexpr uint32_t kFloodKeys = 0x01000000u;
inline constexpr uint32_t kStreetKeys = 0x02000000u;

// The masts along `level`'s piste (none on a map with no track).
std::vector<Lamp> piste_lamps(const mapgen::Level& level);
// The street lamps along `plan`'s streets (the car park's aisles are lit by the square's).
std::vector<Lamp> street_lamps(const mapgen::Level& level, const village::Streets& plan);

// How far the lamps are on with the sun `sun_elevation` (rad) up: 0 by day, 1 from
// kLightsOnElevation under the horizon, a smooth rise over the 3 degrees before.
double lamps_lit(double sun_elevation);

// The unit way a lamp shines.
void lamp_direction(const Lamp& lamp, float out[3]);

// Append `lamps`' lights at `lit` (0: nothing appended), shadowed when `shadows`: a street
// lamp's one under `key_base` + its index k, a mast's two floods under `key_base` + 2k (the near)
// and + 2k + 1 (the throw).
void lamp_lights(std::span<const Lamp> lamps, double lit, bool shadows, uint32_t key_base,
                 std::vector<render::LocalLight>& out);
// Append each lit building's window spill (`lit[k]` nonzero: `buildings[k]`'s windows lit).
void window_lights(std::span<const village::Building> buildings, std::span<const uint8_t> lit,
                   std::vector<render::LocalLight>& out);

// The posts as drawn: three meshes, the low mast, the high mast and the street lamp, each in the
// post's own frame (its foot at the origin, the lamp's head toward +z, y up). Two parts: the
// steel, the lamp's glass.
std::vector<render::MeshSource> lamp_post_meshes();
// The posts' surfaces: a set dark, then a set lit (the glass glowing).
std::vector<render::Surface> lamp_post_surfaces();
render::Instance lamp_instance(const Lamp& lamp, bool lit);

}  // namespace ge::mountain::app
