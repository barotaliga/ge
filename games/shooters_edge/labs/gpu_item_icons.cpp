// The map picture's item icons (item_icons.hpp).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "brush/app/draw_data.hpp"
#include "morph/model.hpp"
#include "morph/read.hpp"
#include "shooters_edge/labs/item_icons.hpp"
#include "ui/image.hpp"

namespace shooters_edge::labs {

namespace {

namespace br = ge::brush;
namespace ba = ge::brush::app;
namespace cl = shooters_edge::client;
namespace ui = ge::ui;

double wrap(double v, double n) {
  const double r = std::fmod(v, n);
  return r < 0 ? r + n : r;
}

}  // namespace


ui::Image draw_icon(const Mesh& m, int size, double azimuth_deg, double elevation_deg) {
  constexpr int kSS = 3;
  const int S = size * kSS;
  const double az = azimuth_deg * 3.14159265358979 / 180, el = elevation_deg * 3.14159265358979 / 180;
  const double d[3] = {std::cos(az) * std::cos(el), std::sin(az) * std::cos(el), std::sin(el)};  // to the eye
  double r[3] = {-d[1], d[0], 0};
  const double rl = std::hypot(r[0], r[1]);
  r[0] /= rl;
  r[1] /= rl;
  const double u[3] = {d[1] * r[2] - d[2] * r[1], d[2] * r[0] - d[0] * r[2], d[0] * r[1] - d[1] * r[0]};
  const size_t nv = m.pos.size() / 3;
  std::vector<double> sx(nv), sy(nv), sz(nv);
  double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
  for (size_t i = 0; i < nv; ++i) {
    const float* p = &m.pos[i * 3];
    sx[i] = p[0] * r[0] + p[1] * r[1] + p[2] * r[2];
    sy[i] = -(p[0] * u[0] + p[1] * u[1] + p[2] * u[2]);
    sz[i] = p[0] * d[0] + p[1] * d[1] + p[2] * d[2];
    lo[0] = std::min(lo[0], sx[i]);
    hi[0] = std::max(hi[0], sx[i]);
    lo[1] = std::min(lo[1], sy[i]);
    hi[1] = std::max(hi[1], sy[i]);
  }
  const double fit = 0.84 * S / std::max({hi[0] - lo[0], hi[1] - lo[1], 1e-6});
  const double cx = (lo[0] + hi[0]) / 2, cy = (lo[1] + hi[1]) / 2;
  for (size_t i = 0; i < nv; ++i) {
    sx[i] = S / 2.0 + (sx[i] - cx) * fit;
    sy[i] = S / 2.0 + (sy[i] - cy) * fit;
  }
  std::vector<double> depth(size_t(S) * size_t(S), -1e30);
  std::vector<float> col(size_t(S) * size_t(S) * 3, 0);
  const double key[3] = {-0.35, 0.45, 0.82};
  for (size_t t = 0; t + 2 < m.idx.size(); t += 3) {
    const uint32_t a = m.idx[t], b = m.idx[t + 1], c = m.idx[t + 2];
    const float* pa = &m.pos[a * 3];
    const float* pb = &m.pos[b * 3];
    const float* pc = &m.pos[c * 3];
    double e1[3], e2[3], n[3];
    for (int q = 0; q < 3; ++q) e1[q] = pb[q] - pa[q], e2[q] = pc[q] - pa[q];
    n[0] = e1[1] * e2[2] - e1[2] * e2[1];
    n[1] = e1[2] * e2[0] - e1[0] * e2[2];
    n[2] = e1[0] * e2[1] - e1[1] * e2[0];
    const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (nl < 1e-12) continue;
    for (double& q : n) q /= nl;
    if (n[0] * d[0] + n[1] * d[1] + n[2] * d[2] < 0)
      for (double& q : n) q = -q;
    const double lambert = std::max(0.0, n[0] * key[0] + n[1] * key[1] + n[2] * key[2]);
    const double rim = std::pow(1 - std::fabs(n[0] * d[0] + n[1] * d[1] + n[2] * d[2]), 3.0);
    const auto shade = float(0.5 + 0.62 * lambert + 0.25 * rim);
    const uint32_t vv[3] = {a, b, c};
    const double area = (sx[b] - sx[a]) * (sy[c] - sy[a]) - (sx[c] - sx[a]) * (sy[b] - sy[a]);
    if (std::fabs(area) < 1e-12) continue;
    const int x0 = std::max(0, int(std::min({sx[a], sx[b], sx[c]})));
    const int x1 = std::min(S - 1, int(std::max({sx[a], sx[b], sx[c]})) + 1);
    const int y0 = std::max(0, int(std::min({sy[a], sy[b], sy[c]})));
    const int y1 = std::min(S - 1, int(std::max({sy[a], sy[b], sy[c]})) + 1);
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        const double px = x + 0.5, py = y + 0.5;
        double w[3];
        w[0] = ((sx[b] - px) * (sy[c] - py) - (sx[c] - px) * (sy[b] - py)) / area;
        w[1] = ((sx[c] - px) * (sy[a] - py) - (sx[a] - px) * (sy[c] - py)) / area;
        w[2] = 1 - w[0] - w[1];
        if (w[0] < 0 || w[1] < 0 || w[2] < 0) continue;
        const double z = w[0] * sz[a] + w[1] * sz[b] + w[2] * sz[c];
        const size_t at = size_t(y) * size_t(S) + size_t(x);
        if (z <= depth[at]) continue;
        depth[at] = z;
        float tc[3] = {0.6f, 0.6f, 0.6f};
        if (m.tw > 0) {
          double s = 0, tt = 0;
          for (int q = 0; q < 3; ++q) s += w[q] * m.st[vv[q] * 2], tt += w[q] * m.st[vv[q] * 2 + 1];
          const float* rc = &m.rect[a * 4];
          const auto X = int(rc[0] + wrap(s, std::max(1.0f, rc[2])));
          const auto Y = int(rc[1] + wrap(tt, std::max(1.0f, rc[3])));
          const uint8_t* p = &m.tex[(size_t(std::clamp(Y, 0, m.th - 1)) * size_t(m.tw) + size_t(std::clamp(X, 0, m.tw - 1))) * 4];
          tc[0] = p[0] / 255.0f, tc[1] = p[1] / 255.0f, tc[2] = p[2] / 255.0f;
        }
        for (int q = 0; q < 3; ++q) col[at * 3 + size_t(q)] = std::min(1.0f, tc[q] * shade);
      }
    }
  }
  // Down to `size`, coverage as alpha; then a dark rim two pixels round the cut-out.
  ui::Image img = ui::make_image(size, size, {0, 0, 0, 0});
  std::vector<float> alpha(size_t(size) * size_t(size), 0);
  for (int Y = 0; Y < size; ++Y)
    for (int X = 0; X < size; ++X) {
      float acc[3] = {0, 0, 0};
      int cover = 0;
      for (int j = 0; j < kSS; ++j)
        for (int i = 0; i < kSS; ++i) {
          const size_t at = size_t(Y * kSS + j) * size_t(S) + size_t(X * kSS + i);
          if (depth[at] < -1e29) continue;
          ++cover;
          for (int q = 0; q < 3; ++q) acc[q] += col[at * 3 + size_t(q)];
        }
      const size_t o = size_t(Y) * size_t(size) + size_t(X);
      alpha[o] = float(cover) / (kSS * kSS);
      if (cover > 0)
        for (int q = 0; q < 3; ++q) img.rgba[o * 4 + size_t(q)] = uint8_t(std::lround(acc[q] / float(cover) * 255));
    }
  for (int Y = 0; Y < size; ++Y)
    for (int X = 0; X < size; ++X) {
      const size_t o = size_t(Y) * size_t(size) + size_t(X);
      float rim = 0;
      for (int j = -2; j <= 2; ++j)
        for (int i = -2; i <= 2; ++i) {
          if (i * i + j * j > 5) continue;
          const int x = X + i, y = Y + j;
          if (x >= 0 && y >= 0 && x < size && y < size) rim = std::max(rim, alpha[size_t(y) * size_t(size) + size_t(x)]);
        }
      const float a = alpha[o];
      // The model over its rim: the rim a near-black 85 %.
      for (int q = 0; q < 3; ++q) {
        const float mc = img.rgba[o * 4 + size_t(q)] / 255.0f;
        const float out_a = a + rim * 0.85f * (1 - a);
        const float v = out_a > 0 ? (mc * a + 0.04f * rim * 0.85f * (1 - a)) / out_a : 0;
        img.rgba[o * 4 + size_t(q)] = uint8_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255));
      }
      img.rgba[o * 4 + 3] = uint8_t(std::lround(std::clamp(a + rim * 0.85f * (1 - a), 0.0f, 1.0f) * 255));
    }
  return img;
}

