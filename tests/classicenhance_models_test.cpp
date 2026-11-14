// The models stage's pieces (plugins/classicenhance/models.hpp, replace.hpp): a model of a dull
// piece and a glowing one (a torch's stick and its painted flame) loses the glowing piece whole
// and keeps the dull one exactly; a model with nothing glowing comes back as it was; and the
// replacement table names its rows' files once.
//
// ge:summary Tests dropping a model's glowing pieces and the table of replaced effects
// ge:keywords test classicenhance models drop-glowing fullbright replace effect torch
// ge:skill classicenhance testing
#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "check.hpp"
#include "classicenhance/cached.hpp"
#include "classicenhance/models.hpp"
#include "classicenhance/replace.hpp"
#include "morph/write.hpp"
#include "vfs/pak.hpp"

namespace ce = ge::classicenhance;
namespace mo = ge::morph;
namespace fs = std::filesystem;

namespace {

// Two copies of the fixture body side by side as one model: the first reads the skin's top half
// (dull), the second its bottom half (fullbright, as a painted flame is).
mo::Model stick_and_flame() {
  const mo::Model one = mo::sample_model();
  mo::Model m = one;
  mo::Skin& s = m.skins[0];
  s.height *= 2;
  s.indices.assign(size_t(s.width) * size_t(s.height), 12);
  for (size_t i = s.indices.size() / 2; i < s.indices.size(); i++) s.indices[i] = 240;
  m.skins.resize(1);
  const uint32_t n = one.vertex_count();
  int files = 0;
  for (const int v : one.source_vertex) files = std::max(files, v + 1);
  for (size_t v = 0; v < n; v++) m.st[v * 2 + 1] = one.st[v * 2 + 1] / 2;
  for (size_t v = 0; v < n; v++) {
    m.st.insert(m.st.end(), {one.st[v * 2], 0.5f + one.st[v * 2 + 1] / 2});
    m.source_vertex.push_back(one.source_vertex[v] + files);
  }
  for (const uint32_t i : one.indices) m.indices.push_back(i + n);
  for (size_t f = 0; f < m.frames.size(); f++) {
    std::vector<float>& p = m.frames[f].positions;
    for (size_t v = 0; v < n; v++)
      p.insert(p.end(), {one.frames[f].positions[v * 3] + 100, one.frames[f].positions[v * 3 + 1],
                         one.frames[f].positions[v * 3 + 2]});
  }
  return m;
}

}  // namespace

TEST(drop_glowing_removes_the_glowing_piece_only) {
  const mo::Model m = stick_and_flame();
  const mo::Model one = mo::sample_model();
  const mo::Model d = ce::drop_glowing(m);
  // The dull piece is kept whole, as it was; the fullbright piece is gone.
  CHECK(d.indices == one.indices);
  CHECK(d.vertex_count() == one.vertex_count() && d.source_vertex == one.source_vertex);
  for (size_t f = 0; f < d.frames.size(); f++)
    CHECK(d.frames[f].positions ==
          std::vector<float>(m.frames[f].positions.begin(),
                             m.frames[f].positions.begin() + long(one.vertex_count()) * 3));
  // One glowing triangle in a piece of eight (under a quarter) keeps the piece.
  mo::Model spotted = stick_and_flame();
  mo::Skin& skin = spotted.skins[0];
  for (size_t i = skin.indices.size() / 2; i < skin.indices.size(); i++) skin.indices[i] = 12;
  const size_t first = one.indices.size();  // the second piece's first triangle
  float u = 0, v = 0;
  for (size_t k = 0; k < 3; k++) {
    u += spotted.st[size_t(spotted.indices[first + k]) * 2] / 3;
    v += spotted.st[size_t(spotted.indices[first + k]) * 2 + 1] / 3;
  }
  skin.indices[size_t(int(v * float(skin.height))) * size_t(skin.width) +
               size_t(int(u * float(skin.width)))] = 240;
  CHECK(one.indices.size() / 3 >= 4);
  CHECK(ce::drop_glowing(spotted).indices.size() == spotted.indices.size());
}

TEST(drop_glowing_keeps_a_model_without_glow) {
  mo::Model m = mo::sample_model();
  for (uint8_t& i : m.skins[0].indices) i = 12;
  const mo::Model d = ce::drop_glowing(m);
  CHECK(d.indices == m.indices && d.st == m.st);
  CHECK(d.frames[1].positions == m.frames[1].positions);
}

