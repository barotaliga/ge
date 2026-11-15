// Tests: a built map brought back to a source (plugins/brush/compile/import.hpp): a room compiled
// to version 29 imported as brushes that hold its air and its walls, with its textures and its
// entities; the source written as .map text, read back and built again; a version 38 world's own
// brushes given back as they were; a brush model's brushes moved under its entity.
//
// ge:summary Tests: a built map imported to a source (29 cells, 38 brushes) and built again
// ge:keywords test import decompile bsp-to-map round-trip cells brushes
// ge:skill brush testing
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/import.hpp"
#include "brush/compile/map_text.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"

namespace br = ge::brush;
namespace mc = ge::brush::mapc;

namespace {

// Whether `p` is inside a brush of the world that holds `contents`.
bool inside(const mc::Source& src, const mc::V3& p, uint32_t contents) {
  for (const mc::Brush& b : src.entities[0].brushes) {
    if ((b.contents & contents) == 0) continue;
    bool in = true;
    for (const mc::Side& s : b.sides)
      if (s.plane.n[0] * p[0] + s.plane.n[1] * p[1] + s.plane.n[2] * p[2] - s.plane.d > -0.01) {
        in = false;
        break;
      }
    if (in) return true;
  }
  return false;
}

// A room 256 x 256 x 128 with a start and a light, built as a version 29 map.
bool built_room(br::Map* m, std::string* err) {
  mc::Source src;
  src.entities.resize(3);
  src.entities[0].set("classname", "worldspawn");
  const auto box = [&](mc::V3 lo, mc::V3 hi, const char* tex) {
    src.entities[0].brushes.push_back(mc::box_brush(lo, hi, tex));
  };
  box({-16, -16, -16}, {272, 272, 0}, "floor1");
  box({-16, -16, 128}, {272, 272, 144}, "ceil1");
  box({-16, -16, 0}, {0, 272, 128}, "wall1");
  box({256, -16, 0}, {272, 272, 128}, "wall1");
  box({0, -16, 0}, {256, 0, 128}, "wall1");
  box({0, 256, 0}, {256, 272, 128}, "wall1");
  src.entities[1].set("classname", "info_player_deathmatch");
  src.entities[1].set("origin", "64 64 24");
  src.entities[2].set("classname", "light");
  src.entities[2].set("origin", "128 128 96");
  mc::set_contents(&src);
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  if (!mc::compile_map(src, {}, &bytes, &rep, err)) return false;
  return br::read_bsp(bytes, m, err);
}

}  // namespace

TEST(a_version_29_room_comes_back_as_cells_that_hold_its_air_and_walls) {
  br::Map m;
  std::string err;
  CHECK_MSG(built_room(&m, &err), err);
  mc::Source src;
  mc::ImportReport rep;
  CHECK_MSG(mc::import_map(m, &src, &rep, &err), err);
  CHECK(src.entities.size() == 3);
  CHECK(rep.brushes > 0 && rep.brushes == int(src.entities[0].brushes.size()));
  CHECK(rep.textured_sides > 0);
  // The air is no brush; the walls, floor and ceiling are.
  CHECK(!inside(src, {128, 128, 64}, mc::kSolid));
  CHECK(inside(src, {-8, 128, 64}, mc::kSolid));
  CHECK(inside(src, {128, 128, -8}, mc::kSolid));
  CHECK(inside(src, {128, 128, 136}, mc::kSolid));
  // The floor's inner face keeps its texture.
  bool floor = false;
  for (const mc::Brush& b : src.entities[0].brushes)
    for (const mc::Side& s : b.sides)
      if (s.plane.n[2] > 0.99 && s.plane.d > -0.01 && s.plane.d < 0.01 && s.texture == "floor1")
        floor = true;
  CHECK(floor);
  CHECK(*src.entities[1].get("classname") == "info_player_deathmatch");
  CHECK(*src.entities[1].get("origin") == "64 64 24");
}

TEST(an_imported_map_is_written_read_and_built_again) {
  br::Map m;
  std::string err;
  CHECK_MSG(built_room(&m, &err), err);
  mc::Source src;
  mc::ImportReport rep;
  CHECK_MSG(mc::import_map(m, &src, &rep, &err), err);
  const std::string text = mc::write_map_text(src);
  mc::Source back;
  CHECK_MSG(mc::read_map_text(text, &back, &err), err);
  CHECK(back.entities.size() == src.entities.size());
  CHECK(back.entities[0].brushes.size() == src.entities[0].brushes.size());
  std::vector<uint8_t> bytes;
  mc::CompileReport crep;
  CHECK_MSG(mc::compile_map(back, {}, &bytes, &crep, &err), err);
  br::Map again;
  CHECK_MSG(br::read_bsp(bytes, &again, &err), err);
  CHECK(again.family == br::Family::v29);
}

TEST(a_version_38_worlds_brushes_come_back_as_they_were) {
  br::BrushWorld w;
  const float lo0[3] = {0, 0, -16}, hi0[3] = {512, 512, 0};
  const float lo1[3] = {200, 200, 0}, hi1[3] = {264, 264, 64};
  w.models.push_back({br::box_brush(lo0, hi0, 1, "base/floor"), br::box_brush(lo1, hi1, 1)});
  const float lo2[3] = {300, 0, 0}, hi2[3] = {316, 64, 96};
  w.models.push_back({br::box_brush(lo2, hi2, 1, "base/door")});
  br::Entity door;
  door.pairs = {{"classname", "func_door"}, {"model", "*1"}};
  w.entities.push_back(door);
  std::vector<uint8_t> bytes;
  std::string err;
  CHECK_MSG(br::write_brush_world(w, &bytes, &err), err);
  br::Map m;
  CHECK_MSG(br::read_bsp(bytes, &m, &err), err);
  mc::Source src;
  mc::ImportReport rep;
  CHECK_MSG(mc::import_map(m, &src, &rep, &err), err);
  CHECK(src.entities[0].brushes.size() == 2);
  CHECK(inside(src, {232, 232, 32}, mc::kSolid));
  CHECK(!inside(src, {100, 100, 32}, mc::kSolid));
  CHECK(rep.models == 1);
  const mc::Entity* d = nullptr;
  for (const mc::Entity& e : src.entities)
    if (e.get("classname") && *e.get("classname") == "func_door") d = &e;
  CHECK(d != nullptr);
  if (d == nullptr) return;
  CHECK(d->get("model") == nullptr);
  CHECK(d->brushes.size() == 1);
}