// A model file (the first family's .mdl or the second's .md2) as a mesh, its skin `skin`.
bool model_mesh(const cl::SearchPath& files, std::span<const uint8_t> palette,
                const std::string& path, int skin, Mesh* out) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = cl::find_file(files, path, store);
  if (bytes.empty()) return false;
  ge::morph::Model m;
  std::string err;
  if (!ge::morph::read_model(bytes, path, &m, &err) || m.frames.empty()) return false;
  const ge::morph::Frame& f = m.frames[0];
  out->pos = f.positions;
  out->idx = m.indices;
  if (!m.skins.empty() && palette.size() >= 768) {
    const ge::morph::Skin& s = m.skins[size_t(std::clamp(skin, 0, int(m.skins.size()) - 1))];
    out->tw = s.width;
    out->th = s.height;
    out->tex.resize(size_t(s.width) * size_t(s.height) * 4);
    for (size_t i = 0; i < s.indices.size(); ++i) {
      for (int q = 0; q < 3; ++q) out->tex[i * 4 + size_t(q)] = palette[size_t(s.indices[i]) * 3 + size_t(q)];
      out->tex[i * 4 + 3] = 255;
    }
  } else if (!m.skin_paths.empty()) {
    std::vector<uint8_t> ps;
    const std::span<const uint8_t> pcx =
        cl::find_file(files, m.skin_paths[size_t(std::clamp(skin, 0, int(m.skin_paths.size()) - 1))], ps);
    ge::assets::IndexedImage pic;
    if (!pcx.empty() && ge::assets::decode_pcx(pcx, path, &pic, &err)) {
      out->tw = pic.width;
      out->th = pic.height;
      out->tex = ge::assets::pcx_rgba(pic, pic.palette.empty() ? palette : std::span<const uint8_t>{});
    }
  }
  const size_t nv = m.vertex_count();
  out->st.resize(nv * 2);
  out->rect.resize(nv * 4);
  for (size_t i = 0; i < nv; ++i) {
    out->st[i * 2] = m.st[i * 2] * float(out->tw);
    out->st[i * 2 + 1] = m.st[i * 2 + 1] * float(out->th);
    out->rect[i * 4 + 0] = 0;
    out->rect[i * 4 + 1] = 0;
    out->rect[i * 4 + 2] = float(out->tw);
    out->rect[i * 4 + 3] = float(out->th);
  }
  return true;
}

