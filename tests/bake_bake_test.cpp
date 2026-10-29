// engine/bake/bake.hpp: a lamp over a floor gives the inverse-square irradiance and points the
// light's direction at itself; a slab between casts a shadow; an open sky gives its brightness
// exactly; a closed glowing box converges on the furnace's geometric sum, pass by pass; a block
// standing on a floor marks the luxels under it as inside and fills them; the bake is the same
// bits on one worker and on four; and the directional shade gives the flat normal its irradiance.
//
// ge:summary Test: the light bake's direct light, shadows, sky, bounce sum, fill, worker bits
// ge:keywords test bake lightmap direct-light shadow sky furnace bounce directional dilate
// ge:keywords bake_scene directional_scale workers determinism
// ge:skill testing render
#include "bake/bake.hpp"

#include <cmath>
#include <cstring>

#include "bake/chart.hpp"
#include "check.hpp"

namespace {

using namespace ge::bake;

// A floor quad (x and z from 0 to `side`, y = 0, facing up), charted at `luxel` m.
Scene floor_scene(double side, double luxel) {
  Scene s;
  s.materials.push_back({});
  const double q[4][3] = {{0, 0, 0}, {0, 0, side}, {side, 0, side}, {side, 0, 0}};
  const uint32_t t = add_quad(s, q, 0);
  const uint32_t tris[2] = {t, t + 1};
  add_chart(s, tris, luxel);
  return s;
}

// The atlas index of the luxel nearest the floor point (x, z), and that luxel's centre.
size_t luxel_at(const Scene& s, double x, double z, double centre[3]) {
  const Chart& c = s.charts[0];
  const double p[3] = {x, 0, z};
  double si = 0, ti = 0;
  chart_point(c, p, &si, &ti);
  const auto i = static_cast<uint32_t>(si), j = static_cast<uint32_t>(ti);
  for (int k = 0; k < 3; k++) centre[k] = c.origin[k] + c.u[k] * (i + 0.5) + c.v[k] * (j + 0.5);
  return size_t{c.y + j} * s.atlas_width + c.x + i;
}

// A box's six faces facing in, centred on the origin, `half` m each way.
void inward_box(Scene& s, double half, uint32_t material, double luxel) {
  const double h = half;
  const double faces[6][4][3] = {
      {{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}},  // floor
      {{-h, h, -h}, {-h, h, h}, {h, h, h}, {h, h, -h}},      // ceiling
      {{-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}},  // -x wall
      {{h, -h, -h}, {h, h, -h}, {h, h, h}, {h, -h, h}},      // +x wall
      {{-h, -h, -h}, {-h, h, -h}, {h, h, -h}, {h, -h, -h}},  // -z wall
      {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}},      // +z wall
  };
  for (const auto& f : faces) {
    // Listed facing out; added the other way round.
    const double in[4][3] = {{f[3][0], f[3][1], f[3][2]},
                             {f[2][0], f[2][1], f[2][2]},
                             {f[1][0], f[1][1], f[1][2]},
                             {f[0][0], f[0][1], f[0][2]}};
    const uint32_t t = add_quad(s, in, material);
    const uint32_t tris[2] = {t, t + 1};
    add_chart(s, tris, luxel);
  }
}

}  // namespace

// A small lamp 2 m over the floor: the luxel under it gets colour cos / (d^2 + r^2), and the
// light's direction points up at it with all its length.
TEST(bake_point_light_inverse_square) {
  Scene s = floor_scene(4, 0.25);
  Light l;
  l.pos[0] = 2;
  l.pos[1] = 2;
  l.pos[2] = 2;
  l.colour[0] = l.colour[1] = l.colour[2] = 8;
  l.radius = 0.01;
  s.lights.push_back(l);
  CHECK(pack_charts(s, 64));
  Options o;
  o.bounces = 0;
  o.light_samples = 1;
  const Lightmap m = bake_scene(s, o);
  double c[3];
  const size_t a = luxel_at(s, 2.1, 2.1, c);
  const double dx = 2 - c[0], dy = 2 - c[1], dz = 2 - c[2];
  const double d2 = dx * dx + dy * dy + dz * dz, cosn = dy / std::sqrt(d2);
  CHECK(std::fabs(m.rgb[a * 3] - 8 * cosn / (d2 + 0.01 * 0.01)) < 1e-5);
  CHECK(m.dir[a * 4 + 1] > 0.99f);
  CHECK(m.dir[a * 4 + 3] == 1.0f);
}

