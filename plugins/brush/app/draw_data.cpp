// The classic map experiment's data (classic_data.hpp): the pack, the atlases, the vertices, the
// liquid bodies and the lights.
//
// ge:summary The classic map experiment's data: the pack, atlases, liquid bodies and point lights
// ge:keywords lab classic bsp pack palette miptex atlas lightmap liquid-body point-light flicker
// ge:skill brush labs
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <numeric>

#include "brush/app/draw_data.hpp"
#include "brush/bytes.hpp"
#include "brush/read.hpp"
#include "core/libm/libm.hpp"

namespace ge::brush::app {

namespace {

using detail::Cursor;

bool fail(std::string* error, std::string what) {
  if (error) *error = std::move(what);
  return false;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// A shelf packer: rectangles placed left to right in rows of the tallest so far.
struct Shelf {
  uint32_t width;
  uint32_t x = 0, y = 0, row = 0;
  explicit Shelf(uint32_t w) : width(w) {}
  void place(uint32_t w, uint32_t h, uint32_t* ox, uint32_t* oy) {
    if (x + w > width) {
      x = 0;
      y += row;
      row = 0;
    }
    *ox = x;
    *oy = y;
    x += w;
    row = std::max(row, h);
  }
  uint32_t height() const { return y + row; }
};

uint32_t up_to(uint32_t v, uint32_t m) { return (v + m - 1) / m * m; }

// The map's miptex records, mip by mip: lump 2 of a version 29 file.
struct Mips {
  uint32_t width = 0, height = 0;
  const uint8_t* level[4] = {nullptr, nullptr, nullptr, nullptr};
};

std::vector<Mips> read_mips(std::span<const uint8_t> file) {
  std::vector<Mips> out;
  Cursor head{file};
  head.u32();
  for (int i = 0; i < 2; ++i) head.u32(), head.u32();
  const uint32_t ofs = head.u32(), len = head.u32();
  if (head.bad || ofs > file.size() || len > file.size() - ofs || len < 4) return out;
  const std::span<const uint8_t> lump = file.subspan(ofs, len);
  Cursor c{lump};
  const uint32_t count = c.u32();
  for (uint32_t i = 0; i < count && !c.bad; ++i) {
    const int32_t at = c.i32();
    Mips m;
    if (at >= 0 && uint32_t(at) + 40 <= len) {
      Cursor r{lump};
      r.at = size_t(at);
      r.name(16);
      m.width = r.u32();
      m.height = r.u32();
      for (int k = 0; k < 4; ++k) {
        const uint32_t o = r.u32();
        const uint64_t bytes = uint64_t(m.width >> k) * (m.height >> k);
        if (o != 0 && uint64_t(at) + o + bytes <= len) m.level[k] = lump.data() + at + o;
      }
      if (!m.level[0]) m.width = m.height = 0;
    }
    out.push_back(m);
  }
  return out;
}

Kind liquid_kind(const std::string& name) {
  const std::string n = lower(name);
  if (n.find("lava") != std::string::npos) return kLava;
  if (n.find("slime") != std::string::npos) return kSlime;
  if (n.find("water") != std::string::npos) return kWater;
  if (n.find("tele") != std::string::npos) return kTele;
  return kWarp;  // another warped surface: drawn as it is
}

double number(const std::string& s, double fallback) {
  if (s.empty()) return fallback;
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  return end == s.c_str() ? fallback : v;
}

bool vec3(const std::string& s, double out[3]) {
  return std::sscanf(s.c_str(), "%lf %lf %lf", &out[0], &out[1], &out[2]) == 3;
}

uint32_t hash32(uint32_t h) {
  h ^= h >> 16;
  h *= 0x7feb352du;
  h ^= h >> 15;
  h *= 0x846ca68bu;
  h ^= h >> 16;
  return h;
}

// A smooth value noise in 0..1 along t (a cubic between hashed knots a second apart).
double noise1(double t, uint32_t seed) {
  const double f = std::floor(t);
  const double u = t - f;
  const auto knot = [&](double k) {
    return double(hash32(uint32_t(int64_t(k)) * 0x9e3779b1u ^ seed) & 0xffffff) / 16777215.0;
  };
  const double s = u * u * (3 - 2 * u);
  return knot(f) + (knot(f + 1) - knot(f)) * s;
}

}  // namespace

bool read_pack(const std::string& path, Pack* out, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return fail(error, "cannot open the pack " + path);
  out->bytes.assign(std::istreambuf_iterator<char>(in), {});
  Cursor c{out->bytes};
  const std::string magic = c.name(4);
  const uint32_t dir = c.u32(), len = c.u32();
  if (magic != "PACK" || c.bad || dir > out->bytes.size() || len > out->bytes.size() - dir ||
      len % 64 != 0) {
    return fail(error,
                path + " is not a pack file (no PACK header, or its directory lies outside it)");
  }
  Cursor d{out->bytes};
  d.at = dir;
  for (uint32_t i = 0; i < len / 64; ++i) {
    Pack::Entry e;
    e.name = d.name(56);
    e.offset = d.u32();
    e.size = d.u32();
    if (e.offset > out->bytes.size() || e.size > out->bytes.size() - e.offset) {
      return fail(error, path + ": the file " + e.name + " lies outside the pack");
    }
    out->files.push_back(std::move(e));
  }
  return true;
}

std::span<const uint8_t> pack_file(const Pack& pack, std::string_view name) {
  for (const Pack::Entry& e : pack.files) {
    if (e.name == name) return std::span<const uint8_t>(pack.bytes).subspan(e.offset, e.size);
  }
  return {};
}

double style_level(std::string_view pattern, double t) {
  if (pattern.empty()) return 1;
  const auto at = size_t(std::floor(std::max(t, 0.0) * kStyleRate)) % pattern.size();
  return double(std::clamp(pattern[at], 'a', 'z') - 'a') / double('m' - 'a');
}

double heading_of(double angle_deg) {
  return 3.14159265358979 / 2 - angle_deg * 3.14159265358979 / 180;
}

double flicker_at(const PointLight& l, double t) {
  const double phase = double(l.seed % 1000) / 1000.0;
  switch (l.flicker) {
    case Flicker::steady:
      return 1;
    case Flicker::candle: {
      // A flame: a slow sway and a fast flutter under it, never out.
      const double sway = noise1(t * 3.1 + phase * 50, l.seed);
      const double flutter = noise1(t * 13.0 + phase * 90, l.seed ^ 0x5bd1e995u);
      return 0.72 + 0.2 * sway + 0.12 * flutter;
    }
    case Flicker::pulse:
      return 0.6 + 0.4 * std::sin(6.2831853 * (t / 2.4 + phase));
    case Flicker::strobe:
      return std::fmod(t * 2 + phase, 1.0) < 0.5 ? 1.0 : 0.12;
    case Flicker::buzz: {
      // A failing tube: on, with now and then a stutter.
      const double n = noise1(t * 7 + phase * 30, l.seed);
      return n > 0.82 ? 0.25 + 0.3 * noise1(t * 40, l.seed ^ 0x27d4eb2du) : 1.0;
    }
  }
  return 1;
}

namespace {

// sRGB 0..255 to linear light.
double linear_of(uint8_t v) {
  const double c = v / 255.0;
  return c <= 0.04045 ? c / 12.92 : ge::core::libm::pow((c + 0.055) / 1.055, 2.4);
}

// A sky texture's colours (RGBA, w x h): the left half its cloud layer, a texel with alpha 1 (the
// palette's first colour) see-through; the right half its back layer.
SkyColours sky_colours_of(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h) {
  SkyColours c;
  double clouds = 0, backs = 0;
  for (uint32_t y = 0; y < h; ++y)
    for (uint32_t x = 0; x < w; ++x) {
      const uint8_t* t = &rgba[(size_t(y) * w + x) * 4];
      const bool front = x < w / 2;
      if (front && t[3] == 1) continue;
      double* sum = front ? c.cloud : c.back;
      for (int k = 0; k < 3; ++k) sum[k] += linear_of(t[k]);
      (front ? clouds : backs) += 1;
    }
  for (int k = 0; k < 3; ++k) {
    c.cloud[k] /= std::max(clouds, 1.0);
    c.back[k] /= std::max(backs, 1.0);
  }
  const uint32_t half = w / 2;  // the cloud layer's texels across
  c.cover = clouds / std::max(double(half) * h, 1.0);
  c.found = backs > 0;
  return c;
}

}  // namespace

bool load_scene(const Pack& pack, std::string_view path, std::span<const uint8_t> palette,
                Scene* out, std::string* error, const TextureSource& source) {
  Scene& s = *out;
  s.name = std::string(path);
  const std::span<const uint8_t> file = pack_file(pack, path);
  if (file.empty()) return fail(error, "the pack has no " + std::string(path));
  if (palette.size() < 768) return fail(error, "the palette is not 768 bytes");
  if (!read_bsp(file, &s.map, error)) return false;
  const Map& m = s.map;

  // ── The texture atlas: every texture at its rectangle, rounded so each mip halves it ──
  // A source may hand a texture larger than the map's (2, 4 or 8 times across: an upscaled copy);
  // its rectangle is that much larger and its faces' texture coordinates scaled to match. The
  // frames of an animated sequence keep one scale, or all fall back to the map's own.
  const std::vector<Mips> mips = read_mips(file);
  struct Placed {
    uint32_t x = 0, y = 0, w = 16, h = 16;
  };
  std::vector<Placed> placed(m.textures.size());
  std::vector<uint32_t> scale_of(m.textures.size(), 1);
  std::vector<std::vector<uint8_t>> own(m.textures.size());
  for (size_t i = 0; i < m.textures.size(); ++i) {
    if (s.sky_texture == UINT32_MAX && m.textures[i].name.size() >= 3 &&
        lower(m.textures[i].name).rfind("sky", 0) == 0) {
      s.sky_texture = uint32_t(i);
    }
  }
  const auto size_of = [&](size_t i) {
    const bool ok = i < mips.size() && mips[i].width > 0;
    return std::pair<uint32_t, uint32_t>{ok ? mips[i].width : 16, ok ? mips[i].height : 16};
  };
  for (size_t i = 0; i < m.textures.size() && source.replace; ++i) {
    if (i >= mips.size() || !mips[i].level[0]) continue;
    const auto [w, h] = size_of(i);
    std::vector<uint8_t> rgba;
    if (!source.replace(source.user, m.textures[i].name, w, h, {mips[i].level[0], size_t(w) * h},
                        palette, &rgba))
      continue;
    for (uint32_t k : {1u, 2u, 4u, 8u}) {
      if (rgba.size() == size_t(w) * k * h * k * 4) {
        own[i] = std::move(rgba);
        scale_of[i] = k;
        break;
      }
    }
  }
  {
    // A sequence's frames ("+0name" .. "+9name", "+aname" ..) at one scale, or none replaced.
    std::map<std::string, std::vector<size_t>> runs;
    for (size_t i = 0; i < m.textures.size(); ++i) {
      const std::string& n = m.textures[i].name;
      if (n.size() >= 3 && n[0] == '+') runs[lower(n.substr(2))].push_back(i);
    }
    for (const auto& [name, members] : runs) {
      bool same = true;
      for (size_t i : members) same = same && scale_of[i] == scale_of[members[0]];
      if (same) continue;
      for (size_t i : members) {
        own[i].clear();
        scale_of[i] = 1;
      }
    }
  }
  const bool scaled =
      std::any_of(scale_of.begin(), scale_of.end(), [](uint32_t k) { return k > 1; });
  s.textures.mips = scaled ? 6 : 4;
  const uint32_t align = 1u << (s.textures.mips - 1);
  {
    std::vector<size_t> order(m.textures.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
      return size_of(a).second * scale_of[a] > size_of(b).second * scale_of[b];
    });
    // The narrowest atlas, 2048 texels and up, whose height stays in a GPU's limits.
    uint32_t width = 2048;
    for (;; width *= 2) {
      Shelf shelf(width);
      for (size_t i : order) {
        const auto [w, h] = size_of(i);
        placed[i].w = w * scale_of[i];
        placed[i].h = h * scale_of[i];
        shelf.place(up_to(placed[i].w, align), up_to(placed[i].h, align), &placed[i].x,
                    &placed[i].y);
      }
      s.textures.height = up_to(std::max(shelf.height(), align), align);
      if (s.textures.height <= 8192 || width >= 8192) break;
    }
    s.textures.width = width;
    // Each texture's levels as RGBA: the map's own through the palette (its own mips where the file
    // has them), or the source's; the levels past those halved from the one above.
    const uint32_t levels = s.textures.mips;
    std::vector<std::vector<std::vector<uint8_t>>> rgba(placed.size(),
                                                        std::vector<std::vector<uint8_t>>(levels));
    for (size_t i = 0; i < placed.size(); ++i) {
      const Placed& p = placed[i];
      const bool replaced = !own[i].empty();
      if (replaced) rgba[i][0] = std::move(own[i]);
      for (uint32_t k = replaced ? 1 : 0; k < levels; ++k) {
        const uint32_t tw = std::max(p.w >> k, 1u), th = std::max(p.h >> k, 1u);
        rgba[i][k].assign(size_t(tw) * th * 4, 0);
        if (replaced || k >= 4 || (p.w >> k) == 0 || (p.h >> k) == 0) {
          const uint32_t pw = std::max(p.w >> (k - 1), 1u), ph = std::max(p.h >> (k - 1), 1u);
          for (uint32_t y = 0; y < th; ++y)
            for (uint32_t x = 0; x < tw; ++x)
              for (int c = 0; c < 4; ++c) {
                int sum = 0;
                for (uint32_t d = 0; d < 4; ++d) {
                  const uint32_t sx = std::min(2 * x + d % 2, pw - 1);
                  const uint32_t sy = std::min(2 * y + d / 2, ph - 1);
                  sum += rgba[i][k - 1][(size_t(sy) * pw + sx) * 4 + size_t(c)];
                }
                rgba[i][k][(size_t(y) * tw + x) * 4 + size_t(c)] = uint8_t(sum / 4);
              }
          continue;
        }
        const bool has = i < mips.size() && mips[i].level[k];
        for (uint32_t y = 0; y < th; ++y) {
          for (uint32_t x = 0; x < tw; ++x) {
            // A missing texture: a grey check.
            const uint8_t idx = has ? mips[i].level[k][size_t(y) * tw + x]
                                    : uint8_t((((x << k) >> 3) ^ ((y << k) >> 3)) & 1 ? 8 : 12);
            uint8_t* t = &rgba[i][k][(size_t(y) * tw + x) * 4];
            t[0] = palette[idx * 3 + 0];
            t[1] = palette[idx * 3 + 1];
            t[2] = palette[idx * 3 + 2];
            t[3] = idx >= 224 && idx != 255 ? 255 : idx == 0 ? 1 : 0;
          }
        }
      }
    }
    if (s.sky_texture < placed.size() && !rgba[s.sky_texture][0].empty())
      s.sky_colours =
          sky_colours_of(rgba[s.sky_texture][0], placed[s.sky_texture].w, placed[s.sky_texture].h);
    for (uint32_t k = 0; k < levels; ++k) {
      const uint32_t w = s.textures.width >> k, h = s.textures.height >> k;
      std::vector<uint8_t> level(size_t(w) * h * 4, 0);
      for (size_t i = 0; i < placed.size(); ++i) {
        const Placed& p = placed[i];
        const uint32_t tw = std::max(p.w >> k, 1u), th = std::max(p.h >> k, 1u);
        for (uint32_t y = 0; y < th; ++y) {
          std::memcpy(&level[((size_t((p.y >> k) + y) * w) + (p.x >> k)) * 4],
                      &rgba[i][k][size_t(y) * tw * 4], size_t(tw) * 4);
        }
      }
      s.textures.levels.push_back(std::move(level));
    }
  }