// A small map (the first family's health and ammo boxes) as a mesh.
bool box_mesh(const cl::SearchPath& files, std::span<const uint8_t> palette, const std::string& path,
              Mesh* out) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = cl::find_file(files, path, store);
  if (bytes.empty() || palette.size() < 768) return false;
  ba::Pack pack;
  pack.bytes.assign(bytes.begin(), bytes.end());
  pack.files.push_back({path, 0, uint32_t(bytes.size())});
  ba::Scene scene;
  std::string err;
  if (!ba::load_scene(pack, path, palette, &scene, &err) || scene.textures.levels.empty()) return false;
  const double k = 1 / br::kUnitM;
  for (const ba::DrawVertex& v : scene.vertices) {
    out->pos.insert(out->pos.end(), {float(v.pos[0] * k), float(v.pos[2] * k), float(v.pos[1] * k)});
    out->st.insert(out->st.end(), {v.st[0], v.st[1]});
    out->rect.insert(out->rect.end(), {v.rect[0], v.rect[1], v.rect[2], v.rect[3]});
  }
  out->idx = scene.indices;
  out->tw = int(scene.textures.width);
  out->th = int(scene.textures.height);
  out->tex = scene.textures.levels[0];
  return true;
}

// ---- what a map's things are --------------------------------------------------------------



