// engine/lab/diff.hpp and the PNG reader: a picture written and read back is the same bits, a
// filtered PNG decodes, the same picture diffs to nothing, a change is measured where it is and
// banded by distance with each band's floor, the distance file round-trips, and two runs on disk
// are compared shot by shot with their sheets and diff.json written.
//
// ge:summary Tests the picture diff: PNG read back, PSNR and SSIM, bands and floors, two runs
// ge:keywords test lab diff png decode psnr ssim distance-band floor verdict diff-runs
// ge:skill labs testing
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "check.hpp"
#include "lab/diff.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"
#include "lab/report.hpp"

using namespace ge::lab;
namespace fs = std::filesystem;

namespace {

// A 64x48 picture: a gradient, so the SSIM has structure to compare.
Picture gradient() {
  Picture p = create_picture(64, 48, {0, 0, 0});
  for (int y = 0; y < 48; ++y) {
    for (int x = 0; x < 64; ++x) set_pixel(p, x, y, {uint8_t(x * 4), uint8_t(y * 5), 120});
  }
  return p;
}

// Near (5 m) in the bottom half, far (900 m) in the top half but for a strip of sky on top, at
// half the picture's size (a render scale).
DistanceMap halves() {
  DistanceMap m{32, 24, std::vector<float>(size_t{32} * 24)};
  for (int y = 0; y < 24; ++y) {
    for (int x = 0; x < 32; ++x) {
      m.metres[size_t(y) * 32 + x] = y < 4    ? std::numeric_limits<float>::infinity()
                                     : y < 12 ? 900.0f
                                              : 5.0f;
    }
  }
  return m;
}

const RegionDiff& band(const ImageDiff& d, const std::string& name) {
  for (const RegionDiff& r : d.bands) {
    if (r.name == name) return r;
  }
  return d.all;
}

}  // namespace

TEST(a_png_reads_back_as_written) {
  const Picture p = gradient();
  const auto back = decode_png(encode_png(p));
  CHECK(back.has_value());
  if (!back) return;
  CHECK(back->width == 64 && back->height == 48 && back->rgba == p.rgba);
  std::vector<uint8_t> bad = encode_png(p);
  bad[40] ^= 1;  // inside IHDR or IDAT: a checksum fails
  CHECK(!decode_png(bad).has_value());
}

TEST(a_compressed_png_with_every_row_filter_decodes) {
  // 3x5 RGBA, rows filtered none, sub, up, average and Paeth, compressed by another encoder.
  const std::vector<uint8_t> png = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
      0x44, 0x52, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x08, 0x06, 0x00, 0x00,
      0x00, 0x80, 0x71, 0x56, 0xa2, 0x00, 0x00, 0x00, 0x43, 0x49, 0x44, 0x41, 0x54, 0x78,
      0xda, 0x63, 0x60, 0x60, 0x60, 0x38, 0xa1, 0xc1, 0xcb, 0xf0, 0x3f, 0x40, 0x8a, 0xe1,
      0x04, 0x23, 0xbb, 0x11, 0xc3, 0x7f, 0x0d, 0x5e, 0xb9, 0x93, 0x40, 0x6c, 0xce, 0x04,
      0xe4, 0x9c, 0x64, 0x37, 0x92, 0x33, 0x67, 0x37, 0xb2, 0x39, 0xc9, 0xcc, 0x97, 0xc2,
      0x30, 0x5b, 0x42, 0xc1, 0xe6, 0xa4, 0x84, 0x82, 0xb7, 0x39, 0x0b, 0x58, 0x06, 0xa8,
      0x84, 0x9d, 0xd7, 0xe6, 0x24, 0x00, 0xd8, 0xa6, 0x0e, 0x6d, 0xc8, 0xb5, 0xb8, 0xe6,
      0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
  const std::vector<uint8_t> want = {0,  0,   0, 200, 40, 13,  0,   255, 80,  26,  0,   200,
                                     7,  50,  0, 255, 47, 63,  30,  200, 87,  76,  60,  255,
                                     14, 100, 0, 200, 54, 113, 60,  255, 94,  126, 120, 200,
                                     21, 150, 0, 255, 61, 163, 90,  200, 101, 176, 180, 255,
                                     28, 200, 0, 200, 68, 213, 120, 255, 108, 226, 240, 200};
  const auto p = decode_png(png);
  CHECK(p.has_value());
  if (!p) return;
  CHECK(p->width == 3 && p->height == 5 && p->rgba == want);
}

TEST(the_same_picture_diffs_to_nothing) {
  const Picture p = gradient();
  const DistanceMap m = halves();
  const ImageDiff d = diff_pictures(p, p, &m, {});
  CHECK(d.pass && d.has_distance);
  CHECK(d.all.psnr == 100 && d.all.ssim > 0.9999 && d.all.max == 0 && d.all.changed == 0);
  CHECK(d.bands.size() == 5);
}

