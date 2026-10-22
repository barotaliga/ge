// THE PICTURE DIFF: did a change move what a lab's pictures show, by how much, and how near the
// player? `labs <id> --compare=<tag>` (and `--diff=<tag>`, which runs nothing) holds every shot
// of a run against the same shot of the run kept with `--tag=<tag>`, and `ge lab diff` reads
// what it finds as findings.
//
// THE NUMBERS, each over the whole picture and over every DISTANCE BAND when the lab handed a
// distance map with the shot (lab.hpp's `DistanceMap`, off the frame's depth):
//   mean, max   the mean and the largest change of a colour channel, levels of 255
//   rmse, psnr  the root mean square change, and the peak signal over it in dB (100: the same)
//   ssim        the structural similarity of the luma, over 8x8 windows every 4 pixels
//   changed     the share of pixels whose largest channel change is over the tolerance
// and over the whole picture a NEAR-WEIGHTED mean: each pixel's mean change weighted by
// 1 / (1 + distance / weight_metres), so a change at the skis counts and one on the far
// ridge hardly does.
//
// THE VERDICT is per band: a band passes while its PSNR stays at or over its floor (by default
// 40 dB near, 36, 32 and 28 farther out, 28 for the sky), so a far change may be coarser than a
// near one. A band with fewer than 64 pixels is reported, not judged. With no distance map the
// whole picture is held to the nearest band's floor.
//
// THE SHEET (<out>/<shot>.png) is four panels: BEFORE and AFTER, the CHANGE (the after dimmed,
// every changed pixel coloured blue to yellow to red by how much, the bands' edges drawn as
// contour lines) and the DISTANCE (each band in its own colour, labelled in metres), with every
// band's numbers and verdict in the caption band under them.
//
// ge:summary The picture diff: per-band PSNR, SSIM and change, verdicts, the four-panel sheet
// ge:keywords lab diff image-diff pixel-diff compare before after heatmap psnr ssim rmse
// ge:keywords distance-band depth threshold verdict contour sheet lab-diff ge-lab-diff
// ge:skill labs
// ge:test tests/lab_diff_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace ge::lab {

struct DiffOptions {
  // The bands' edges, m, ascending: {25, 100, 400} makes 0-25, 25-100, 100-400, 400+ and the sky.
  std::vector<double> edges = {25, 100, 400};
  // Each band's floor, dB, nearest first, then the sky's; the last is repeated if short.
  std::vector<double> min_psnr = {40, 36, 32, 28, 28};
  int tolerance = 8;          // levels: a pixel changed when a channel moved more
  double weight_metres = 25;  // the near-weighted mean's falloff
};

// "25,100,400" and "40,36,32,28,28" into `options`; false with `error` when one does not parse.
bool parse_diff_options(const std::string& edges, const std::string& min_psnr, DiffOptions& options,
                        std::string& error);

struct RegionDiff {
  std::string name;  // "all", "0-25m", "400m+", "sky"
  size_t pixels = 0;
  double mean = 0, max = 0, rmse = 0, psnr = 100, ssim = 1, changed = 0;
  double min_psnr = 0;  // the floor it is held to
  bool judged = false;  // enough pixels, and a floor
  bool pass = true;
};

struct ImageDiff {
  int width = 0, height = 0;
  bool has_distance = false;
  RegionDiff all;
  std::vector<RegionDiff> bands;  // nearest first, then the sky; empty without a distance map
  double weighted_mean = 0;       // levels; the plain mean without a distance map
  bool pass = true;
};

// `before` against `after` (the same size), banded by `distance` when given.
ImageDiff diff_pictures(const Picture& before, const Picture& after, const DistanceMap* distance,
                        const DiffOptions& options);

// The four panels; captioned by the caller.
Picture diff_sheet(const Picture& before, const Picture& after, const DistanceMap* distance,
                   const DiffOptions& options);

// A distance map as the file a report writes beside a shot (<id>.distance: "GEDIST1\n", the
// width and height as little-endian u32, then the metres as little-endian f32), and back.
std::vector<uint8_t> encode_distance(const DistanceMap& map);
std::optional<DistanceMap> decode_distance(std::span<const uint8_t> bytes);

// EVERY SHOT OF TWO RUNS. The shots are those the after run's report.json lists; each is
// cropped out of its caption band, banded by the after run's distance map, and its sheet written
// to `out_dir`, with diff.json beside them.
struct ShotDiff {
  std::string id;
  std::string sheet;    // the sheet's path; empty when the shot could not be compared
  std::string problem;  // why not: missing before, a different size, an unreadable file
  ImageDiff diff;
};
struct RunDiff {
  std::string before_dir, after_dir, out_dir;
  std::vector<ShotDiff> shots;
  bool pass = true;   // every compared shot passes
  std::string error;  // the run could not be compared at all
};
RunDiff diff_runs(const std::string& before_dir, const std::string& after_dir,
                  const std::string& out_dir, const DiffOptions& options);

// The printed table (a row a shot and band, the failures first marked), and diff.json's text.
std::string format_run_diff(const RunDiff& run);
std::string run_diff_json(const RunDiff& run);

}  // namespace ge::lab