  // ── The lightmap atlas: each face's styles, a layer each, with a luxel's border ──
  struct LmPlace {
    uint32_t x = 0, y = 0;
  };
  std::vector<LmPlace> lm_at(m.lightmaps.size());
  {
    Shelf shelf(2048);
    for (size_t i = 0; i < m.lightmaps.size(); ++i) {
      shelf.place(m.lightmaps[i].width + 2, m.lightmaps[i].height + 2, &lm_at[i].x, &lm_at[i].y);
    }
    s.light_layers = 1;
    for (const Surface& sf : m.surfaces) {
      if (sf.lightmap >= 0) s.light_layers = std::max<uint32_t>(s.light_layers, sf.styles);
    }
    const uint32_t layer = std::max(shelf.height(), 4u);
    s.light.width = 2048;
    s.light.height = layer * s.light_layers;
    std::vector<uint8_t> level(size_t(s.light.width) * s.light.height * 4, 0);
    for (const Surface& sf : m.surfaces) {
      if (sf.lightmap < 0) continue;
      const size_t i = size_t(sf.lightmap);
      const Lightmap& l = m.lightmaps[i];
      const size_t one = size_t(l.width) * l.height * 3;  // one style's bytes
      for (uint32_t k = 0; k < sf.styles; ++k) {
        for (uint32_t y = 0; y < l.height + 2; ++y) {
          for (uint32_t x = 0; x < l.width + 2; ++x) {
            const uint32_t sx = std::clamp(x, 1u, l.width) - 1,
                           sy = std::clamp(y, 1u, l.height) - 1;
            const size_t from = l.offset + one * k + (size_t(sy) * l.width + sx) * 3;
            const size_t to =
                ((size_t(k * layer + lm_at[i].y + y) * s.light.width) + lm_at[i].x + x) * 4;
            for (int c = 0; c < 3; ++c)
              level[to + size_t(c)] =
                  from + 2 < m.light_rgb.size() ? m.light_rgb[from + size_t(c)] : 0;
            level[to + 3] = 255;
          }
        }
      }
    }
    s.light.levels.push_back(std::move(level));
  }

