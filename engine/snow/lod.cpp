// The level of detail: focus actors, folding and refining tiles, the coarse press (lod.hpp).
//
// ge:summary Snow level of detail: focus, fold, refine, the stalest tile, the coarse press
// ge:keywords snow lod focus coarse fold refine budget stalest press_coarse bytes_of
// ge:skill snow
// ge:test tests/snow_lod_test.cpp
#include "snow/lod.hpp"

#include <cmath>
#include <limits>

#include "core/math.hpp"
#include "core/num.hpp"
#include "snow/contact.hpp"
#include "snow/time.hpp"

namespace ge::snow {

namespace {

// A berm thrown inside a coarse cell lies at this density (press.cpp's range, its middle).
constexpr double kBermDensity = 300;

uint32_t round_u32(double v) {
  const double q = core::num::round(v);
  return q <= 0 ? 0 : q >= 4294967295.0 ? 4294967295u : static_cast<uint32_t>(q);
}

// The first sample of coarse cell (i, j) of tile (tx, tz), and its size clipped to the region.
struct Patch {
  int c0, r0, w, h;
};

Patch patch_of(const Layer& l, int tx, int tz, int i, int j) {
  Patch p;
  p.c0 = tx * kTileCells + i * kCoarseCells;
  p.r0 = tz * kTileCells + j * kCoarseCells;
  p.w = l.cols - p.c0 < kCoarseCells ? l.cols - p.c0 : kCoarseCells;
  p.h = l.rows - p.r0 < kCoarseCells ? l.rows - p.r0 : kCoarseCells;
  if (p.w < 0) p.w = 0;
  if (p.h < 0) p.h = 0;
  return p;
}

CoarseBlock& block_for(Layer& l, int tx, int tz) {
  int32_t& slot = l.coarse_slots[static_cast<size_t>(tz) * l.tile_cols + tx];
  if (slot < 0) {
    slot = static_cast<int32_t>(l.coarse.size());
    CoarseBlock& b = l.coarse.emplace_back();
    b.tx = tx;
    b.tz = tz;
  }
  stamp_coarse(l, slot);
  return l.coarse[slot];
}

// How far tile (tx, tz)'s samples lie from (x, z), m (0 inside).
double tile_distance(const Layer& l, int tx, int tz, double x, double z) {
  const double x0 = sample_x(l, tx * kTileCells),
               x1 = sample_x(l, tx * kTileCells + kTileCells - 1);
  const double z0 = sample_z(l, tz * kTileCells),
               z1 = sample_z(l, tz * kTileCells + kTileCells - 1);
  const double dx = x < x0 ? x0 - x : x > x1 ? x - x1 : 0;
  const double dz = z < z0 ? z0 - z : z > z1 ? z - z1 : 0;
  return std::sqrt(dx * dx + dz * dz);
}

bool tile_near_focus(const Layer& l, int tx, int tz) {
  for (size_t k = 0; k + 1 < l.focus.size(); k += 2) {
    if (tile_distance(l, tx, tz, l.focus[k], l.focus[k + 1]) <= l.spec.near) return true;
  }
  return false;
}

}  // namespace

void set_focus(Layer& layer, std::span<const double> xz, double t) {
  layer.focus.assign(xz.begin(), xz.end());
  layer.clock = t;
  for (Tile& tile : layer.tiles) {
    if (tile.tx >= 0 && tile_near_focus(layer, tile.tx, tile.tz) && tile.near_t < t)
      tile.near_t = t;
  }
}

bool far_from_focus(const Layer& layer, double x, double z) {
  if (layer.focus.size() < 2) return false;
  const double near2 = layer.spec.near * layer.spec.near;
  for (size_t k = 0; k + 1 < layer.focus.size(); k += 2) {
    const double dx = x - layer.focus[k], dz = z - layer.focus[k + 1];
    if (dx * dx + dz * dz <= near2) return false;
  }
  return true;
}

int stalest_tile(const Layer& layer) {
  int best = -1;
  for (size_t i = 0; i < layer.tiles.size(); i++) {
    const Tile& t = layer.tiles[i];
    if (t.tx < 0 || t.near_t >= layer.clock) continue;
    if (best < 0 || t.near_t < layer.tiles[best].near_t) best = static_cast<int>(i);
  }
  return best;
}

int fold_stale(Layer& layer, double t) {
  layer.clock = t;
  int folded = 0;
  for (size_t i = 0; i < layer.tiles.size(); i++) {
    const Tile& tile = layer.tiles[i];
    if (tile.tx < 0 || t - tile.near_t <= layer.spec.stale) continue;
    if (tile_near_focus(layer, tile.tx, tile.tz)) continue;
    fold_tile(layer, static_cast<int>(i));
    folded++;
  }
  return folded;
}

CoarseBlock summary_of(const Layer& layer, const Tile& tile, uint64_t* mass_in) {
  CoarseBlock b;
  b.tx = tile.tx;
  b.tz = tile.tz;
  for (int j = 0; j < kCoarsePerTile; j++) {
    for (int i = 0; i < kCoarsePerTile; i++) {
      const Patch p = patch_of(layer, tile.tx, tile.tz, i, j);
      uint64_t mass = 0, depth = 0;
      bool touched = false, pressed = false;
      uint16_t minute = 0;
      int counts[static_cast<int>(Kind::count)] = {};
      for (int r = 0; r < p.h; r++) {
        for (int c = 0; c < p.w; c++) {
          const Cell& cell = tile.cells[(j * kCoarseCells + r) * kTileCells + i * kCoarseCells + c];
          mass += cell.mass;
          depth += cell.depth_mm;
          counts[static_cast<int>(cell.kind)]++;
          touched = touched || (cell.flags & kTouched);
          pressed = pressed || (cell.flags & kPressed);
          if (cell.minute > minute) minute = cell.minute;
        }
      }
      if (!touched) {
        // The cover again: its units leave the books with the tile.
        if (mass_in) *mass_in -= mass;
        continue;
      }
      int kind = 0;
      for (int k = 1; k < static_cast<int>(Kind::count); k++) {
        if (counts[k] > counts[kind]) kind = k;
      }
      CoarseCell& cc = b.cells[j * kCoarsePerTile + i];
      cc.mass = static_cast<uint32_t>(mass);
      cc.depth_mm = static_cast<uint32_t>(depth);
      cc.kind = static_cast<Kind>(kind);
      cc.flags = static_cast<uint8_t>(kTouched | (pressed ? kPressed : 0));
      cc.minute = minute;
    }
  }
  return b;
}

void fold_tile(Layer& layer, int slot) {
  Tile& tile = layer.tiles[slot];
  if (tile.tx < 0) return;
  const int tx = tile.tx, tz = tile.tz;
  if (layer.spec.weather) {
    // Every changed sample brought to the fold's minute, so the coarse cell starts from it.
    Moment m = moment(layer, minute_of(layer.clock) * 60.0);
    for (int j = 0; j < kTileCells; j++) {
      for (int i = 0; i < kTileCells; i++) {
        Cell& cell = tile.cells[j * kTileCells + i];
        if (cell.flags & kTouched)
          bring_to(layer, m, cell, tx * kTileCells + i, tz * kTileCells + j);
      }
    }
  }
  const CoarseBlock sum = summary_of(layer, tile, &layer.mass_in);
  for (int k = 0; k < kCoarsePerTile * kCoarsePerTile; k++) {
    if (!(sum.cells[k].flags & kTouched)) continue;
    block_for(layer, tx, tz).cells[k] = sum.cells[k];
  }
  layer.slots[static_cast<size_t>(tz) * layer.tile_cols + tx] = -1;
  tile.tx = tile.tz = -1;
  layer.free_tiles.push_back(slot);
  stamp_tile(layer, slot);
  layer.folds++;
}

void refine_tile(Layer& layer, Tile& tile) {
  const int tx = tile.tx, tz = tile.tz;
  const int32_t bslot = layer.coarse_slots[static_cast<size_t>(tz) * layer.tile_cols + tx];
  bool refined = false;
  for (int j = 0; j < kCoarsePerTile; j++) {
    for (int i = 0; i < kCoarsePerTile; i++) {
      const Patch p = patch_of(layer, tx, tz, i, j);
      CoarseCell* cc = bslot >= 0 ? &layer.coarse[bslot].cells[j * kCoarsePerTile + i] : nullptr;
      const bool from_coarse = cc && (cc->flags & kTouched) && p.w * p.h > 0;
      const auto n = static_cast<uint32_t>(p.w * p.h);
      uint32_t k = 0;
      for (int r = 0; r < kCoarseCells; r++) {
        for (int c = 0; c < kCoarseCells; c++) {
          const int sc = p.c0 + c, sr = p.r0 + r;
          Cell& cell = tile.cells[(j * kCoarseCells + r) * kTileCells + i * kCoarseCells + c];
          if (c >= p.w || r >= p.h) {
            cell = Cell{};  // past the region: no snow kept
            continue;
          }
          if (!from_coarse) {
            cell = cover_cell(layer, sc, sr);
            layer.mass_in += cell.mass;
            continue;
          }
          // Its units spread evenly, the remainder a unit each to the first samples.
          const uint32_t mass = cc->mass / n + (k < cc->mass % n ? 1 : 0);
          const uint32_t depth = cc->depth_mm / n + (k < cc->depth_mm % n ? 1 : 0);
          cell.mass = static_cast<uint16_t>(mass > kMaxMass ? kMaxMass : mass);
          cell.depth_mm = static_cast<uint16_t>(depth > kMaxDepthMm ? kMaxDepthMm : depth);
          cell.kind = cc->kind;
          cell.flags = static_cast<uint8_t>(kTouched | (cc->flags & kPressed));
          cell.minute = cc->minute;
          k++;
        }
      }
      if (from_coarse) {
        *cc = CoarseCell{};
        stamp_coarse(layer, bslot);
        refined = true;
      }
    }
  }
  if (refined) layer.refines++;
}

Pressed press_coarse(Layer& layer, const Imprint& imp, double t) {
  Pressed out;
  if (imp.shape.half_width <= 0) return out;
  const double area = imp.area > 0 ? imp.area : shape_area(imp.shape);
  out.pressure = area > 0 ? imp.load / area : 0;
  const bool by_sink = imp.sink >= 0;
  if (!by_sink && out.pressure <= 0) return out;
  const double push =
      core::clamp(imp.side_push >= 0 ? imp.side_push : default_side_push(imp.motion), 0, 1);
  const Shape& t0 = imp.shape;
  const double b = bearing_width(t0.half_width, t0.kind == ShapeKind::capsule ? 0 : t0.half_length);
  // Read at least three quarters of a sample wide, as `press` reads it.
  Shape s = t0;
  const double narrowest = 0.75 * layer.spec.cell;
  if (s.half_width < narrowest) s.half_width = narrowest;
  if (s.kind != ShapeKind::capsule && s.half_length < narrowest) s.half_length = narrowest;
  const uint16_t minute = minute_of(t);
  const Weather* weather = layer.spec.weather;
  Moment now = moment(layer, minute * 60.0);
  // The outline's box in coarse cells.
  double r = s.half_width;
  if (s.kind != ShapeKind::capsule && s.half_length > r) r = s.half_length;
  const double x0 = (s.kind == ShapeKind::capsule && s.bx < s.ax ? s.bx : s.ax) - r;
  const double x1 = (s.kind == ShapeKind::capsule && s.bx > s.ax ? s.bx : s.ax) + r;
  const double z0 = (s.kind == ShapeKind::capsule && s.bz < s.az ? s.bz : s.az) - r;
  const double z1 = (s.kind == ShapeKind::capsule && s.bz > s.az ? s.bz : s.az) + r;
  const double span = layer.spec.cell * kCoarseCells;
  const int ccols = (layer.cols + kCoarseCells - 1) / kCoarseCells;
  const int crows = (layer.rows + kCoarseCells - 1) / kCoarseCells;
  auto clampi = [](int v, int n) { return v < 0 ? 0 : v >= n ? n - 1 : v; };
  const int i0 = clampi(static_cast<int>(std::floor((x0 - layer.spec.region.x0) / span)), ccols);
  const int i1 = clampi(static_cast<int>(std::floor((x1 - layer.spec.region.x0) / span)), ccols);
  const int j0 = clampi(static_cast<int>(std::floor((z0 - layer.spec.region.z0) / span)), crows);
  const int j1 = clampi(static_cast<int>(std::floor((z1 - layer.spec.region.z0) / span)), crows);
  Kind kind = layer.spec.cover.kind;
  for (int cj = j0; cj <= j1; cj++) {
    for (int ci = i0; ci <= i1; ci++) {
      const int tx = ci / kCoarsePerTile, tz = cj / kCoarsePerTile;
      if (layer.slots[static_cast<size_t>(tz) * layer.tile_cols + tx] >= 0) continue;  // fine
      const Patch p = patch_of(layer, tx, tz, ci % kCoarsePerTile, cj % kCoarsePerTile);
      const int n = p.w * p.h;
      if (n == 0) continue;
      // The share of the cell the outline covers, counted over its samples.
      int inside = 0;
      for (int pj = 0; pj < p.h; pj++) {
        for (int pi = 0; pi < p.w; pi++) {
          if (shape_u(s, sample_x(layer, p.c0 + pi), sample_z(layer, p.r0 + pj)) <= 1) inside++;
        }
      }
      if (inside == 0) continue;
      const double share = static_cast<double>(inside) / n;
      CoarseCell& cc = block_for(layer, tx, tz)
                           .cells[(cj % kCoarsePerTile) * kCoarsePerTile + ci % kCoarsePerTile];
      const int mc = p.c0 + p.w / 2, mr = p.r0 + p.h / 2;  // the sample the cover is read at
      if (!(cc.flags & kTouched)) {
        // Materialised from the cover, sample by sample, as a fine tile would be.
        uint64_t mass = 0, depth = 0;
        for (int rr = 0; rr < p.h; rr++) {
          for (int c = 0; c < p.w; c++) {
            const Cell cell = cover_cell(layer, p.c0 + c, p.r0 + rr);
            mass += cell.mass;
            depth += cell.depth_mm;
          }
        }
        cc.mass = static_cast<uint32_t>(mass);
        cc.depth_mm = static_cast<uint32_t>(depth);
        cc.kind = layer.spec.cover.kind;
        cc.flags = kTouched;
        cc.minute = minute;
        layer.mass_in += mass;
      }
      double cover_depth;
      if (weather) {
        // Brought to the press's minute: the cover and the cell read as `sample` would.
        Stored st{cc.depth_mm * 0.001 / n, cc.mass * kMassUnit / n, cc.kind, cc.minute,
                  (cc.flags & kPressed) != 0};
        const Spell sp = spell(*weather, st.minute * 60.0, now.t);
        const Snowpack cover = cover_now(now, mc, mr);
        Snowpack here = weathered(*weather, sp, {st.depth, st.mass, st.kind, st.pressed});
        here = filled(*weather, sp, here, cover.depth - here.depth);
        const uint32_t mass = round_u32(here.mass / kMassUnit * n);
        layer.mass_in = layer.mass_in + mass - cc.mass;
        cc.mass = mass;
        cc.depth_mm = round_u32(here.depth * 1000 * n);
        cc.kind = here.kind;
        cover_depth = cover.depth;
      } else {
        cover_depth = cover_at(layer.spec.cover, sample_x(layer, mc), sample_z(layer, mr)).depth;
      }
      const double d = cc.depth_mm * 0.001 / n;
      const double rho = d > 0 ? cc.mass * kMassUnit / n / d : 0;
      if (d <= 0) continue;
      const double sink =
          by_sink ? imp.sink : column_sink(cc.kind, cover_depth, rho, b, out.pressure);
      const double target = cover_depth - sink < 0 ? 0 : cover_depth - sink;
      const double cut = d - target;
      if (cut <= 0) continue;
      // The pressed share sinks; what it pushed aside stays in the cell as a berm.
      const double lower = share * cut * (1 - push * rho / kBermDensity);
      const double depth = d - (lower > 0 ? lower : 0);
      cc.depth_mm = round_u32(depth * 1000 * n);
      cc.flags = static_cast<uint8_t>(cc.flags | kTouched | kPressed);
      cc.minute = minute;
      kind = cc.kind;
      out.coarse++;
      out.samples += static_cast<int>(core::num::round(share * n));
      out.compacted += cc.mass * kMassUnit * layer.spec.cell * layer.spec.cell * share;
      if (cover_depth - target > out.sink) out.sink = cover_depth - target;
    }
  }
  if (out.coarse > 0) {
    if (layer.records.size() < layer.spec.max_records) {
      layer.records.push_back(
          ImprintRecord{t0, imp.motion, kind, static_cast<float>(out.sink), 0.0f, t});
    } else {
      layer.records_dropped++;
    }
  }
  return out;
}

size_t bytes_of(const Layer& layer) {
  return static_cast<size_t>(live_tiles(layer)) * sizeof(Tile) +
         layer.coarse.size() * sizeof(CoarseBlock) +
         layer.records.capacity() * sizeof(ImprintRecord) +
         (layer.slots.size() + layer.coarse_slots.size()) * sizeof(int32_t);
}

int live_tiles(const Layer& layer) {
  int n = 0;
  for (const Tile& t : layer.tiles) n += t.tx >= 0 ? 1 : 0;
  return n;
}

}  // namespace ge::snow
