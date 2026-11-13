// The model and sprite readers (plugins/morph/read.hpp) on the fixtures of write.hpp: a model
// written and read back keeps its skins and their groups, its triangles, its seam (the back
// triangles read the skin's right half), its frames, their names, groups and intervals, and its
// positions to the packing's precision; a sprite keeps its frames; and every truncation and a
// wrong magic or version is refused with what was wrong, never read out of range.
//
// ge:summary Tests the model and sprite readers: round trips, the seam, groups, refused damage
// ge:keywords test morph read write mdl spr seam group interval truncated refuse fixture
// ge:skill morph testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "morph/read.hpp"
#include "morph/write.hpp"

namespace mo = ge::morph;

TEST(model_round_trip_keeps_skins_frames_and_seam) {
  const mo::Model in = mo::sample_model();
  const std::vector<uint8_t> bytes = mo::write_model(in);
  mo::Model out;
  std::string error;
  CHECK_MSG(mo::read_model(bytes, "sample.mdl", &out, &error), error);
  CHECK(out.skins.size() == 3 && out.skins[0].group == 0 && out.skins[2].group == 1);
  CHECK(out.skins[0].interval == 0.0f && std::fabs(out.skins[2].interval - 0.25f) < 1e-6f);
  CHECK(out.skins[1].indices == in.skins[1].indices);
  CHECK(out.indices.size() == in.indices.size());
  CHECK(out.vertex_count() == in.vertex_count());
  // The same triangles over the same texture coordinates: the seam split came back.
  for (size_t i = 0; i < in.indices.size(); i++) {
    const uint32_t a = in.indices[i], b = out.indices[i];
    CHECK(in.source_vertex[a] == out.source_vertex[b]);
    CHECK(in.st[size_t{a} * 2] == out.st[size_t{b} * 2] &&
          in.st[size_t{a} * 2 + 1] == out.st[size_t{b} * 2 + 1]);
  }
  CHECK(out.frames.size() == 3 && out.frames[0].name == "stand" && out.frames[2].name == "grow2");
  CHECK(out.frames[0].group == 0 && out.frames[1].group == 1 && out.frames[2].group == 1);
  CHECK(std::fabs(out.frames[2].interval - 0.1f) < 1e-6f);
  // Positions within half a packing step (48 units over 255 steps), vertex for vertex through
  // the triangles (the reader numbers vertices in the order the triangles name them).
  for (size_t f = 0; f < 3; f++) {
    for (size_t i = 0; i < in.indices.size(); i++) {
      for (size_t k = 0; k < 3; k++) {
        const float a = in.frames[f].positions[size_t(in.indices[i]) * 3 + k];
        const float b = out.frames[f].positions[size_t(out.indices[i]) * 3 + k];
        CHECK(std::fabs(a - b) <= 0.5f * 48.0f / 255.0f + 1e-4f);
      }
    }
  }
}

TEST(sprite_round_trip_keeps_frames) {
  const mo::Sprite in = mo::sample_sprite();
  mo::Sprite out;
  std::string error;
  CHECK_MSG(mo::read_sprite(mo::write_sprite(in), "sample.spr", &out, &error), error);
  CHECK(out.frames.size() == 3 && out.facing == in.facing && out.max_width == 8);
  for (size_t i = 0; i < 3; i++) {
    CHECK(out.frames[i].width == in.frames[i].width &&
          out.frames[i].indices == in.frames[i].indices);
    CHECK(out.frames[i].origin_x == in.frames[i].origin_x &&
          out.frames[i].group == in.frames[i].group);
  }
}