  // ── The animated textures: "+0name" .. "+9name", each sequence's frames together ──
  std::vector<uint32_t> anim_of(m.textures.size(), 0);
  {
    std::map<std::string, std::array<int32_t, 10>> sequences;
    for (size_t i = 0; i < m.textures.size(); ++i) {
      const std::string& n = m.textures[i].name;
      if (n.size() < 3 || n[0] != '+' || n[1] < '0' || n[1] > '9') continue;
      auto [it, added] = sequences.try_emplace(lower(n.substr(2)));
      if (added) it->second.fill(-1);
      it->second[size_t(n[1] - '0')] = int32_t(i);
    }
    for (const auto& [name, frames] : sequences) {
      uint32_t count = 0;
      while (count < 10 && frames[count] >= 0) ++count;  // frames run from +0 without a gap
      if (count < 2) continue;
      const uint32_t first = uint32_t(s.frames.size());
      for (uint32_t k = 0; k < count; ++k) {
        const Placed& p = placed[size_t(frames[k])];
        s.frames.push_back({float(p.x), float(p.y), float(p.w), float(p.h)});
      }
      for (uint32_t k = 0; k < count; ++k) {
        // Each frame starts the sequence at its own place, as the map names it.
        anim_of[size_t(frames[k])] = first | (count << 16) | (k << 24);
      }
    }
  }

