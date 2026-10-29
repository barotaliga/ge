// The snow layer: its tiles, the cover and the sample query (layer.hpp).
//
// ge:summary The snow layer's tiles, cover and bilinear sample query
// ge:keywords snow layer tile cover sample bilinear cell_for mass_units
// ge:skill snow
// ge:test tests/snow_layer_test.cpp
#include "snow/layer.hpp"

#include <cmath>

#include "core/num.hpp"
#include "snow/lod.hpp"

namespace ge::snow {

namespace {

// The sample's index along an axis for a world offset, clamped to the grid.
int clamp_index(int i, int n) { return i < 0 ? 0 : i >= n ? n - 1 : i; }

uint16_t quantize(double v, double unit, uint16_t most) {
  const double q = core::num::round(v / unit);
  return q <= 0 ? 0 : q >= most ? most : static_cast<uint16_t>(q);
}

}  // namespace

double ground_at(const Ground& g, double x, double z) {
  return g.field ? core::sample_field(*g.field, x, z) : g.height;
}

CoverSample cover_at(const Cover& c, double x, double z) {
  CoverSample s;
  s.kind = c.kind;
  s.depth = c.depth_grid ? core::sample_field(*c.depth_grid, x, z) : c.depth;
  if (s.depth < 0) s.depth = 0;
  s.packed = c.packed_grid ? core::clamp(core::sample_field(*c.packed_grid, x, z), 0, 1) : c.packed;
  s.density = c.density > 0 ? c.density : density_from_packed(c.kind, s.packed);
  return s;
}

Layer& add_layer(SnowWorld& world, const LayerSpec& spec) {
  auto layer = std::make_unique<Layer>();
  layer->spec = spec;
  const Region& r = spec.region;
  layer->cols = static_cast<int>((r.x1 - r.x0) / spec.cell) + 1;
  layer->rows = static_cast<int>((r.z1 - r.z0) / spec.cell) + 1;
  layer->tile_cols = (layer->cols + kTileCells - 1) / kTileCells;
  layer->tile_rows = (layer->rows + kTileCells - 1) / kTileCells;
  layer->slots.assign(static_cast<size_t>(layer->tile_cols) * layer->tile_rows, -1);
  layer->coarse_slots.assign(layer->slots.size(), -1);
  layer->tiles.reserve(spec.max_tiles < 64 ? spec.max_tiles : 64);
  layer->records.reserve(spec.max_records);
  world.layers.push_back(std::move(layer));
  return *world.layers.back();
}

namespace {

bool holds(const Region& r, double x, double z) {
  return x >= r.x0 && x <= r.x1 && z >= r.z0 && z <= r.z1;
}

}  // namespace

const Layer* layer_at(const SnowWorld& world, double x, double z) {
  for (const auto& l : world.layers) {
    if (holds(l->spec.region, x, z)) return l.get();
  }
  return nullptr;
}

Layer* layer_at(SnowWorld& world, double x, double z) {
  for (auto& l : world.layers) {
    if (holds(l->spec.region, x, z)) return l.get();
  }
  return nullptr;
}

const Cell* cell_at(const Layer& layer, int c, int r) {
  if (c < 0 || r < 0 || c >= layer.cols || r >= layer.rows) return nullptr;
  const int slot =
      layer.slots[static_cast<size_t>(r / kTileCells) * layer.tile_cols + c / kTileCells];
  if (slot < 0) return nullptr;
  return &layer.tiles[slot].cells[(r % kTileCells) * kTileCells + c % kTileCells];
}

Cell cover_cell(const Layer& layer, int c, int r) {
  Cell cell;
  if (c >= layer.cols || r >= layer.rows) return cell;  // past the region: no snow kept
  const CoverSample s = cover_at(layer.spec.cover, sample_x(layer, c), sample_z(layer, r));
  cell.depth_mm = quantize(s.depth, 0.001, kMaxDepthMm);
  cell.mass = quantize(s.density * s.depth, kMassUnit, kMaxMass);
  cell.kind = s.kind;
  return cell;
}

Cell* cell_for(Layer& layer, int c, int r) {
  if (c < 0 || r < 0 || c >= layer.cols || r >= layer.rows) return nullptr;
  const int tx = c / kTileCells, tz = r / kTileCells;
  const size_t at = static_cast<size_t>(tz) * layer.tile_cols + tx;
  if (layer.slots[at] < 0) {
    if (layer.free_tiles.empty() && layer.tiles.size() >= layer.spec.max_tiles) {
      const int stalest = stalest_tile(layer);
      if (stalest < 0) return nullptr;
      fold_tile(layer, stalest);
    }
    int32_t slot;
    if (!layer.free_tiles.empty()) {
      slot = layer.free_tiles.back();
      layer.free_tiles.pop_back();
      layer.tiles[slot] = Tile{};
    } else {
      slot = static_cast<int32_t>(layer.tiles.size());
      layer.tiles.emplace_back();
    }
    layer.slots[at] = slot;
    Tile& t = layer.tiles[slot];
    t.tx = tx;
    t.tz = tz;
    refine_tile(layer, t);
  }
  Tile& t = layer.tiles[layer.slots[at]];
  if (t.near_t < layer.clock) t.near_t = layer.clock;
  return &t.cells[(r % kTileCells) * kTileCells + c % kTileCells];
}

const CoarseCell* coarse_at(const Layer& layer, int c, int r, int* samples) {
  if (c < 0 || r < 0 || c >= layer.cols || r >= layer.rows) return nullptr;
  const int tx = c / kTileCells, tz = r / kTileCells;
  const int32_t slot = layer.coarse_slots[static_cast<size_t>(tz) * layer.tile_cols + tx];
  if (slot < 0) return nullptr;
  const int i = (c % kTileCells) / kCoarseCells, j = (r % kTileCells) / kCoarseCells;
  if (samples) {
    const int c0 = tx * kTileCells + i * kCoarseCells, r0 = tz * kTileCells + j * kCoarseCells;
    const int w = layer.cols - c0 < kCoarseCells ? layer.cols - c0 : kCoarseCells;
    const int h = layer.rows - r0 < kCoarseCells ? layer.rows - r0 : kCoarseCells;
    *samples = w * h;
  }
  return &layer.coarse[slot].cells[j * kCoarsePerTile + i];
}

bool stored_at(const Layer& layer, int c, int r, Stored& out) {
  if (const Cell* cell = cell_at(layer, c, r)) {
    if (!(cell->flags & kTouched)) return false;
    out = {cell_depth(*cell), cell_mass(*cell), cell->kind, cell->minute,
           (cell->flags & kPressed) != 0};
    return true;
  }
  int n = 0;
  const CoarseCell* cc = coarse_at(layer, c, r, &n);
  if (!cc || !(cc->flags & kTouched) || n == 0) return false;
  out = {cc->depth_mm * 0.001 / n, cc->mass * kMassUnit / n, cc->kind, cc->minute,
         (cc->flags & kPressed) != 0};
  return true;
}

Column sample(const Layer& layer, double x, double z) {
  Column col;
  col.ground = ground_at(layer.spec.ground, x, z);
  const CoverSample here = cover_at(layer.spec.cover, x, z);
  const double fx = (x - layer.spec.region.x0) / layer.spec.cell;
  const double fz = (z - layer.spec.region.z0) / layer.spec.cell;
  const int c0 = clamp_index(static_cast<int>(std::floor(fx)), layer.cols);
  const int r0 = clamp_index(static_cast<int>(std::floor(fz)), layer.rows);
  const int c1 = clamp_index(c0 + 1, layer.cols), r1 = clamp_index(r0 + 1, layer.rows);
  const int cs[4] = {c0, c1, c0, c1}, rs[4] = {r0, r0, r1, r1};
  Stored st[4];
  bool kept[4];
  bool any = false;
  for (int k = 0; k < 4; k++) {
    kept[k] = stored_at(layer, cs[k], rs[k], st[k]);
    any = any || kept[k];
  }
  if (!any) {
    col.depth = here.depth;
    col.density = here.density;
    col.packed = here.packed;
    col.kind = here.kind;
    col.surface = col.ground + col.depth;
    return col;
  }
  const double tx = core::clamp(fx - c0, 0, 1), tz = core::clamp(fz - r0, 0, 1);
  const double w[4] = {(1 - tx) * (1 - tz), tx * (1 - tz), (1 - tx) * tz, tx * tz};
  double depth = 0, mass = 0, packed = 0, best = -1;
  for (int k = 0; k < 4; k++) {
    double d, m, p;
    Kind kind;
    if (kept[k]) {
      d = st[k].depth;
      m = st[k].mass;
      kind = st[k].kind;
      p = d > 0 ? packed_from_density(kind, m / d) : 1.0;
    } else {
      const CoverSample s =
          cover_at(layer.spec.cover, sample_x(layer, cs[k]), sample_z(layer, rs[k]));
      d = s.depth;
      m = s.depth * s.density;
      p = s.packed;
      kind = s.kind;
    }
    depth += w[k] * d;
    mass += w[k] * m;
    packed += w[k] * p;
    if (w[k] > best) {
      best = w[k];
      col.kind = kind;
    }
  }
  col.touched = true;
  col.depth = depth;
  col.density = depth > 0 ? mass / depth : 0.0;
  col.packed = packed;
  col.surface = col.ground + depth;
  col.drop = here.depth - depth;
  return col;
}

void sample(const Layer& layer, std::span<const double> xz, std::span<Column> out) {
  const size_t n = xz.size() / 2 < out.size() ? xz.size() / 2 : out.size();
  for (size_t i = 0; i < n; i++) out[i] = sample(layer, xz[2 * i], xz[2 * i + 1]);
}

bool sample(const SnowWorld& world, double x, double z, Column& out) {
  const Layer* l = layer_at(world, x, z);
  if (!l) return false;
  out = sample(*l, x, z);
  return true;
}

uint64_t mass_units(const Layer& layer) {
  uint64_t sum = 0;
  for (const Tile& t : layer.tiles) {
    if (t.tx < 0) continue;  // a free slot
    for (const Cell& c : t.cells) sum += c.mass;
  }
  for (const CoarseBlock& b : layer.coarse) {
    for (const CoarseCell& c : b.cells) {
      if (c.flags & kTouched) sum += c.mass;
    }
  }
  return sum;
}

void clear_records(Layer& layer) {
  layer.records.clear();
  layer.records_dropped = 0;
}

}  // namespace ge::snow