TEST(replacements_name_each_file_once) {
  std::set<std::string> paths;
  for (const ce::Replacement& r : ce::kReplacements) {
    CHECK(paths.insert(std::string(r.path)).second);
    CHECK(!r.effect.empty() && !r.why.empty());
    CHECK(ce::replacement_of(r.path) == &r);
  }
  CHECK(ce::replacement_of("progs/flame.mdl")->effect == "fire.wall_torch");
  CHECK(ce::replacement_of("progs/player.mdl") == nullptr);
}

TEST(model_name_is_the_path_under_its_kinds_folder) {
  CHECK(ce::model_name("progs/armor.mdl") == "armor");
  CHECK(ce::model_name("progs/s_light.spr") == "s_light");
  CHECK(ce::model_name("models/items/armor/tris.md2") == "items/armor/tris");
  CHECK(ce::model_name("sprites/s_bfg1.sp2") == "s_bfg1");
}

TEST(the_sequels_models_keep_their_own_names_and_their_outside_skins_and_frames) {
  // Two models both called tris in their folders, their skins PCX pictures in the pack, and a
  // sprite whose frames are pictures too: each lands in its own place with its pictures.
  const ce::Profile& sequel = *ce::find_profile("sequel");
  mo::Model m = mo::sample_model();
  const mo::Skin skin = m.skins[0];
  m.skins.clear();
  m.skin_width = skin.width;
  m.skin_height = skin.height;
  m.skin_paths = {"models/items/armor/skin.pcx"};
  const auto pcx_of = [](int w, int h, uint8_t index) {
    ge::assets::IndexedImage img;
    img.width = w, img.height = h;
    img.indices.assign(size_t(w) * size_t(h), index);
    img.palette.assign(768, 0);
    return ge::assets::encode_pcx(img);
  };
  mo::Model health = m;
  health.skin_paths = {"models/items/health/skin.pcx"};
  mo::Sprite sp = mo::sample_sprite();
  for (mo::SpriteFrame& f : sp.frames) {
    f.picture = "sprites/flash" + std::to_string(&f - sp.frames.data()) + ".pcx";
    f.indices.clear();
  }
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {
      {std::string(sequel.palette), pcx_of(8, 8, 1)},
      {"models/items/armor/tris.md2", mo::write_model2(m)},
      {"models/items/armor/skin.pcx", pcx_of(skin.width, skin.height, 40)},
      {"models/items/health/tris.md2", mo::write_model2(health)},
      {"sprites/flash.sp2", mo::write_sprite2(sp)}};
  for (const mo::SpriteFrame& f : sp.frames) files.push_back({f.picture, pcx_of(8, 8, 50)});
  std::vector<uint8_t> pack;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &pack, &error), error);
  ce::Palette palette;
  for (size_t i = 0; i < palette.rgb.size(); i++) palette.rgb[i] = uint8_t(i);
  const fs::path dir = fs::temp_directory_path() / "ge_ce_sequel_models_test";
  fs::remove_all(dir);
  ce::ModelStats stats;
  ce::EnhanceOptions t;
  t.scale = 1;
  ce::ModelOptions o;
  o.skin_scale = 1;
  CHECK_MSG(ce::enhance_models(pack, palette, dir, o, t, &stats, &error, sequel), error);
  CHECK(stats.models == 2 && stats.sprites == 1);
  CHECK(fs::exists(dir / "models" / "items_armor_tris.glb"));
  CHECK(fs::exists(dir / "models" / "items_health_tris.glb"));
  // The armour's skin came from its picture; the health's names one the pack lacks: no skin.
  CHECK(fs::exists(dir / "models" / "items_armor_tris" / "skin0.png"));
  CHECK(!fs::exists(dir / "models" / "items_health_tris" / "skin0.png"));
  std::vector<ge::assets::Image> big;
  CHECK(ce::cached_skins(dir, ce::model_name("models/items/armor/tris.md2"), uint32_t(skin.width),
                         uint32_t(skin.height), &big) &&
        big.size() == 1);
  for (size_t f = 0; f < sp.frames.size(); f++) {
    CHECK(
        fs::exists(dir / "sprites" / "flash" / ((f < 10 ? "0" : "") + std::to_string(f) + ".png")));
  }
  fs::remove_all(dir);
}
