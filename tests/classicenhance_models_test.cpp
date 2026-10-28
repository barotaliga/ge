// The models stage's pieces (plugins/classicenhance/models.hpp, replace.hpp): a model of a dull
// piece and a glowing one (a torch's stick and its painted flame) loses the glowing piece whole
// and keeps the dull one exactly; a model with nothing glowing comes back as it was; and the
// replacement table names its rows' files once.
//
// ge:summary Tests dropping a model's glowing pieces and the table of replaced effects
// ge:keywords test classicenhance models drop-glowing fullbright replace effect torch
// ge:skill classicenhance testing
#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/models.hpp"
#include "classicenhance/replace.hpp"
#include "morph/write.hpp"

namespace ce = ge::classicenhance;
namespace mo = ge::morph;

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
  for (uint32_t v = 0; v < n; v++) m.st[v * 2 + 1] = one.st[v * 2 + 1] / 2;
  for (uint32_t v = 0; v < n; v++) {
    m.st.insert(m.st.end(), {one.st[v * 2], 0.5f + one.st[v * 2 + 1] / 2});
    m.source_vertex.push_back(one.source_vertex[v] + files);
  }
  for (const uint32_t i : one.indices) m.indices.push_back(i + n);
  for (size_t f = 0; f < m.frames.size(); f++) {
    std::vector<float>& p = m.frames[f].positions;
    for (uint32_t v = 0; v < n; v++)
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
  skin.indices[size_t(int(v * float(skin.height)) * skin.width + int(u * float(skin.width)))] = 240;
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