// A slab between the lamp and the floor leaves the floor under it dark, and the edge lit.
TEST(bake_shadow) {
  Scene s = floor_scene(4, 0.25);
  s.materials.push_back({});
  const double slab[4][3] = {
      {1.5, 1, 1.5}, {2.5, 1, 1.5}, {2.5, 1, 2.5}, {1.5, 1, 2.5}};  // faces down
  add_quad(s, slab, 1);
  Light l;
  l.pos[0] = 2;
  l.pos[1] = 2;
  l.pos[2] = 2;
  s.lights.push_back(l);
  CHECK(pack_charts(s, 64));
  Options o;
  o.bounces = 0;
  const Lightmap m = bake_scene(s, o);
  double c[3];
  CHECK(m.rgb[luxel_at(s, 2, 2, c) * 3] == 0.0f);
  CHECK(m.rgb[luxel_at(s, 0.2, 0.2, c) * 3] > 0.0f);
}

// A floor under nothing but sky: every gather ray escapes, so its irradiance is the sky's.
TEST(bake_open_sky) {
  Scene s = floor_scene(2, 0.25);
  s.sky[0] = 0.6f;
  s.sky[1] = 0.7f;
  s.sky[2] = 0.9f;
  CHECK(pack_charts(s, 32));
  Options o;
  o.bounces = 1;
  o.samples = 16;
  const Lightmap m = bake_scene(s, o);
  double c[3];
  const size_t a = luxel_at(s, 1, 1, c);
  CHECK(std::fabs(m.rgb[a * 3] - 0.6f) < 1e-5);
  CHECK(std::fabs(m.rgb[a * 3 + 2] - 0.9f) < 1e-5);
  // The sky's light comes from all round: its mean direction is the normal's, about 2/3 long.
  CHECK(m.dir[a * 4 + 1] > 0.5f && m.dir[a * 4 + 1] < 0.8f);
}

// THE FURNACE: a closed box glowing `e` everywhere with albedo `a` gets e (1 + a + .. + a^(k-1))
// after k passes, every luxel, since every ray meets a face as bright as its own.
TEST(bake_furnace_sum) {
  for (const int k : {1, 2, 3}) {
    Scene s;
    Material m;
    m.albedo[0] = m.albedo[1] = m.albedo[2] = 0.5f;
    m.emit[0] = m.emit[1] = m.emit[2] = 1.0f;
    s.materials.push_back(m);
    inward_box(s, 1, 0, 0.25);
    CHECK(pack_charts(s, 64));
    Options o;
    o.bounces = k;
    o.samples = 8;
    const Lightmap map = bake_scene(s, o);
    double want = 0, p = 1;
    for (int i = 0; i < k; i++, p *= 0.5) want += p;
    for (size_t c = 0; c < s.charts.size(); c++) {
      const Chart& ch = s.charts[c];
      const size_t a = size_t{ch.y + ch.height / 2} * s.atlas_width + ch.x + ch.width / 2;
      CHECK(std::fabs(map.rgb[a * 3] - want) < 1e-4 * want);
    }
  }
}

