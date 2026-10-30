// The classic models as the engine draws them (classic_models.hpp).
//
// ge:summary Classic models, sprites and brush items as the engine's morph meshes and sprites
// ge:keywords classic-models morph_from_alias morph_from_brush sprite_frames_of light_under
// ge:skill render shooter morph
// ge:test tests/shooters_edge_classic_models_test.cpp
#include "shooters_edge/app/classic_models.hpp"

#include <algorithm>
#include <cmath>

#include "core/libm/libm.hpp"
#include "morph/smooth.hpp"

namespace shooters_edge::models {

namespace gr = ge::render;
namespace mo = ge::morph;
namespace br = ge::brush;
namespace lm = ge::core::libm;

namespace {

constexpr double kPi = 3.14159265358979323846;

float to_linear(uint8_t c) {
  const double v = c / 255.0;
  return float(v <= 0.04045 ? v / 12.92 : lm::pow((v + 0.055) / 1.055, 2.4));
}

void colour_of(std::span<const uint8_t> palette, uint8_t index, uint8_t* rgba) {
  const size_t at = size_t(index) * 3;
  for (size_t k = 0; k < 3; k++) rgba[k] = at + k < palette.size() ? palette[at + k] : 0;
  rgba[3] = 255;
}

uint8_t mark_of(uint8_t index) {
  if (index >= kFullbrightFirst) return gr::kMarkGlow;
  if (index >= kShirtRow * 16 && index < kShirtRow * 16 + 16)
    return uint8_t(gr::kMarkTintA | (index - kShirtRow * 16));
  if (index >= kPantsRow * 16 && index < kPantsRow * 16 + 16)
    return uint8_t(gr::kMarkTintB | (index - kPantsRow * 16));
  return gr::kMarkPlain;
}

// A file's point (or direction, `scale` 1) in the engine's frame.
void to_engine_f(const float* p, float* out, double scale) {
  out[0] = float(p[0] * scale);
  out[1] = float(p[2] * scale);
  out[2] = float(p[1] * scale);
}

template <typename Item>
Groups groups_of(const std::vector<Item>& items) {
  Groups g;
  for (uint32_t i = 0; i < items.size(); i++) {
    const int group = items[i].group;
    if (group >= int(g.first.size())) {
      g.first.resize(size_t(group) + 1, i);
      g.count.resize(size_t(group) + 1, 0);
      g.ends.resize(size_t(group) + 1);
      g.first[size_t(group)] = i;
    }
    g.count[size_t(group)]++;
    if (items[i].interval > 0) {
      std::vector<float>& e = g.ends[size_t(group)];
      e.push_back((e.empty() ? 0.0f : e.back()) + items[i].interval);
    }
  }
  return g;
}

void quat_axis(const float axis[3], double angle, float out[4]) {
  const double s = lm::sin(angle / 2), c = lm::cos(angle / 2);
  for (int k = 0; k < 3; k++) out[k] = float(axis[k] * s);
  out[3] = float(c);
}

void quat_mul(const float a[4], const float b[4], float out[4]) {
  const float r[4] = {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
                      a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                      a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
                      a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
  for (int k = 0; k < 4; k++) out[k] = r[k];
}

void luxel_rgb(const br::Map& map, int32_t l, double u, double v, float out[3]) {
  const br::Lightmap& lmap = map.lightmaps[size_t(l)];
  const double x = std::clamp(u * lmap.width - 0.5, 0.0, double(lmap.width) - 1);
  const double y = std::clamp(v * lmap.height - 0.5, 0.0, double(lmap.height) - 1);
  const auto x0 = uint32_t(x), y0 = uint32_t(y);
  const uint32_t x1 = std::min(x0 + 1, lmap.width - 1), y1 = std::min(y0 + 1, lmap.height - 1);
  const double fx = x - x0, fy = y - y0;
  for (size_t c = 0; c < 3; c++) {
    const auto g = [&](uint32_t a, uint32_t b) {
      const size_t at = lmap.offset + (size_t(b) * lmap.width + a) * 3 + c;
      return at < map.light_rgb.size() ? double(map.light_rgb[at]) : 128.0;
    };
    out[c] = float(((g(x0, y0) * (1 - fx) + g(x1, y0) * fx) * (1 - fy) +
                    (g(x0, y1) * (1 - fx) + g(x1, y1) * fx) * fy) /
                   128.0);
  }
}

}  // namespace

gr::TintRamps palette_ramps(std::span<const uint8_t> palette) {
  gr::TintRamps r;
  r.rgb.resize(size_t(kColourRows) * gr::kTintSteps * 3);
  for (uint32_t i = 0; i < kColourRows * gr::kTintSteps; i++) {
    uint8_t c[4];
    colour_of(palette, uint8_t(i), c);
    for (size_t k = 0; k < 3; k++) r.rgb[size_t(i) * 3 + k] = to_linear(c[k]);
  }
  return r;
}

gr::MorphModel morph_from_alias(const mo::Model& model, std::span<const uint8_t> palette,
                                int skin) {
  gr::MorphModel out;
  const uint32_t n = model.vertex_count();
  for (const mo::Frame& f : model.frames) {
    std::vector<float> p(size_t(n) * 3), nr(size_t(n) * 3);
    const std::vector<float> normals = mo::frame_normals(model, f);
    for (size_t v = 0; v < n; v++) {
      to_engine_f(&f.positions[v * 3], &p[v * 3], br::kUnitM);
      to_engine_f(&normals[v * 3], &nr[v * 3], 1.0);
    }
    out.positions.push_back(std::move(p));
    out.normals.push_back(std::move(nr));
  }
  // The reflection into the engine's frame turns every front inward; the other winding turns it
  // back out.
  out.indices.reserve(model.indices.size());
  for (size_t t = 0; t + 2 < model.indices.size(); t += 3)
    out.indices.insert(out.indices.end(),
                       {model.indices[t], model.indices[t + 2], model.indices[t + 1]});
  out.uv = model.st;
  if (model.skins.empty()) {
    out.skin.width = out.skin.height = 1;
    out.skin.rgba = {128, 128, 128, 255};
    return out;
  }
  const mo::Skin& s = model.skins[size_t(std::clamp(skin, 0, int(model.skins.size()) - 1))];
  out.skin.width = uint32_t(s.width);
  out.skin.height = uint32_t(s.height);
  out.skin.rgba.resize(s.indices.size() * 4);
  out.skin.marks.resize(s.indices.size());
  for (size_t i = 0; i < s.indices.size(); i++) {
    colour_of(palette, s.indices[i], &out.skin.rgba[i * 4]);
    out.skin.marks[i] = mark_of(s.indices[i]);
  }
  return out;
}

gr::MorphModel morph_from_brush(const br::Map& item, std::span<const uint8_t> palette) {
  gr::MorphModel out;
  std::vector<float> pos, nrm;
  struct Piece {
    uint32_t surface = 0;
    int32_t s0 = 0, t0 = 0;
    uint32_t w = 0, h = 0, x = 0, y = 0;
  };
  std::vector<Piece> pieces;
  if (item.models.empty()) return out;
  const br::Model& world = item.models[0];
  // Each textured face's tiles as one rect, shelf-packed into a skin kSkinWidth wide.
  constexpr uint32_t kSkinWidth = 512, kMostSide = 256;
  uint32_t shelf_x = 0, shelf_y = 0, shelf_h = 0;
  for (uint32_t s = world.first_surface; s < world.first_surface + world.surface_count; s++) {
    if (s >= item.surfaces.size()) break;
    const br::Surface& surf = item.surfaces[s];
    if (surf.texture >= item.textures.size() || surf.count < 3) continue;
    const br::Texture& tex = item.textures[surf.texture];
    if (tex.indexed.empty() || tex.width == 0 || tex.height == 0) continue;
    double lo[2] = {1e30, 1e30}, hi[2] = {-1e30, -1e30};
    for (uint32_t i = surf.first; i < surf.first + surf.count; i++) {
      const br::Vertex& v = item.vertices[item.indices[i]];
      for (int k = 0; k < 2; k++) {
        const double st =
            item.st_texels ? v.st[k] : v.st[k] * double(k == 0 ? tex.width : tex.height);
        lo[k] = std::min(lo[k], st);
        hi[k] = std::max(hi[k], st);
      }
    }
    Piece p;
    p.surface = s;
    p.s0 = int32_t(std::floor(lo[0]));
    p.t0 = int32_t(std::floor(lo[1]));
    p.w = std::clamp(uint32_t(std::ceil(hi[0]) - p.s0), 1u, kMostSide);
    p.h = std::clamp(uint32_t(std::ceil(hi[1]) - p.t0), 1u, kMostSide);
    if (shelf_x + p.w > kSkinWidth) {
      shelf_y += shelf_h + 1;
      shelf_x = shelf_h = 0;
    }
    p.x = shelf_x;
    p.y = shelf_y;
    shelf_x += p.w + 1;
    shelf_h = std::max(shelf_h, p.h);
    pieces.push_back(p);
  }
  if (pieces.empty()) return out;
  const uint32_t skin_h = shelf_y + shelf_h;
  out.skin.width = kSkinWidth;
  out.skin.height = skin_h;
  out.skin.rgba.assign(size_t(kSkinWidth) * skin_h * 4, 0);
  out.skin.marks.assign(size_t(kSkinWidth) * skin_h, gr::kMarkPlain);
  for (const Piece& p : pieces) {
    const br::Surface& surf = item.surfaces[p.surface];
    const br::Texture& tex = item.textures[surf.texture];
    for (uint32_t j = 0; j < p.h; j++) {
      for (uint32_t i = 0; i < p.w; i++) {
        const int32_t sx =
            ((p.s0 + int32_t(i)) % int32_t(tex.width) + int32_t(tex.width)) % int32_t(tex.width);
        const int32_t ty =
            ((p.t0 + int32_t(j)) % int32_t(tex.height) + int32_t(tex.height)) % int32_t(tex.height);
        const uint8_t index = tex.indexed[size_t(ty) * tex.width + size_t(sx)];
        const size_t at = size_t(p.y + j) * kSkinWidth + p.x + i;
        colour_of(palette, index, &out.skin.rgba[at * 4]);
        out.skin.marks[at] = index >= kFullbrightFirst ? gr::kMarkGlow : gr::kMarkPlain;
      }
    }
    for (uint32_t t = surf.first; t + 2 < surf.first + surf.count; t += 3) {
      const uint32_t corners[3] = {item.indices[t], item.indices[t + 2], item.indices[t + 1]};
      for (uint32_t c : corners) {
        const br::Vertex& v = item.vertices[c];
        float e[3], en[3];
        to_engine_f(v.pos, e, br::kUnitM);
        to_engine_f(v.nrm, en, 1.0);
        pos.insert(pos.end(), e, e + 3);
        nrm.insert(nrm.end(), en, en + 3);
        double st[2];
        for (int k = 0; k < 2; k++)
          st[k] = item.st_texels ? v.st[k] : v.st[k] * double(k == 0 ? tex.width : tex.height);
        out.uv.push_back(float((st[0] - p.s0 + p.x) / kSkinWidth));
        out.uv.push_back(float((st[1] - p.t0 + p.y) / skin_h));
        out.indices.push_back(uint32_t(out.indices.size()));
      }
    }
  }
  out.positions.push_back(std::move(pos));
  out.normals.push_back(std::move(nrm));
  return out;
}

std::vector<gr::SpriteFrame> sprite_frames_of(const mo::Sprite& sprite,
                                              std::span<const uint8_t> palette) {
  std::vector<gr::SpriteFrame> out;
  for (const mo::SpriteFrame& f : sprite.frames) {
    gr::SpriteFrame g;
    g.width = uint32_t(std::max(f.width, 1));
    g.height = uint32_t(std::max(f.height, 1));
    g.rgba.assign(size_t(g.width) * g.height * 4, 0);
    for (size_t i = 0; i < f.indices.size() && i < size_t(g.width) * g.height; i++) {
      if (f.indices[i] == 255) continue;  // clear
      colour_of(palette, f.indices[i], &g.rgba[i * 4]);
    }
    // The file's origin is the picture's top left against the sprite's place, units right and up.
    g.origin[0] = float(-f.origin_x);
    g.origin[1] = float(f.origin_y);
    out.push_back(std::move(g));
  }
  return out;
}

gr::SpriteFacing facing_of(mo::Facing facing) {
  switch (facing) {
    case mo::Facing::upright:
      return gr::SpriteFacing::upright;
    case mo::Facing::facing:
      return gr::SpriteFacing::parallel_upright;
    case mo::Facing::parallel:
      return gr::SpriteFacing::parallel;
    case mo::Facing::oriented:
      return gr::SpriteFacing::oriented;
    case mo::Facing::parallel_oriented:
      return gr::SpriteFacing::parallel_oriented;
  }
  return gr::SpriteFacing::parallel;
}

Groups frame_groups(const mo::Model& model) { return groups_of(model.frames); }
Groups skin_groups(const mo::Model& model) { return groups_of(model.skins); }
Groups sprite_groups(const mo::Sprite& sprite) { return groups_of(sprite.frames); }

uint32_t frame_in(const Groups& g, int group, double t) {
  if (g.first.empty()) return 0;
  if (group < 0 || group >= int(g.first.size())) group = 0;
  const auto k = size_t(group);
  const std::vector<float>& ends = g.ends[k];
  if (g.count[k] <= 1 || ends.size() != g.count[k] || ends.back() <= 0) return g.first[k];
  const double at = t - std::floor(t / ends.back()) * ends.back();
  for (uint32_t i = 0; i < ends.size(); i++) {
    if (at < ends[i]) return g.first[k] + i;
  }
  return g.first[k] + g.count[k] - 1;
}

void rotation_of(const float angles[3], bool alias, float out[4]) {
  // The file turns yaw counter-clockwise from +x toward +y seen from above, which the engine's
  // frame (the file's y is its z) sees as a turn the other way about its up; pitch and roll
  // likewise reverse.
  const double deg = kPi / 180;
  const float up[3] = {0, 1, 0}, side[3] = {0, 0, 1}, ahead[3] = {1, 0, 0};
  const double pitch = alias ? -angles[0] : angles[0];
  float qy[4], qp[4], qr[4], t[4];
  quat_axis(up, -angles[1] * deg, qy);
  quat_axis(side, -pitch * deg, qp);
  quat_axis(ahead, -angles[2] * deg, qr);
  quat_mul(qy, qp, t);
  quat_mul(t, qr, out);
}

LightProbe make_probe(const br::Map& map) {
  LightProbe p;
  if (map.models.empty()) return p;
  const br::Model& world = map.models[0];
  p.x0 = int32_t(std::floor(world.mins[0] / p.cell)) - 1;
  p.y0 = int32_t(std::floor(world.mins[1] / p.cell)) - 1;
  p.nx = std::max(1, int32_t(std::ceil(world.maxs[0] / p.cell)) + 1 - p.x0);
  p.ny = std::max(1, int32_t(std::ceil(world.maxs[1] / p.cell)) + 1 - p.y0);
  std::vector<std::vector<uint32_t>> cells(size_t(p.nx) * size_t(p.ny));
  std::vector<int32_t> surface_of;
  for (uint32_t s = world.first_surface; s < world.first_surface + world.surface_count; s++) {
    if (s >= map.surfaces.size()) break;
    const br::Surface& surf = map.surfaces[s];
    if (surf.lightmap < 0 || surf.kind != br::SurfaceKind::planar) continue;
    for (uint32_t t = surf.first; t + 2 < surf.first + surf.count; t += 3) {
      const float* a = map.vertices[map.indices[t]].pos;
      const float* b = map.vertices[map.indices[t + 1]].pos;
      const float* c = map.vertices[map.indices[t + 2]].pos;
      const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
      const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
      const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
      const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
      if (len <= 0 || std::abs(nz) / len < 0.2) continue;  // a wall
      const double lo[2] = {std::min({a[0], b[0], c[0]}), std::min({a[1], b[1], c[1]})};
      const double hi[2] = {std::max({a[0], b[0], c[0]}), std::max({a[1], b[1], c[1]})};
      const int32_t cx0 = std::clamp(int32_t(std::floor(lo[0] / p.cell)) - p.x0, 0, p.nx - 1);
      const int32_t cx1 = std::clamp(int32_t(std::floor(hi[0] / p.cell)) - p.x0, 0, p.nx - 1);
      const int32_t cy0 = std::clamp(int32_t(std::floor(lo[1] / p.cell)) - p.y0, 0, p.ny - 1);
      const int32_t cy1 = std::clamp(int32_t(std::floor(hi[1] / p.cell)) - p.y0, 0, p.ny - 1);
      const auto id = uint32_t(surface_of.size());
      surface_of.push_back(int32_t(t));
      p.lightmap.push_back(surf.lightmap);
      for (int32_t y = cy0; y <= cy1; y++)
        for (int32_t x = cx0; x <= cx1; x++)
          cells[size_t(y) * size_t(p.nx) + size_t(x)].push_back(id);
    }
  }
  p.start.reserve(cells.size() + 1);
  for (const auto& c : cells) {
    p.start.push_back(uint32_t(p.tris.size()));
    for (uint32_t id : c) p.tris.push_back(id);
  }
  p.start.push_back(uint32_t(p.tris.size()));
  // `tris` holds triangle ids; turn each into its first index, keeping the lightmap per id.
  std::vector<int32_t> lmap(p.tris.size());
  for (size_t i = 0; i < p.tris.size(); i++) {
    lmap[i] = p.lightmap[p.tris[i]];
    p.tris[i] = uint32_t(surface_of[p.tris[i]]);
  }
  p.lightmap = std::move(lmap);
  return p;
}

bool light_under(const LightProbe& probe, const br::Map& map, const float at[3], float rgb[3],
                 float depth) {
  rgb[0] = rgb[1] = rgb[2] = 1;
  if (probe.start.empty()) return false;
  const int32_t cx = int32_t(std::floor(at[0] / probe.cell)) - probe.x0;
  const int32_t cy = int32_t(std::floor(at[1] / probe.cell)) - probe.y0;
  if (cx < 0 || cy < 0 || cx >= probe.nx || cy >= probe.ny) return false;
  const size_t cell = size_t(cy) * size_t(probe.nx) + size_t(cx);
  double best = double(at[2]) - depth;
  bool hit = false;
  for (uint32_t i = probe.start[cell]; i < probe.start[cell + 1]; i++) {
    const uint32_t t = probe.tris[i];
    const br::Vertex& a = map.vertices[map.indices[t]];
    const br::Vertex& b = map.vertices[map.indices[t + 1]];
    const br::Vertex& c = map.vertices[map.indices[t + 2]];
    const double d = (double(b.pos[1]) - c.pos[1]) * (double(a.pos[0]) - c.pos[0]) +
                     (double(c.pos[0]) - b.pos[0]) * (double(a.pos[1]) - c.pos[1]);
    if (std::abs(d) < 1e-9) continue;
    const double w0 = ((double(b.pos[1]) - c.pos[1]) * (double(at[0]) - c.pos[0]) +
                       (double(c.pos[0]) - b.pos[0]) * (double(at[1]) - c.pos[1])) /
                      d;
    const double w1 = ((double(c.pos[1]) - a.pos[1]) * (double(at[0]) - c.pos[0]) +
                       (double(a.pos[0]) - c.pos[0]) * (double(at[1]) - c.pos[1])) /
                      d;
    const double w2 = 1 - w0 - w1;
    if (w0 < 0 || w1 < 0 || w2 < 0) continue;
    const double z = w0 * a.pos[2] + w1 * b.pos[2] + w2 * c.pos[2];
    if (z > at[2] + 1 || z <= best) continue;
    best = z;
    hit = true;
    luxel_rgb(map, probe.lightmap[i], w0 * a.lm[0] + w1 * b.lm[0] + w2 * c.lm[0],
              w0 * a.lm[1] + w1 * b.lm[1] + w2 * c.lm[1], rgb);
  }
  return hit;
}

}  // namespace shooters_edge::models
