// FALL LINE'S QUALITY LADDER: the picture's rows, each stop's settings on the frame
// (`FrameOptions`), its price on the reference machine and its worth to the look, the presets, and
// the automatic preset through render/ladder.hpp's fit (docs/rendering.md § The quality ladder).
//
// - A ROW A BILL. A machine can be short of pixels and rich in passes, or the other way round, so
//   each row is its own: RESOLUTION (the render size TAA-U brings up to the output), SHADOWS (the
//   sun's maps' texels), SNOW (the snow's material, or the albedo lit), TRAILS (the trail maps'
//   texels, or none), SPRAY (thrown or not), SNOWFALL (the flakes round the eye), CLOUDS (the
//   march's share of its steps at a quarter of the pixels, every pixel at the top, or no layer:
//   the sky's mean cloud), BLOOM (off, on, or on with the lens pass: render/lens.hpp) and
//   DISTANCE (how far the ground and what stands on it are drawn, closed into the sky by a mist;
//   the whole ground, unmisted, at the top).
// - CHEAPEST FIRST: every row's stops run from its cheapest, so a stop's index is its rung.
// - RESOLUTION IS GIVEN UP FIRST: the pixels' softness before anything in the picture.
// - THE PRICES are `kPrices`' reference machine's, filled by the `ladder` lab (`labs ladder
//   --price`): each row lowered alone from the top picture, the frame timed from seed 38's start
//   under a flurry, looking up into its broken sky. The WORTHS are judgements on one 0-100 scale,
//   argued beside each row in ladder.cpp.
//
// ge:summary Fall Line's quality ladder: its rows and stops on FrameOptions, prices, presets
// ge:keywords fall-line quality-ladder picture preset auto-preset price-table frame-options
// ge:keywords picture_rows apply_picture preset_pick auto_picture
// ge:skill render perf
// ge:test tests/fall_line_ladder_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "fall_line/app/frame.hpp"
#include "render/ladder.hpp"

namespace fall_line::app {

enum class PictureRow : uint8_t {
  resolution,
  shadows,
  snow,
  trails,
  spray,
  snowfall,
  clouds,
  bloom,
  distance,
  count
};
inline constexpr int kPictureRows = static_cast<int>(PictureRow::count);

// The reference machine the prices were measured on, and its frame with every row at its cheapest.
struct PriceTable {
  std::string_view machine;
  uint32_t width = 0, height = 0;
  double floor_ms = 0;
};
const PriceTable& price_table();

// The rows, priced, in PictureRow's order.
std::span<const ge::render::LadderRow> picture_rows();

// The options with `pick`'s stop of every row set on `base` (what no row touches is kept).
FrameOptions apply_picture(const ge::render::LadderPick& pick, FrameOptions base = {});

// The presets, cheapest first; `top` every row at its dearest.
enum class Preset : uint8_t { low, medium, high, top };
std::string_view preset_name(Preset p);
ge::render::LadderPick preset_pick(Preset p);

// THE AUTOMATIC PRESET: the picture of most worth that holds `budget_ms` on a machine that drew
// `drawn` in `timed_ms` at the table's size (render/ladder.hpp's fit over the table's scale).
ge::render::LadderFit auto_picture(double budget_ms, const ge::render::LadderPick& drawn,
                                   double timed_ms);

}  // namespace fall_line::app
