// engine/render/lamp_shadows.hpp: a tile's view puts a point on its axis at the tile's middle at
// depth near / distance, the cube's six faces cover every way out of a point light, and a spot's
// tile holds its whole cone; the deal gives the brightest lamps on the eye their tiles (a point
// light six, a spot one) while they last, keeps a held lamp's run where it was, gives the slot to
// the light's row, and redraws a tile only when it is new, its lamp moved, or a mover enters its
// view.
//
// ge:summary Tests for the lamps' shadows: the tiles' views, the deal, the cache and the movers
// ge:keywords test render lamp-shadows lamp_tile_view deal_lamp_tiles cube-faces cache movers
// ge:skill render testing
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "check.hpp"
#include "render/lamp_shadows.hpp"
#include "render/lights.hpp"

namespace {

namespace r = ge::render;

// The clip of `p` (relative to the lamp) through a tile's matrix (column-major).
void clip_of(const float m[16], const double p[3], double c[4]) {
  for (int row = 0; row < 4; ++row) {
    c[row] = m[12 + row];
    for (int i = 0; i < 3; ++i) c[row] += m[i * 4 + row] * p[i];
  }
}

bool inside(const float m[16], const double p[3]) {
  double c[4];
  clip_of(m, p, c);
  return c[3] > 0 && std::fabs(c[0]) <= c[3] && std::fabs(c[1]) <= c[3];
}

r::FrameView a_view() {
  r::FrameView v;
  v.eye[0] = 0;
  v.eye[1] = 1.7;
  v.eye[2] = 0;
  v.fov_y = 1.0;
  v.near_m = 0.1;
  v.time_s = 1;
  return v;
}

r::LocalLight lamp(double x, double z, uint32_t key, float candela, bool spot) {
  r::LocalLight l;
  l.position[0] = x;
  l.position[1] = 2.5;
  l.position[2] = z;
  l.candela = candela;
  l.range_m = 8;
  l.shadow = key;
  if (spot) {
    l.outer_deg = 40;
    l.inner_deg = 25;
  }
  return l;
}

void deal(r::LampShadows& s, r::LightBins& b, const std::vector<r::LocalLight>& list,
          std::span<const std::array<double, 4>> movers = {}) {
  const r::FrameView v = a_view();
  r::bin_lights(b, list, v, 640, 360);
  r::deal_lamp_tiles(s, b, list, v, movers);
}

}  // namespace

TEST(tile_views_center_the_axis_and_cover_every_way_out) {
  const r::LampShadowSettings set;
  const r::LocalLight point = lamp(0, 0, 1, 50, false);
  // The cube's faces: every way out of the lamp falls inside at least one, and the axis of each
  // lands in its middle at depth near / distance.
  for (int face = 0; face < 6; ++face) {
    float m[16], planes[5][4];
    r::lamp_tile_view(set, point, face, m, planes);
    double axis[3] = {0, 0, 0};
    axis[face / 2] = face % 2 == 0 ? 4.0 : -4.0;
    double c[4];
    clip_of(m, axis, c);
    CHECK(std::fabs(c[0] / c[3]) < 1e-6 && std::fabs(c[1] / c[3]) < 1e-6);
    CHECK(std::fabs(c[2] / c[3] - set.near_m / 4.0) < 1e-6);
    // The planes agree with the matrix: the axis inside every one.
    for (int k = 0; k < 5; ++k) {
      CHECK(planes[k][0] * axis[0] + planes[k][1] * axis[1] + planes[k][2] * axis[2] +
                planes[k][3] >
            0);
    }
  }
  for (int i = 0; i < 500; ++i) {
    const double a = i * 2.39996, z = 1 - 2 * (i + 0.5) / 500.0, rr = std::sqrt(1 - z * z);
    const double d[3] = {rr * std::cos(a), z, rr * std::sin(a)};
    bool any = false;
    for (int face = 0; face < 6; ++face) {
      float m[16], planes[5][4];
      r::lamp_tile_view(set, point, face, m, planes);
      any = any || inside(m, d);
    }
    CHECK(any);
  }
  // A spot's one tile holds its whole outer cone.
  r::LocalLight spot = lamp(0, 0, 2, 50, true);
  spot.direction[0] = 0.3f;
  spot.direction[1] = -1;
  spot.direction[2] = 0.2f;
  float m[16], planes[5][4];
  r::lamp_tile_view(set, spot, 0, m, planes);
  const double dl = std::sqrt(0.09 + 1 + 0.04);
  const double f[3] = {0.3 / dl, -1 / dl, 0.2 / dl};
  const double side[3] = {1, 0.3, 0};  // not along f: a way across the axis
  double across[3] = {f[1] * side[2] - f[2] * side[1], f[2] * side[0] - f[0] * side[2],
                      f[0] * side[1] - f[1] * side[0]};
  const double al =
      std::sqrt(across[0] * across[0] + across[1] * across[1] + across[2] * across[2]);
  for (double& c : across) c /= al;
  const double up[3] = {f[1] * across[2] - f[2] * across[1], f[2] * across[0] - f[0] * across[2],
                        f[0] * across[1] - f[1] * across[0]};
  const double t = std::tan(39.5 * 3.14159265358979 / 180);
  for (int k = 0; k < 64; ++k) {
    const double a = k * 3.14159265358979 / 32;
    const double p[3] = {f[0] + t * (std::cos(a) * across[0] + std::sin(a) * up[0]),
                         f[1] + t * (std::cos(a) * across[1] + std::sin(a) * up[1]),
                         f[2] + t * (std::cos(a) * across[2] + std::sin(a) * up[2])};
    CHECK(inside(m, p));
  }
}

