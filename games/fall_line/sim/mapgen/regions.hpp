// R21 — THE REGIONS: which kind of snow country a mountain stands in, as data. Port of game5
// `engine/mapgen/regions.ts`. Every number is a multiple of the rule book's own, so the ALPINE
// row is all ones and lays nothing: a band scaled by one is the same band, and the alpine
// path draws exactly what it would with no region table at all. Nothing else in mapgen names
// a region; the terrain, the forest, the sun and the surface read their row from here.
// Parity: through tests/mapgen_generate_test.cpp (a map in every region).
//
// ge:summary R21 the regions: each snow country's row of multiples, roster and crust
// ge:keywords region regions alpine fell continental maritime regionRow treeKindAt
// ge:oracle game5/engine/mapgen/regions.ts
// ge:skill sim
// ge:test tests/mapgen_generate_test.cpp
#pragma once

#include <array>
#include <optional>
#include <span>

#include "mapgen/rules.hpp"
#include "mapgen/types.hpp"

namespace fall_line::mapgen {

struct RosterRow {
  TreeKind kind;
  double share;
};

// WIND CRUST (R21): a packed share pressed into the powder over `cover` of the country in
// patches `scale` m across and over every exposed crest (`exposed`); `packed` is how much it
// holds a skier up, as a share of the groomer.
struct Crust {
  double cover;
  double packed;
  double scale;
  double exposed;
};

struct Region {
  RegionId id;
  struct {
    double vertical, flank, crests, hills, ridges, rollers;
    struct {
      double count, radius, depth;
    } bowls;
    struct {
      double count, drop;
    } headwalls;
  } relief;
  struct {
    double density, meadow, height;
    std::span<const RosterRow> roster;  // what grows, each kind with its share (drawn only)
  } forest;
  struct {
    Band base, tree_line;  // m over the sea
  } altitude;
  double kickers;  // multiplier on the off-piste kickers' (and the cliffs') count
  struct {
    Band latitude, day_of_year;
  } sun;
  std::optional<Crust> crust;  // none: the powder is untouched
  bool rock;                   // whether rock breaks through (read by nothing the generator builds)
};

// The row for a region; the alpine's when none is asked.
const Region& region_row(std::optional<RegionId> id);

// The region's name as the oracle spells it ("alpine", "fell", …): the digest mixes it in.
const char* region_name(RegionId id);

// A band scaled by `k` (the same band at one).
inline Band scale_band(Band band, double k) {
  return k == 1 ? band : Band{band.min * k, band.max * k};
}

// A whole-number band scaled by `k` and rounded (the same band at one).
Band scale_count(Band band, double k);

// WHICH TREE STANDS HERE, off a hash of where it stands: no draw off any stream, so the
// roster moves nothing the forest draws.
TreeKind tree_kind_at(const Region& region, double x, double z);

}  // namespace fall_line::mapgen