// A block standing on the floor: the floor's luxels under it see the block's backs and are
// filled from their neighbours, not left dark.
TEST(bake_inside_luxels_filled) {
  Scene s = floor_scene(4, 0.25);
  s.sky[0] = s.sky[1] = s.sky[2] = 1;
  // The block: a closed outward box from (1.5, 0, 1.5) to (2.5, 1, 2.5), uncharted.
  const double x0 = 1.5, x1 = 2.5, y1 = 1;
  const double faces[5][4][3] = {
      {{x0, y1, x0}, {x0, y1, x1}, {x1, y1, x1}, {x1, y1, x0}},  // top, up
      {{x0, 0, x0}, {x0, y1, x0}, {x1, y1, x0}, {x1, 0, x0}},    // facing -z
      {{x0, 0, x1}, {x1, 0, x1}, {x1, y1, x1}, {x0, y1, x1}},    // facing +z
      {{x0, 0, x0}, {x0, 0, x1}, {x0, y1, x1}, {x0, y1, x0}},    // facing -x
      {{x1, 0, x0}, {x1, y1, x0}, {x1, y1, x1}, {x1, 0, x1}},    // facing +x
  };
  for (const auto& f : faces) add_quad(s, f, 0);
  CHECK(pack_charts(s, 64));
  Options o;
  o.bounces = 1;
  o.samples = 32;
  Stats st;
  const Lightmap m = bake_scene(s, o, &st);
  CHECK(st.filled >= 9);  // at least the 3 x 3 well inside the block's 4 x 4 luxels
  double c[3];
  const size_t a = luxel_at(s, 2, 2, c);
  CHECK(m.dir[a * 4 + 3] == 0.0f);
  CHECK(m.rgb[a * 3] > 0.0f);  // filled from the lit floor round it
}

// One worker or four: the same bits.
TEST(bake_same_bits_any_workers) {
  Scene s;
  Material grey, red;
  red.albedo[0] = 0.8f;
  red.albedo[1] = 0.1f;
  red.albedo[2] = 0.1f;
  s.materials.push_back(grey);
  s.materials.push_back(red);
  inward_box(s, 2, 0, 0.2);
  s.tri_material[4] = s.tri_material[5] = 1;  // a red wall
  Light l;
  l.pos[1] = 1;
  l.radius = 0.2;
  l.colour[0] = l.colour[1] = l.colour[2] = 4;
  s.lights.push_back(l);
  CHECK(pack_charts(s, 128));
  Options o;
  o.samples = 16;
  o.workers = 1;
  const Lightmap one = bake_scene(s, o);
  o.workers = 4;
  const Lightmap four = bake_scene(s, o);
  CHECK(one.rgb.size() == four.rgb.size());
  CHECK(std::memcmp(one.rgb.data(), four.rgb.data(), one.rgb.size() * sizeof(float)) == 0);
  CHECK(std::memcmp(one.dir.data(), four.dir.data(), one.dir.size() * sizeof(float)) == 0);
  // The red wall (-x) bleeds: the floor next to it is redder than the floor across the room.
  const Chart& floor = s.charts[0];
  const size_t near = size_t{floor.y + floor.height / 2} * s.atlas_width + floor.x + 1;
  const size_t far = size_t{floor.y + floor.height / 2} * s.atlas_width + floor.x + floor.width - 2;
  const float rn = one.rgb[near * 3] / one.rgb[near * 3 + 1];
  const float rf = one.rgb[far * 3] / one.rgb[far * 3 + 1];
  CHECK(rn > rf + 0.02f);
}

// The directional shade: the flat normal gets 1; a bump turned to the light more, away less;
// no direction, 1 for any normal.
TEST(bake_directional_scale) {
  const double up[3] = {0, 1, 0}, dir[3] = {0.6, 0.6, 0}, none[3] = {0, 0, 0};
  const double toward[3] = {0.6, 0.8, 0}, away[3] = {-0.6, 0.8, 0};
  CHECK(std::fabs(directional_scale(dir, up, up) - 1) < 1e-12);
  CHECK(directional_scale(dir, toward, up) > 1);
  CHECK(directional_scale(dir, away, up) < 1);
  CHECK(directional_scale(none, away, up) == 1);
}
