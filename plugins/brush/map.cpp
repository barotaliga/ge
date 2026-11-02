// The map view's queries: a point's leaf down a model's tree, one cluster seeing or hearing
// another, a point's contents in a clip hull, the engine's frame, the view's digest, and a liquid's
// kind by its name.
//
// ge:summary The map view's queries: leaf_at, cluster_sees, to_engine, map_digest, family names
// ge:keywords leaf_at point leaf tree walk cluster_sees pvs to_engine frame digest fnv phs
// ge:keywords build_phs cluster_hears hull_contents clip-hull liquid_by_name liquid-kind
// ge:skill brush
// ge:test tests/brush_read_test.cpp
#include <cstring>
#include <string>

#include "brush/map.hpp"

namespace ge::brush {

Liquid liquid_by_name(std::string_view name) {
  std::string n(name);
  for (char& c : n) {
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  if (n.find("lava") != std::string::npos) return Liquid::lava;
  if (n.find("slime") != std::string::npos) return Liquid::slime;
  if (n.find("water") != std::string::npos) return Liquid::water;
  if (n.find("tele") != std::string::npos) return Liquid::teleport;
  return Liquid::other;
}

const char* liquid_name(Liquid l) {
  switch (l) {
    case Liquid::none:
      return "none";
    case Liquid::water:
      return "water";
    case Liquid::slime:
      return "slime";
    case Liquid::lava:
      return "lava";
    case Liquid::teleport:
      return "teleport";
    case Liquid::other:
      return "other";
  }
  return "none";
}

const char* family_name(Family f) {
  switch (f) {
    case Family::v29:
      return "29";
    case Family::bsp2:
      return "BSP2";
    case Family::v38:
      return "38";
    case Family::v46:
      return "46";
  }
  return "?";
}

const std::string& Entity::get(const std::string& key) const {
  static const std::string kNone;
  for (const auto& [k, v] : pairs) {
    if (k == key) return v;
  }
  return kNone;
}

uint32_t leaf_at(const Map& map, const float p[3], uint32_t model) {
  int32_t at = model < map.models.size() ? map.models[model].head_node : 0;
  // The reader checked every child is in range; a tree that loops still ends here.
  for (size_t steps = 0; at >= 0 && steps <= map.nodes.size(); ++steps) {
    const Node& n = map.nodes[size_t(at)];
    const Plane& pl = map.planes[n.plane];
    const float d = pl.n[0] * p[0] + pl.n[1] * p[1] + pl.n[2] * p[2] - pl.d;
    at = n.child[d >= 0 ? 0 : 1];
  }
  return at < 0 ? uint32_t(-(int64_t(at) + 1)) : 0;
}

bool cluster_sees(const Map& map, int32_t from, int32_t to) {
  if (from < 0 || to < 0 || from >= map.clusters || to >= map.clusters) return false;
  if (map.pvs.empty()) return true;
  const uint8_t byte = map.pvs[size_t(from) * map.row_bytes + size_t(to) / 8];
  return (byte >> (to % 8)) & 1;
}

uint32_t potentially_visible(const Map& map, uint32_t leaf, std::vector<uint8_t>* seen) {
  const int32_t from = leaf < map.leaves.size() ? map.leaves[leaf].cluster : -1;
  if (from < 0 || map.pvs.empty()) {
    seen->assign(map.surfaces.size(), 1);
    return uint32_t(map.surfaces.size());
  }
  seen->assign(map.surfaces.size(), 0);
  uint32_t n = 0;
  for (const Leaf& l : map.leaves) {
    if (l.marks == 0 || !cluster_sees(map, from, l.cluster)) continue;
    for (uint32_t k = l.first_mark; k < l.first_mark + l.marks; ++k) {
      uint8_t& s = (*seen)[map.marks[k]];
      n += s == 0;
      s = 1;
    }
  }
  return n;
}

void build_phs(Map* map) {
  Map& m = *map;
  if (!m.phs.empty() || m.pvs.empty() || m.clusters <= 0) return;
  const size_t rows = size_t(m.clusters), bytes = m.row_bytes;
  m.phs.assign(rows * bytes, 0);
  for (size_t a = 0; a < rows; ++a) {
    const uint8_t* seen = m.pvs.data() + a * bytes;
    uint8_t* out = m.phs.data() + a * bytes;
    for (size_t b = 0; b < rows; ++b) {
      if (!((seen[b / 8] >> (b % 8)) & 1)) continue;
      const uint8_t* far = m.pvs.data() + b * bytes;
      for (size_t k = 0; k < bytes; ++k) out[k] |= far[k];
    }
  }
}

bool cluster_hears(const Map& map, int32_t from, int32_t to) {
  if (from < 0 || to < 0 || from >= map.clusters || to >= map.clusters) return false;
  if (map.phs.empty()) return true;
  const uint8_t byte = map.phs[size_t(from) * map.row_bytes + size_t(to) / 8];
  return (byte >> (to % 8)) & 1;
}

int32_t hull_contents(const Map& map, const float p[3], uint32_t model, int hull) {
  if (hull == 0 || map.clip_nodes.empty() || model >= map.models.size()) {
    return map.leaves[leaf_at(map, p, model)].contents;
  }
  int32_t at = map.models[model].hulls[hull - 1];
  // The reader refused a clip hull that loops, so this walk ends.
  while (at >= 0) {
    const ClipNode& n = map.clip_nodes[size_t(at)];
    const Plane& pl = map.planes[n.plane];
    const float d = pl.n[0] * p[0] + pl.n[1] * p[1] + pl.n[2] * p[2] - pl.d;
    at = n.child[d >= 0 ? 0 : 1];
  }
  return at;
}

void to_engine(const float p[3], double out[3]) {
  out[0] = double(p[0]) * kUnitM;
  out[1] = double(p[2]) * kUnitM;
  out[2] = double(p[1]) * kUnitM;
}

uint64_t map_digest(const Map& map) {
  uint64_t h = 0xcbf29ce484222325ull;
  const auto mix = [&](const void* data, size_t n) {
    const auto* b = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < n; ++i) {
      h ^= b[i];
      h *= 0x100000001b3ull;
    }
  };
  const auto mix_u = [&](uint64_t v) { mix(&v, sizeof v); };
  mix_u(uint64_t(map.family));
  for (const Vertex& v : map.vertices) mix(&v, sizeof v);
  mix(map.indices.data(), map.indices.size() * 4);
  for (const Surface& s : map.surfaces) {
    mix_u(s.texture);
    mix_u(s.first);
    mix_u(s.count);
    mix_u(uint64_t(int64_t(s.lightmap)));
    mix_u(uint64_t(s.kind));
  }
  for (const Texture& t : map.textures) {
    mix(t.name.data(), t.name.size());
    mix_u(t.width);
    mix_u(t.height);
    mix_u(t.indexed.size());
    mix(t.indexed.data(), t.indexed.size());
  }
  for (const Lightmap& l : map.lightmaps) {
    mix_u(l.width);
    mix_u(l.height);
    mix_u(l.offset);
  }
  mix(map.light_rgb.data(), map.light_rgb.size());
  for (const Plane& p : map.planes) mix(&p, sizeof p);
  for (const Node& n : map.nodes) mix(&n, sizeof n);
  for (const Leaf& l : map.leaves) {
    mix_u(uint64_t(int64_t(l.contents)));
    mix_u(uint64_t(int64_t(l.cluster)));
    mix(l.mins, sizeof l.mins);
    mix(l.maxs, sizeof l.maxs);
  }
  mix(map.pvs.data(), map.pvs.size());
  for (const ClipNode& n : map.clip_nodes) mix(&n, sizeof n);
  for (const Model& md : map.models) mix(md.hulls, sizeof md.hulls);
  mix(map.entity_text.data(), map.entity_text.size());
  return h;
}

const BspxLump* bspx_lump(const Map& map, std::string_view name) {
  for (const BspxLump& l : map.bspx) {
    if (l.name.size() != name.size()) continue;
    bool same = true;
    for (size_t k = 0; k < name.size() && same; ++k) {
      const auto low = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
      same = low(l.name[k]) == low(name[k]);
    }
    if (same) return &l;
  }
  return nullptr;
}

}  // namespace ge::brush