  // ── Which models are hidden (the triggers), by the entities that name them ──
  std::vector<bool> hidden(m.models.size(), false);
  for (const Entity& e : m.entities) {
    const std::string& model = e.get("model");
    if (model.size() > 1 && model[0] == '*' && e.get("classname").rfind("trigger_", 0) == 0) {
      const size_t i = size_t(std::atoi(model.c_str() + 1));
      if (i < hidden.size()) hidden[i] = true;
    }
  }
  std::vector<int32_t> model_of(m.surfaces.size(), 0);
  for (size_t mi = 0; mi < m.models.size(); ++mi) {
    const Model& md = m.models[mi];
    for (uint32_t k = 0; k < md.surface_count && md.first_surface + k < m.surfaces.size(); ++k)
      model_of[md.first_surface + k] = int32_t(mi);
  }

  // ── The vertices, a surface's own, and the liquids' tops ──
  for (int k = 0; k < 3; ++k) s.mins[k] = 1e30, s.maxs[k] = -1e30;
  struct Top {
    Kind kind;
    double level, box[4], area, centre[2];
  };
  std::vector<Top> tops;
  std::vector<Portal> faces;  // the teleporters' faces, merged into panels below
  s.face_runs.assign(m.surfaces.size(), {0, 0});
  for (size_t si = 0; si < m.surfaces.size(); ++si) {
    const Surface& sf = m.surfaces[si];
    Kind kind = kSolid;
    if (hidden[size_t(model_of[si])])
      kind = kHidden;
    else if (sf.kind == SurfaceKind::sky)
      kind = kSky;
    else if (sf.kind == SurfaceKind::liquid)
      kind = liquid_kind(m.textures[sf.texture].name);
    const Placed& p = placed[sf.texture];
    const LmPlace* lp = sf.lightmap >= 0 ? &lm_at[size_t(sf.lightmap)] : nullptr;
    const Lightmap* lmap = sf.lightmap >= 0 ? &m.lightmaps[size_t(sf.lightmap)] : nullptr;
    // The surface's vertices: the indices are fanned over a face's own run of vertices.
    uint32_t lo = UINT32_MAX, hi = 0;
    for (uint32_t i = sf.first; i < sf.first + sf.count; ++i) {
      lo = std::min(lo, m.indices[i]);
      hi = std::max(hi, m.indices[i]);
    }
    if (sf.count == 0) continue;
    const uint32_t base = uint32_t(s.vertices.size());
    double box[4] = {1e30, 1e30, -1e30, -1e30}, level = 0;
    bool up = true;
    for (uint32_t v = lo; v <= hi; ++v) {
      const Vertex& x = m.vertices[v];
      DrawVertex d{};
      double e[3];
      to_engine(x.pos, e);
      for (int k = 0; k < 3; ++k) d.pos[k] = float(e[k]);
      d.nrm[0] = x.nrm[0];
      d.nrm[1] = x.nrm[2];
      d.nrm[2] = x.nrm[1];
      d.st[0] = x.st[0] * float(scale_of[sf.texture]);
      d.st[1] = x.st[1] * float(scale_of[sf.texture]);
      if (lp && lmap) {
        d.lm[0] = (float(lp->x) + 1 + x.lm[0] * float(lmap->width)) / float(s.light.width);
        d.lm[1] = (float(lp->y) + 1 + x.lm[1] * float(lmap->height)) / float(s.light.height);
      } else {
        d.lm[0] = d.lm[1] = -1;  // no lightmap: lit full (a liquid, the sky)
      }
      d.rect[0] = float(p.x);
      d.rect[1] = float(p.y);
      d.rect[2] = float(p.w);
      d.rect[3] = float(p.h);
      d.kind = kind;
      d.source = v;
      d.lm2[0] = d.lm2[1] = -1;
      d.styles = 0xffffffffu;
      if (lp) {
        for (uint32_t k = 0; k < sf.styles; ++k)
          d.styles &= ~(uint32_t(255 - sf.style[k]) << (8 * k));
      }
      d.anim = anim_of[sf.texture];
      s.vertices.push_back(d);
      if (model_of[si] == 0) {
        for (int k = 0; k < 3; ++k)
          s.mins[k] = std::min(s.mins[k], e[k]), s.maxs[k] = std::max(s.maxs[k], e[k]);
      }
      box[0] = std::min(box[0], e[0]);
      box[1] = std::min(box[1], e[2]);
      box[2] = std::max(box[2], e[0]);
      box[3] = std::max(box[3], e[2]);
      level = e[1];
      up = up && d.nrm[1] > 0.95f;
    }
    double area = 0;
    s.face_runs[si] = {uint32_t(s.indices.size()), sf.count};
    for (uint32_t i = sf.first; i < sf.first + sf.count; ++i)
      s.indices.push_back(base + m.indices[i] - lo);
    for (uint32_t i = sf.first; i + 2 < sf.first + sf.count; i += 3) {
      const DrawVertex& a = s.vertices[base + m.indices[i] - lo];
      const DrawVertex& b = s.vertices[base + m.indices[i + 1] - lo];
      const DrawVertex& c = s.vertices[base + m.indices[i + 2] - lo];
      const double ux = b.pos[0] - a.pos[0], uz = b.pos[2] - a.pos[2];
      const double vx = c.pos[0] - a.pos[0], vz = c.pos[2] - a.pos[2];
      area += 0.5 * std::fabs(ux * vz - uz * vx);
    }
    s.counts[std::min<uint32_t>(kind, kKinds - 1)] += sf.count / 3;
    if (kind == kTele && base < s.vertices.size()) {
      // A teleporter's face: its box and the way it faces, level; panels merge after the loop.
      const float* nv = s.vertices[base].nrm;
      const double hl = std::sqrt(double(nv[0]) * nv[0] + double(nv[2]) * nv[2]);
      if (hl > 0.5) {
        Portal f;
        f.normal[0] = nv[0] / hl;
        f.normal[2] = nv[2] / hl;
        f.normal[1] = 0;
        double flo[3] = {1e30, 1e30, 1e30}, fhi[3] = {-1e30, -1e30, -1e30};
        for (uint32_t v = base; v < uint32_t(s.vertices.size()); ++v)
          for (int k = 0; k < 3; ++k) {
            flo[k] = std::min(flo[k], double(s.vertices[v].pos[k]));
            fhi[k] = std::max(fhi[k], double(s.vertices[v].pos[k]));
          }
        for (int k = 0; k < 3; ++k) f.centre[k] = 0.5 * (flo[k] + fhi[k]);
        f.width =
            std::abs(f.normal[2]) * (fhi[0] - flo[0]) + std::abs(f.normal[0]) * (fhi[2] - flo[2]);
        f.height = fhi[1] - flo[1];
        f.area = f.width * f.height;
        faces.push_back(f);
      }
    }
    if ((kind == kWater || kind == kSlime || kind == kLava) && up && model_of[si] == 0) {
      double cx = 0, cz = 0;
      for (uint32_t v = base; v < uint32_t(s.vertices.size()); ++v) {
        cx += s.vertices[v].pos[0];
        cz += s.vertices[v].pos[2];
      }
      const double nv = double(s.vertices.size() - base);
      // A top has the liquid under it and the air over it (the underside of a raised tank's
      // water faces up too, with the water over it).
      const auto contents = [&](double dy) {
        const float f[3] = {float(cx / nv / kUnitM), float(cz / nv / kUnitM),
                            float((level + dy) / kUnitM)};
        return m.leaves[leaf_at(m, f)].contents;
      };
      const int32_t below = contents(-0.1), above = contents(0.1);
      if (below > -3 || below < -5 || (above <= -3 && above >= -5)) continue;
      tops.push_back(Top{kind, level, {box[0], box[1], box[2], box[3]}, area, {cx / nv, cz / nv}});
    }
  }

