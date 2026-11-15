// Tests: the map editors' .map text (plugins/brush/compile/map_text.hpp): a classic-form room read
// into brushes with their planes, contents and axial projections; the 220 form's axes; a source
// written and read back to the same planes and projections; a room read from text compiled into a
// map the reader takes; faults named by line.
//
// ge:summary Tests: .map text read (classic and 220 forms), written, read back, compiled
// ge:keywords test map-text dot-map valve-220 round-trip three-points projection
// ge:skill brush testing
#include <cmath>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/map_text.hpp"
#include "brush/read.hpp"
#include "check.hpp"

namespace br = ge::brush;
namespace mc = ge::brush::mapc;

namespace {

// A hollow box room 256 x 256 x 128 of six wall brushes, a light and a start.
std::string room_text() {
  std::string t = "// a test room\n{\n\"classname\" \"worldspawn\"\n";
  const auto box = [&](int x0, int y0, int z0, int x1, int y1, int z1, const char* tex) {
    const auto pt = [](int x, int y, int z) {
      return "( " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z) + " ) ";
    };
    const std::string tx = std::string(tex) + " 0 0 0 1 1\n";
    t += "{\n";
    t += pt(x0, 0, 0) + pt(x0, 1, 0) + pt(x0, 0, 1) + tx;  // -x
    t += pt(x1, 0, 0) + pt(x1, 0, 1) + pt(x1, 1, 0) + tx;  // +x
    t += pt(0, y0, 0) + pt(0, y0, 1) + pt(1, y0, 0) + tx;  // -y
    t += pt(0, y1, 0) + pt(1, y1, 0) + pt(0, y1, 1) + tx;  // +y
    t += pt(0, 0, z0) + pt(1, 0, z0) + pt(0, 1, z0) + tx;  // -z
    t += pt(0, 0, z1) + pt(0, 1, z1) + pt(1, 0, z1) + tx;  // +z
    t += "}\n";
  };
  box(-16, -16, -16, 272, 272, 0, "floor1");
  box(-16, -16, 128, 272, 272, 144, "ceil1");
  box(-16, -16, 0, 0, 272, 128, "wall1");
  box(256, -16, 0, 272, 272, 128, "wall1");
  box(0, -16, 0, 256, 0, 128, "wall1");
  box(0, 256, 0, 256, 272, 128, "wall1");
  t += "}\n{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"64 64 24\"\n}\n";
  t += "{\n\"classname\" \"light\"\n\"origin\" \"128 128 96\"\n\"light\" \"300\"\n}\n";
  return t;
}

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

}  // namespace

TEST(a_classic_room_reads_into_brushes_facing_out) {
  mc::Source src;
  std::string err;
  CHECK_MSG(mc::read_map_text(room_text(), &src, &err), err);
  CHECK(src.entities.size() == 3);
  CHECK(src.entities[0].brushes.size() == 6);
  const mc::Brush& floor = src.entities[0].brushes[0];
  CHECK(floor.contents == mc::kSolid);
  // The floor brush's top side faces up at z = 0, its bottom down at z = -16.
  const mc::Side& top = floor.sides[5];
  CHECK(near(top.plane.n[2], 1) && near(top.plane.d, 0));
  const mc::Side& bottom = floor.sides[4];
  CHECK(near(bottom.plane.n[2], -1) && near(bottom.plane.d, 16));
  CHECK(top.texture == "floor1");
  // An axial floor: s along +x, t along -y.
  CHECK(near(top.vecs[0][0], 1) && near(top.vecs[1][1], -1));
  CHECK(*src.entities[1].get("origin") == "64 64 24");
}

