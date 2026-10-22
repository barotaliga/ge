// R25 — A REAL FACE: a real mountainside's heights over the map's square, read back off the
// grid the bake keeps (real_faces/, one generated file a face) and sampled smoothly.
//
// THE GRID: `kFaceN` samples a side `kFaceCell` m apart (as fine as a 30 m elevation model
// honestly is), row 0 the map's top edge (z = 0), column 0 its left (x = 0). It is kept as the
// face's lowest height (`floor`, whole metres) and every sample in `kFaceStep` m steps over it,
// each step count second-differenced against its left, upper and upper-left neighbours (a
// neighbour off the grid reads 0), zig-zagged (n >= 0 to 2n, n < 0 to -2n-1), written as base-128
// varints (low seven bits first, the top bit set on every byte but a value's last) and the bytes
// as base64. A mountainside is smooth, so most differences fit a byte: about 12 KB a face.
//
// THE SAMPLE: `face_height` is Catmull–Rom bicubic between the samples, the edges held, so a 2 m
// grid baked off it has no creases at 32 m.
//
// THE GLOBE: `globe_at` and `map_at` turn a map point of a face to (lat, lon) and back, on a
// sphere's metres a degree (111,320 m of latitude, that times cos(lat) of longitude at the
// face's middle): what the bake crops the elevation model by and what a hint of the real ski
// area is laid on the map by.
//
// The elevation data: Copernicus DEM GLO-30, © DLR e.V. 2010–2014 and © Airbus Defence and
// Space GmbH 2014–2018, provided under COPERNICUS by the European Union and ESA; free to use and
// redistribute with this notice.
//
// ge:summary R25 a real face's heights: the kept grid decoded, sampled bicubically, the globe
// ge:keywords real-face face-height face-grid decode bicubic catmull-rom globe-at map-at r25
// ge:skill mountain
// ge:test tests/mountain_real_face_test.cpp
#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mountain/mapgen/real_face_crops.hpp"

namespace ge::mountain::mapgen {

// The grid: samples a side, their spacing (m) and a height's step (m).
inline constexpr int kFaceN = 126;
inline constexpr double kFaceCell = 32;
inline constexpr double kFaceStep = 0.5;

// One face as baked (real_faces/): its lowest height, m, and the grid as base64.
struct FaceData {
  const char* id;
  double floor;
  const char* data;
};

// Every face baked, in the crops' order (real_faces/index.cpp, generated).
std::span<const FaceData* const> baked_faces();

// One face decoded: its crop and its heights, m, `kFaceN` × `kFaceN`, row 0 the map's top edge.
struct RealFace {
  const FaceCrop* crop = nullptr;
  std::vector<float> heights;
};

// The crop of the face `id`, or null for an id no face has.
const FaceCrop* face_crop(std::string_view id);

// The face `id` decoded, or nothing for an id no face has or one not baked.
std::optional<RealFace> real_face(std::string_view id);

// The grid `data` (base64) over `floor` decoded to heights, or nothing when it is not a whole
// grid. `encode_face_grid` is its inverse over heights already on the step.
std::optional<std::vector<float>> decode_face_grid(std::string_view data, double floor);
std::string encode_face_grid(std::span<const int> steps);

// The face's real height at map point (x, z), m: bicubic between its samples.
double face_height(const RealFace& face, double x, double z);

// The mean of the face's heights over the box x0..x1, z0..z1, read every `kFaceCell` m.
double face_mean(const RealFace& face, double x0, double x1, double z0, double z1);

// The x between x0 and x1 (every `kFaceCell` m) whose column stands highest over the rows z0..z1,
// or lowest with `low`.
double face_extreme(const RealFace& face, double x0, double x1, double z0, double z1,
                    bool low = false);

// Where the map point (x, z) of a face lies on the globe, (lat, lon) in °, and back.
std::pair<double, double> globe_at(const FaceCrop& crop, double x, double z);
std::pair<double, double> map_at(const FaceCrop& crop, double lat, double lon);

}  // namespace ge::mountain::mapgen