  // ── The tops grouped: one kind, one level, boxes that touch ──
  std::vector<size_t> parent(tops.size());
  std::iota(parent.begin(), parent.end(), size_t{0});
  const auto root = [&](size_t i) {
    while (parent[i] != i) i = parent[i] = parent[parent[i]];
    return i;
  };
  for (size_t i = 0; i < tops.size(); ++i) {
    for (size_t j = i + 1; j < tops.size(); ++j) {
      const Top& a = tops[i];
      const Top& b = tops[j];
      if (a.kind != b.kind || std::fabs(a.level - b.level) > 0.02) continue;
      const double g = 0.05;
      if (a.box[0] > b.box[2] + g || b.box[0] > a.box[2] + g || a.box[1] > b.box[3] + g ||
          b.box[1] > a.box[3] + g)
        continue;
      parent[root(i)] = root(j);
    }
  }
  std::vector<int> group(tops.size(), -1);
  for (size_t i = 0; i < tops.size(); ++i) {
    const size_t r = root(i);
    if (group[r] < 0) {
      group[r] = int(s.liquids.size());
      LiquidPatch lp;
      lp.kind = tops[i].kind;
      lp.level = tops[i].level;
      lp.box[0] = lp.box[1] = 1e30;
      lp.box[2] = lp.box[3] = -1e30;
      s.liquids.push_back(lp);
    }
    LiquidPatch& lp = s.liquids[size_t(group[r])];
    lp.box[0] = std::min(lp.box[0], tops[i].box[0]);
    lp.box[1] = std::min(lp.box[1], tops[i].box[1]);
    lp.box[2] = std::max(lp.box[2], tops[i].box[2]);
    lp.box[3] = std::max(lp.box[3], tops[i].box[3]);
    if (tops[i].area > lp.largest) {
      lp.largest = tops[i].area;
      lp.centre[0] = tops[i].centre[0];
      lp.centre[1] = tops[i].centre[1];
    }
    lp.area += tops[i].area;
  }
  std::sort(s.liquids.begin(), s.liquids.end(),
            [](const LiquidPatch& a, const LiquidPatch& b) { return a.area > b.area; });

