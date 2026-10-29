// engine/grass/trample.hpp: a stamp presses flat at its middle, less toward its rim, not past it; a
// pressed blade stands back up over its kind's spring; the way packs to snorm16
// and back; the cells wrap round the world and a stale cell reads as unpressed; a harder press is
// kept over a softer one; an unpressed blade's shape keeps its bits and a pressed one lies down
// along the way; and the presses keep their bits in tests/golden/grass_trample.txt (rewrite with
// GE_GOLDEN_WRITE=1 for a deliberate move).
//
// ge:summary Test: trampled grass: stamps, the spring back, the way's packing, wrapping, the shape
// ge:keywords test grass trample stamp press spring snorm wrap golden press_at
// ge:skill grass testing
#include <cmath>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "grass/sway.hpp"
#include "grass/trample.hpp"
#include "grass_golden.hpp"

namespace {

namespace gg = ge::grass;
using ge::test::fbits;
using ge::test::grass_preset;

gg::Trample fresh(float cell = 0.25f, int n = 64) {
  gg::Trample tr;
  gg::init_trample(tr, cell, n);
  return tr;
}

bool same_shape(const gg::Shape& a, const gg::Shape& b) {
  const auto bits = [](const gg::Shape& s) {
    return std::vector<uint64_t>{fbits(s.v0[0]), fbits(s.v0[1]), fbits(s.v0[2]), fbits(s.v1[0]),
                                 fbits(s.v1[1]), fbits(s.v1[2]), fbits(s.v2[0]), fbits(s.v2[1]),
                                 fbits(s.v2[2]), fbits(s.width)};
  };
  return bits(a) == bits(b);
}

}  // namespace

TEST(nothing_is_pressed_before_a_stamp) {
  const gg::Trample tr = fresh();
  const gg::Kind k = grass_preset("meadow");
  for (const float x : {-7.3f, 0.0f, 0.1f, 5.6f})
    CHECK(gg::press_at(tr, k, x, 1.2f, 3.0f).amount == 0.0f);
  const gg::Trample none;  // never initialised: no cells at all
  CHECK(gg::press_at(none, k, 0, 0, 0).amount == 0.0f);
}

TEST(a_stamp_presses_hardest_at_its_middle_and_not_past_its_rim) {
  gg::Trample tr = fresh();
  const gg::Kind k = grass_preset("meadow");
  gg::stamp(tr, 2.0f, 3.0f, 0.6f, 1, 0, 10.0f);
  const float mid = gg::press_at(tr, k, 2.0f, 3.0f, 10.0f).amount;
  const float half = gg::press_at(tr, k, 2.5f, 3.0f, 10.0f).amount;
  const float out = gg::press_at(tr, k, 3.2f, 3.0f, 10.0f).amount;
  CHECK_MSG(mid > 0.8f, "middle " + std::to_string(mid));
  CHECK(half > 0.0f && half < mid);
  CHECK(out == 0.0f);
}

TEST(a_pressed_blade_stands_up_over_its_spring) {
  gg::Trample tr = fresh();
  const gg::Kind lawn = grass_preset("lawn"), long_grass = grass_preset("long");
  gg::stamp(tr, 0.125f, 0.125f, 1.0f, 0, 1, 0.0f);
  const float p0 = gg::press_at(tr, long_grass, 0.125f, 0.125f, 0.0f).amount;
  float last = p0;
  for (int i = 1; i <= 10; ++i) {
    const float t = long_grass.spring * static_cast<float>(i) / 10;
    const float p = gg::press_at(tr, long_grass, 0.125f, 0.125f, t).amount;
    CHECK(p < last);
    last = p;
  }
  CHECK(last == 0.0f);
  // A quarter of the way through the spring the press is r² = 0.5625 of what it was.
  const float q = gg::press_at(tr, long_grass, 0.125f, 0.125f, long_grass.spring * 0.25f).amount;
  CHECK(q > 0.5f * p0);
  // The lawn, quicker to spring, is up long before the long grass.
  CHECK(gg::press_at(tr, lawn, 0.125f, 0.125f, lawn.spring).amount == 0.0f);
  CHECK(gg::press_at(tr, long_grass, 0.125f, 0.125f, lawn.spring).amount > 0.0f);
}

TEST(the_way_packs_to_snorm16_and_back) {
  for (const float x : {-1.0f, -0.5f, 0.0f, 0.3f, 0.7071f, 1.0f}) {
    for (const float z : {-1.0f, -0.2f, 0.0f, 0.7071f, 1.0f}) {
      float back[2];
      gg::unpack_way(gg::pack_way(x, z), back);
      CHECK(std::fabs(back[0] - x) < 1.0f / 32767);
      CHECK(std::fabs(back[1] - z) < 1.0f / 32767);
    }
  }
  float clamped[2];
  gg::unpack_way(gg::pack_way(4.0f, -9.0f), clamped);
  CHECK(clamped[0] == 1.0f && clamped[1] == -1.0f);
}

