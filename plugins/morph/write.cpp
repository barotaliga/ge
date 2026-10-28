// The fixture writers (write.hpp).
//
// ge:summary Packs a model view into model-file bytes and a sprite into sprite bytes; the fixtures
// ge:keywords write pack seam onseam facesfront group interval fixture octahedron sprite
// ge:skill morph
// ge:test tests/morph_read_test.cpp
#include "morph/write.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include "core/num.hpp"

namespace ge::morph {

namespace {

void put32(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back(uint8_t(v >> (8 * i)));
}
void puti(std::vector<uint8_t>& b, int v) { put32(b, static_cast<uint32_t>(v)); }
void putf(std::vector<uint8_t>& b, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  put32(b, v);
}

int round_int(double v) { return static_cast<int>(core::num::round(v)); }

// Runs of consecutive items sharing a group: [first, last).
template <class T>
std::vector<std::pair<size_t, size_t>> runs(const std::vector<T>& items) {
  std::vector<std::pair<size_t, size_t>> out;
  for (size_t i = 0; i < items.size();) {
    size_t j = i + 1;
    while (j < items.size() && items[j].group == items[i].group) j++;
    out.emplace_back(i, j);
    i = j;
  }
  return out;
}

// A lone item is written alone when it has no interval; anything else is a group.
template <class T>
bool lone(const std::vector<T>& items, std::pair<size_t, size_t> r) {
  return r.second - r.first == 1 && items[r.first].interval == 0.0f;
}

}  // namespace

std::vector<uint8_t> write_model(const Model& m) {
  // The packing: each axis's extent over every frame in 255 steps.
  float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
  for (const Frame& f : m.frames) {
    for (size_t i = 0; i + 2 < f.positions.size(); i += 3) {
      for (size_t k = 0; k < 3; k++) {
        lo[k] = std::min(lo[k], f.positions[i + k]);
        hi[k] = std::max(hi[k], f.positions[i + k]);
      }
    }
  }
  float scale[3];
  for (size_t k = 0; k < 3; k++) scale[k] = hi[k] > lo[k] ? (hi[k] - lo[k]) / 255.0f : 1.0f;
  auto pack = [&](float p, size_t k) {
    return uint8_t(std::clamp(round_int(double(p - lo[k]) / double(scale[k])), 0, 255));
  };

  const int sw = m.skins.empty() ? 8 : m.skins[0].width;
  const int sh = m.skins.empty() ? 8 : m.skins[0].height;
  int sources = 0;
  for (const int s : m.source_vertex) sources = std::max(sources, s + 1);
  // Each file vertex's front coordinates (the smallest s among its copies) and whether a copy
  // reads the right half (an on-seam vertex).
  struct St {
    int s = 1 << 30, t = 0;
    bool seam = false;
  };
  std::vector<St> st(static_cast<size_t>(sources));
  std::vector<int> texel_s(m.vertex_count()), texel_t(m.vertex_count());
  std::vector<uint32_t> first(size_t(sources), UINT32_MAX);
  for (uint32_t v = 0; v < m.vertex_count(); v++) {
    texel_s[v] = round_int(double(m.st[size_t{v} * 2]) * sw - 0.5);
    texel_t[v] = round_int(double(m.st[size_t{v} * 2 + 1]) * sh - 0.5);
    St& q = st[size_t(m.source_vertex[v])];
    if (texel_s[v] < q.s) q.s = texel_s[v], q.t = texel_t[v];
  }
  std::vector<bool> back(m.vertex_count(), false);
  for (uint32_t v = 0; v < m.vertex_count(); v++) {
    St& q = st[size_t(m.source_vertex[v])];
    if (texel_s[v] != q.s) back[v] = q.seam = true;
  }

  std::vector<uint8_t> b;
  b.insert(b.end(), {'I', 'D', 'P', 'O'});
  puti(b, 6);
  for (const float s : scale) putf(b, s);
  for (const float t : lo) putf(b, t);
  putf(b, m.radius);
  for (const float e : m.eye) putf(b, e);
  const auto skin_runs = runs(m.skins);
  const auto frame_runs = runs(m.frames);
  puti(b, int(skin_runs.size()));
  puti(b, sw);
  puti(b, sh);
  puti(b, sources);
  puti(b, int(m.indices.size() / 3));
  puti(b, int(frame_runs.size()));
  puti(b, m.synced ? 0 : 1);
  put32(b, m.flags);
  putf(b, 1.0f);

  for (const auto& r : skin_runs) {
    if (lone(m.skins, r)) {
      puti(b, 0);
    } else {
      puti(b, 1);
      puti(b, int(r.second - r.first));
      float t = 0;
      for (size_t i = r.first; i < r.second; i++) putf(b, t += m.skins[i].interval);
    }
    for (size_t i = r.first; i < r.second; i++) {
      b.insert(b.end(), m.skins[i].indices.begin(), m.skins[i].indices.end());
    }
  }
  for (const St& q : st) {
    puti(b, q.seam ? 0x20 : 0);
    puti(b, q.s);
    puti(b, q.t);
  }
  for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
    // The view winds the other way round from the file.
    const uint32_t v[3] = {m.indices[t], m.indices[t + 2], m.indices[t + 1]};
    puti(b, back[v[0]] || back[v[1]] || back[v[2]] ? 0 : 1);
    for (const uint32_t i : v) puti(b, m.source_vertex[i]);
  }

