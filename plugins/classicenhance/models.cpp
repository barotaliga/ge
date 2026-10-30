// The models stage (models.hpp).
//
// ge:summary Writes the pack's models as glTF with enhanced skins and its sprites as PNGs
// ge:keywords models stage mdl spr glb skin enhance emissive sprite alpha replaced index glow
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#include "classicenhance/models.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/replace.hpp"
#include "lab/png.hpp"
#include "morph/glb.hpp"
#include "morph/read.hpp"
#include "morph/smooth.hpp"
#include "vfs/pak.hpp"

namespace ge::classicenhance {

namespace fs = std::filesystem;

namespace {

constexpr uint8_t kClear = 255;  // a sprite's transparent index

std::vector<uint8_t> png_of(const assets::Image& img) {
  return lab::encode_png({img.width, img.height, img.rgba});
}

// "progs/name.mdl" to "name".
std::string stem_of(const std::string& path) {
  const size_t slash = path.rfind('/'), dot = path.rfind('.');
  const size_t from = slash == std::string::npos ? 0 : slash + 1;
  return path.substr(from, (dot == std::string::npos || dot < from ? path.size() : dot) - from);
}

bool write_model(const vfs::PakEntry& e, std::span<const uint8_t> pack, const Palette& palette,
                 const fs::path& dir, const ModelOptions& o, const EnhanceOptions& textures,
                 std::string& index, ModelStats* stats, std::string* error) {
  morph::Model m;
  if (!morph::read_model(vfs::pak_file(pack, e), e.path, &m, error)) return false;
  const Replacement* rep = replacement_of(e.path);
  if (rep != nullptr && !rep->keep_glowing) m = drop_glowing(m);
  if (o.round > 1) {
    morph::SmoothOptions s;
    s.level = o.round;
    m = morph::smooth_model(m, s);
  }
  const std::string name = safe_name(stem_of(e.path));
  if (m.indices.empty()) {  // all of it glowed: the engine's effect is the whole of it
    stats->models++;
    index += "model " + name + " " + e.path + " 0 0 - - - " +
             std::string(rep != nullptr ? rep->effect : "-") + "\n";
    return true;
  }
  EnhanceOptions t = textures;
  t.scale = std::clamp(o.skin_scale, 1, 8);
  std::vector<morph::GlbSkin> skins;
  for (const morph::Skin& s : m.skins) {
    const Enhanced en = enhance_texture(e.path, s.indices, s.width, s.height, palette, t);
    skins.push_back({png_of(en.albedo), en.glows ? png_of(en.emissive) : std::vector<uint8_t>{}});
  }
  morph::GlbOptions g;
  g.unit_m = float(brush::kUnitM);
  const fs::path at = dir / "models" / (name + ".glb");
  if (!write_whole(at, morph::write_glb(m, name, skins, g))) {
    *error = "cannot write " + at.string();
    return false;
  }
  stats->models++;
  stats->files++;
  index += "model " + name + " " + e.path + " " + std::to_string(m.vertex_count()) + " " +
           std::to_string(m.indices.size() / 3) + " " + std::to_string(m.frames.size()) + " " +
           std::to_string(morph::clips_of(m).names.size()) + " " + std::to_string(skins.size()) +
           " " + std::string(rep != nullptr ? rep->effect : "-") + "\n";
  return true;
}

bool write_sprite(const vfs::PakEntry& e, std::span<const uint8_t> pack, const Palette& palette,
                  const fs::path& dir, std::string& index, ModelStats* stats, std::string* error) {
  morph::Sprite s;
  if (!morph::read_sprite(vfs::pak_file(pack, e), e.path, &s, error)) return false;
  const std::string name = safe_name(stem_of(e.path));
  for (size_t f = 0; f < s.frames.size(); f++) {
    const morph::SpriteFrame& fr = s.frames[f];
    assets::Image img = to_rgba(fr.indices, fr.width, fr.height, palette);
    for (size_t i = 0; i < fr.indices.size(); i++) {
      if (fr.indices[i] == kClear) img.rgba[i * 4 + 3] = 0;
    }
    const std::string file = (f < 10 ? "0" : "") + std::to_string(f) + ".png";
    const fs::path at = dir / "sprites" / name / file;
    if (!write_whole(at, png_of(img))) {
      *error = "cannot write " + at.string();
      return false;
    }
    stats->files++;
  }
  stats->sprites++;
  const Replacement* rep = replacement_of(e.path);
  index += "sprite " + name + " " + e.path + " - - " + std::to_string(s.frames.size()) + " - - " +
           std::string(rep != nullptr ? rep->effect : "-") + "\n";
  return true;
}

}  // namespace

morph::Model drop_glowing(const morph::Model& m) {
  if (m.skins.empty()) return m;
  const morph::Skin& skin = m.skins[0];
  const size_t tris = m.indices.size() / 3;
  // The pieces: triangles joined through a shared file vertex.
  std::vector<int> root(
      m.source_vertex.empty()
          ? 0
          : size_t(*std::max_element(m.source_vertex.begin(), m.source_vertex.end())) + 1);
  for (size_t i = 0; i < root.size(); i++) root[i] = int(i);
  auto find = [&](int x) {
    while (root[size_t(x)] != x) x = root[size_t(x)] = root[size_t(root[size_t(x)])];
    return x;
  };
  for (size_t t = 0; t < tris; t++) {
    const int a = find(m.source_vertex[m.indices[t * 3]]);
    for (size_t k = 1; k < 3; k++) root[size_t(find(m.source_vertex[m.indices[t * 3 + k]]))] = a;
  }
  // A triangle glows when its skin is fullbright at its middle; a piece goes when a quarter of
  // its triangles glow (a painted flame's pieces glow over half; a torch's stick not at all).
  std::vector<int> count(root.size(), 0), glowing(root.size(), 0);
  std::vector<int> piece(tris);
  for (size_t t = 0; t < tris; t++) {
    float u = 0, v = 0;
    for (size_t k = 0; k < 3; k++) {
      u += m.st[size_t(m.indices[t * 3 + k]) * 2] / 3.0f;
      v += m.st[size_t(m.indices[t * 3 + k]) * 2 + 1] / 3.0f;
    }
    const int x = std::clamp(int(u * float(skin.width)), 0, skin.width - 1);
    const int y = std::clamp(int(v * float(skin.height)), 0, skin.height - 1);
    piece[t] = find(m.source_vertex[m.indices[t * 3]]);
    count[size_t(piece[t])]++;
    if (skin.indices[size_t(y) * size_t(skin.width) + size_t(x)] >= kFullbrightFirst)
      glowing[size_t(piece[t])]++;
  }
  morph::Model out = m;
  out.indices.clear();
  for (size_t t = 0; t < tris; t++) {
    if (4 * glowing[size_t(piece[t])] >= count[size_t(piece[t])]) continue;
    out.indices.insert(out.indices.end(), m.indices.begin() + long(t * 3),
                       m.indices.begin() + long(t * 3 + 3));
  }
  // Keep only the vertices a triangle still uses, in their order.
  std::vector<int64_t> remap(m.vertex_count(), -1);
  for (const uint32_t i : out.indices) remap[i] = 0;
  uint32_t next = 0;
  out.st.clear();
  out.source_vertex.clear();
  for (uint32_t v = 0; v < m.vertex_count(); v++) {
    if (remap[v] < 0) continue;
    remap[v] = next++;
    out.st.insert(out.st.end(), {m.st[size_t(v) * 2], m.st[size_t(v) * 2 + 1]});
    out.source_vertex.push_back(m.source_vertex[v]);
  }
  for (uint32_t& i : out.indices) i = uint32_t(remap[i]);
  for (size_t f = 0; f < m.frames.size(); f++) {
    std::vector<float>& p = out.frames[f].positions;
    p.clear();
    for (uint32_t v = 0; v < m.vertex_count(); v++) {
      if (remap[v] < 0) continue;
      const float* q = &m.frames[f].positions[size_t(v) * 3];
      p.insert(p.end(), q, q + 3);
    }
  }
  return out;
}

bool enhance_models(std::span<const uint8_t> pack, const Palette& palette, const fs::path& dir,
                    const ModelOptions& opts, const EnhanceOptions& textures, ModelStats* stats,
                    std::string* error) {
  *stats = {};
  vfs::Pak pak;
  if (!vfs::read_pak(pack, &pak, error)) return false;
  std::vector<const vfs::PakEntry*> files;
  for (const vfs::PakEntry& e : pak.entries) {
    if (e.path.rfind("progs/", 0) == 0 && (e.path.ends_with(".mdl") || e.path.ends_with(".spr")))
      files.push_back(&e);
  }
  std::sort(files.begin(), files.end(), [](auto* a, auto* b) { return a->path < b->path; });
  std::string index = "# kind name path vertices triangles frames clips skins replaced-by\n";
  if (opts.each && !opts.each({}, 0, uint32_t(files.size()))) return false;
  uint32_t done = 0;
  for (const vfs::PakEntry* e : files) {
    const bool ok = e->path.ends_with(".mdl")
                        ? write_model(*e, pack, palette, dir, opts, textures, index, stats, error)
                        : write_sprite(*e, pack, palette, dir, index, stats, error);
    if (!ok) return false;
    if (opts.each && !opts.each(e->path.substr(6), ++done, uint32_t(files.size()))) return false;
  }
  std::string replaced = "# path effect keeps-glowing why\n";
  for (const Replacement& r : kReplacements) {
    replaced += std::string(r.path) + " " + std::string(r.effect) + " " +
                (r.keep_glowing ? "yes" : "no") + " " + std::string(r.why) + "\n";
    if (vfs::pak_find(pak, std::string(r.path)) != nullptr) stats->replaced++;
  }
  if (!write_text(dir / "models" / "index.txt", index) ||
      !write_text(dir / "replaced.txt", replaced)) {
    *error = "cannot write the models' index or replaced.txt under " + dir.string();
    return false;
  }
  stats->files += 2;
  return true;
}

}  // namespace ge::classicenhance
