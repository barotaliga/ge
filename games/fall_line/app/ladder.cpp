// Fall Line's quality ladder: the stops, what each sets on the frame, the price table and the
// worths, the presets and the automatic preset.
//
// ge:summary Fall Line's quality ladder: the stops' settings, the prices and worths, the presets
// ge:keywords fall-line quality-ladder price-table worth preset
// ge:skill render perf
// ge:test tests/fall_line_ladder_test.cpp
#include "fall_line/app/ladder.hpp"

#include <array>

namespace fall_line::app {

namespace {

namespace r = ge::render;

// What each stop sets, by row, cheapest first.
constexpr double kScale[] = {0.5, 0.67, 0.83, 1.0};
constexpr uint32_t kShadowSize[] = {1024, 2048, 4096};
constexpr uint32_t kTrailSize[] = {0, 1024, 2048};  // 0: no trails
constexpr uint32_t kFlakes[] = {0, 8192, 24576};    // 0: no falling snow
constexpr double kCloudSteps[] = {0, 0.5, 1.0};     // 0: no layer
constexpr double kBloom[] = {0, 0.04};

// THE PRICE TABLE: ms a frame over the row's cheapest stop on the reference machine, from
// `labs ladder --price` (each row lowered alone from the top picture). THE WORTHS, on one scale:
//
// RESOLUTION: the pixels' softness; TAA-U brings the frame up to the output either way, so the
//   loss is a blur, never a missing thing. Given up first by rule, and worth least a millisecond.
// SHADOWS: the trees' and the figures' shadows are drawn at every stop; a finer map only sharpens
//   their edges and keeps the far cascade's from stepping.
// SNOW: the material is the snow itself (the corduroy, the glitter, the wrap's blue terminator,
//   the tracks' grooves); without it the ground is its albedo lit: the heaviest loss on the list.
// TRAILS: the tracks are the game's own mark, the line a skier reads back; the higher stop is a
//   sharper groove.
// SPRAY: what a turn throws, the most alive thing at the skis.
// SNOWFALL: on a snowing day the flakes are the weather near the eye; the low stop keeps a third.
// CLOUDS: a broken sky's layers; the low stop marches half the steps, a slightly blockier cloud;
//   off, the sky keeps its mean cloud and loses its shapes and shadows.
// BLOOM: the sun's glow off the snow and the lamps, small.
//
// Measured on the software adapter in the cloud container (lavapipe, 1280x720, seed 38 under a
// flurry, the chase camera, the least median of three rounds): its ratios are a CPU raster's, not
// a GPU's, and the reference machine's (`needs-mac`) replace them. From the chase camera most of
// the screen is snow near the skis, so the cloud march's half steps and the finer trails and
// flakes measured no cheaper than their top stops; a stop that measured cheaper than the one under
// it is set level with it.
constexpr PriceTable kTable{"lavapipe (software), 1280x720", 1280, 720, 82.90};

constexpr r::LadderStop kResolution[] = {
    {"half", 0, 0}, {"two-thirds", 19.92, 3}, {"five-sixths", 36.44, 5}, {"full", 57.48, 6}};
constexpr r::LadderStop kShadows[] = {{"1024", 0, 0}, {"2048", 1.25, 5}, {"4096", 11.14, 7}};
constexpr r::LadderStop kSnow[] = {{"plain", 0, 0}, {"material", 11.40, 60}};
constexpr r::LadderStop kTrails[] = {{"off", 0, 0}, {"low", 9.57, 30}, {"high", 9.57, 36}};
constexpr r::LadderStop kSpray[] = {{"off", 0, 0}, {"on", 0, 20}};
constexpr r::LadderStop kSnowfall[] = {{"off", 0, 0}, {"low", 1.77, 10}, {"high", 1.77, 14}};
constexpr r::LadderStop kClouds[] = {{"off", 0, 0}, {"low", 23.99, 12}, {"high", 23.99, 15}};
constexpr r::LadderStop kBloomStops[] = {{"off", 0, 0}, {"on", 18.87, 4}};

const std::array<r::LadderRow, kPictureRows> kRows = {{
    {"resolution", kResolution, 1},
    {"shadows", kShadows},
    {"snow", kSnow},
    {"trails", kTrails},
    {"spray", kSpray},
    {"snowfall", kSnowfall},
    {"clouds", kClouds},
    {"bloom", kBloomStops},
}};

}  // namespace

const PriceTable& price_table() { return kTable; }

std::span<const r::LadderRow> picture_rows() { return kRows; }

FrameOptions apply_picture(const r::LadderPick& pick, FrameOptions o) {
  auto at = [&](PictureRow row) { return static_cast<size_t>(pick[static_cast<size_t>(row)]); };
  o.taa = true;
  o.scale = kScale[at(PictureRow::resolution)];
  o.shadow_size = kShadowSize[at(PictureRow::shadows)];
  o.snow = at(PictureRow::snow) > 0;
  const uint32_t trail = kTrailSize[at(PictureRow::trails)];
  o.trails = trail > 0;
  if (trail > 0) o.trail_maps.fine_size = o.trail_maps.coarse_size = trail;
  o.spray = at(PictureRow::spray) > 0;
  const uint32_t flakes = kFlakes[at(PictureRow::snowfall)];
  o.snowfall = flakes > 0;
  if (flakes > 0) o.flakes.flakes = flakes;
  o.clouds = kCloudSteps[at(PictureRow::clouds)];
  o.bloom = kBloom[at(PictureRow::bloom)];
  return o;
}

std::string_view preset_name(Preset p) {
  switch (p) {
    case Preset::low:
      return "low";
    case Preset::medium:
      return "medium";
    case Preset::high:
      return "high";
    case Preset::top:
      return "top";
  }
  return "top";
}

r::LadderPick preset_pick(Preset p) {
  // resolution, shadows, snow, trails, spray, snowfall, clouds, bloom
  switch (p) {
    case Preset::low:
      return {0, 0, 1, 1, 1, 1, 0, 0};
    case Preset::medium:
      return {1, 1, 1, 1, 1, 1, 1, 1};
    case Preset::high:
      return {2, 1, 1, 2, 1, 2, 2, 1};
    case Preset::top:
      break;
  }
  r::LadderPick top(kRows.size());
  for (size_t i = 0; i < kRows.size(); ++i) top[i] = static_cast<int>(kRows[i].stops.size()) - 1;
  return top;
}

r::LadderFit auto_picture(double budget_ms, const r::LadderPick& drawn, double timed_ms) {
  const double scale = r::machine_scale(kRows, drawn, kTable.floor_ms, timed_ms);
  return r::fit_ladder(kRows, budget_ms, kTable.floor_ms, scale);
}

}  // namespace fall_line::app
