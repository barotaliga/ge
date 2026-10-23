// THE REAL STARS: the stars the eye can see (a bright-star catalogue's 9,096 with a magnitude, to
// the eighth), each at its place on the celestial sphere (J2000), its visual magnitude and its
// colour index B-V, as the sky pass draws them (render/sky.wgsl's `stars`).
//
// - THE TABLE is generated (render/star_data.cpp, by the `stars` lab's --bake off the catalogue's
//   text) and decoded once here: six bytes a star, brightest first.
// - THE MAP the GPU reads is the sphere cut as a cube (the face whose axis is largest, each face
//   kStarCells cells a side, gnomonic), each cell holding up to kStarSlots stars as their place in
//   the cell (0..1 a side), magnitude and B-V; an empty slot's magnitude is kNoStar. The sky pass
//   looks in the pixel's cell and the eight round it, so a star near an edge is whole. A cell with
//   more stars than slots keeps its brightest (the `stars` lab counts the ones dropped).
// - THE SPHERE turns with the sky as render/view.hpp's `Night` says (the sidereal angle about
//   the pole the latitude lifts), so the constellations stand where the place and the hour put
//   them. The positions are the catalogue's epoch's: the precession since then, under half a
//   degree, is not applied.
//
// ge:summary The real stars: the catalogue decoded, cut into the cube map the sky pass reads
// ge:keywords render stars star-catalogue bright-star magnitude colour-index b-v constellations
// ge:keywords celestial-sphere right-ascension declination star-map star_catalogue star_cells
// ge:skill render
// ge:test tests/render_stars_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/device.hpp"

namespace ge::render {

struct Star {
  double ra = 0;   // right ascension, rad (J2000)
  double dec = 0;  // declination, rad (J2000)
  double mag = 0;  // visual magnitude
  double bv = 0;   // colour index B-V
};

// The catalogue, brightest first, decoded once from the generated table.
const std::vector<Star>& star_catalogue();

// The unit vector toward a star on the celestial sphere: x toward the vernal equinox, z toward
// the north pole (render/sky.wgsl's `celestial` frame).
void star_direction(const Star& s, double out[3]);

// ── The map the sky pass reads ──

inline constexpr int kStarCells = 160;    // a cube face's cells a side
inline constexpr int kStarSlots = 3;      // stars a cell holds
inline constexpr double kNoStar = 10.75;  // an empty slot's magnitude (its byte's most)

struct StarCell {
  float u = 0, v = 0;  // the star's place in its cell, 0..1 a side
  float mag = static_cast<float>(kNoStar);
  float bv = 0;
};

// The cube cell a direction falls in: the face (0..5: +x, +y, +z, -x, -y, -z) and the cell's
// column and row, with the place in it.
struct CubeCell {
  int face = 0, col = 0, row = 0;
  double u = 0, v = 0;
};
CubeCell cube_cell(const double d[3]);

// The map: kStarSlots layers of 6 kStarCells x kStarCells cells (face-major along x), and how
// many stars no slot held.
struct StarMap {
  std::vector<StarCell> cells;  // [slot][row][face * kStarCells + col]
  int dropped = 0;
};
StarMap star_map(const std::vector<Star>& stars);

// The map on the GPU: an RGBA8 2D array (kStarSlots layers), read with textureLoad: the place in
// the cell in 255ths, the magnitude and B-V as the table holds them (a byte each).
struct StarTexture {
  wgpu::Texture texture;
  wgpu::TextureView view;
};
StarTexture create_star_texture(gpu::Device& device);

// ── The table's bake (the `stars` lab) ──

// One line of the catalogue's fixed-width text as a star; false for a line with no position or
// no magnitude (a nova's or a cluster's left in the catalogue's numbering).
bool parse_catalogue_line(std::string_view line, Star& out);
// The stars as the generated table's base64 (six bytes each: right ascension in 2^16ths of a
// turn, declination in 32767ths of a right angle, the magnitude in twentieths over -2 and B-V
// in seventieths over -0.5), and back.
std::string encode_stars(const std::vector<Star>& stars);
bool decode_stars(std::string_view base64, std::vector<Star>& out);

}  // namespace ge::render
