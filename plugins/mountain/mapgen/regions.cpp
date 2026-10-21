// The definitions behind regions.hpp: the four rows and the helpers over them.
//
// ge:summary The four regions' rows and the tree roster's hash pick
// ge:keywords region
// ge:skill mountain
// ge:test tests/mountain_generate_test.cpp
#include "mountain/mapgen/regions.hpp"

#include "core/num.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;
using TK = TreeKind;

namespace {

constexpr std::array<RosterRow, 10> kAlpineRoster{{{TK::spruce, 0.3},
                                                   {TK::fir, 0.2},
                                                   {TK::larch, 0.14},
                                                   {TK::stonepine, 0.12},
                                                   {TK::dwarfpine, 0.08},
                                                   {TK::beech, 0.04},
                                                   {TK::pine, 0.04},
                                                   {TK::rowan, 0.03},
                                                   {TK::maple, 0.02},
                                                   {TK::snag, 0.03}}};
constexpr std::array<RosterRow, 8> kFellRoster{{{TK::birch, 0.58},
                                                {TK::spruce, 0.12},
                                                {TK::pine, 0.1},
                                                {TK::willow, 0.06},
                                                {TK::rowan, 0.04},
                                                {TK::juniper, 0.04},
                                                {TK::aspen, 0.03},
                                                {TK::snag, 0.03}}};
constexpr std::array<RosterRow, 8> kContinentalRoster{{{TK::lodgepole, 0.22},
                                                       {TK::spruce, 0.2},
                                                       {TK::fir, 0.18},
                                                       {TK::whitepine, 0.12},
                                                       {TK::aspen, 0.14},
                                                       {TK::pine, 0.05},
                                                       {TK::juniper, 0.04},
                                                       {TK::snag, 0.05}}};
constexpr std::array<RosterRow, 8> kMaritimeRoster{{{TK::fir, 0.38},
                                                    {TK::birch, 0.26},
                                                    {TK::beech, 0.1},
                                                    {TK::hemlock, 0.08},
                                                    {TK::spruce, 0.06},
                                                    {TK::maple, 0.04},
                                                    {TK::alder, 0.04},
                                                    {TK::snag, 0.04}}};

// In RegionId order. Each row's comment says what kind of country it is.
const std::array<Region, 4> kRegions{{
    // THE ALPINE: the rules as written. All ones, nothing laid.
    {RegionId::alpine,
     {1, 1, 1, 1, 1, 1, {1, 1, 1}, {1, 1}},
     {1, 1, 1, kAlpineRoster},
     {{1300, 1600}, {1800, 2200}},
     1,
     {rules::sun::latitude, rules::sun::day_of_year},
     std::nullopt,
     true},
    // THE FELL: a low, rounded northern mountain with a wind crust over its open tops.
    {RegionId::fell,
     {0.55, 0.55, 0.5, 0.8, 0.55, 0.8, {0.8, 1.2, 0.5}, {0.6, 0.6}},
     {0.6, 2, 0.6, kFellRoster},
     {{350, 500}, {600, 1000}},
     0.8,
     {{60, 69}, {40, 90}},
     Crust{0.45, 0.55, 140, 1},
     true},
    // THE CONTINENTAL: a high, cold, dry range; a thin crust on the exposed crests.
    {RegionId::continental,
     {1.2, 1.15, 1.2, 1.1, 1.3, 1, {1.2, 1.1, 1.4}, {1.3, 1.2}},
     {0.85, 1.2, 1, kContinentalRoster},
     {{2300, 2700}, {3000, 3500}},
     1.2,
     {{39, 52}, {1, 80}},
     Crust{0.12, 0.5, 110, 0.7},
     true},
    // THE MARITIME: a mountain by the sea under deep, heavy snow; no crust.
    {RegionId::maritime,
     {0.9, 0.9, 0.8, 1.2, 0.8, 1.1, {1, 1.1, 0.9}, {0.8, 0.9}},
     {1.05, 0.8, 0.9, kMaritimeRoster},
     {{300, 600}, {1400, 1600}},
     1,
     {{36, 44}, {10, 80}},
     std::nullopt,
     false},
}};

}  // namespace

const Region& region_row(std::optional<RegionId> id) {
  return kRegions[static_cast<size_t>(id.value_or(RegionId::alpine))];
}

const char* region_name(RegionId id) {
  switch (id) {
    case RegionId::alpine:
      return "alpine";
    case RegionId::fell:
      return "fell";
    case RegionId::continental:
      return "continental";
    case RegionId::maritime:
      return "maritime";
  }
  return "alpine";
}

Band scale_count(Band band, double k) {
  if (k == 1) return band;
  return Band{num::max(0.0, num::round(band.min * k)), num::max(0.0, num::round(band.max * k))};
}

TreeKind tree_kind_at(const Region& region, double x, double z) {
  const auto roster = region.forest.roster;
  if (roster.size() == 1) return roster[0].kind;
  // `(round(x*8) * 374761393 + round(z*8) * 668265263) | 0`, summed in double first.
  uint32_t h = static_cast<uint32_t>(
      num::to_int32(num::round(x * 8) * 374761393.0 + num::round(z * 8) * 668265263.0));
  h = (h ^ (h >> 13)) * 1274126177u;
  const double u = static_cast<double>(h ^ (h >> 16)) / 4294967296.0;
  double acc = 0;
  for (const RosterRow& row : roster) {
    acc += row.share;
    if (u < acc) return row.kind;
  }
  return roster.back().kind;
}

}  // namespace ge::mountain::mapgen