  // ── The teleporters' panels: faces facing one way, in one plane, touching or nearly ──
  {
    struct Box {
      Portal p;
      double lo[3], hi[3];
    };
    std::vector<Box> panels;
    for (const Portal& f : faces) {
      double lo[3], hi[3];
      const double half[3] = {0.5 * f.width * std::abs(f.normal[2]), 0.5 * f.height,
                              0.5 * f.width * std::abs(f.normal[0])};
      for (int k = 0; k < 3; ++k) lo[k] = f.centre[k] - half[k], hi[k] = f.centre[k] + half[k];
      Box* into = nullptr;
      for (Box& b : panels) {
        const double facing = b.p.normal[0] * f.normal[0] + b.p.normal[2] * f.normal[2];
        const double plane = (f.centre[0] - b.p.centre[0]) * f.normal[0] +
                             (f.centre[2] - b.p.centre[2]) * f.normal[2];
        bool near = facing > 0.9 && std::abs(plane) < 0.3;
        for (int k = 0; k < 3 && near; ++k) near = lo[k] < b.hi[k] + 0.5 && hi[k] > b.lo[k] - 0.5;
        if (near) {
          into = &b;
          break;
        }
      }
      if (!into) {
        panels.push_back({f, {lo[0], lo[1], lo[2]}, {hi[0], hi[1], hi[2]}});
        continue;
      }
      for (int k = 0; k < 3; ++k) {
        into->lo[k] = std::min(into->lo[k], lo[k]);
        into->hi[k] = std::max(into->hi[k], hi[k]);
      }
      into->p.area += f.area;
    }
    for (Box& b : panels) {
      Portal& p = b.p;
      for (int k = 0; k < 3; ++k) p.centre[k] = 0.5 * (b.lo[k] + b.hi[k]);
      p.width =
          std::abs(p.normal[2]) * (b.hi[0] - b.lo[0]) + std::abs(p.normal[0]) * (b.hi[2] - b.lo[2]);
      p.height = b.hi[1] - b.lo[1];
      s.portals.push_back(p);
    }
    std::sort(s.portals.begin(), s.portals.end(),
              [](const Portal& a, const Portal& b) { return a.area > b.area; });
  }