  auto simple = [&](const Frame& f) {
    std::vector<uint8_t> packed(size_t(sources) * 4, 0);
    uint8_t fmin[3] = {255, 255, 255}, fmax[3] = {0, 0, 0};
    for (uint32_t v = 0; v < m.vertex_count(); v++) {
      for (size_t k = 0; k < 3; k++) {
        const uint8_t p = pack(f.positions[size_t(v) * 3 + k], k);
        packed[size_t(m.source_vertex[v]) * 4 + k] = p;
        fmin[k] = std::min(fmin[k], p);
        fmax[k] = std::max(fmax[k], p);
      }
    }
    b.insert(b.end(), {fmin[0], fmin[1], fmin[2], 0, fmax[0], fmax[1], fmax[2], 0});
    char name[16] = {};
    std::copy_n(f.name.data(), std::min<size_t>(f.name.size(), 15), name);  // the rest stays 0
    b.insert(b.end(), name, name + 16);
    b.insert(b.end(), packed.begin(), packed.end());
  };
  for (const auto& r : frame_runs) {
    if (lone(m.frames, r)) {
      puti(b, 0);
      simple(m.frames[r.first]);
      continue;
    }
    puti(b, 1);
    puti(b, int(r.second - r.first));
    b.insert(b.end(), {0, 0, 0, 0, 255, 255, 255, 0});
    float t = 0;
    for (size_t i = r.first; i < r.second; i++) putf(b, t += m.frames[i].interval);
    for (size_t i = r.first; i < r.second; i++) simple(m.frames[i]);
  }
  return b;
}

std::vector<uint8_t> write_sprite(const Sprite& s) {
  std::vector<uint8_t> b;
  b.insert(b.end(), {'I', 'D', 'S', 'P'});
  puti(b, 1);
  puti(b, int(s.facing));
  putf(b, s.radius);
  puti(b, s.max_width);
  puti(b, s.max_height);
  const auto frame_runs = runs(s.frames);
  puti(b, int(frame_runs.size()));
  putf(b, 0.0f);
  puti(b, 0);
  auto one = [&](const SpriteFrame& f) {
    puti(b, f.origin_x);
    puti(b, f.origin_y);
    puti(b, f.width);
    puti(b, f.height);
    b.insert(b.end(), f.indices.begin(), f.indices.end());
  };
  for (const auto& r : frame_runs) {
    if (lone(s.frames, r)) {
      puti(b, 0);
      one(s.frames[r.first]);
      continue;
    }
    puti(b, 1);
    puti(b, int(r.second - r.first));
    float t = 0;
    for (size_t i = r.first; i < r.second; i++) putf(b, t += s.frames[i].interval);
    for (size_t i = r.first; i < r.second; i++) one(s.frames[i]);
  }
  return b;
}

