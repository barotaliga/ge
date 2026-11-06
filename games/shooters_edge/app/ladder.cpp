// Shooter's Edge's quality ladder: the price table and the worths, the presets, the picture's
// stops.
//
// ge:summary Shooter's Edge's quality ladder: the prices and worths, the presets, the stops
// ge:keywords shooter quality-ladder price-table worth preset picture_stops
// ge:skill render perf
// ge:test tests/shooters_edge_ladder_test.cpp
#include "shooters_edge/app/ladder.hpp"

#include <array>

namespace shooters_edge::look {

namespace {

namespace r = ge::render;

// THE PRICE TABLE: ms a frame over the row's `off` on the reference machine, from the
// `se-timedemo` lab's `--price=1 --price-stops=1` (each row lowered alone from the top picture).
// THE WORTHS, on one scale:
//
// SHADOWS: low keeps every wall's and pillar's shadow from the torches (the kept cubes), and that
//   is most of what a shadow does for a room; medium adds the players' and the monsters' shadows,
//   hard-edged, from the eight lights that light them most; high softens each by its flame's
//   size and lets the other lights' shadow them too (the capsules).
// WATER: the pools are where the modern look shows most against the pack's warped picture: low
//   is already our water (its colour, its depth, the room's light in it, the waves a player
//   sets off), still and unreflecting; medium the chop, the wet band at the waterline and the room
//   reflected where the screen holds it; high the room mirrored whole (the map drawn again in the
//   pool's plane), the sun's caustics on the bed and its shafts under the surface.
// SKIES: low is our sky through the sky faces, its stars, moon and the light it puts in the rooms,
//   with only its mean cloud; medium the map's own layers of cloud racing over it; high the far
//   land round the map too, seen over its walls from its high places. (The cloud marched at a
//   quarter of the pixels costs what every pixel does here, where the sky is a few faces of the
//   frame: the layer's own passes are the bill, not its pixels.)
//
// Measured on the reference machine (an Apple M3 Max, Metal through Dawn, 2560x1440, the classic
// pack's demo1 played flat out with the HUD, the modern look, every other switch on): each row
// lowered alone to each stop, between runs with everything on, the saving by the median GPU span
// (`se-timedemo --basedir=<the classic packs> --demo=demo1 --size=2560,1440 --price-stops=1`).
// A stop's cost is its row's `off` saving less its own; a stop that measured cheaper than the one
// under it is set level with it (the skies' low measured 0.02 ms under off). Priced on 2026-11-06:
// the top's span 6.38 ms, every row off 4.09; the presets saved 1.72 (low) and 1.15 ms (medium)
// against the top, which itself is 0.2 ms under the tree's before them (the water's wet band
// scissored to its waterline, the mirror's far side dropped before it is shaded).
constexpr r::PriceTable kTable{"Apple M3 Max (Metal), 2560x1440", 2560, 1440, 4.09};

constexpr r::LadderStop kShadows[] = {
    {"off", 0, 0}, {"low", 0.35, 30}, {"medium", 0.38, 36}, {"high", 1.14, 40}};
constexpr r::LadderStop kWater[] = {
    {"off", 0, 0}, {"low", 0.46, 40}, {"medium", 0.80, 46}, {"high", 0.96, 52}};
constexpr r::LadderStop kSkies[] = {
    {"off", 0, 0}, {"low", 0, 20}, {"medium", 0.26, 24}, {"high", 0.43, 26}};

const std::array<r::LadderRow, kLadderRows> kRows = {{
    {"shadows", kShadows},
    {"water", kWater},
    {"skies", kSkies},
}};

// The presets, cheapest first, a stop for each row in LadderRow's order: shadows, water, skies.
// `high` is everything but the shadows' soft edges and the capsules (three quarters of a
// millisecond for the least of the look); `top` every row at its dearest, the default look.
constexpr int kLow[] = {1, 1, 1};
constexpr int kMedium[] = {2, 2, 2};
constexpr int kHigh[] = {2, 3, 3};
constexpr int kTop[] = {3, 3, 3};
constexpr r::LadderPreset kPresets[] = {
    {"low", kLow}, {"medium", kMedium}, {"high", kHigh}, {"top", kTop}};

const r::QualityLadder kLadder{kRows, kTable, kPresets};

}  // namespace

const r::QualityLadder& quality_ladder() { return kLadder; }

std::span<const r::LadderRow> ladder_rows() { return kRows; }

Stops stops_of(const r::LadderPick& pick) {
  const auto at = [&](LadderRow row) { return static_cast<Stop>(pick[static_cast<size_t>(row)]); };
  return {at(LadderRow::shadows), at(LadderRow::water), at(LadderRow::skies)};
}

r::LadderPick pick_of(const Stops& stops) {
  return {static_cast<int>(stops.shadows), static_cast<int>(stops.water),
          static_cast<int>(stops.skies)};
}

Stops picture_stops(const Picture& picture, uint32_t width, uint32_t height, std::string& error) {
  if (const auto pick =
          r::quality_pick(kLadder, picture.quality, picture.budget_ms, width, height, error))
    return stops_of(*pick);
  return custom_stops(picture);
}

Stops picture_stops(const Picture& picture, uint32_t width, uint32_t height, KeptStops& kept,
                    std::string& fresh_error) {
  if (picture.quality != kept.quality || picture.budget_ms != kept.budget_ms ||
      width != kept.width || height != kept.height) {
    kept.quality = picture.quality;
    kept.budget_ms = picture.budget_ms;
    kept.width = width;
    kept.height = height;
    const auto pick =
        r::quality_pick(kLadder, picture.quality, picture.budget_ms, width, height, fresh_error);
    kept.picked = pick.has_value();
    if (pick) kept.stops = stops_of(*pick);
  }
  return kept.picked ? kept.stops : custom_stops(picture);
}

ge::classicenhance::Switches drawn_switches(ge::classicenhance::Switches ce, const Stops& stops) {
  ce.shadows = stops.shadows != Stop::off;
  ce.water = stops.water != Stop::off;
  ce.skies = stops.skies != Stop::off;
  return ce;
}

}  // namespace shooters_edge::look
