// SHOOTER'S EDGE'S QUALITY LADDER: the three dearest enhancements of the modern look as rows, each
// stop's price on the reference machine and its worth to the look, the presets, and the player's
// one word for them through render/ladder.hpp's fit (docs/rendering.md § The quality ladder).
//
// - THE ROWS are SHADOWS, WATER and SKIES, each `off` (its ce_ switch off: the classic original)
//   then on at low, medium and high (look.hpp's `Stops`; what each stop sets on the passes is
//   look.cpp's and client/world_extras.cpp's). Cheapest first, so a stop's index is its rung.
// - THE PRICES are `kTable`'s reference machine's, filled by the `se-timedemo` lab's `--price`
//   over the stops (`--price-stops=1`): the classic pack's demo played with each row lowered alone
//   from the top. The WORTHS are judgements on one 0-100 scale, argued beside each row in
//   ladder.cpp.
// - THE QUALITY SETTING (`r_quality`, with `r_budget` for `auto`) picks the three stops off the
//   ladder (`picture_stops`); `custom`, the default, leaves each ce_ switch and its level as set.
//
// ge:summary Shooter's Edge's quality ladder: shadows, water and skies by stop, prices, presets
// ge:keywords shooter quality-ladder picture preset auto-preset price-table r_quality r_budget
// ge:keywords stops picture_stops stops_of quality_ladder ce_shadows_quality ce_water_quality
// ge:keywords ce_skies_quality
// ge:skill render perf
// ge:test tests/shooters_edge_ladder_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "render/ladder.hpp"
#include "shooters_edge/app/look.hpp"

namespace shooters_edge::look {

enum class LadderRow : uint8_t { shadows, water, skies, count };
inline constexpr int kLadderRows = static_cast<int>(LadderRow::count);

// The whole ladder (its rows, prices and presets) for the engine's generic quality setting.
const ge::render::QualityLadder& quality_ladder();
// The rows, priced, in LadderRow's order.
std::span<const ge::render::LadderRow> ladder_rows();

// The stops a pick names (a stop for each row, by index, cheapest first) and back.
Stops stops_of(const ge::render::LadderPick& pick);
ge::render::LadderPick pick_of(const Stops& stops);

// THE PICTURE'S STOPS: r_quality's pick off the ladder for a `width` x `height` frame, or with
// r_quality custom (or a word it does not know, said in `error`) the switches and levels as set.
Stops picture_stops(const Picture& picture, uint32_t width, uint32_t height, std::string& error);
// The same each frame, the ladder's pick kept between frames and worked out again only when
// r_quality, r_budget or the size changes (a frame allocates nothing); `fresh_error` is set to
// the refusal when the word first fails, empty otherwise.
struct KeptStops {
  std::string quality;
  double budget_ms = -1;
  uint32_t width = 0, height = 0;
  bool picked = false;  // r_quality named a pick (else the switches and levels rule)
  Stops stops;
};
Stops picture_stops(const Picture& picture, uint32_t width, uint32_t height, KeptStops& kept,
                    std::string& fresh_error);

// The switches as drawn at `stops`: `ce` with the three rows' switches on or off by their stop.
ge::classicenhance::Switches drawn_switches(ge::classicenhance::Switches ce, const Stops& stops);

}  // namespace shooters_edge::look
