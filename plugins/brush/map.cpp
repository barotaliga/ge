// The map view's queries: a point's leaf down a model's tree, one cluster seeing or hearing
// another, a point's contents in a clip hull, the engine's frame, the view's digest, and a liquid's
// kind by its name.
//
// ge:summary The map view's queries: leaf_at, cluster_sees, to_engine, map_digest, family names
// ge:keywords leaf_at point leaf tree walk cluster_sees pvs to_engine frame digest fnv phs
// ge:keywords build_phs cluster_hears hull_contents clip-hull liquid_by_name liquid-kind
// ge:skill brush
// ge:test tests/brush_read_test.cpp
#include <algorithm>
#include <cstddef>
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

const std::string& Entity::get_any_case(const std::string& key) const {
  const std::string& exact = get(key);
  if (!exact.empty()) return exact;
  const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
  for (const auto& [k, v] : pairs) {
    if (k.size() != key.size()) continue;
    bool same = true;
    for (size_t i = 0; i < k.size() && same; i++) same = lower(k[i]) == lower(key[i]);
    if (same) return v;
  }
  return exact;
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

uint32_t box_leaves(const Map& map, const float lo[3], const float hi[3], uint32_t* out,
                    uint32_t most) {
  if (map.nodes.empty()) {
    if (most > 0 && !map.leaves.empty()) out[0] = 0;
    return map.leaves.empty() ? 0 : 1;
  }
  // A stack of nodes still to walk; a tree deeper than it is cut short (a checked map never is).
  int32_t stack[256];
  int top = 0;
  uint32_t found = 0;
  stack[top++] = map.models.empty() ? 0 : map.models[0].head_node;
  while (top > 0) {
    int32_t at = stack[--top];
    for (size_t steps = 0; at >= 0 && steps <= map.nodes.size(); ++steps) {
      const Node& n = map.nodes[size_t(at)];
      const Plane& pl = map.planes[n.plane];
      // The box's nearest and farthest corners along the plane's normal.
      float near = 0, far = 0;
      for (int k = 0; k < 3; k++) {
        const float a = pl.n[k] * lo[k], b = pl.n[k] * hi[k];
        near += a < b ? a : b;
        far += a < b ? b : a;
      }
      if (near - pl.d >= 0) {
        at = n.child[0];
      } else if (far - pl.d < 0) {
        at = n.child[1];
      } else {
        if (top < 256) stack[top++] = n.child[1];
        at = n.child[0];
      }
    }
    if (at >= 0) continue;  // a loop in the tree: nothing reached
    if (found < most) out[found] = uint32_t(-(int64_t(at) + 1));
    found++;
  }
  return found;
}

void clusters_seen_from(const Map& map, const float p[3], float slack, std::vector<uint8_t>& seen) {
  seen.assign(map.row_bytes, 0);
  if (map.pvs.empty() || map.clusters <= 0) {
    seen.assign(map.row_bytes, 0xFF);
    return;
  }
  const float lo[3] = {p[0] - slack, p[1] - slack, p[2] - slack};
  const float hi[3] = {p[0] + slack, p[1] + slack, p[2] + slack};
  uint32_t leaves[64];
  const uint32_t n = std::min<uint32_t>(box_leaves(map, lo, hi, leaves, 64), 64);
  bool any = false;
  for (uint32_t i = 0; i < n; i++) {
    const int32_t c = leaves[i] < map.leaves.size() ? map.leaves[leaves[i]].cluster : -1;
    if (c < 0 || c >= map.clusters) continue;
    any = true;
    const uint8_t* row = &map.pvs[size_t(c) * map.row_bytes];
    for (uint32_t b = 0; b < map.row_bytes; b++) seen[b] |= row[b];
  }
  if (!any) seen.assign(map.row_bytes, 0xFF);
}

void clusters_heard_from(const Map& map, const float p[3], std::vector<uint8_t>& heard) {
  const uint32_t leaf = leaf_at(map, p);
  const int32_t c = leaf < map.leaves.size() ? map.leaves[leaf].cluster : -1;
  if (map.phs.empty() || c < 0 || c >= map.clusters) {
    heard.assign(map.row_bytes, 0xFF);
    return;
  }
  heard.assign(map.phs.begin() + std::ptrdiff_t(size_t(c) * map.row_bytes),
               map.phs.begin() + std::ptrdiff_t(size_t(c + 1) * map.row_bytes));
}

uint32_t second_view_leaf(const Map& map, const float p[3], uint32_t eye_leaf) {
  if (map.family != Family::v38 || eye_leaf >= map.leaves.size()) return ~0u;
  const Leaf& eye = map.leaves[eye_leaf];
  const float q[3] = {p[0], p[1], p[2] + (eye.contents != 0 ? 16.0f : -16.0f)};
  const uint32_t l = leaf_at(map, q);
  if (l >= map.leaves.size() || map.leaves[l].solid || map.leaves[l].cluster == eye.cluster)
    return ~0u;
  return l;
}

uint32_t potentially_visible(const Map& map, uint32_t leaf, std::vector<uint8_t>* seen,
                             std::span<const uint8_t> areas, uint32_t also) {
  const int32_t from = leaf < map.leaves.size() ? map.leaves[leaf].cluster : -1;
  const int32_t from2 = also < map.leaves.size() ? map.leaves[also].cluster : -1;
  const bool every = from < 0 || map.pvs.empty();
  if (every && areas.empty()) {
    seen->assign(map.surfaces.size(), 1);
    return uint32_t(map.surfaces.size());
  }
  const auto area_open = [&](int32_t a) {
    return a >= 0 && size_t(a) / 8 < areas.size() && ((areas[size_t(a) / 8] >> (a % 8)) & 1) != 0;
  };
  seen->assign(map.surfaces.size(), 0);
  uint32_t n = 0;
  for (const Leaf& l : map.leaves) {
    if (l.marks == 0 ||
        (!every && !cluster_sees(map, from, l.cluster) && !cluster_sees(map, from2, l.cluster)))
      continue;
    if (!areas.empty() && (l.solid || !area_open(l.area))) continue;
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
  for (const Plane& p : map.planes) {
    mix(p.n, sizeof p.n);
    mix(&p.d, sizeof p.d);
  }
  for (const Node& n : map.nodes) mix(&n, sizeof n);
  for (const Leaf& l : map.leaves) {
    mix_u(uint64_t(int64_t(l.contents)));
    mix_u(uint64_t(int64_t(l.cluster)));
    mix(l.mins, sizeof l.mins);
    mix(l.maxs, sizeof l.maxs);
    if (l.brushes) {  // brushes a leaf lists (38): absent from a map without them
      mix_u(l.first_brush);
      mix_u(l.brushes);
    }
  }
  for (const Brush& b : map.brushes) {
    mix_u(b.first_side);
    mix_u(b.sides);
    mix_u(uint64_t(int64_t(b.contents)));
  }
  for (const BrushSide& s : map.brush_sides) {
    mix_u(s.plane);
    mix_u(uint64_t(int64_t(s.texture)));
    mix_u(uint64_t(int64_t(s.flags)));
    mix_u(uint64_t(int64_t(s.value)));
  }
  mix(map.leaf_brushes.data(), map.leaf_brushes.size() * 4);
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