TEST(the_cells_wrap_and_a_stale_cell_reads_unpressed) {
  // 16 cells of 0.5 m: 8 m round. A stamp at x = 1 and the same slot 8 m on.
  gg::Trample tr = fresh(0.5f, 16);
  const gg::Kind k = grass_preset("long");
  gg::stamp(tr, 1.25f, 1.25f, 0.3f, 1, 0, 0.0f);
  CHECK(gg::press_at(tr, k, 1.25f, 1.25f, 0.0f).amount > 0.0f);
  CHECK(gg::press_at(tr, k, 9.25f, 1.25f, 0.0f).amount == 0.0f);  // same slot, other key
  CHECK(gg::press_at(tr, k, -6.75f, 1.25f, 0.0f).amount == 0.0f);
  // The newer print takes the slot; the old place reads as unpressed.
  gg::stamp(tr, 9.25f, 1.25f, 0.3f, 1, 0, 1.0f);
  CHECK(gg::press_at(tr, k, 9.25f, 1.25f, 1.0f).amount > 0.0f);
  CHECK(gg::press_at(tr, k, 1.25f, 1.25f, 1.0f).amount == 0.0f);
}

TEST(a_harder_press_is_kept_over_a_softer_one) {
  gg::Trample tr = fresh();
  const gg::Kind k = grass_preset("long");
  gg::stamp(tr, 0.125f, 0.125f, 1.0f, 1, 0, 0.0f);
  const float hard = gg::press_at(tr, k, 0.125f, 0.125f, 0.0f).amount;
  gg::stamp(tr, 0.125f, 0.125f, 1.0f, 0, 1, 0.0f, 0.2f);
  CHECK(gg::press_at(tr, k, 0.125f, 0.125f, 0.0f).amount == hard);
  const gg::Press p = gg::press_at(tr, k, 0.125f, 0.125f, 0.0f);
  CHECK(p.way[0] > 0.9f);  // still laid the first stamp's way
}

TEST(a_stamp_marks_its_cells_dirty_until_cleaned) {
  gg::Trample tr = fresh();
  CHECK(tr.dirty_hi > tr.dirty_lo);  // a fresh square uploads whole
  gg::clean(tr);
  CHECK(tr.dirty_hi <= tr.dirty_lo);
  gg::stamp(tr, 3.0f, 3.0f, 0.4f, 1, 0, 0.0f);
  CHECK(tr.dirty_hi > tr.dirty_lo);
  for (size_t i = 0; i < tr.cells.size(); ++i)
    if (tr.cells[i].key != 0xffffffffu) CHECK(i >= tr.dirty_lo && i < tr.dirty_hi);
}

TEST(an_unpressed_blade_keeps_its_shape_and_a_pressed_one_lies_down) {
  const gg::Kind k = grass_preset("long");
  gg::Blade b{};
  b.root[0] = 1;
  b.root[2] = 1;
  b.seed = 0xabcdefu;
  b.height = 0.8f;
  b.width = 0.006f;
  b.lean[0] = 0.1f;
  const float air[2] = {2.0f, 0.5f};
  CHECK(same_shape(gg::blade_shape(k, b, air, 1.3f), gg::blade_shape(k, b, air, 1.3f, {})));
  gg::Press flat;
  flat.amount = 1;
  flat.way[0] = 0;
  flat.way[1] = -1;
  const gg::Shape up = gg::blade_shape(k, b, air, 1.3f);
  const gg::Shape down = gg::blade_shape(k, b, air, 1.3f, flat);
  CHECK(down.v2[1] - down.v0[1] < 0.25f * (up.v2[1] - up.v0[1]));
  CHECK(down.v2[2] < down.v0[2]);  // along the press's way, against the wind
}

TEST(the_presses_keep_their_bits) {
  gg::Trample tr = fresh();
  const gg::Kind k = grass_preset("meadow");
  // A walk across and a wheel's line down it.
  for (int i = 0; i < 12; ++i)
    gg::stamp(tr, -2.0f + 0.35f * static_cast<float>(i),
              0.4f * static_cast<float>(ge::core::libm::sin(0.5 * i)), 0.18f, 1, 0,
              0.25f * static_cast<float>(i));
  for (int i = 0; i < 20; ++i)
    gg::stamp(tr, 0.3f, -2.0f + 0.2f * static_cast<float>(i), 0.12f, 0, 1, 4.0f, 0.9f);
  std::vector<std::vector<uint64_t>> rows;
  for (const float t : {3.0f, 5.0f, 9.0f}) {
    for (int i = 0; i < 9; ++i) {
      for (int j = 0; j < 9; ++j) {
        const float x = -2.0f + 0.47f * static_cast<float>(i),
                    z = -2.0f + 0.47f * static_cast<float>(j);
        const gg::Press p = gg::press_at(tr, k, x, z, t);
        rows.push_back({fbits(p.amount), fbits(p.way[0]), fbits(p.way[1])});
      }
    }
  }
  ge::test::hold_grass_golden("tests/golden/grass_trample.txt", rows);
}