TEST(the_deal_ranks_keeps_and_redraws_only_what_is_due) {
  r::LampShadows s;
  r::LightBins b;
  r::reserve_bins(b);
  // Eleven point lights ahead of the eye, brighter the further (each 6 tiles: ten fit in 64),
  // a keyless one, and two spots.
  std::vector<r::LocalLight> list;
  for (int i = 0; i < 11; ++i)
    list.push_back(lamp(-6 + i, 6 + i * 0.5, 10 + i, 20 + 10.0f * static_cast<float>(i), false));
  list.push_back(lamp(0, 5, 0, 500, false));
  list.push_back(lamp(2, 4, 40, 400, true));
  list.push_back(lamp(-2, 4, 41, 1, true));
  deal(s, b, list);
  // The spots and ten of the points: 2 + 60 tiles; the dimmest point light (key 10) goes without.
  CHECK(s.lamps == 12);
  CHECK(s.used == 62);
  CHECK(s.drawn == 62);
  CHECK(s.unshadowed == 1);
  for (uint32_t i = 0; i < b.head.info[0]; ++i) {
    const r::LocalLight& l = list[b.order[i]];
    const float slot = b.lights[i].cone[1];
    if (l.shadow == 0 || l.shadow == 10) {
      CHECK(slot == -1.0f);
    } else {
      CHECK(slot >= 0);
      const uint32_t first = static_cast<uint32_t>(slot);
      CHECK(s.tiles[first].key == l.shadow);
      CHECK(s.tiles[first].face == 0);
    }
  }
  // Nothing moved: nothing redrawn, every lamp keeps its run.
  std::array<r::LampTileState, r::kMaxLampTiles> held = s.tiles;
  deal(s, b, list);
  CHECK(s.drawn == 0);
  for (uint32_t k = 0; k < r::kMaxLampTiles; ++k) CHECK(s.tiles[k].key == held[k].key);
  // A mover redraws only the faces it falls in.
  const r::LocalLight& near = list[12];  // the bright spot at (2, 2.5, 4), aimed down
  const std::array<double, 4> mover = {near.position[0], 0.3, near.position[2], 0.3};
  deal(s, b, list, std::span(&mover, 1));
  // At most the three faces a small sphere can straddle of each lamp it is in reach of.
  uint32_t bound = 0;
  for (const r::LocalLight& l : list) {
    const double d[3] = {mover[0] - l.position[0], mover[1] - l.position[1],
                         mover[2] - l.position[2]};
    if (l.shadow == 0 || l.shadow == 10) continue;
    if (std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) > l.range_m + mover[3]) continue;
    bound += l.outer_deg > 0 ? 1 : 3;
  }
  CHECK(s.drawn >= 1);
  CHECK(s.drawn <= bound);
  CHECK(s.drawn < s.used / 2);
  // The lamp moved: its own tiles redrawn, no other.
  list[12].position[0] += 0.01;
  deal(s, b, list);
  CHECK(s.drawn == 1);
  // The dimmest point light brightened past another: it takes the tiles the loser gave up, and a
  // lamp that stayed keeps its run.
  list[0].candela = 5000;
  deal(s, b, list);
  CHECK(s.unshadowed == 1);
  bool got = false;
  for (const r::LampTileState& t : s.tiles) got = got || t.key == 10;
  CHECK(got);
  CHECK(s.drawn == 6);
  // Forgotten: everything dealt is redrawn.
  r::reset_lamp_shadows(s);
  deal(s, b, list);
  CHECK(s.drawn == s.used);
}
