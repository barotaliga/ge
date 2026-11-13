// A cursor over the bytes that refuses to step past their end, and the two files read with it.
//
// ge:summary The model and sprite readers: a checked cursor, headers, skins, triangles, frames
// ge:keywords read cursor mdl spr md2 sp2 seam onseam facesfront trivertx group interval
// ge:keywords texture-coordinate-index offsets
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
      refuse(std::string(what) + " runs past the end (" + std::to_string(n) + " bytes at offset " +
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
  // Moves to `to`, an offset the header gave (refused past the end).
  bool seek(int64_t to, const char* what) {
    if (!ok_) return false;
    if (to < 0 || static_cast<uint64_t>(to) > b_.size()) {
      refuse(std::string(what) + " is at offset " + std::to_string(to) + ", past the end (" +
             std::to_string(b_.size()) + " bytes)");
      return false;
    }
    at_ = static_cast<size_t>(to);
    return true;
  }
  int16_t i16(const char* what) {
    if (!need(2, what)) return 0;
    const uint8_t* p = b_.data() + at_;
    at_ += 2;
    return int16_t(uint16_t(p[0] | (p[1] << 8)));
  }
  const uint8_t* bytes(size_t n, const char* what) {
    if (!need(n, what)) return nullptr;
    const uint8_t* p = b_.data() + at_;
    at_ += n;
    return p;
  }
  // Refuses the file: `what` was wrong; the next step (fetch it again or skip it) is added here.
  void refuse(const std::string& what) {
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
      refuse(std::string(what) + " is " + std::to_string(v) + ", outside " + std::to_string(lo) +
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

// A fixed-length name field: up to its first zero.
std::string name_of(const uint8_t* p, size_t n) {
  return {reinterpret_cast<const char*>(p), strnlen(reinterpret_cast<const char*>(p), n)};
}

bool magic(Cursor& c, const char* m, int version) {
  const uint8_t* p = c.bytes(4, "the magic");
  if (p == nullptr) return false;
  if (std::memcmp(p, m, 4) != 0) {
    c.refuse(std::string("no '") + m + "' magic");
    return false;
  }
  const int32_t v = c.i32("the version");
  if (c.ok() && v != version)
    c.refuse("version " + std::to_string(v) + ", not " + std::to_string(version));
  return c.ok();
}

bool starts(std::span<const uint8_t> b, const char* m) {
  return b.size() >= 4 && std::memcmp(b.data(), m, 4) == 0;
}

// The second family's model (model.hpp): offsets to each part, skins named, every file vertex and
// texture coordinate pair a vertex of the view.
bool read_model2(std::span<const uint8_t> bytes, const std::string& where, Model* out,
                 std::string* error) {
  Cursor c(bytes, where, error);
  if (!magic(c, "IDP2", 8)) return false;
  Model m;
  m.skin_width = c.count("the skin width", 1, kMaxSide);
  m.skin_height = c.count("the skin height", 1, kMaxSide);
  const int frame_size = c.count("the frame size", 40, kMaxCount * 4 + 40);
  const int skins = c.count("the skin count", 0, 256);
  const int verts = c.count("the vertex count", 1, kMaxCount);
  const int coords = c.count("the texture coordinate count", 1, kMaxCount);
  const int tris = c.count("the triangle count", 1, kMaxCount);
  (void)c.i32("the draw command count");
  const int frames = c.count("the frame count", 1, kMaxCount);
  const int32_t at_skins = c.i32("the skins' offset");
  const int32_t at_st = c.i32("the texture coordinates' offset");
  const int32_t at_tris = c.i32("the triangles' offset");
  const int32_t at_frames = c.i32("the frames' offset");
  if (!c.ok()) return false;
  if (frame_size != 40 + 4 * verts) {
    c.refuse("the frame size is " + std::to_string(frame_size) + ", not 40 + 4 x " +
             std::to_string(verts) + " vertices");
    return false;
  }
  c.seek(at_skins, "the skins");
  for (int s = 0; s < skins && c.ok(); s++) {
    const uint8_t* p = c.bytes(64, "a skin's name");
    if (p != nullptr) m.skin_paths.push_back(name_of(p, 64));
  }
  std::vector<int16_t> st(size_t(coords) * 2);
  c.seek(at_st, "the texture coordinates");
  for (int16_t& v : st) v = c.i16("a texture coordinate");
  c.seek(at_tris, "the triangles");
  // A vertex of the view for each (file vertex, coordinate) pair the triangles name.
  std::map<std::pair<int, int>, uint32_t> made;
  for (int t = 0; t < tris && c.ok(); t++) {
    int v[3], s[3];
    for (int& i : v) i = c.i16("a triangle's vertex");
    for (int& i : s) i = c.i16("a triangle's texture coordinate");
    if (!c.ok()) break;
    for (int k = 0; k < 3; k++) {
      if (v[k] < 0 || v[k] >= verts || s[k] < 0 || s[k] >= coords) {
        c.refuse("triangle " + std::to_string(t) + " names vertex " + std::to_string(v[k]) +
                 " of " + std::to_string(verts) + " and coordinate " + std::to_string(s[k]) +
                 " of " + std::to_string(coords));
        break;
      }
      auto [it, fresh] = made.try_emplace({v[k], s[k]}, uint32_t(m.source_vertex.size()));
      if (fresh) {
        m.source_vertex.push_back(v[k]);
        m.st.push_back(float(st[size_t(s[k]) * 2]) / float(m.skin_width));
        m.st.push_back(float(st[size_t(s[k]) * 2 + 1]) / float(m.skin_height));
      }
      m.indices.push_back(it->second);
    }
  }
  if (!c.ok()) return false;
  // Wound clockwise seen from outside, as the first family; the view's order is the other way.
  for (size_t i = 0; i + 2 < m.indices.size(); i += 3)
    std::swap(m.indices[i + 1], m.indices[i + 2]);
  c.seek(at_frames, "the frames");
  for (int f = 0; f < frames && c.ok(); f++) {
    float scale[3], translate[3];
    for (float& v : scale) v = c.f32("a frame's scale");
    for (float& v : translate) v = c.f32("a frame's origin");
    const uint8_t* name = c.bytes(16, "a frame's name");
    const uint8_t* packed = c.bytes(size_t(verts) * 4, "a frame's vertices");
    if (packed == nullptr) break;
    Frame fr;
    fr.group = f;
    fr.name = name_of(name, 16);
    fr.min = {1e30f, 1e30f, 1e30f};
    fr.max = {-1e30f, -1e30f, -1e30f};
    for (int v = 0; v < verts; v++) {
      for (size_t k = 0; k < 3; k++) {
        const float x = float(packed[size_t(v) * 4 + k]) * scale[k] + translate[k];
        if (x < fr.min[k]) fr.min[k] = x;
        if (x > fr.max[k]) fr.max[k] = x;
      }
    }
    fr.positions.resize(m.source_vertex.size() * 3);
    for (size_t v = 0; v < m.source_vertex.size(); v++) {
      const uint8_t* p = packed + size_t(m.source_vertex[v]) * 4;
      for (size_t k = 0; k < 3; k++)
        fr.positions[v * 3 + k] = float(p[k]) * scale[k] + translate[k];
    }
    m.frames.push_back(std::move(fr));
  }
  if (!c.ok()) return false;
  *out = std::move(m);
  return true;
}

// The second family's sprite: each frame a picture file of the game's and the point of it that
// sits on the entity's origin; always facing the viewer.
bool read_sprite2(std::span<const uint8_t> bytes, const std::string& where, Sprite* out,
                  std::string* error) {
  Cursor c(bytes, where, error);
  if (!magic(c, "IDS2", 2)) return false;
  Sprite s;
  s.facing = Facing::parallel;
  const int frames = c.count("the frame count", 1, kMaxCount);
  for (int f = 0; f < frames && c.ok(); f++) {
    SpriteFrame fr;
    fr.group = f;
    fr.width = c.count("a frame's width", 1, kMaxSide);
    fr.height = c.count("a frame's height", 1, kMaxSide);
    const int32_t ox = c.i32("a frame's origin");
    const int32_t oy = c.i32("a frame's origin");
    const uint8_t* name = c.bytes(64, "a frame's picture name");
    if (name == nullptr) break;
    // The view's origin is the picture's top left against the sprite's origin, y up.
    fr.origin_x = -ox;
    fr.origin_y = oy;
    fr.picture = name_of(name, 64);
    s.max_width = fr.width > s.max_width ? fr.width : s.max_width;
    s.max_height = fr.height > s.max_height ? fr.height : s.max_height;
    s.frames.push_back(std::move(fr));
  }
  if (!c.ok()) return false;
  *out = std::move(s);
  return true;
}

}  // namespace

bool read_model(std::span<const uint8_t> bytes, const std::string& where, Model* out,
                std::string* error) {
  if (starts(bytes, "IDP2")) return read_model2(bytes, where, out, error);
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
        c.refuse("triangle " + std::to_string(t) + " names vertex " + std::to_string(v[k]) +
                 " of " + std::to_string(verts));
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
    f.name = name_of(name, 16);
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
  if (starts(bytes, "IDS2")) return read_sprite2(bytes, where, out, error);
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
