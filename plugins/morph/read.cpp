// A cursor over the bytes that refuses to step past their end, and the two files read with it.
//
// ge:summary The model and sprite readers: a checked cursor, headers, skins, triangles, frames
// ge:keywords read cursor mdl spr seam onseam facesfront trivertx group interval
// ge:skill morph
// ge:test tests/morph_read_test.cpp
#include "morph/read.hpp"

#include <cstring>
#include <map>
#include <utility>

namespace ge::morph {

namespace {

constexpr int kMaxSide = 4096;      // a skin or a sprite frame larger than this is damage
constexpr int kMaxCount = 1 << 20;  // so is a count of vertices, triangles or frames past this

class Cursor {
 public:
  Cursor(std::span<const uint8_t> b, const std::string& where, std::string* error)
      : b_(b), where_(where), error_(error) {}
  bool ok() const { return ok_; }
  size_t at() const { return at_; }
  bool need(size_t n, const char* what) {
    if (!ok_) return false;
    if (n > b_.size() - at_) {
      fail(std::string(what) + " runs past the end (" + std::to_string(n) + " bytes at offset " +
           std::to_string(at_) + " of " + std::to_string(b_.size()) + ")");
    }
    return ok_;
  }
  int32_t i32(const char* what) {
    if (!need(4, what)) return 0;
    const uint8_t* p = b_.data() + at_;
    at_ += 4;
    return int32_t(uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
                   (uint32_t(p[3]) << 24));
  }
  float f32(const char* what) {
    const int32_t v = i32(what);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
  }
  const uint8_t* bytes(size_t n, const char* what) {
    if (!need(n, what)) return nullptr;
    const uint8_t* p = b_.data() + at_;
    at_ += n;
    return p;
  }
  void fail(const std::string& what) {
    if (!ok_) return;
    ok_ = false;
    *error_ = where_ + ": " + what +
              "; next: the file is damaged or not this format, so fetch it again or skip it "
              "(skill: morph)";
  }
  // A count read and held to [lo, hi].
  int count(const char* what, int lo, int hi) {
    const int32_t v = i32(what);
    if (ok_ && (v < lo || v > hi)) {
      fail(std::string(what) + " is " + std::to_string(v) + ", outside " + std::to_string(lo) +
           ".." + std::to_string(hi));
    }
    return ok_ ? v : 0;
  }

