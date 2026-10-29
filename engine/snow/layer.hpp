// A LAYER OF SNOW over any ground (docs/snow.md § The model), and the one query everything asks
// of it. A layer is a plan rectangle (`Region`) over a ground (a baked heightfield from any
// source: a terrain, the top faces of a brush world traced down, a flat yard) with a COVER: what
// lies there before anything crosses it, analytic and free (constants and optional grids of depth
// and packed share). Where something has pressed the snow (`press.hpp`), the changed columns are
// kept in FINE TILES of 64 x 64 samples at the layer's cell, allocated on the first imprint, so a
// layer nobody has crossed costs no memory.
//
// THE CELL is a column as fixed-point integers, the same bits on every machine and uploaded to the
// GPU as it is: its depth in mm and its MASS in units of 0.05 kg/m2, so a press that moves snow
// keeps the mass to the unit (the depth is where that mass stands; its density is mass over
// depth). A tile's samples are filled from the cover when it is allocated, and a sample no press
// has changed is flagged untouched: `sample` reads the cover itself there, so an untouched layer
// answers its cover bit for bit (what lets a game move its physics onto a layer without moving a
// digest).
//
// `sample` reads the four samples round a point bilinearly (depth and mass, the density their
// quotient, the packed share), and the cover at the point itself when none of the four was
// changed. Samples outside the region clamp to its edge.
//
// ge:summary A snow layer over any ground: cover, sparse fixed-point tiles, the sample query
// ge:keywords snow layer snow-world cover column depth density mass packed tile cell region
// ge:keywords ground bake_ground sample layer_at add_layer fixed-point untouched mass_units
// ge:skill snow
// ge:test tests/snow_layer_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "snow/kinds.hpp"

