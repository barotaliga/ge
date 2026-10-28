// LAB `snow-lod`: the snow layer's level of detail (engine/snow/lod.hpp) over a long run. A
// --length m strip of powder, 128 m wide, is skied top to bottom by a skier the game names as the
// focus, while two skiers and a snowmobile far from him cross it too. The layer keeps --budget
// fine tiles at most; what no focus is near is folded into the coarse layer every second, and the
// far traffic lands there directly. Pictured:
//
// - THE WHOLE STRIP from above, shaded by how far the snow stands under the cover (the worn share),
//   with the fine tiles live at the end outlined in red and the coarse blocks in blue;
// - THE MEMORY over the run against the budget, and the live tiles;
// - TWO CLOSE-UPS of the skier's own track, relief-shaded: where he is at the end (fine) and where
//   he started (folded, then read from the coarse layer).
//
// The lab fails when the live tiles pass the budget, a sample is refused, or the mass the tiles and
// the coarse layer hold plus what spilled is not what they were given.
//
// ge:summary Lab: snow level of detail over a long run: focus, folding, the coarse layer, memory
// ge:keywords lab snow lod focus fold refine coarse budget memory tiles track far-traffic
// ge:skill snow labs
// ge:test tests/snow_lod_test.cpp
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "snow/layer.hpp"
#include "snow/lod.hpp"
#include "snow/press.hpp"
#include "snow/reference.hpp"