 private:
  std::span<const uint8_t> b_;
  const std::string& where_;
  std::string* error_;
  size_t at_ = 0;
  bool ok_ = true;
};

bool magic(Cursor& c, const char* m, int version) {
  const uint8_t* p = c.bytes(4, "the magic");
  if (p == nullptr) return false;
  if (std::memcmp(p, m, 4) != 0) {
    c.fail(std::string("no '") + m + "' magic");
    return false;
  }
  const int32_t v = c.i32("the version");
  if (c.ok() && v != version)
    c.fail("version " + std::to_string(v) + ", not " + std::to_string(version));
  return c.ok();
}

}  // namespace

bool read_model(std::span<const uint8_t> bytes, const std::string& where, Model* out,
                std::string* error) {
  Cursor c(bytes, where, error);
  if (!magic(c, "IDPO", 6)) return false;
  float scale[3], translate[3];
  for (float& v : scale) v = c.f32("the scale");
  for (float& v : translate) v = c.f32("the origin");
  Model m;
  m.radius = c.f32("the radius");
  for (float& v : m.eye) v = c.f32("the eye");
  const int skins = c.count("the skin count", 0, 256);
  const int sw = c.count("the skin width", 1, kMaxSide);
  const int sh = c.count("the skin height", 1, kMaxSide);
  const int verts = c.count("the vertex count", 1, kMaxCount);
  const int tris = c.count("the triangle count", 1, kMaxCount);
  const int frames = c.count("the frame count", 1, kMaxCount);
  m.synced = c.i32("the sync type") == 0;
  m.flags = uint32_t(c.i32("the flags"));
  (void)c.f32("the size");
  if (!c.ok()) return false;
  const size_t texels = size_t(sw) * size_t(sh);
  for (int s = 0; s < skins && c.ok(); s++) {
    const int32_t type = c.i32("a skin's type");
    if (type == 0) {
      const uint8_t* p = c.bytes(texels, "a skin");
      if (p != nullptr) m.skins.push_back({sw, sh, {p, p + texels}, 0.0f, s});
      continue;
    }
    const int n = c.count("a skin group's size", 1, 256);
    std::vector<float> times(static_cast<size_t>(n));
    for (float& t : times) t = c.f32("a skin group's interval");
    for (int i = 0; i < n && c.ok(); i++) {
      const uint8_t* p = c.bytes(texels, "a grouped skin");
      const float prev = i > 0 ? times[size_t(i - 1)] : 0.0f;
      if (p != nullptr) m.skins.push_back({sw, sh, {p, p + texels}, times[size_t(i)] - prev, s});
    }
  }
  struct St {
    int seam, s, t;
  };
  std::vector<St> st(size_t(verts), St{0, 0, 0});
  for (St& v : st) {
    v.seam = c.i32("a texture coordinate");
    v.s = c.i32("a texture coordinate");
    v.t = c.i32("a texture coordinate");
  }
  // The seam split: a vertex on the seam read by a back-facing triangle takes the skin's right
  // half, so it becomes a second vertex with the same position.
  std::map<std::pair<int, bool>, uint32_t> made;
  for (int t = 0; t < tris && c.ok(); t++) {
    const bool front = c.i32("a triangle") != 0;
    int v[3];
    for (int& i : v) i = c.i32("a triangle's vertex");
    if (!c.ok()) break;
    for (int k = 0; k < 3; k++) {
      if (v[k] < 0 || v[k] >= verts) {
        c.fail("triangle " + std::to_string(t) + " names vertex " + std::to_string(v[k]) + " of " +
               std::to_string(verts));
        break;
      }
      const bool back = !front && st[size_t(v[k])].seam != 0;
      auto [it, fresh] = made.try_emplace({v[k], back}, uint32_t(m.source_vertex.size()));
      if (fresh) {
        const St& q = st[size_t(v[k])];
        m.source_vertex.push_back(v[k]);
        m.st.push_back((float(q.s + (back ? sw / 2 : 0)) + 0.5f) / float(sw));
        m.st.push_back((float(q.t) + 0.5f) / float(sh));
      }
      m.indices.push_back(it->second);
    }
  }
  if (!c.ok()) return false;
  // The file winds triangles clockwise seen from outside; the view's order is the other way.
  for (size_t i = 0; i + 2 < m.indices.size(); i += 3)
    std::swap(m.indices[i + 1], m.indices[i + 2]);
  auto simple = [&](int group, float interval) {
    Frame f;
    f.group = group;
    f.interval = interval;
    const uint8_t* lo = c.bytes(4, "a frame's bounds");
    const uint8_t* hi = c.bytes(4, "a frame's bounds");
    const uint8_t* name = c.bytes(16, "a frame's name");
    const uint8_t* packed = c.bytes(size_t(verts) * 4, "a frame's vertices");
    if (packed == nullptr) return;
    for (int k = 0; k < 3; k++) {
      f.min[size_t(k)] = float(lo[k]) * scale[k] + translate[k];
      f.max[size_t(k)] = float(hi[k]) * scale[k] + translate[k];
    }
    f.name.assign(reinterpret_cast<const char*>(name),
                  strnlen(reinterpret_cast<const char*>(name), 16));
    f.positions.resize(m.source_vertex.size() * 3);
    for (size_t v = 0; v < m.source_vertex.size(); v++) {
      const uint8_t* p = packed + size_t(m.source_vertex[v]) * 4;
      for (int k = 0; k < 3; k++)
        f.positions[v * 3 + size_t(k)] = float(p[k]) * scale[k] + translate[k];
    }
    m.frames.push_back(std::move(f));
  };
  for (int f = 0; f < frames && c.ok(); f++) {
    const int32_t type = c.i32("a frame's type");
    if (type == 0) {
      simple(f, 0.0f);
      continue;
    }
    const int n = c.count("a frame group's size", 1, kMaxCount);
    (void)c.bytes(8, "a frame group's bounds");
    std::vector<float> times(static_cast<size_t>(n));
    for (float& t : times) t = c.f32("a frame group's interval");
    for (int i = 0; i < n && c.ok(); i++)
      simple(f, times[size_t(i)] - (i > 0 ? times[size_t(i - 1)] : 0.0f));
  }
  if (!c.ok()) return false;
  *out = std::move(m);
  return true;
}

bool read_sprite(std::span<const uint8_t> bytes, const std::string& where, Sprite* out,
                 std::string* error) {
  Cursor c(bytes, where, error);
  if (!magic(c, "IDSP", 1)) return false;
  Sprite s;
  const int type = c.count("the facing type", 0, 4);
  s.facing = Facing(type);
  s.radius = c.f32("the radius");
  s.max_width = c.count("the width", 0, kMaxSide);
  s.max_height = c.count("the height", 0, kMaxSide);
  const int frames = c.count("the frame count", 1, kMaxCount);
  (void)c.f32("the beam length");
  (void)c.i32("the sync type");
  auto one = [&](int group, float interval) {
    SpriteFrame f;
    f.group = group;
    f.interval = interval;
    f.origin_x = c.i32("a frame's origin");
    f.origin_y = c.i32("a frame's origin");
    f.width = c.count("a frame's width", 1, kMaxSide);
    f.height = c.count("a frame's height", 1, kMaxSide);
    const uint8_t* p = c.bytes(size_t(f.width) * size_t(f.height), "a frame's picture");
    if (p == nullptr) return;
    f.indices.assign(p, p + size_t(f.width) * size_t(f.height));
    s.frames.push_back(std::move(f));
  };
  for (int f = 0; f < frames && c.ok(); f++) {
    if (c.i32("a frame's type") == 0) {
      one(f, 0.0f);
      continue;
    }
    const int n = c.count("a frame group's size", 1, kMaxCount);
    std::vector<float> times(static_cast<size_t>(n));
    for (float& t : times) t = c.f32("a frame group's interval");
    for (int i = 0; i < n && c.ok(); i++)
      one(f, times[size_t(i)] - (i > 0 ? times[size_t(i - 1)] : 0.0f));
  }
  if (!c.ok()) return false;
  *out = std::move(s);
  return true;
}

}  // namespace ge::morph