TEST(the_220_form_gives_the_axes_outright) {
  const std::string t =
      "{\n\"classname\" \"worldspawn\"\n{\n"
      "( 0 0 0 ) ( 0 1 0 ) ( 0 0 1 ) *water1 [ 0 1 0 8 ] [ 0 0 -1 4 ] 0 2 0.5\n"
      "( 64 0 0 ) ( 64 0 1 ) ( 64 1 0 ) *water1 [ 0 1 0 8 ] [ 0 0 -1 4 ] 0 2 0.5\n"
      "( 0 0 0 ) ( 0 0 1 ) ( 1 0 0 ) *water1 [ 1 0 0 0 ] [ 0 0 -1 0 ] 0 1 1\n"
      "( 0 64 0 ) ( 1 64 0 ) ( 0 64 1 ) *water1 [ 1 0 0 0 ] [ 0 0 -1 0 ] 0 1 1\n"
      "( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) *water1 [ 1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1\n"
      "( 0 0 64 ) ( 0 1 64 ) ( 1 0 64 ) *water1 [ 1 0 0 0 ] [ 0 -1 0 0 ] 0 1 1 0 4 7\n"
      "}\n}\n";
  mc::Source src;
  std::string err;
  CHECK_MSG(mc::read_map_text(t, &src, &err), err);
  const mc::Brush& b = src.entities[0].brushes[0];
  CHECK(b.contents == mc::kWater);
  CHECK(near(b.sides[0].vecs[0][1], 0.5) && near(b.sides[0].vecs[0][3], 8));
  CHECK(near(b.sides[0].vecs[1][2], -2) && near(b.sides[0].vecs[1][3], 4));
  CHECK(b.sides[5].flags == 4 && b.sides[5].value == 7);
}

TEST(a_source_written_reads_back_the_same) {
  mc::Source src;
  std::string err;
  CHECK_MSG(mc::read_map_text(room_text(), &src, &err), err);
  // A slanted side and a turned, scaled projection, which only the 220 form can write.
  mc::Side& s = src.entities[0].brushes[1].sides[4];
  s.plane.n = mc::normalized({0.3, 0.1, -1});
  s.plane.d = -120;
  s.vecs[0][0] = 0.25, s.vecs[0][1] = 0.5, s.vecs[0][2] = 0, s.vecs[0][3] = 17;
  const std::string text = mc::write_map_text(src);
  CHECK(text.find("\"mapversion\" \"220\"") != std::string::npos);
  mc::Source back;
  CHECK_MSG(mc::read_map_text(text, &back, &err), err + "\n" + text);
  CHECK(back.entities.size() == src.entities.size());
  for (size_t e = 0; e < src.entities.size(); ++e) {
    CHECK(back.entities[e].brushes.size() == src.entities[e].brushes.size());
    for (size_t b = 0; b < src.entities[e].brushes.size(); ++b) {
      for (size_t i = 0; i < src.entities[e].brushes[b].sides.size(); ++i) {
        const mc::Side& a = src.entities[e].brushes[b].sides[i];
        const mc::Side& c = back.entities[e].brushes[b].sides[i];
        CHECK(a.texture == c.texture);
        for (int k = 0; k < 3; ++k)
          CHECK_MSG(std::fabs(a.plane.n[size_t(k)] - c.plane.n[size_t(k)]) < 1e-9, text);
        CHECK(std::fabs(a.plane.d - c.plane.d) < 1e-6);
        for (int r = 0; r < 2; ++r)
          for (int k = 0; k < 4; ++k) CHECK(std::fabs(a.vecs[r][k] - c.vecs[r][k]) < 1e-9);
      }
    }
  }
  // An axial source's text is a fixed point: written, read and written again, the same bytes.
  mc::Source axial;
  CHECK(mc::read_map_text(room_text(), &axial, &err));
  const std::string once = mc::write_map_text(axial);
  mc::Source twice;
  CHECK(mc::read_map_text(once, &twice, &err));
  CHECK(mc::write_map_text(twice) == once);
}

TEST(a_room_read_from_text_compiles) {
  mc::Source src;
  std::string err;
  CHECK_MSG(mc::read_map_text(room_text(), &src, &err), err);
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  CHECK_MSG(mc::compile_map(src, {}, &bytes, &rep, &err), err);
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &err), err);
  CHECK(!m.surfaces.empty());
}

TEST(faults_are_named_by_line) {
  mc::Source src;
  std::string err;
  CHECK(!mc::read_map_text(
      "{\n\"classname\" \"worldspawn\"\n{\n( 0 0 0 ) ( 1 1 1 ) ( 2 2 2 ) t 0 0 0 1 1\n}\n}\n", &src,
      &err));
  CHECK_MSG(err.find("line 4:") != std::string::npos && err.find("in a line") != std::string::npos,
            err);
  CHECK_MSG(err.find("next:") != std::string::npos, err);
  CHECK(!mc::read_map_text("{\n\"classname\" \"worldspawn\"\n", &src, &err));
  CHECK_MSG(err.find("not closed") != std::string::npos, err);
}
