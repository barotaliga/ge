// Fall Line's quality ladder: the stops, what each sets on the frame, the price table and the
// worths, the presets and the automatic preset.
//
// ge:summary Fall Line's quality ladder: the stops' settings, the prices and worths, the presets
// ge:keywords fall-line quality-ladder price-table worth preset
// ge:skill render perf
// ge:test tests/fall_line_ladder_test.cpp
#include "fall_line/app/ladder.hpp"

#include <array>

#include "console/cvar.hpp"

namespace fall_line::app {

namespace {

namespace r = ge::render;

// What each stop sets, by row, cheapest first.
constexpr double kScale[] = {0.5, 0.67, 0.83, 1.0};
constexpr uint32_t kShadowSize[] = {1024, 2048, 4096};
constexpr uint32_t kTrailSize[] = {0, 1024, 2048};    // 0: no trails
constexpr uint32_t kParticles[] = {0, 8192, 24576};   // 0: no falling snow
constexpr double kCloudSteps[] = {0, 0.5, 1.0, 1.0};  // 0: no layer
constexpr bool kCloudQuarter[] = {true, true, true, false};
constexpr double kBloom[] = {0, 0.04, 0.04};
constexpr int kReachLevels[] = {5, 6, 7, kGroundLevels};  // the ground's levels drawn

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
// PRECIP: on a snowing or a raining day what falls is the weather near the eye; the low stop keeps
//   a third of it.
// CLOUDS: a broken sky's layers, marched at a quarter of the pixels (a quarter of those each
//   frame, the rest reprojected from the last) and upsampled; the low stop
//   marches half the steps, a slightly blockier cloud; the top marches every pixel, the
//   reference, a hair sharper at a ridge's edge; off, the sky keeps its mean cloud and loses its
//   shapes and shadows.
// BLOOM: the sun's glow off the snow and the lamps, small; its top stop adds the lens pass (the
//   sun streaming through the trees and over the ridges, and the flare when the player has it on),
//   a quarter of the pixels and only round the sun, worth a little more where the sun is in view.
// DISTANCE: the ground drawn to 1, 2 or 4 km and misted into the sky from half of it, or the
//   whole ground to the horizon; the near mountain is the run, the far one where it lies.
//
// Measured on the reference machine (an Apple M3 Max, Metal through Dawn, 1920x1080, seed 38
// under a flurry, the `start` view raised 15 degrees so the broken sky fills its top half, 60
// frames timed after 10, the least median of three rounds): `labs ladder --price --width=1920
// --height=1080 --frames=60 --warm=10 --rounds=3`. A picture's time is the frame's span on the
// GPU (`FrameStats::gpu_span_ms`), never the sum of its passes, which on a tiled GPU counts their
// overlap again and again. A stop that measured cheaper than the one under it is set level with
// it. The DISTANCE row is priced where it is seen, from the `vista` over the whole mountain on a
// clear day (`--view=vista --lift=0 --fall=0`): from the start the far ground is behind the slope
// and costs nothing; from above, the ground past 1 km is most of it. The presets measured 1.52
// (low), 2.88 (medium), 3.79 (high) and 7.89 ms (top): the top's every-pixel cloud march is
// most of the step from high.
constexpr PriceTable kTable{"Apple M3 Max (Metal), 1920x1080", 1920, 1080, 1.18};

constexpr r::LadderStop kResolution[] = {
    {"half", 0, 0}, {"two-thirds", 1.12, 3}, {"five-sixths", 2.16, 5}, {"full", 4.10, 6}};
constexpr r::LadderStop kShadows[] = {{"1024", 0, 0}, {"2048", 0.02, 5}, {"4096", 0.02, 7}};
constexpr r::LadderStop kSnow[] = {{"plain", 0, 0}, {"material", 0.25, 60}};
constexpr r::LadderStop kTrails[] = {{"off", 0, 0}, {"low", 0.08, 30}, {"high", 0.17, 36}};
constexpr r::LadderStop kSpray[] = {{"off", 0, 0}, {"on", 0.08, 20}};
constexpr r::LadderStop kPrecip[] = {{"off", 0, 0}, {"low", 0.07, 10}, {"high", 0.17, 14}};
constexpr r::LadderStop kClouds[] = {
    {"off", 0, 0}, {"low", 1.81, 12}, {"high", 2.07, 15}, {"full", 5.57, 16}};
constexpr r::LadderStop kBloomStops[] = {{"off", 0, 0}, {"on", 0.05, 4}, {"lens", 0.09, 6}};
constexpr r::LadderStop kDistance[] = {
    {"1 km", 0, 0}, {"2 km", 1.46, 10}, {"4 km", 3.05, 18}, {"full", 3.26, 22}};

const std::array<r::LadderRow, kPictureRows> kRows = {{
    {"resolution", kResolution, 1},
    {"shadows", kShadows},
    {"snow", kSnow},
    {"trails", kTrails},
    {"spray", kSpray},
    {"precip", kPrecip},
    {"clouds", kClouds},
    {"bloom", kBloomStops},
    {"distance", kDistance},
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
  const uint32_t particles = kParticles[at(PictureRow::precip)];
  o.precip = particles > 0;
  if (particles > 0) o.fall.particles = particles;
  o.clouds = kCloudSteps[at(PictureRow::clouds)];
  o.cloud_quarter = kCloudQuarter[at(PictureRow::clouds)];
  o.bloom = kBloom[at(PictureRow::bloom)];
  if (at(PictureRow::bloom) < 2) o.lens = {};  // the lens pass only at the row's top
  const int levels = kReachLevels[at(PictureRow::distance)];
  o.reach_m = levels < kGroundLevels ? r::clipmap_reach(kGroundSpacing, levels) : 0;
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
  // resolution, shadows, snow, trails, spray, precip, clouds, bloom, distance
  switch (p) {
    case Preset::low:
      return {0, 0, 1, 1, 1, 1, 0, 0, 1};
    case Preset::medium:
      return {1, 1, 1, 1, 1, 1, 1, 1, 2};
    case Preset::high:
      return {2, 1, 1, 2, 1, 2, 2, 2, 3};
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

std::optional<r::LadderPick> quality_pick(std::string_view quality, double budget_ms,
                                          uint32_t width, uint32_t height, std::string& error) {
  if (quality.empty() || quality == "custom") return std::nullopt;
  for (Preset p : {Preset::low, Preset::medium, Preset::high, Preset::top})
    if (quality == preset_name(p)) return preset_pick(p);
  // The table's prices are the reference machine's at its own size; a frame's cost is mostly its
  // pixels', so another window scales them by its share of the table's.
  const double pixels = static_cast<double>(width) * static_cast<double>(height);
  const double scale =
      pixels > 0 ? pixels / (static_cast<double>(kTable.width) * kTable.height) : 1.0;
  if (quality == "auto") return r::fit_ladder(kRows, budget_ms, kTable.floor_ms, scale).pick;
  double share = 0;
  if (!ge::console::parse_number(quality, share) || !(share >= 0 && share <= 1)) {
    error = "r_quality \"" + std::string(quality) +
            "\" is none of custom, low, medium, high, top, auto or a number from 0 to 1; the "
            "individual r_ settings are used";
    return std::nullopt;
  }
  const double top_ms = r::ladder_cost(kRows, preset_pick(Preset::top));
  return r::fit_ladder(kRows, (kTable.floor_ms + share * top_ms) * scale, kTable.floor_ms, scale)
      .pick;
}

}  // namespace fall_line::app
