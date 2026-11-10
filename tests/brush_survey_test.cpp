// A map surveyed (plugins/brush/survey.hpp): leaf cells by family, liquid bodies joined where their
// leaves touch and ordered deepest first, a slice through the fixture's halls, entities' places and
// the class glob.
//
// ge:summary Test: leaf cells by family, liquid bodies, a slice, entity places and the class glob
// ge:keywords test brush survey cell liquid-bodies deepest slice entity-place glob
// ge:skill brush testing
#include <cmath>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/survey.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace br = ge::brush;

namespace {

br::Map fixture(br::Family family) {
  std::vector<uint8_t> bytes;
  std::string error;
  br::Map m;
  CHECK(br::write_bsp(br::sample_fixture(), family, &bytes, &error));
  CHECK(br::read_bsp(bytes, &m, &error));
  return m;
}

br::Leaf box(int32_t contents, float x0, float y0, float z0, float x1, float y1, float z1) {
  br::Leaf l;
  l.contents = contents;
  l.mins[0] = x0, l.mins[1] = y0, l.mins[2] = z0;
  l.maxs[0] = x1, l.maxs[1] = y1, l.maxs[2] = z1;
  return l;
}

}  // namespace

TEST(leaf_cells_read_each_familys_contents) {
  br::Map m;
  m.family = br::Family::v29;
  CHECK(br::leaf_cell(m, box(-1, 0, 0, 0, 0, 0, 0)) == br::Cell::empty);
  CHECK(br::leaf_cell(m, box(-2, 0, 0, 0, 0, 0, 0)) == br::Cell::solid);
  CHECK(br::leaf_cell(m, box(-3, 0, 0, 0, 0, 0, 0)) == br::Cell::water);
  CHECK(br::leaf_cell(m, box(-4, 0, 0, 0, 0, 0, 0)) == br::Cell::slime);
  CHECK(br::leaf_cell(m, box(-5, 0, 0, 0, 0, 0, 0)) == br::Cell::lava);
  CHECK(br::leaf_cell(m, box(-6, 0, 0, 0, 0, 0, 0)) == br::Cell::sky);
  CHECK(br::leaf_cell(m, box(-10, 0, 0, 0, 0, 0, 0)) == br::Cell::water);
  m.family = br::Family::v38;
  CHECK(br::leaf_cell(m, box(1 | 0x20, 0, 0, 0, 0, 0, 0)) == br::Cell::solid);
  CHECK(br::leaf_cell(m, box(0x8, 0, 0, 0, 0, 0, 0)) == br::Cell::lava);
  CHECK(br::leaf_cell(m, box(0x20, 0, 0, 0, 0, 0, 0)) == br::Cell::water);
  CHECK(br::leaf_cell(m, box(0, 0, 0, 0, 0, 0, 0)) == br::Cell::empty);
  CHECK(br::cell_char(br::Cell::lava) == '!' && std::string(br::cell_name(br::Cell::sky)) == "sky");
}

TEST(liquid_bodies_join_touching_leaves_deepest_first) {
  br::Map m;
  m.family = br::Family::v29;
  m.leaves = {
      box(-3, 0, 0, -64, 64, 64, 0),      // a pool of water in two leaves that touch
      box(-3, 64, 0, -32, 128, 64, 0),    //
      box(-5, 512, 0, -128, 576, 64, 0),  // a deep lava pit
      box(-3, 900, 0, -16, 960, 64, 0),   // a shallow puddle apart from the pool
      box(-2, 0, 0, 0, 999, 999, 999),    // solid: not a liquid
  };
  const std::vector<br::LiquidBody> b = br::liquid_bodies(m);
  CHECK(b.size() == 3);
  CHECK(b[0].kind == br::Cell::lava && b[0].depth() == 128 && b[0].leaves == 1);
  CHECK(b[1].kind == br::Cell::water && b[1].depth() == 64 && b[1].leaves == 2);
  CHECK(b[1].mins[0] == 0 && b[1].maxs[0] == 128);
  CHECK(b[2].depth() == 16 && b[2].narrowest() == 60);
  CHECK(br::liquid_bodies(fixture(br::Family::v29)).empty());
}

TEST(a_slice_tells_the_halls_from_the_rock_round_them) {
  for (br::Family f : {br::Family::v29, br::Family::v38}) {
    const br::Map m = fixture(f);
    br::Slice s;
    std::string error;
    CHECK(br::slice_map(m, 2, 64, 32, &s, &error));
    CHECK(s.u_axis == 0 && s.v_axis == 1 && s.step == 32 && !s.rows.empty());
    // The hall's middle (256, 192) is open; the cell at the box's far corner is rock.
    const int col = int(std::lround((256 - s.u0) / 32)), row = int(std::lround((s.v0 - 192) / 32));
    CHECK(s.rows[size_t(row)][size_t(col)] == '.');
    CHECK(s.rows.back().back() == '#' || s.rows.front().back() == '#');
    size_t open = 0;
    for (const std::string& r : s.rows) {
      for (char c : r) open += c == '.';
    }
    CHECK(open > 100);
    CHECK(!br::slice_map(m, 2, 99999, 32, &s, &error) &&
          error.find("outside") != std::string::npos);
    CHECK(!br::slice_map(m, 0, 64, 0.5, &s, &error) && error.find("--step") != std::string::npos);
    CHECK(br::slice_map(m, 0, 256, 0, &s, &error) && s.u_axis == 1 && s.v_axis == 2 && s.step >= 8);
  }
}

TEST(entity_places_and_the_class_glob) {
  br::Map m;
  m.models.resize(2);
  m.models[1].mins[0] = -8, m.models[1].maxs[0] = 8;
  m.models[1].mins[1] = 0, m.models[1].maxs[1] = 32;
  m.models[1].mins[2] = 0, m.models[1].maxs[2] = 64;
  br::Entity door;
  door.pairs = {{"classname", "func_door"}, {"model", "*1"}, {"origin", "100 0 0"}};
  br::Entity torch;
  torch.pairs = {{"classname", "light_torch_small_walltorch"}, {"origin", "10 20 30"}};
  br::Entity bare;
  bare.pairs = {{"classname", "info_null"}};
  float lo[3], hi[3];
  CHECK(br::entity_place(m, door, lo, hi) && lo[0] == 92 && hi[0] == 108 && hi[2] == 64);
  const float inside[3] = {100, 16, 32}, beside[3] = {120, 16, 32};
  CHECK(br::box_distance(lo, hi, inside) == 0 && br::box_distance(lo, hi, beside) == 12);
  CHECK(br::entity_place(m, torch, lo, hi) && lo[1] == 20 && hi[1] == 20);
  CHECK(!br::entity_place(m, bare, lo, hi));
  CHECK(br::glob_match("light_torch*", "light_torch_small_walltorch"));
  CHECK(br::glob_match("*torch*", "light_torch_small_walltorch"));
  CHECK(br::glob_match("info_?ull", "info_null") && br::glob_match("light", "light"));
  CHECK(!br::glob_match("light", "light_torch") && !br::glob_match("torch*", "light_torch"));
}