namespace ge::snow {

// A plan rectangle, m: [x0, x1] x [z0, z1].
struct Region {
  double x0 = 0, z0 = 0, x1 = 0, z1 = 0;
};

// The ground under the snow: a baked field (any source; `bake_ground`), or a flat height.
struct Ground {
  const core::Heightfield* field = nullptr;
  double height = 0;  // m, where there is no field
};

// What lies there before anything crosses it. Each grid, when set, replaces its constant.
struct Cover {
  double depth = 0.3;  // m of snow over the ground
  double packed = 0;   // 0 loose .. 1 packed
  double density = 0;  // kg/m3; 0 reads it off the packed share by the kind
  Kind kind = Kind::powder;
  const core::Heightfield* depth_grid = nullptr;   // m
  const core::Heightfield* packed_grid = nullptr;  // 0..1
};

struct CoverSample {
  double depth, density, packed;
  Kind kind;
};

struct Weather;  // time.hpp

struct LayerSpec {
  Region region;
  Ground ground;
  Cover cover;
  const Weather* weather = nullptr;  // the snow over time (time.hpp); none: the layer is static
  double cell = 0.1;                 // m between samples
  uint32_t max_tiles =
      2048;           // fine tiles kept (32 KB each); past it the stalest is folded (lod.hpp)
  double near = 64;   // m from a focus actor inside which imprints keep fine detail
  double stale = 60;  // s a fine tile no focus came near is kept before `fold_stale`
  uint32_t max_records = 4096;  // imprint records kept between `clear_records`
};

inline constexpr int kTileCells = 64;      // samples a tile side
inline constexpr double kMassUnit = 0.05;  // kg/m2 a mass unit
inline constexpr uint16_t kMaxDepthMm = 65535;
inline constexpr uint16_t kMaxMass = 65535;

enum CellFlag : uint8_t {
  kTouched = 1,   // changed by a press: the cell, not the cover, is the snow here
  kPressed = 2,   // under a contact (its floor)
  kBerm = 4,      // snow thrown here from beside it
  kCorduroy = 8,  // groomed and not crossed since (time.hpp, `groom`)
};

struct Cell {
  uint16_t depth_mm = 0;
  uint16_t mass = 0;  // kMassUnit units
  Kind kind = Kind::powder;
  uint8_t flags = 0;
  uint16_t minute = 0;  // the layer's clock when it last changed, min
};
static_assert(sizeof(Cell) == 8, "a cell is 8 bytes (docs/snow.md)");

// THE COARSE LAYER (lod.hpp): a column every kCoarseCells samples, kept where a fine tile was
// folded or an imprint far from every focus landed. Its mass and depth are SUMS over the samples
// it stands for, so folding and refining move whole units and keep the mass to the bit.
inline constexpr int kCoarseCells = 16;                           // samples a coarse cell side
inline constexpr int kCoarsePerTile = kTileCells / kCoarseCells;  // coarse cells a tile side

struct CoarseCell {
  uint32_t mass = 0;      // kMassUnit units over its samples
  uint32_t depth_mm = 0;  // mm summed over its samples
  Kind kind = Kind::powder;
  uint8_t flags = 0;    // kTouched where it, not the cover, is the snow
  uint16_t minute = 0;  // when it last changed
};

struct CoarseBlock {
  int32_t tx = 0, tz = 0;  // the tile it lies under
  std::array<CoarseCell, static_cast<size_t>(kCoarsePerTile) * kCoarsePerTile> cells;
};

struct Tile {
  int32_t tx = 0, tz = 0;  // -1: a free slot (the tile was folded)
  double near_t = 0;       // s, the last moment a focus was near it or a press wrote it
  std::array<Cell, static_cast<size_t>(kTileCells) * kTileCells> cells;
};

// What one press left, for the readers that are not the physics (the marks, the spray, the
// sound); see press.hpp.
enum class Motion : uint8_t { step, roll, slide, plough, dig };
enum class ShapeKind : uint8_t { capsule, disc, box, ellipse };

// A contact's outline on the plan. A capsule runs from (ax, az) to (bx, bz); a disc, a box and an
// ellipse are centred on (ax, az), a box and an ellipse `half_length` along `heading` (0 = +z,
// clockwise from above) and `half_width` across it; a disc's radius is `half_width`.
struct Shape {
  ShapeKind kind = ShapeKind::capsule;
  double ax = 0, az = 0, bx = 0, bz = 0;
  double half_width = 0;
  double half_length = 0;
  double heading = 0;
};

struct ImprintRecord {
  Shape shape;
  Motion motion = Motion::step;
  Kind kind = Kind::powder;
  float sink = 0;    // m below the untouched surface, on the axis
  float thrown = 0;  // kg thrown aside
  double t = 0;      // s
};

struct Layer {
  LayerSpec spec;
  int cols = 0, rows = 0;            // samples
  int tile_cols = 0, tile_rows = 0;  // tiles over them
  std::vector<int32_t> slots;        // the tile's index by tile_cols * tz + tx, -1 none
  std::vector<Tile> tiles;
  std::vector<ImprintRecord> records;
  uint64_t mass_in = 0;          // mass units the tiles were given: their cover, and the weather
  uint64_t spilled = 0;          // mass units thrown past the region's edge or onto no tile
  uint32_t records_dropped = 0;  // past max_records
  uint32_t refused = 0;          // samples a press could not keep (no tile left)
  // The level of detail (lod.hpp).
  std::vector<int32_t> free_tiles;    // tile slots a fold emptied, reused first
  std::vector<int32_t> coarse_slots;  // the coarse block's index by tile_cols * tz + tx, -1 none
  std::vector<CoarseBlock> coarse;
  std::vector<double> focus;  // the focus actors' x, z pairs (`set_focus`)
  double clock = 0;           // s, the moment of the operation in progress
  uint32_t folds = 0, refines = 0;
  // What changed since a reader last looked (a GPU mirror, render/snow_mirror.hpp): every write
  // to a tile or a coarse block stamps it with the next `edits`. Nothing in the snow reads them.
  uint64_t edits = 0;
  std::vector<uint64_t> tile_stamp;    // by tile slot
  std::vector<uint64_t> coarse_stamp;  // by coarse block
};

struct SnowWorld {
  std::vector<std::unique_ptr<Layer>> layers;
};

// A layer over `spec`'s region, its tiles empty. The reference is stable for the world's life.
Layer& add_layer(SnowWorld& world, const LayerSpec& spec);
// The first layer whose region holds (x, z), or none.
const Layer* layer_at(const SnowWorld& world, double x, double z);
Layer* layer_at(SnowWorld& world, double x, double z);

double ground_at(const Ground& g, double x, double z);
CoverSample cover_at(const Cover& c, double x, double z);

// A ground baked over `region` at `cell` m from any height function (a terrain, a trace down a
// brush world): `height_at(x, z)` at every sample.
template <class F>
core::Heightfield bake_ground(const Region& region, double cell, F&& height_at) {
  const int cols = static_cast<int>((region.x1 - region.x0) / cell) + 1;
  const int rows = static_cast<int>((region.z1 - region.z0) / cell) + 1;
  core::Heightfield f = core::create_heightfield(region.x0, region.z0, cell, cols, rows);
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      f.data[static_cast<size_t>(r) * cols + c] =
          static_cast<float>(height_at(region.x0 + c * cell, region.z0 + r * cell));
    }
  }
  return f;
}