Model sample_model() {
  // An octahedron 32 units across, z up. The four corners on the y = 0 plane are the seam: the
  // front triangles (through +y) read the skin's left half, the back ones (through -y) its right.
  constexpr int kW = 8, kH = 4;
  const float corner[6][3] = {{16, 0, 0},  {-16, 0, 0}, {0, 16, 0},
                              {0, -16, 0}, {0, 0, 16},  {0, 0, -16}};
  // Each corner's texel on the left half (the back pole sits on the right half already).
  const int texel[6][2] = {{3, 1}, {0, 1}, {1, 1}, {5, 1}, {1, 0}, {1, 3}};
  const int faces[8][3] = {{2, 0, 4}, {1, 2, 4}, {0, 2, 5}, {2, 1, 5},
                           {0, 3, 4}, {3, 1, 4}, {3, 0, 5}, {1, 3, 5}};
  Model m;
  m.radius = 16;
  m.eye = {0, 0, 8};
  for (int g = 0; g < 2; g++) {
    const int count = g == 0 ? 1 : 2;
    for (int i = 0; i < count; i++) {
      Skin s{kW, kH, std::vector<uint8_t>(size_t(kW) * kH), g == 0 ? 0.0f : 0.25f, g};
      for (size_t k = 0; k < s.indices.size(); k++)
        s.indices[k] = uint8_t(k * 7 + size_t(i) * 50 + size_t(g) * 20);
      m.skins.push_back(std::move(s));
    }
  }
  std::vector<std::pair<int, bool>> made;  // (corner, reads the right half)
  auto vertex = [&](int c, bool right) {
    for (size_t i = 0; i < made.size(); i++) {
      if (made[i] == std::make_pair(c, right)) return uint32_t(i);
    }
    made.emplace_back(c, right);
    m.source_vertex.push_back(c);
    m.st.push_back((float(texel[c][0] + (right ? kW / 2 : 0)) + 0.5f) / kW);
    m.st.push_back((float(texel[c][1]) + 0.5f) / kH);
    return uint32_t(made.size() - 1);
  };
  for (const auto& face : faces) {
    // Counter-clockwise seen from outside: the face's normal points away from the centre.
    int f[3] = {face[0], face[1], face[2]};
    float e1[3], e2[3], mid[3];
    for (int k = 0; k < 3; k++) {
      e1[k] = corner[f[1]][k] - corner[f[0]][k];
      e2[k] = corner[f[2]][k] - corner[f[0]][k];
      mid[k] = corner[f[0]][k] + corner[f[1]][k] + corner[f[2]][k];
    }
    const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]};
    if (n[0] * mid[0] + n[1] * mid[1] + n[2] * mid[2] < 0) std::swap(f[1], f[2]);
    const bool back = f[0] == 3 || f[1] == 3 || f[2] == 3;
    for (const int c : f) m.indices.push_back(vertex(c, back && c != 3 && c != 2));
  }
  const float sizes[3] = {1.0f, 1.25f, 1.5f};
  const char* names[3] = {"stand", "grow1", "grow2"};
  for (int i = 0; i < 3; i++) {
    Frame f;
    f.name = names[i];
    f.group = i == 0 ? 0 : 1;
    f.interval = i == 0 ? 0.0f : 0.1f;
    for (const auto& [c, right] : made) {
      for (int k = 0; k < 3; k++) f.positions.push_back(corner[c][k] * sizes[i]);
    }
    m.frames.push_back(std::move(f));
  }
  return m;
}

Sprite sample_sprite() {
  Sprite s;
  s.facing = Facing::parallel;
  s.radius = 12;
  s.max_width = 8;
  s.max_height = 6;
  for (int i = 0; i < 3; i++) {
    SpriteFrame f;
    f.width = 8 - i * 2;
    f.height = 6 - i;
    f.origin_x = -f.width / 2;
    f.origin_y = f.height / 2;
    f.group = i == 0 ? 0 : 1;
    f.interval = i == 0 ? 0.0f : 0.1f;
    f.indices.resize(size_t(f.width) * size_t(f.height));
    for (size_t k = 0; k < f.indices.size(); k++) f.indices[k] = uint8_t(k * 13 + size_t(i) * 40);
    s.frames.push_back(std::move(f));
  }
  return s;
}

}  // namespace ge::morph
