// The map compiler's version 38 layout (parts.hpp): nineteen lumps from the compiled models.
//
// - Planes in pairs: canonical plane i written at 2i and its flip at 2i + 1, so a brush side's
//   oriented id (geom.hpp: canonical × 2 + flipped) is its plane number as it stands; nodes and
//   faces name the even one.
// - Nodes and leaves numbered model by model in pre-order, as the 29 layout does; every leaf is
//   one of its own (a node's child −(leaf + 1)), leaf 0 a solid one nothing names. A leaf lists
//   its marked faces and the brushes in it: the tree's own, and each clip brush whose bounds
//   overlap the leaf's.
// - Texinfo names its texture (31 characters at most) with the side's surface flags and value;
//   no animation chain.
// - RGB luxels: each grey luxel three times.
// - One cluster, which sees itself: every open leaf of the world is in it; one area.
//
// ge:summary Writes the compiled models as a version 38 map: plane pairs, brushes, RGB light
// ge:keywords map-compiler write v38 lumps brushes brushsides leafbrushes texinfo clusters
// ge:keywords rgb-light plane-pairs bevel
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include <array>
#include <cmath>
#include <cstring>
#include <map>

#include "brush/bytes.hpp"
#include "brush/compile/parts.hpp"

namespace ge::brush::mapc {

namespace {

using detail::Out;

// The family's contents bits for ours: solid (the sky's brushes too) 1, lava 8, slime 16, water
// 32, a clip brush the player's and the monsters' clip.
int32_t contents38(uint32_t c) {
  if (c & (kSolid | kSky)) return 0x1;
  if (c & kLava) return 0x8;
  if (c & kSlime) return 0x10;
  if (c & kWater) return 0x20;
  if (c & kClip) return 0x10000 | 0x20000;
  return 0;
}

int16_t clamp16(double v) { return int16_t(std::fmax(-32768.0, std::fmin(32767.0, v))); }

bool overlap(const V3& alo, const V3& ahi, const V3& blo, const V3& bhi) {
  for (size_t k = 0; k < 3; ++k) {
    if (ahi[k] <= blo[k] || bhi[k] <= alo[k]) return false;
  }
  return true;
}

}  // namespace

bool write_v38_map(const Layout& L, std::vector<uint8_t>* file, std::string* error,
                   CompileReport* report) {
  constexpr int kLumps = 19;
  std::array<Out, kLumps> lump;
  Out& entities = lump[0];
  Out& planes = lump[1];
  Out& vertexes = lump[2];
  Out& visibility = lump[3];
  Out& nodes = lump[4];
  Out& texinfo = lump[5];
  Out& faces = lump[6];
  Out& lighting = lump[7];
  Out& leafs = lump[8];
  Out& leaffaces = lump[9];
  Out& leafbrushes = lump[10];
  Out& edges = lump[11];
  Out& surfedges = lump[12];
  Out& models = lump[13];
  Out& brushes = lump[14];
  Out& brushsides = lump[15];
  Out& areas = lump[17];

  for (char ch : L.entities) entities.u8(uint8_t(ch));
  entities.u8(0);

  for (size_t i = 0; i < L.planes->size(); ++i) {
    for (int flipped = 0; flipped < 2; ++flipped) {
      const PlaneD p = L.planes->oriented(int(i) * 2 + flipped);
      for (double c : p.n) planes.f32(float(c));
      planes.f32(float(p.d));
      planes.i32(L.planes->type(int(i)));
    }
  }

  for (const TexInfo& t : L.texinfos) {
    for (const auto& row : t.vecs) {
      for (float v : row) texinfo.f32(v);
    }
    texinfo.i32(t.surf);
    texinfo.i32(t.value);
    texinfo.name(L.textures[t.texture].substr(0, 31), 32);
    texinfo.i32(-1);
  }

  // The brushes, every model's, numbered in order; a model's tree names its own by leaf_index.
  std::vector<std::map<int, uint32_t>> brush_of(L.models.size());
  std::vector<uint32_t> first_brush(L.models.size(), 0);
  uint32_t brush_total = 0, side_total = 0;
  for (size_t mi = 0; mi < L.models.size(); ++mi) {
    first_brush[mi] = brush_total;
    for (const BrushOut& b : L.models[mi].brushes) {
      if (b.leaf_index >= 0) brush_of[mi][b.leaf_index] = brush_total;
      brushes.i32(int32_t(side_total));
      brushes.i32(int32_t(b.planes.size()));
      brushes.i32(contents38(b.contents));
      for (size_t k = 0; k < b.planes.size(); ++k) {
        brushsides.u16(uint16_t(b.planes[k]));
        brushsides.i16(b.texinfos[k]);
      }
      side_total += uint32_t(b.planes.size());
      ++brush_total;
    }
  }

  // Corners and edges, shared.
  std::map<std::array<uint32_t, 3>, uint32_t> corner_of;
  std::map<std::pair<uint32_t, uint32_t>, uint32_t> edge_of;
  uint32_t corner_count = 0, edge_count = 1, surfedge_count = 0;
  edges.u16(0);
  edges.u16(0);
  const auto corner = [&](const V3& p) {
    const float f[3] = {float(p[0]), float(p[1]), float(p[2])};
    std::array<uint32_t, 3> key{};
    for (size_t k = 0; k < 3; ++k) std::memcpy(&key[k], &f[k], 4);
    auto it = corner_of.find(key);
    if (it != corner_of.end()) return it->second;
    for (float x : f) vertexes.f32(x);
    corner_of.emplace(key, corner_count);
    return corner_count++;
  };

  // Leaf 0: a solid leaf nothing names.
  leafs.i32(0x1);
  leafs.i16(-1);
  leafs.i16(0);
  for (int k = 0; k < 6; ++k) leafs.i16(0);
  for (int k = 0; k < 4; ++k) leafs.u16(0);

  size_t node_total = 0, leaf_total = 1, face_total = 0;
  uint32_t mark_total = 0, leafbrush_total = 0;
  struct NodeRec {
    int plane;
    int32_t child[2];
    V3 lo, hi;
    uint32_t first_face, faces;
  };
  std::vector<NodeRec> node_recs;
  std::vector<std::array<int32_t, 3>> model_recs;  // head, first face, faces

  for (size_t mi = 0; mi < L.models.size(); ++mi) {
    const ModelOut& m = L.models[mi];
    const Tree& t = *m.tree;
    std::vector<int32_t> node_num(t.nodes.size(), 0);
    std::vector<int> order;
    std::vector<int> stack = {t.head};
    while (!stack.empty()) {
      const int n = stack.back();
      stack.pop_back();
      const Node& node = t.nodes[size_t(n)];
      order.push_back(n);
      if (node.plane < 0) {
        node_num[size_t(n)] = -int32_t(leaf_total + 1);
        ++leaf_total;
        continue;
      }
      node_num[size_t(n)] = int32_t(node_total++);
      stack.push_back(node.child[1]);
      stack.push_back(node.child[0]);
    }
    std::vector<std::vector<int>> faces_of(t.nodes.size());
    for (size_t i = 0; i < t.faces.size(); ++i) faces_of[size_t(t.faces[i].node)].push_back(int(i));
    std::vector<int32_t> face_index(t.faces.size(), -1);
    const auto first_face = uint32_t(face_total);
    for (int n : order) {
      for (int fi : faces_of[size_t(n)]) face_index[size_t(fi)] = int32_t(face_total++);
    }
    for (int n : order) {
      const Node& node = t.nodes[size_t(n)];
      if (node.plane < 0) {
        int32_t contents = (node.contents & kSolid) ? 0x1 : contents38(node.contents);
        std::vector<uint32_t> in;
        for (int b : node.brushes) {
          auto it = brush_of[mi].find(b);
          if (it != brush_of[mi].end()) in.push_back(it->second);
        }
        for (size_t k = 0; k < m.brushes.size(); ++k) {  // the clip brushes: by their bounds
          const BrushOut& b = m.brushes[k];
          if (b.leaf_index >= 0 || !overlap(b.lo, b.hi, node.lo, node.hi)) continue;
          in.push_back(first_brush[mi] + uint32_t(k));
          contents |= contents38(b.contents);
        }
        const bool open = !(node.contents & kSolid);
        leafs.i32(contents);
        leafs.i16(open && mi == 0 ? 0 : -1);
        leafs.i16(open ? 1 : 0);
        for (size_t k = 0; k < 3; ++k) leafs.i16(clamp16(std::floor(node.lo[k])));
        for (size_t k = 0; k < 3; ++k) leafs.i16(clamp16(std::ceil(node.hi[k])));
        leafs.u16(uint16_t(mark_total));
        leafs.u16(uint16_t(node.faces.size()));
        leafs.u16(uint16_t(leafbrush_total));
        leafs.u16(uint16_t(in.size()));
        for (int fi : node.faces) leaffaces.u16(uint16_t(face_index[size_t(fi)]));
        for (uint32_t b : in) leafbrushes.u16(uint16_t(b));
        mark_total += uint32_t(node.faces.size());
        leafbrush_total += uint32_t(in.size());
        continue;
      }
      NodeRec r{};
      r.plane = node.plane * 2;
      for (int k = 0; k < 2; ++k) r.child[k] = node_num[size_t(node.child[k])];
      r.lo = node.lo;
      r.hi = node.hi;
      const auto& fl = faces_of[size_t(n)];
      r.first_face = fl.empty() ? 0 : uint32_t(face_index[size_t(fl[0])]);
      r.faces = uint32_t(fl.size());
      node_recs.push_back(r);
    }
    std::vector<int> by_new(t.faces.size());
    for (size_t i = 0; i < t.faces.size(); ++i) by_new[size_t(face_index[i]) - first_face] = int(i);
    for (int fi : by_new) {
      const Face& f = t.faces[size_t(fi)];
      const uint32_t first_edge = surfedge_count;
      const size_t nv = f.w.size();
      std::vector<uint32_t> cs;
      for (size_t k = nv; k-- > 0;) cs.push_back(corner(f.w[k]));
      for (size_t k = 0; k < nv; ++k) {
        const uint32_t a = cs[k], b = cs[(k + 1) % nv];
        auto it = edge_of.find({b, a});
        if (it != edge_of.end()) {
          surfedges.i32(-int32_t(it->second));
          edge_of.erase(it);
        } else {
          edges.u16(uint16_t(a));
          edges.u16(uint16_t(b));
          edge_of[{a, b}] = edge_count;
          surfedges.i32(int32_t(edge_count++));
        }
        ++surfedge_count;
      }
      faces.u16(uint16_t(f.plane * 2));
      faces.i16(f.back ? 1 : 0);
      faces.i32(int32_t(first_edge));
      faces.i16(int32_t(nv));
      faces.i16(f.texinfo);
      for (uint8_t s : f.styles) faces.u8(s);
      if (f.luxels.empty()) {
        faces.i32(-1);
      } else {
        faces.i32(int32_t(lighting.b.size()));
        for (uint8_t g : f.luxels) {
          for (int k = 0; k < 3; ++k) lighting.u8(g);
        }
      }
    }
    model_recs.push_back(
        {node_num[size_t(t.head)], int32_t(first_face), int32_t(face_total - first_face)});
  }

  for (const NodeRec& r : node_recs) {
    nodes.i32(r.plane);
    for (int32_t ch : r.child) nodes.i32(ch);
    for (size_t k = 0; k < 3; ++k) nodes.i16(clamp16(std::floor(r.lo[k])));
    for (size_t k = 0; k < 3; ++k) nodes.i16(clamp16(std::ceil(r.hi[k])));
    nodes.u16(uint16_t(r.first_face));
    nodes.u16(uint16_t(r.faces));
  }

  for (size_t i = 0; i < L.models.size(); ++i) {
    const ModelOut& m = L.models[i];
    for (size_t k = 0; k < 3; ++k) models.f32(float(m.lo[k] - 1));
    for (size_t k = 0; k < 3; ++k) models.f32(float(m.hi[k] + 1));
    for (int k = 0; k < 3; ++k) models.f32(0);
    for (int32_t v : model_recs[i]) models.i32(v);
  }

  // One cluster that sees itself: the count, its two rows' offsets, the one-byte row.
  visibility.i32(1);
  visibility.i32(12);
  visibility.i32(12);
  visibility.u8(1);

  // Area 0 is never used; every open leaf is in area 1, with no area portals.
  for (int k = 0; k < 4; ++k) areas.i32(0);

  const auto over = [&](const char* what, size_t n, size_t cap) {
    if (n <= cap) return false;
    *error = std::string("the map has ") + std::to_string(n) + " " + what + ", past the " +
             std::to_string(cap) + " a version 38 map can hold";
    return true;
  };
  if (over("faces", face_total, 65535) || over("corners", corner_count, 65535) ||
      over("leaves", leaf_total, 65535) || over("marked faces", mark_total, 65535) ||
      over("leaf brushes", leafbrush_total, 65535) || over("brushes", brush_total, 65535) ||
      over("planes", L.planes->size() * 2, 65535)) {
    return false;
  }

  Out head;
  head.name("IBSP", 4);
  head.i32(38);
  const size_t dir = head.b.size();
  for (int k = 0; k < kLumps; ++k) {
    head.i32(0);
    head.i32(0);
  }
  for (int k = 0; k < kLumps; ++k) {
    head.align4();
    head.put_i32(dir + size_t(k) * 8, int32_t(head.b.size()));
    head.put_i32(dir + size_t(k) * 8 + 4, int32_t(lump[size_t(k)].b.size()));
    head.bytes(lump[size_t(k)].b);
  }
  *file = std::move(head.b);
  report->nodes = node_total;
  report->leaves = leaf_total;
  report->faces = face_total;
  report->clip_nodes = 0;
  return true;
}

}  // namespace ge::brush::mapc