// THE COLUMN at a point.
struct Column {
  double ground = 0;   // m, the ground's height
  double depth = 0;    // m of snow over it
  double density = 0;  // kg/m3
  double packed = 0;   // 0..1
  double surface = 0;  // m, ground + depth
  double drop = 0;     // m the surface stands under the untouched cover's (negative: a berm)
  Kind kind = Kind::powder;
  bool touched = false;  // any of the four samples read was changed by a press
};

Column sample(const Layer& layer, double x, double z);
// The column at each of `xz` (pairs x, z) into `out`.
void sample(const Layer& layer, std::span<const double> xz, std::span<Column> out);
// The column under (x, z) on the first layer holding it; false (and `out` untouched) for none.
bool sample(const SnowWorld& world, double x, double z, Column& out);

// The world point of sample (c, r).
inline double sample_x(const Layer& l, int c) { return l.spec.region.x0 + c * l.spec.cell; }
inline double sample_z(const Layer& l, int r) { return l.spec.region.z0 + r * l.spec.cell; }

// The cell at sample (c, r), or null where its tile is not allocated (the cover is the snow).
const Cell* cell_at(const Layer& layer, int c, int r);
// The cell at sample (c, r), its tile allocated and filled first (from the coarse layer where it
// holds the snow there, the cover elsewhere; the stalest tile folded to make room past the
// budget); null when no tile can be had.
Cell* cell_for(Layer& layer, int c, int r);
// The cell the cover fills sample (c, r) with (no snow past the region).
Cell cover_cell(const Layer& layer, int c, int r);

inline double cell_depth(const Cell& c) { return c.depth_mm * 0.001; }
inline double cell_mass(const Cell& c) { return c.mass * kMassUnit; }
inline double cell_density(const Cell& c) {
  return c.depth_mm ? cell_mass(c) / cell_depth(c) : 0.0;
}

// The coarse cell over sample (c, r) and how many samples it stands for; null where none is kept.
const CoarseCell* coarse_at(const Layer& layer, int c, int r, int* samples = nullptr);
// What is stored at sample (c, r) in place of the cover: a touched fine cell, or the mean of a
// touched coarse cell over it. False where the cover is the snow.
struct Stored {
  double depth = 0, mass = 0;  // m, kg/m2
  Kind kind = Kind::powder;
  uint16_t minute = 0;
  bool pressed = false;
};
bool stored_at(const Layer& layer, int c, int r, Stored& out);

// Stamp tile slot `slot` or coarse block `block` as changed now (Layer::edits).
void stamp_tile(Layer& layer, int32_t slot);
void stamp_coarse(Layer& layer, int32_t block);

// Every mass unit the tiles and the coarse layer hold; with `spilled`, always `mass_in`.
uint64_t mass_units(const Layer& layer);
// The records kept since the last call are read; start a new frame's.
void clear_records(Layer& layer);

}  // namespace ge::snow