namespace {

namespace sn = ge::snow;
namespace gl = ge::lab;
namespace lm = ge::core::libm;

constexpr double kHz = 120;        // steps a second
constexpr double kHalfWidth = 64;  // m, the strip's half width
constexpr double kSpeed = 12;      // m/s down the strip

std::string fmt(double v, int places) {
  char b[48];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

// A skier's pair of skis at (x, z) heading `heading`, each from where it stood a step ago.
struct Skier {
  double last[2][2] = {{0, 0}, {0, 0}};
  bool started = false;
  void step(sn::Layer& layer, double x, double z, double heading, double t, int& presses) {
    const double rx = lm::cos(heading), rz = -lm::sin(heading);
    for (int k = 0; k < 2; k++) {
      const double side = k ? 0.11 : -0.11;
      const double sx = x + rx * side, sz = z + rz * side;
      sn::Imprint imp;
      imp.shape = sn::capsule(started ? last[k][0] : sx, started ? last[k][1] : sz, sx, sz, 0.035);
      imp.sink = 0.08;
      imp.side_push = 0.05;
      imp.motion = sn::Motion::slide;
      sn::press(layer, imp, t);
      presses++;
      last[k][0] = sx;
      last[k][1] = sz;
    }
    started = true;
  }
};

// The skier's line: S-turns down the strip.
double line_x(double z, double phase) { return 18 * lm::sin(z * 0.06 + phase); }

void relief(gl::Picture& pic, const sn::Layer& layer, double x0, double z0, double size, int ppm) {
  const double e = 0.05;
  const double lx = -0.5 / 0.985, ly = 0.6 / 0.985, lz = 0.6 / 0.985;
  for (int j = 0; j < pic.height; j++) {
    for (int i = 0; i < pic.width; i++) {
      const double x = x0 + (i + 0.5) / ppm, z = z0 + size - (j + 0.5) / ppm;
      auto top = [&](double qx, double qz) { return sn::sample(layer, qx, qz).depth * 2; };
      const double gx = (top(x + e, z) - top(x - e, z)) / (2 * e);
      const double gz = (top(x, z + e) - top(x, z - e)) / (2 * e);
      const double nn = std::sqrt(gx * gx + 1 + gz * gz);
      const double lit = ge::core::clamp((-gx * lx + ly - gz * lz) / nn, 0, 1);
      auto ch = [&](double sun, double amb) {
        return static_cast<uint8_t>(ge::core::clamp(sun * lit + amb * 0.8, 0, 255));
      };
      gl::set_pixel(pic, i, j, {ch(235, 70), ch(238, 92), ch(240, 128), 255});
    }
  }
}

void run(gl::Lab& lab) {
  const double length = lab.arg_num("length");
  const auto budget = static_cast<uint32_t>(lab.arg_num("budget"));
  const int ppm = static_cast<int>(lab.arg_num("px-per-m"));
  sn::LayerSpec spec;
  spec.region = {-kHalfWidth, 0, kHalfWidth, length};
  spec.cover.depth = 0.5;
  spec.max_tiles = budget;
  spec.near = 32;
  spec.stale = 20;
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, spec);

  Skier focus_skier, far_a, far_b;
  const sn::Imprint tread = sn::reference_contact(4).imprint;
  const double v = kSpeed / kHz;
  const int steps = static_cast<int>((length - 8) / v);
  int presses = 0, peak_tiles = 0, far_presses = 0;
  size_t peak_bytes = 0;
  gl::Trace memory{"memory (KB)", {}, {}}, tiles{"live tiles", {}, {}};
  for (int n = 0; n < steps; n++) {
    const double t = n / kHz;
    const double z = length - 4 - n * v;
    const double x = line_x(z, 0);
    const double focus[2] = {x, z};
    sn::set_focus(layer, focus, t);
    const double heading = lm::atan2(line_x(z - v, 0) - x, -v);
    focus_skier.step(layer, x, z, heading, t, presses);
    // Far traffic: two skiers 200 m behind, out on the wings, and a snowmobile climbing.
    const double zb = z + 200;
    if (zb < length - 4) {
      far_a.step(layer, -44 + line_x(zb, 1), zb, heading, t, presses);
      far_b.step(layer, 44 + line_x(zb, 2), zb, heading, t, presses);
      far_presses += 4;
    }
    if (n % 4 == 0) {
      const double zs = 4 + n * v * 1.5;
      if (zs < length - 4) {
        sn::press(layer, sn::place(tread, -30, zs, 0), t);
        presses++;
        far_presses++;
      }
    }
    if (n % static_cast<int>(kHz) == 0) {
      sn::fold_stale(layer, t);
      const size_t bytes = sn::bytes_of(layer);
      memory.x.push_back(t);
      memory.y.push_back(static_cast<double>(bytes) / 1024);
      tiles.x.push_back(t);
      tiles.y.push_back(sn::live_tiles(layer));
    }
    if (sn::live_tiles(layer) > peak_tiles) peak_tiles = sn::live_tiles(layer);
    if (sn::bytes_of(layer) > peak_bytes) peak_bytes = sn::bytes_of(layer);
  }

  const bool held = sn::mass_units(layer) + layer.spilled == layer.mass_in;
  if (!held) {
    lab.fail("the level of detail made or lost snow",
             "fold, refine and the coarse press must keep the mass to the unit: run "
             "tests/snow_lod_test.cpp (skill: snow).");
  }
  if (peak_tiles > static_cast<int>(budget)) {
    lab.fail("the live tiles reached " + std::to_string(peak_tiles) + ", over the budget of " +
                 std::to_string(budget),
             "Read `cell_for` and `stalest_tile` in engine/snow (skill: snow).");
  }
  if (layer.refused > 0) {
    lab.fail(std::to_string(layer.refused) + " samples were refused with folding on",
             "A press larger than the whole budget is refused; raise --budget or read "
             "`stalest_tile` (skill: snow).");
  }
  lab.metric("presses", presses, "count");
  lab.metric("far-presses", far_presses, "count");
  lab.metric("folds", layer.folds, "count");
  lab.metric("refines", layer.refines, "count");
  lab.metric("peak-tiles", peak_tiles, "count");
  lab.metric("coarse-blocks", static_cast<double>(layer.coarse.size()), "count");
  lab.metric("peak-memory", static_cast<double>(peak_bytes) / 1024, "KB");
  lab.metric("mass-held", held ? 1 : 0, "");
  const double all_fine =
      static_cast<double>(layer.tile_cols * layer.tile_rows) * sizeof(sn::Tile) / (1024 * 1024);
  gl::LabTable table{"the run", {"what", "number"}, {}};
  table.rows.push_back({"presses (far from the focus)",
                        std::to_string(presses) + " (" + std::to_string(far_presses) + ")"});
  table.rows.push_back({"tiles folded / refined",
                        std::to_string(layer.folds) + " / " + std::to_string(layer.refines)});
  table.rows.push_back({"peak live tiles (budget)",
                        std::to_string(peak_tiles) + " (" + std::to_string(budget) + ")"});
  table.rows.push_back({"coarse blocks", std::to_string(layer.coarse.size())});
  table.rows.push_back({"peak memory", fmt(static_cast<double>(peak_bytes) / 1024, 0) + " KB"});
  table.rows.push_back({"every tile fine would be", fmt(all_fine, 1) + " MB"});
  lab.table(table);

  const std::vector<std::pair<std::string, std::string>> inputs = {{"length", lab.arg("length")},
                                                                   {"budget", lab.arg("budget")}};

  // THE STRIP.
  const int w = static_cast<int>(2 * kHalfWidth * ppm), h = static_cast<int>(length * ppm);
  gl::Picture strip = gl::create_picture(w, h, {255, 255, 255, 255});
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      const double x = -kHalfWidth + (i + 0.5) / ppm, z = length - (j + 0.5) / ppm;
      const sn::Column c = sn::sample(layer, x, z);
      const double worn = ge::core::clamp(c.drop / 0.06, 0, 1);
      auto mix = [&](double a, double b) { return static_cast<uint8_t>(a + (b - a) * worn); };
      gl::set_pixel(strip, i, j, {mix(250, 40), mix(251, 70), mix(255, 120), 255});
    }
  }
  const double tile_m = sn::kTileCells * spec.cell;
  auto box = [&](int tx, int tz, gl::Rgba colour) {
    const double x0 = tx * tile_m * ppm, y0 = h - (tz + 1) * tile_m * ppm, s = tile_m * ppm;
    gl::draw_line(strip, x0, y0, x0 + s, y0, colour);
    gl::draw_line(strip, x0, y0 + s, x0 + s, y0 + s, colour);
    gl::draw_line(strip, x0, y0, x0, y0 + s, colour);
    gl::draw_line(strip, x0 + s, y0, x0 + s, y0 + s, colour);
  };
  for (const sn::CoarseBlock& b : layer.coarse) box(b.tx, b.tz, {60, 110, 200, 160});
  for (const sn::Tile& t : layer.tiles) {
    if (t.tx >= 0) box(t.tx, t.tz, {210, 60, 40, 255});
  }
  lab.shot(
      {"strip", "THE WHOLE RUN, FINE WHERE THE SKIER IS AND COARSE BEHIND HIM",
       fmt(length, 0) +
           " m of powder, 128 m wide, from above (downhill is down), shaded by "
           "how far the snow stands under the cover. The focus skier's S-turns run the middle; "
           "two skiers far behind him ski the wings and a snowmobile climbs the left. Red: the " +
           std::to_string(sn::live_tiles(layer)) +
           " fine tiles live at the end (6.4 m); blue: the coarse blocks holding the rest.",
       "red tiles only round the skier's last few seconds at the bottom; his track above "
       "them a softer, wider coarse band (1.6 m cells), unbroken; the far tracks coarse from "
       "the start",
       inputs},
      strip);

  // THE MEMORY.
  gl::LineChart mem;
  mem.frame = {"Memory over the run", "the tiles, coarse blocks and records; the budget dashed",
               "s", "KB"};
  mem.traces = {memory};
  mem.rules = {static_cast<double>(budget) * sizeof(sn::Tile) / 1024};
  mem.spikes = false;
  const gl::Chart drawn = gl::line_chart(mem);
  lab.shot({"memory", "MEMORY AGAINST THE BUDGET",
            "What the layer holds each second: rising to the budget as the skier lays fine tiles, "
            "then held under it while the stalest tiles fold, with the coarse layer's slow growth "
            "on top.",
            "a quick rise, then a flat line just under the dashed budget, creeping up as coarse "
            "blocks are added",
            inputs},
           gl::to_picture(drawn.plot));

  // THE CLOSE-UPS.
  const int cppm = ppm * 6;
  const double size = 20;
  const double z_end = length - 4 - (steps - 1) * v, z_start = length - 24;
  gl::Picture near_pic = gl::create_picture(static_cast<int>(size * cppm),
                                            static_cast<int>(size * cppm), {255, 255, 255, 255});
  gl::Picture far_pic = near_pic;
  relief(near_pic, layer, line_x(z_end, 0) - size / 2, z_end, size, cppm);
  relief(far_pic, layer, line_x(z_start + 10, 0) - size / 2, z_start, size, cppm);
  lab.shot({"near", "WHERE THE SKIER IS: FINE",
            "20 m of his track at the end of the run, relief-shaded from fine tiles.",
            "two sharp parallel ski grooves curving through the frame", inputs},
           near_pic);
  lab.shot({"start", "WHERE HE STARTED: FOLDED",
            "20 m of his track at the start, folded into the coarse layer a minute later and read "
            "back from it: the snow's mass where it was, its detail spread over 1.6 m cells.",
            "a soft stepped band where the grooves were, no sharp edges", inputs},
           far_pic);
}

const gl::LabSpec kSpec{
    "snow-lod",
    gl::LabKind::picture,
    "engine",
    "engine/snow/lod.hpp",
    "snow",
    "Does a long run keep fine snow only where the skier is, inside the budget, without losing "
    "snow?",
    {
        {"length", "768", "the strip's length, m"},
        {"budget", "48", "fine tiles kept at most"},
        {"px-per-m", "2", "the strip's pixels a metre (the close-ups six times that)"},
    },
    "--length=240 --budget=24 --px-per-m=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