// The first family's things, then the second's.
constexpr ItemKind kItems[] = {
    {"item_artifact_super_damage", -1, "progs/quaddama.mdl", 0, false, Tier::power, "QUAD", 0x4f8cff, 60},
    {"item_artifact_invulnerability", -1, "progs/invulner.mdl", 0, false, Tier::power, "PENT", 0xff4b4b, 300},
    {"item_artifact_invisibility", -1, "progs/invisibl.mdl", 0, false, Tier::power, "RING", 0xffd34b, 300},
    {"item_artifact_envirosuit", -1, "progs/suit.mdl", 0, false, Tier::major, "SUIT", 0x8fdc5a, 60},
    {"item_armorInv", -1, "progs/armor.mdl", 2, false, Tier::power, "RA", 0xff4b4b, 20},
    {"item_armor2", -1, "progs/armor.mdl", 1, false, Tier::major, "YA", 0xffd34b, 20},
    {"item_armor1", -1, "progs/armor.mdl", 0, false, Tier::major, "GA", 0x6ad46a, 20},
    {"item_health", 2, "maps/b_bh100.bsp", 0, true, Tier::power, "MH", 0x4fd0ff, 20},
    {"item_health", 1, "maps/b_bh10.bsp", 0, true, Tier::minor, "+15", 0x9ad0a0, 0},
    {"item_health", -1, "maps/b_bh25.bsp", 0, true, Tier::minor, "+25", 0x9ad0a0, 0},
    {"weapon_rocketlauncher", -1, "progs/g_rock2.mdl", 0, false, Tier::power, "RL", 0xff8a3c, 0},
    {"weapon_lightning", -1, "progs/g_light.mdl", 0, false, Tier::power, "LG", 0x8fd8ff, 0},
    {"weapon_grenadelauncher", -1, "progs/g_rock.mdl", 0, false, Tier::major, "GL", 0xff8a3c, 0},
    {"weapon_supernailgun", -1, "progs/g_nail2.mdl", 0, false, Tier::major, "SNG", 0xd0d0d0, 0},
    {"weapon_nailgun", -1, "progs/g_nail.mdl", 0, false, Tier::minor, "NG", 0xd0d0d0, 0},
    {"weapon_supershotgun", -1, "progs/g_shot.mdl", 0, false, Tier::minor, "SSG", 0xd0d0d0, 0},
    {"item_rockets", 1, "maps/b_rock1.bsp", 0, true, Tier::minor, "R", 0xb08a5a, 0},
    {"item_rockets", -1, "maps/b_rock0.bsp", 0, true, Tier::minor, "R", 0xb08a5a, 0},
    {"item_shells", 1, "maps/b_shell1.bsp", 0, true, Tier::minor, "S", 0xb08a5a, 0},
    {"item_shells", -1, "maps/b_shell0.bsp", 0, true, Tier::minor, "S", 0xb08a5a, 0},
    {"item_spikes", 1, "maps/b_nail1.bsp", 0, true, Tier::minor, "N", 0xb08a5a, 0},
    {"item_spikes", -1, "maps/b_nail0.bsp", 0, true, Tier::minor, "N", 0xb08a5a, 0},
    {"item_cells", 1, "maps/b_batt1.bsp", 0, true, Tier::minor, "C", 0xb08a5a, 0},
    {"item_cells", -1, "maps/b_batt0.bsp", 0, true, Tier::minor, "C", 0xb08a5a, 0},
    {"item_quad", -1, "models/items/quaddama/tris.md2", 0, false, Tier::power, "QUAD", 0x4f8cff, 60},
    {"item_invulnerability", -1, "models/items/invulner/tris.md2", 0, false, Tier::power, "INV", 0xff4b4b, 300},
    {"item_armor_body", -1, "models/items/armor/body/tris.md2", 0, false, Tier::power, "BA", 0xff4b4b, 20},
    {"item_armor_combat", -1, "models/items/armor/combat/tris.md2", 0, false, Tier::major, "CA", 0xffd34b, 20},
    {"item_armor_jacket", -1, "models/items/armor/jacket/tris.md2", 0, false, Tier::major, "JA", 0x6ad46a, 20},
    {"item_health_mega", -1, "models/items/mega_h/tris.md2", 0, false, Tier::power, "MH", 0x4fd0ff, 20},
    {"item_health_large", -1, "models/items/healing/large/tris.md2", 0, false, Tier::minor, "+25", 0x9ad0a0, 0},
    {"item_health", -1, "models/items/healing/medium/tris.md2", 0, false, Tier::minor, "+10", 0x9ad0a0, 0},
    {"weapon_railgun", -1, "models/weapons/g_rail/tris.md2", 0, false, Tier::power, "RG", 0x8fd8ff, 0},
    {"weapon_hyperblaster", -1, "models/weapons/g_hyperb/tris.md2", 0, false, Tier::major, "HB", 0xd0d0d0, 0},
    {"weapon_chaingun", -1, "models/weapons/g_chain/tris.md2", 0, false, Tier::major, "CG", 0xd0d0d0, 0},
    {"weapon_machinegun", -1, "models/weapons/g_machn/tris.md2", 0, false, Tier::minor, "MG", 0xd0d0d0, 0},
    {"weapon_shotgun", -1, "models/weapons/g_shotg/tris.md2", 0, false, Tier::minor, "SG", 0xd0d0d0, 0},
    {"weapon_bfg", -1, "models/weapons/g_bfg/tris.md2", 0, false, Tier::power, "BFG", 0x63e68a, 0},
};

const ItemKind* item_of(const br::Entity& e) {
  const std::string& cls = e.get("classname");
  const int flags = std::atoi(e.get("spawnflags").c_str());
  if ((flags & 2048) != 0) return nullptr;  // not in deathmatch
  for (const ItemKind& k : kItems)
    if (cls == k.cls && (k.flags < 0 || (flags & k.flags) != 0)) return &k;
  return nullptr;
}

int icon_size(Tier t) { return t == Tier::power ? 76 : t == Tier::major ? 58 : 38; }



}  // namespace shooters_edge::labs