  // ── The lights and the start ──
  uint32_t n = 0;
  for (const Entity& e : m.entities) {
    const std::string cls = e.get("classname");
    double o[3];
    if (cls == "info_player_start" && vec3(e.get("origin"), o)) {
      float f[3] = {float(o[0]), float(o[1]), float(o[2] + 22)};
      to_engine(f, s.start);
      s.start_heading = heading_of(number(e.get("angle"), 0));
    }
    if (cls.rfind("light", 0) != 0 || !vec3(e.get("origin"), o)) continue;
    PointLight l;
    float f[3] = {float(o[0]), float(o[1]), float(o[2])};
    to_engine(f, l.pos);
    l.value = number(e.get("light"), 300);
    const double wait = std::max(number(e.get("wait"), 1), 0.1);
    l.reach = l.value / wait * kUnitM;
    l.seed = hash32(++n * 0x9e3779b1u);
    l.flame = cls.find("torch") != std::string::npos || cls.find("flame") != std::string::npos;
    if (l.flame) l.fire = cls.find("torch") != std::string::npos ? Fire::wall_torch : Fire::large;
    const int style = int(number(e.get("style"), 0));
    switch (style) {
      case 2:
      case 5:
      case 11:
        l.flicker = Flicker::pulse;
        break;
      case 4:
      case 9:
        l.flicker = Flicker::strobe;
        break;
      case 10:
        l.flicker = Flicker::buzz;
        break;
      case 1:
      case 3:
      case 6:
      case 7:
      case 8:
        l.flicker = Flicker::candle;
        break;
      default:
        l.flicker = l.flame ? Flicker::candle : Flicker::steady;
    }
    // A flame's light is a fire's (about 1900 K), a lamp's a warm bulb's (2900 K), a tube's cool.
    const double* c = nullptr;
    static const double kFlame[3] = {1.0, 0.56, 0.24}, kBulb[3] = {1.0, 0.8, 0.58},
                        kTube[3] = {0.86, 0.95, 1.0};
    c = l.flame ? kFlame : cls.find("fluoro") != std::string::npos ? kTube : kBulb;
    const double luma = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
    for (int k = 0; k < 3; ++k) l.colour[k] = c[k] / luma;
    s.lights.push_back(l);
  }
  return true;
}

std::vector<std::array<float, 3>> texture_means(const Scene& scene) {
  std::vector<std::array<float, 3>> out(scene.map.textures.size(), {0.5f, 0.5f, 0.5f});
  // A texture's atlas rectangle, from any vertex drawn with it.
  std::vector<int64_t> drawn(scene.map.vertices.size(), -1);
  for (size_t i = 0; i < scene.vertices.size(); ++i) {
    if (scene.vertices[i].source < drawn.size()) drawn[scene.vertices[i].source] = int64_t(i);
  }
  std::vector<bool> done(out.size(), false);
  const Atlas& a = scene.textures;
  if (a.levels.empty()) return out;
  for (const Surface& sf : scene.map.surfaces) {
    if (sf.count == 0 || sf.texture >= out.size() || done[sf.texture]) continue;
    const int64_t di = drawn[scene.map.indices[sf.first]];
    if (di < 0) continue;
    const float* r = scene.vertices[size_t(di)].rect;
    const auto x0 = uint32_t(r[0]), y0 = uint32_t(r[1]), w = uint32_t(r[2]), h = uint32_t(r[3]);
    double sum[3] = {0, 0, 0}, n = 0;
    for (uint32_t y = y0; y < std::min(y0 + h, a.height); ++y)
      for (uint32_t x = x0; x < std::min(x0 + w, a.width); ++x) {
        const uint8_t* t = &a.levels[0][(size_t(y) * a.width + x) * 4];
        for (int k = 0; k < 3; ++k) sum[k] += linear_of(t[k]);
        n += 1;
      }
    if (n > 0) {
      for (int k = 0; k < 3; ++k) out[sf.texture][size_t(k)] = float(sum[k] / n);
    }
    done[sf.texture] = true;
  }
  return out;
}

std::vector<bake::Light> bake_lights(const Scene& scene, double strength) {
  std::vector<bake::Light> out;
  for (const PointLight& l : scene.lights) {
    if (l.physical) continue;
    bake::Light b;
    for (int k = 0; k < 3; ++k) b.pos[k] = l.pos[k];
    // The real-time path's strength a metre off (bsp_draw.cpp, prepare_bsp): the classic's light
    // at half its reach, value / 2 of 128, matched by an inverse square.
    const double half = l.value * kUnitM / 2;
    const double at1 = l.value / 256.0 * half * half * strength;
    for (int k = 0; k < 3; ++k) b.colour[k] = l.colour[k] * at1;
    b.reach = l.reach * 1.5;
    b.radius = 0.2;
    out.push_back(b);
  }
  return out;
}

void attach_rebake(Scene& scene, Rebaked baked) {
  scene.rebaked = std::move(baked);
  const std::vector<float>& lm = scene.rebaked.lm;
  for (DrawVertex& v : scene.vertices) {
    const bool has = size_t(v.source) * 2 + 1 < lm.size();
    v.lm2[0] = has ? lm[size_t(v.source) * 2] : -1;
    v.lm2[1] = has ? lm[size_t(v.source) * 2 + 1] : -1;
  }
}

}  // namespace ge::brush::app