TEST(a_far_change_passes_a_floor_a_near_one_would_fail) {
  const Picture a = gradient();
  const DistanceMap m = halves();
  // The same 12-level change near (rows 24..47), then far (rows 8..23).
  Picture near_change = a, far_change = a;
  for (int y = 0; y < 48; ++y) {
    for (int x = 0; x < 64; ++x) {
      Picture& p = y >= 24 ? near_change : far_change;
      if (y < 8) continue;
      uint8_t* px = &p.rgba[(size_t(y) * 64 + x) * 4];
      px[2] = uint8_t(px[2] + 12);
    }
  }
  DiffOptions o;
  o.min_psnr = {30, 30, 30, 20, 20};  // the 400 m+ band is held to 20 dB
  const ImageDiff near = diff_pictures(a, near_change, &m, o);
  const ImageDiff far = diff_pictures(a, far_change, &m, o);
  // 12 levels on one of three channels: rmse sqrt(48), psnr 20 log10(255 / 6.93) = 31.3 dB.
  CHECK(std::fabs(band(near, "0-25m").psnr - 31.32) < 0.05);
  CHECK(band(near, "0-25m").changed == 1.0 && band(near, "400m+").psnr == 100);
  CHECK(band(far, "400m+").pass && band(far, "0-25m").psnr == 100);
  CHECK(near.pass && far.pass);
  o.min_psnr = {35, 35, 35, 25, 25};
  CHECK(!diff_pictures(a, near_change, &m, o).pass);
  CHECK(diff_pictures(a, far_change, &m, o).pass);
  // The near-weighted mean counts the near change far more.
  CHECK(near.weighted_mean > 20 * far.weighted_mean);
  // With no distance map the whole picture is held to the nearest floor.
  const ImageDiff flat = diff_pictures(a, far_change, nullptr, o);
  CHECK(!flat.has_distance && flat.bands.empty() && flat.all.judged);
}

TEST(the_options_parse_or_say_why) {
  DiffOptions o;
  std::string error;
  CHECK(parse_diff_options("10,50", "45,40,30", o, error));
  CHECK(o.edges.size() == 2 && o.min_psnr.size() == 3);
  CHECK(!parse_diff_options("50,10", "", o, error) && error.find("ascending") != std::string::npos);
  CHECK(!parse_diff_options("", "40,x", o, error) &&
        error.find("--diff-psnr") != std::string::npos);
}

TEST(a_distance_map_round_trips) {
  const DistanceMap m = halves();
  const auto back = decode_distance(encode_distance(m));
  CHECK(back.has_value());
  if (!back) return;
  CHECK(back->width == 32 && back->height == 24);
  CHECK(std::isinf(back->metres[0]) && back->metres[size_t(23) * 32] == 5.0f);
  CHECK(!decode_distance(std::vector<uint8_t>(20, 0)).has_value());
}

TEST(two_runs_on_disk_compare_shot_by_shot) {
  const fs::path root = fs::temp_directory_path() / "ge_lab_diff_test";
  fs::remove_all(root);
  static const LabSpec spec{"diff-test", LabKind::picture, "engine", "", "labs", "", {},
                            "",          nullptr};
  auto write = [&](const std::string& dir, uint8_t shift) {
    Lab lab(spec, {});
    Picture p = gradient();
    for (size_t i = 0; i < p.rgba.size(); i += 4) p.rgba[i + 1] = uint8_t(p.rgba[i + 1] + shift);
    lab.shot({"view", "A VIEW", "a gradient", "", {}}, p, Det::measured, halves());
    lab.shot({"plain", "NO DISTANCE", "a gradient", "", {}}, gradient());
    std::string error;
    CHECK(write_report(lab, (root / dir).string(), error));
  };
  write("lab@before", 0);
  write("lab", 40);
  const RunDiff run = diff_runs((root / "lab@before").string(), (root / "lab").string(),
                                (root / "lab" / "diff").string(), {});
  CHECK(run.error.empty() && run.shots.size() == 2);
  CHECK(!run.pass);  // the view moved 40 levels everywhere
  for (const ShotDiff& s : run.shots) {
    CHECK(s.problem.empty());
    CHECK(fs::exists(s.sheet));
    const auto sheet = read_png(s.sheet);
    CHECK(sheet.has_value());
    if (sheet) CHECK(sheet->width >= 2 * 64);
    if (s.id == "plain") CHECK(s.diff.pass && !s.diff.has_distance);
    if (s.id == "view") CHECK(!s.diff.pass && s.diff.has_distance);
  }
  CHECK(fs::exists(root / "lab" / "diff" / "diff.json"));
  CHECK(format_run_diff(run).find("DIFF view") != std::string::npos);
  // A kept run that is not there.
  const RunDiff none =
      diff_runs((root / "lab@nope").string(), (root / "lab").string(), (root / "x").string(), {});
  CHECK(!none.pass && none.error.find("--tag") != std::string::npos);
  fs::remove_all(root);
}