// The second family: skins named, not carried; a seam vertex is a second texture coordinate; every
// frame packed to its own extent.
TEST(second_family_model_round_trips_names_coordinates_and_frames) {
  mo::Model in = mo::sample_model();
  in.skins.clear();
  in.skin_paths = {"models/items/sample/skin.pcx", "models/items/sample/skin2.pcx"};
  in.skin_width = 16;
  in.skin_height = 8;
  const std::vector<uint8_t> bytes = mo::write_model2(in);
  CHECK(bytes[0] == 'I' && bytes[3] == '2');
  mo::Model out;
  std::string error;
  CHECK_MSG(mo::read_model(bytes, "sample.md2", &out, &error), error);
  CHECK(out.skins.empty() && out.skin_paths == in.skin_paths);
  CHECK(out.skin_width == 16 && out.skin_height == 8);
  CHECK(out.indices.size() == in.indices.size() && out.vertex_count() == in.vertex_count());
  for (size_t i = 0; i < in.indices.size(); i++) {
    const uint32_t a = in.indices[i], b = out.indices[i];
    CHECK(in.source_vertex[a] == out.source_vertex[b]);
    // Coordinates are whole texels in the file: within half a texel.
    CHECK(std::fabs(in.st[size_t{a} * 2] - out.st[size_t{b} * 2]) <= 0.5f / 16 + 1e-6f);
    CHECK(std::fabs(in.st[size_t{a} * 2 + 1] - out.st[size_t{b} * 2 + 1]) <= 0.5f / 8 + 1e-6f);
  }
  CHECK(out.frames.size() == in.frames.size() && out.frames[2].name == "grow2");
  for (size_t f = 0; f < out.frames.size(); f++) {
    CHECK(out.frames[f].group == int(f) && out.frames[f].interval == 0.0f);
    for (size_t i = 0; i < in.indices.size(); i++) {
      for (size_t k = 0; k < 3; k++) {
        const float a = in.frames[f].positions[size_t(in.indices[i]) * 3 + k];
        const float b = out.frames[f].positions[size_t(out.indices[i]) * 3 + k];
        CHECK(std::fabs(a - b) <= 0.5f * 48.0f / 255.0f + 1e-4f);
      }
      CHECK(out.frames[f].min[0] <= out.frames[f].max[0]);
    }
  }
  // Every truncation is refused; so are a wrong version and a triangle past the vertices.
  for (size_t n = 0; n < bytes.size(); n += 5) {
    mo::Model m;
    CHECK(!mo::read_model({bytes.data(), n}, "cut.md2", &m, &error));
  }
  CHECK(error.find("cut.md2") == 0);
  std::vector<uint8_t> bad = bytes;
  bad[4] = 9;
  mo::Model m;
  CHECK(!mo::read_model(bad, "v9.md2", &m, &error) && error.find("version 9") != std::string::npos);
  bad = bytes;
  const size_t at_tris = size_t(bad[52]) | size_t(bad[53]) << 8;  // the triangles' offset
  bad[at_tris] = 99;
  bad[at_tris + 1] = 0;
  CHECK(!mo::read_model(bad, "tri.md2", &m, &error) &&
        error.find("names vertex 99") != std::string::npos);
}

TEST(second_family_sprite_names_its_pictures) {
  mo::Sprite in;
  for (int i = 0; i < 2; i++) {
    mo::SpriteFrame f;
    f.width = 56;
    f.height = 40 + i;
    f.origin_x = -28;
    f.origin_y = 20;
    f.picture = "sprites/s_sample_" + std::to_string(i) + ".pcx";
    in.frames.push_back(f);
  }
  mo::Sprite out;
  std::string error;
  CHECK_MSG(mo::read_sprite(mo::write_sprite2(in), "sample.sp2", &out, &error), error);
  CHECK(out.frames.size() == 2 && out.facing == mo::Facing::parallel);
  CHECK(out.max_width == 56 && out.max_height == 41);
  CHECK(out.frames[1].picture == "sprites/s_sample_1.pcx" && out.frames[1].indices.empty());
  CHECK(out.frames[0].origin_x == -28 && out.frames[0].origin_y == 20);
}

TEST(damage_is_refused_with_a_next_step) {
  const std::vector<uint8_t> model = mo::write_model(mo::sample_model());
  const std::vector<uint8_t> sprite = mo::write_sprite(mo::sample_sprite());
  std::string error;
  // Every truncation of either file is refused.
  for (size_t n = 0; n < model.size(); n += 7) {
    mo::Model m;
    CHECK(!mo::read_model({model.data(), n}, "cut.mdl", &m, &error));
  }
  for (size_t n = 0; n < sprite.size(); n += 3) {
    mo::Sprite s;
    CHECK(!mo::read_sprite({sprite.data(), n}, "cut.spr", &s, &error));
  }
  CHECK(error.find("cut.spr") == 0 && error.find("next:") != std::string::npos);
  std::vector<uint8_t> bad = model;
  bad[4] = 7;  // the version
  mo::Model m;
  CHECK(!mo::read_model(bad, "v7.mdl", &m, &error) && error.find("version 7") != std::string::npos);
  bad = model;
  bad[0] = 'X';
  CHECK(!mo::read_model(bad, "x.mdl", &m, &error) && error.find("magic") != std::string::npos);
  // A triangle naming a vertex past the count.
  bad = model;
  const size_t header = 84;
  const size_t skins = 4 + 32 + 4 + 4 + 8 + 64;  // a lone skin, then a group of two
  const size_t first_tri = header + skins + size_t{6} * 12;
  bad[first_tri + 4] = 99;
  CHECK(!mo::read_model(bad, "tri.mdl", &m, &error) &&
        error.find("names vertex 99") != std::string::npos);
}
