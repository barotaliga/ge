// The pattern painters: bricks and tiles in their joints, planks with grain and knots, metal
// panels with seams and rivets, uneven dressed blocks (ashlar), a field of studs (domes,
// diamond-plate bars, pyramids or holes) and overlapping scales. Each cuts its structure into the
// height (proud faces, sunken joints, rounded edges) so the life layers after it find joints to
// fill and edges to wear, and gives every piece its own colour off a hash of its place, so the tile
// still repeats.
//
// ge:summary Pattern painters: bricks, tiles, planks, riveted panels, blocks, studs, scales
// ge:keywords pattern bricks tiles mortar grout planks wood grain knots panels seams rivets
// ge:keywords blocks ashlar masonry studs diamond-plate dimples grate holes scales arches
// ge:skill textures
// ge:test tests/texgen_bake_test.cpp
#include <algorithm>
#include <cmath>
#include <vector>

#include "texgen/layers.hpp"
#include "texgen/noise.hpp"

namespace ge::texgen {
namespace {

// A piece's colour: `c` shifted in brightness by up to `vary` and a little in hue, off the hash.
Rgb varied(Rgb c, double vary, uint32_t a, uint32_t b, uint32_t seed) {
  const double k = 1 + vary * (2 * hash01(a, b, seed) - 1);
  const double hue = vary * 0.12;
  return {c.r * k * (1 + hue * (2 * hash01(a, b, seed + 1) - 1)), c.g * k,
          c.b * k * (1 + hue * (2 * hash01(a, b, seed + 2) - 1))};
}

}  // namespace

void paint_bricks(Surface& s, const Args& a) {
  const int rows = a.i("rows"), cols = a.i("cols");
  const double offset = a.n("offset"), depth = a.n("depth"), vary = a.n("vary");
  const double rough = a.n("rough");
  // Joint and bevel widths in tile units: shares of a course's height.
  const double joint = a.n("mortar") / rows * 0.5;
  const double bevel = std::max(a.n("bevel") / rows * 0.5, 1.0 / s.size);
  const Rgb brick = a.c("color"), mortar = a.c("mortar_color");
  NoiseSpec ragged{NoiseKind::perlin, rows * 4, 3, 0.5, 1, 0, a.seed + 5};
  const double aa = 1.0 / s.size;
  for_texels(s.size, [&](int x, int y, double u, double v) {
    const double gy = v * rows;
    const int row = static_cast<int>(std::floor(gy));
    const double gx = u * cols + offset * (row % 2);
    const double col = std::floor(gx);
    const double fx = gx - col, fy = gy - row;
    const double d = std::min(std::min(fx, 1 - fx) / cols, std::min(fy, 1 - fy) / rows);
    // The joint's edge wanders a little so no two bricks meet the mortar the same way.
    const double w = joint * (0.75 + 0.5 * sample(ragged, u, v));
    const uint32_t bx = wrap_cell(col, cols), by = static_cast<uint32_t>(row);
    const double face = smoothstep(w, w + bevel, d);
    const double in = smoothstep(w - aa, w + aa, d);
    const double tilt = depth * 0.08 * (hash01(bx, by, a.seed + 9) - 0.5);
    const size_t i = s.at(x, y);
    set_colour(s, i, mix(mortar, varied(brick, vary, bx, by, a.seed), in));
    s.plane[kHeight][i] = static_cast<float>(clamp01(0.5 + depth * (face - 0.5) + tilt * face));
    s.plane[kRough][i] = static_cast<float>(lerp(0.95, rough, in));
    s.plane[kMetal][i] = 0;
  });
}

void paint_planks(Surface& s, const Args& a) {
  const int rows = a.i("rows");
  const double vary = a.n("vary"), seam = a.n("seam") / rows * 0.5, grain = a.n("grain");
  const double knots = a.n("knots"), depth = a.n("depth"), rough = a.n("rough");
  const Rgb wood = a.c("color");
  const double aa = 1.0 / s.size;
  // The grain: a ring field squeezed along y and wandering, read as bands.
  NoiseSpec rings{NoiseKind::perlin, 2, 3, 0.5, 1, 0.08, a.seed + 3};
  NoiseSpec fibre{NoiseKind::value, 64, 2, 0.5, 1, 0, a.seed + 4};
  NoiseSpec knot{NoiseKind::cells, 3, 1, 0.5, 1, 0, a.seed + 6};
  for_texels(s.size, [&](int x, int y, double u, double v) {
    const double gy = v * rows;
    const int row = static_cast<int>(std::floor(gy));
    const double fy = gy - row;
    const auto by = static_cast<uint32_t>(row);
    // One board end a row, somewhere along it.
    const double end = hash01(by, 0, a.seed + 1);
    double du = std::fabs(u - end);
    du = std::min(du, 1 - du);
    const double d = std::min(du, std::min(fy, 1 - fy) / rows);
    const bool before = u < end;
    const uint32_t board = by * 2 + (before ? 1u : 0u);
    const double shift = hash01(board, 7, a.seed);
    // Squeezing y by a whole number keeps the field tiling; the grain runs along the board.
    const double r = sample(rings, u + shift, v * rows * 2);
    // Bands from the ring field, thin and dark against the wood between.
    const double band = r * 14 - std::floor(r * 14);
    double g = smoothstep(0.0, 0.25, band) * smoothstep(1.0, 0.55, band);
    g = lerp(1.0, 0.55 + 0.45 * g, grain) * (0.92 + 0.08 * sample(fibre, u, v * 8));
    const double k = sample(knot, u + shift, v);
    const double knotted = knots > 0 ? smoothstep(0.12 * knots, 0.0, k) : 0;
    const double in = smoothstep(seam - aa, seam + aa, d);
    const size_t i = s.at(x, y);
    Rgb c = scaled(varied(wood, vary, board, 3, a.seed), g * (1 - 0.45 * knotted));
    c = mix({0.08, 0.06, 0.05}, c, in);
    set_colour(s, i, c);
    s.plane[kHeight][i] = static_cast<float>(
        clamp01(0.5 + depth * (smoothstep(seam, seam + 2 * aa, d) - 0.5) - 0.03 * (1 - g)));
    s.plane[kRough][i] = static_cast<float>(lerp(0.95, rough, in));
    s.plane[kMetal][i] = 0;
  });
}

void paint_panels(Surface& s, const Args& a) {
  const int rows = a.i("rows"), cols = a.i("cols"), rivets = a.i("rivets");
  const double seam = a.n("seam") * 0.5, bevel = std::max(a.n("bevel"), 1.0 / s.size);
  const double depth = a.n("depth"), vary = a.n("vary"), rough = a.n("rough");
  const double metal = a.n("metal"), rr = a.n("rivet_size"), rh = a.n("rivet_height");
  const std::string& sides = a.w("rivet_sides");
  const bool on_x = sides != "y", on_y = sides != "x";
  const Rgb plate = a.c("color");
  const double aa = 1.0 / s.size;
  for_texels(s.size, [&](int x, int y, double u, double v) {
    const double gx = u * cols, gy = v * rows;
    const double cx = std::floor(gx), cy = std::floor(gy);
    // In panel units: shares of the smaller side.
    const double side = std::min(1.0 / cols, 1.0 / rows);
    const double px = (gx - cx) / cols / side, py = (gy - cy) / rows / side;
    const double wx = 1.0 / cols / side, wy = 1.0 / rows / side;
    const double d = std::min(std::min(px, wx - px), std::min(py, wy - py));
    const double in = smoothstep(seam - aa / side, seam + aa / side, d);
    const double face = smoothstep(seam, seam + bevel, d);
    double rivet = 0;
    if (rivets > 0) {
      // Rivets in a row along each edge, a little inside the bevel.
      const double inset = seam + bevel + rr * 1.6;
      const double ex = std::min(px, wx - px), ey = std::min(py, wy - py);
      auto along = [&](double t, double len) {
        const double step = (len - 2 * inset) / std::max(1, rivets - 1);
        const double k = std::clamp(std::floor((t - inset) / step + 0.5), 0.0, rivets - 1.0);
        return t - (inset + k * step);
      };
      const double dx1 = ex - inset, dy1 = along(py, wy);
      const double dx2 = along(px, wx), dy2 = ey - inset;
      const double r2 =
          std::min(on_x ? dx1 * dx1 + dy1 * dy1 : 1e9, on_y ? dx2 * dx2 + dy2 * dy2 : 1e9);
      const double rd = std::sqrt(r2) / rr;
      rivet = rd < 1 ? std::sqrt(1 - rd * rd) : 0;
    }
    const auto bx = wrap_cell(cx, cols), by = wrap_cell(cy, rows);
    const size_t i = s.at(x, y);
    Rgb c = varied(plate, vary, bx, by, a.seed);
    c = mix(scaled(plate, 0.35), c, in);
    set_colour(s, i, mix(c, scaled(plate, 1.1), rivet * 0.5));
    s.plane[kHeight][i] = static_cast<float>(clamp01(0.5 + depth * (face - 0.5) + rh * rivet));
    s.plane[kRough][i] = static_cast<float>(lerp(0.9, rough, in) - 0.1 * rivet);
    s.plane[kMetal][i] = static_cast<float>(metal * in);
  });
}

namespace {

// Uneven cuts of [0, 1): `n` pieces whose sizes differ by up to `vary`, as n + 1 boundaries.
std::vector<double> cuts(int n, double vary, uint32_t a, uint32_t seed) {
  std::vector<double> b(static_cast<size_t>(n) + 1, 0.0);
  for (int k = 0; k < n; ++k)
    b[static_cast<size_t>(k) + 1] = b[static_cast<size_t>(k)] + 1 +
                                    vary * 1.6 * (hash01(a, static_cast<uint32_t>(k), seed) - 0.5);
  for (double& x : b) x /= b.back();
  return b;
}

// The piece of `b` that holds t, and t's distance to that piece's nearer cut.
int piece(const std::vector<double>& b, double t, double& edge) {
  int k = 0;
  while (k + 2 < static_cast<int>(b.size()) && t >= b[static_cast<size_t>(k) + 1]) ++k;
  edge = std::min(t - b[static_cast<size_t>(k)], b[static_cast<size_t>(k) + 1] - t);
  return k;
}

}  // namespace

void paint_blocks(Surface& s, const Args& a) {
  const int rows = a.i("rows"), cols = a.i("cols");
  const double vary_size = a.n("vary_size"), depth = a.n("depth"), vary = a.n("vary");
  const double rough = a.n("rough"), chip = a.n("chip");
  const double joint = a.n("mortar") / rows * 0.5;
  const double bevel = std::max(a.n("bevel") / rows * 0.5, 1.0 / s.size);
  const Rgb stone = a.c("color"), mortar = a.c("mortar_color");
  // The courses' heights, then each course's blocks: how many, how wide, how far shifted.
  const std::vector<double> course = cuts(rows, vary_size, 0, a.seed);
  std::vector<std::vector<double>> block(static_cast<size_t>(rows));
  std::vector<double> shift(static_cast<size_t>(rows));
  for (int r = 0; r < rows; ++r) {
    const auto rr = static_cast<uint32_t>(r);
    const int n = std::max(
        1, static_cast<int>(
               std::floor(cols + cols * vary_size * (hash01(rr, 1, a.seed + 2) - 0.5) + 0.5)));
    block[static_cast<size_t>(r)] = cuts(n, vary_size, rr + 1, a.seed + 3);
    shift[static_cast<size_t>(r)] = hash01(rr, 2, a.seed + 4);
  }
  NoiseSpec ragged{NoiseKind::perlin, rows * 4, 3, 0.5, 1, 0, a.seed + 5};
  const double aa = 1.0 / s.size;
  for_texels(s.size, [&](int x, int y, double u, double v) {
    double ey = 0, ex = 0;
    const int r = piece(course, v, ey);
    double t = u + shift[static_cast<size_t>(r)];
    t -= std::floor(t);
    const int k = piece(block[static_cast<size_t>(r)], t, ex);
    const double d = std::min(ex, ey);
    const double w = joint * (1 + chip * (sample(ragged, u, v) - 0.5) * 2);
    const auto bx = static_cast<uint32_t>(k), by = static_cast<uint32_t>(r);
    const double face = smoothstep(w, w + bevel, d);
    const double in = smoothstep(w - aa, w + aa, d);
    const double tilt = depth * 0.1 * (hash01(bx, by, a.seed + 9) - 0.5);
    const size_t i = s.at(x, y);
    set_colour(s, i, mix(mortar, varied(stone, vary, bx, by + 1000, a.seed), in));
    s.plane[kHeight][i] = static_cast<float>(clamp01(0.5 + depth * (face - 0.5) + tilt * face));
    s.plane[kRough][i] = static_cast<float>(lerp(0.95, rough, in));
    s.plane[kMetal][i] = 0;
  });
}

void paint_studs(Surface& s, const Args& a) {
  const int rows = a.i("rows"), cols = a.i("cols");
  const double size = a.n("size") * 0.5, offset = a.n("offset"), depth = a.n("depth");
  const double amount = a.n("amount");
  const std::string& shape = a.w("shape");
  const bool coloured = a.given("color");
  const Rgb colour = a.c("color");
  const double sx = static_cast<double>(std::max(rows, cols)) / cols;
  const double sy = static_cast<double>(std::max(rows, cols)) / rows;
  const double aa = static_cast<double>(std::max(rows, cols)) / s.size;
  for_texels(s.size, [&](int x, int y, double u, double v) {
    const double gy = v * rows;
    const int row = static_cast<int>(std::floor(gy));
    const double gx = u * cols + offset * (row % 2);
    const int col = static_cast<int>(std::floor(gx));
    // Centred in the cell, in units of its shorter side, so a stud is round in any cell.
    const double px = (gx - col - 0.5) * sx, py = (gy - row - 0.5) * sy;
    double r = 0;  // 0 at the stud's middle, 1 at its rim
    if (shape == "diamond") {
      r = (std::fabs(px) + std::fabs(py)) / size;
    } else if (shape == "bar") {
      // Diamond plate: a short bar, leaning one way or the other like a checkerboard.
      const double sgn = ((row + col) & 1) != 0 ? 1.0 : -1.0;
      const double along = (px + sgn * py) * 0.7071067811865476;
      const double across = (px - sgn * py) * 0.7071067811865476;
      const double len = std::max(0.0, std::fabs(along) - size * 0.9);
      r = std::sqrt(len * len + across * across) / (size * 0.35);
    } else {
      r = std::sqrt(px * px + py * py) / size;
    }
    const double cover = smoothstep(1 + aa / size, 1 - aa / size, r);
    if (cover <= 0) return;
    const double dome = std::sqrt(std::max(0.0, 1 - std::min(r, 1.0) * std::min(r, 1.0)));
    const double lift = shape == "hole" ? -depth : depth * (shape == "diamond" ? 1 - r : dome);
    const size_t i = s.at(x, y);
    s.plane[kHeight][i] = static_cast<float>(clamp01(s.plane[kHeight][i] + lift * cover));
    Rgb c = colour_at(s, i);
    if (shape == "hole") c = scaled(c, 1 - 0.8 * cover);
    if (coloured) c = mix(c, colour, amount * cover);
    set_colour(s, i, c);
  });
}

void paint_scales(Surface& s, const Args& a) {
  const int rows = std::max(2, a.i("rows") + a.i("rows") % 2), cols = a.i("cols");
  const double rim = std::max(a.n("rim"), 1.0 * cols / s.size), depth = a.n("depth");
  const double vary = a.n("vary"), rough = a.n("rough");
  const bool up = a.w("dir") == "up";
  const Rgb colour = a.c("color");
  const bool rimmed = a.given("rim_color");
  const Rgb rim_colour = a.c("rim_color");
  // Distances in scale widths: a row is `ry` of a width tall, so the discs stay round.
  const double ry = static_cast<double>(cols) / rows;
  constexpr double kR = 0.6;  // a scale's radius, in widths: just enough that the rows close up
  const double reach = kR / ry;
  for_texels(s.size, [&](int x, int y, double u, double v) {
    const double gx = u * cols, gy = (up ? v : 1 - v) * rows;
    // The scale on top is the lowest row whose disc holds the texel: rows below lie over it.
    int hit_row = 0, hit_col = 0;
    double hit = -1;
    for (int j = static_cast<int>(std::floor(gy + reach));
         j >= static_cast<int>(std::floor(gy - reach)); --j) {
      const double off = (j & 1) != 0 ? 0.5 : 0.0;
      const double ci = std::floor(gx - off + 0.5) + off;
      // Within a row the nearest centre wins, so neighbours meet on the line between them.
      for (double c : {ci - 1, ci, ci + 1}) {
        const double dx = gx - c, dy = (gy - j) * ry;
        const double d = kR - std::sqrt(dx * dx + dy * dy);
        if (d > hit) {
          hit = d;
          hit_row = j;
          hit_col = static_cast<int>(std::floor(c));
        }
      }
      if (hit > 0) break;
    }
    if (hit < 0) return;
    const double face = smoothstep(0, rim, hit);
    const auto bx = wrap_cell(hit_col, cols), by = wrap_cell(hit_row, rows);
    const size_t i = s.at(x, y);
    Rgb c = varied(colour, vary, bx, by, a.seed);
    if (rimmed) c = mix(rim_colour, c, face);
    set_colour(s, i, c);
    s.plane[kHeight][i] =
        static_cast<float>(clamp01(0.5 + depth * (face - 0.5) + 0.15 * depth * hit / kR));
    s.plane[kRough][i] = static_cast<float>(rough);
  });
}

}  // namespace ge::texgen
