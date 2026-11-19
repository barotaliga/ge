// The map compiler's version 29 layout (parts.hpp): fifteen lumps from the compiled models.
//
// - Nodes are written model by model, each model's tree in pre-order, so a model's head is its
//   first node; its faces follow its nodes' order, so each node's and each model's faces are one
//   run. Every solid leaf is leaf 0 (child −1); every other leaf has a number of its own, the
//   world's first, so its leaves are the visible ones (1 .. visleafs).
// - Corners and edges are shared: a corner once per exact float position, an edge once per pair
//   of corners, a face naming it backwards with a negative surfedge.
// - A face's corners are written clockwise seen from where it faces (the family's winding).
// - No visibility rows: every leaf's offset is −1 (sees all).
//
// ge:summary Writes the compiled models as a version 29 map: planes, miptex, faces, hulls, leaves
// ge:keywords map-compiler write v29 lumps miptex texinfo faces edges surfedges clipnodes leafs
// ge:keywords marksurfaces models entities pattern
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>

#include "brush/bytes.hpp"
#include "brush/compile/parts.hpp"

namespace ge::brush::mapc {

namespace {

using detail::Out;

int32_t leaf_contents(uint32_t c) {
  if (c & kSolid) return -2;
  if (c & kSky) return -6;
  if (c & kLava) return -5;
  if (c & kSlime) return -4;
  if (c & kWater) return -3;
  return -1;
}

int16_t clamp16(double v) { return int16_t(std::fmax(-32768.0, std::fmin(32767.0, v))); }

uint64_t name_hash(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (char c : s) h = (h ^ uint8_t(c)) * 1099511628211ull;
  return h;
}

}  // namespace

TextureImage pattern_image(const std::string& name) {
  const bool sky = name.rfind("sky", 0) == 0;
  TextureImage img;
  img.width = sky ? 256 : 64;
  img.height = sky ? 128 : 64;
  const uint64_t h = name_hash(name);
  const auto a = uint8_t(16 + 16 * (h % 12)), b = uint8_t(a + 4 + (h >> 8) % 6);
  for (uint32_t level = 0; level < 4; ++level) {
    const uint32_t w = img.width >> level, hgt = img.height >> level, cell = 16u >> level;
    img.levels[level].resize(size_t(w) * hgt);
    for (uint32_t y = 0; y < hgt; ++y) {
      for (uint32_t x = 0; x < w; ++x) {
        img.levels[level][size_t(y) * w + x] = ((x / cell + y / cell) % 2) ? a : b;
      }
    }
  }
  return img;
}

bool write_v29_map(const Layout& L, std::vector<uint8_t>* file, std::string* error,
                   CompileReport* report) {
  constexpr int kLumps = 15;
  std::array<Out, kLumps> lump;
  Out& entities = lump[0];
  Out& planes = lump[1];
  Out& textures = lump[2];
  Out& vertexes = lump[3];
  Out& nodes = lump[5];
  Out& texinfo = lump[6];
  Out& faces = lump[7];
  Out& lighting = lump[8];
  Out& clipnodes = lump[9];
  Out& leafs = lump[10];
  Out& marksurfaces = lump[11];
  Out& edges = lump[12];
  Out& surfedges = lump[13];
  Out& models = lump[14];

  for (char ch : L.entities) entities.u8(uint8_t(ch));
  entities.u8(0);

  for (size_t i = 0; i < L.planes->size(); ++i) {
    const PlaneD& p = L.planes->canonical(int(i));
    for (double c : p.n) planes.f32(float(c));
    planes.f32(float(p.d));
    planes.i32(L.planes->type(int(i)));
  }

  textures.i32(int32_t(L.textures.size()));
  {
    uint32_t at = 4 + 4 * uint32_t(L.textures.size());
    std::vector<Out> recs(L.textures.size());
    for (size_t k = 0; k < L.textures.size(); ++k) {
      const TextureImage& img = L.images[k];
      Out& o = recs[k];
      o.name(L.textures[k].substr(0, 15), 16);
      o.u32(img.width);
      o.u32(img.height);
      uint32_t off = 40;
      for (const auto& level : img.levels) {
        o.u32(off);
        off += uint32_t(level.size());
      }
      for (const auto& level : img.levels) o.bytes(level);
      textures.i32(int32_t(at));
      at += uint32_t(o.b.size());
    }
    for (const Out& o : recs) textures.bytes(o.b);
  }

  for (const TexInfo& t : L.texinfos) {
    for (const auto& row : t.vecs) {
      for (float v : row) texinfo.f32(v);
    }
    texinfo.i32(int32_t(t.texture));
    texinfo.i32(t.flags);
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

  // Numbering: nodes pre-order model by model, leaves in the same walk.
  size_t node_total = 0, leaf_total = 1, face_total = 0, mark_total = 0;
  std::vector<int32_t> face_index;  // a model's faces' new numbers
  struct NodeRec {
    int plane;
    int32_t child[2];
    V3 lo, hi;
    uint32_t first_face, faces;
  };
  std::vector<NodeRec> node_recs;
  struct LeafRec {
    int32_t contents;
    V3 lo, hi;
    std::vector<int32_t> marks;
  };
  std::vector<LeafRec> leaf_recs;
  std::vector<std::array<int32_t, 4>> heads;
  std::vector<std::array<uint32_t, 3>> model_counts;  // visleafs, first face, faces
  uint32_t clip_total = 0;

  for (const ModelOut& m : L.models) {
    const Tree& t = *m.tree;
    std::vector<int32_t> node_num(t.nodes.size(), 0);
    std::vector<int> order;
    // Pre-order: nodes numbered, leaves numbered.
    std::vector<int> stack = {t.head};
    const size_t first_leaf = leaf_total;
    while (!stack.empty()) {
      const int n = stack.back();
      stack.pop_back();
      const Node& node = t.nodes[size_t(n)];
      if (node.plane < 0) {
        if (node.contents & kSolid) {
          node_num[size_t(n)] = -1;
        } else {
          node_num[size_t(n)] = -int32_t(leaf_total + 1);
          ++leaf_total;
        }
        order.push_back(n);
        continue;
      }
      node_num[size_t(n)] = int32_t(node_total++);
      order.push_back(n);
      stack.push_back(node.child[1]);
      stack.push_back(node.child[0]);
    }
    // Faces in node order.
    std::vector<std::vector<int>> faces_of(t.nodes.size());
    for (size_t i = 0; i < t.faces.size(); ++i) faces_of[size_t(t.faces[i].node)].push_back(int(i));
    face_index.assign(t.faces.size(), -1);
    const auto first_face = uint32_t(face_total);
    for (int n : order) {
      for (int fi : faces_of[size_t(n)]) face_index[size_t(fi)] = int32_t(face_total++);
    }
    for (int n : order) {
      const Node& node = t.nodes[size_t(n)];
      if (node.plane < 0) {
        if (node.contents & kSolid) continue;
        LeafRec r{leaf_contents(node.contents), node.lo, node.hi, {}};
        for (int fi : node.faces) r.marks.push_back(face_index[size_t(fi)]);
        mark_total += r.marks.size();
        leaf_recs.push_back(std::move(r));
        continue;
      }
      NodeRec r{};
      r.plane = node.plane;
      for (int k = 0; k < 2; ++k) r.child[k] = node_num[size_t(node.child[k])];
      r.lo = node.lo;
      r.hi = node.hi;
      const auto& fl = faces_of[size_t(n)];
      r.first_face = fl.empty() ? 0 : uint32_t(face_index[size_t(fl[0])]);
      r.faces = uint32_t(fl.size());
      node_recs.push_back(r);
    }
    // The faces themselves, in their new order.
    std::vector<int> by_new(t.faces.size());
    for (size_t i = 0; i < t.faces.size(); ++i) by_new[size_t(face_index[i]) - first_face] = int(i);
    for (int fi : by_new) {
      const Face& f = t.faces[size_t(fi)];
      const uint32_t first_edge = surfedge_count;
      const size_t nv = f.w.size();
      // Clockwise seen from where it faces: the counter-clockwise winding backwards.
      std::vector<uint32_t> cs;
      for (size_t k = nv; k-- > 0;) cs.push_back(corner(f.w[k]));
      for (size_t k = 0; k < nv; ++k) {
        const uint32_t a = cs[k], b = cs[(k + 1) % nv];
        auto it = edge_of.find({b, a});
        if (it != edge_of.end()) {
          surfedges.i32(-int32_t(it->second));
          edge_of.erase(it);  // an edge serves two faces at most
        } else {
          edges.u16(uint16_t(a));
          edges.u16(uint16_t(b));
          edge_of[{a, b}] = edge_count;
          surfedges.i32(int32_t(edge_count++));
        }
        ++surfedge_count;
      }
      faces.i16(f.plane);
      faces.i16(f.back ? 1 : 0);
      faces.i32(int32_t(first_edge));
      faces.i16(int32_t(nv));
      faces.i16(f.texinfo);
      for (uint8_t s : f.styles) faces.u8(s);
      if (f.luxels.empty()) {
        faces.i32(-1);
      } else {
        faces.i32(int32_t(lighting.b.size()));
        lighting.bytes(f.luxels);
      }
    }
    std::array<int32_t, 4> head = {node_num[size_t(t.head)], 0, 0, 0};
    // A hull's clip nodes in pre-order, its root first: a server takes the root's number as the
    // hull's first clip node and refuses a trace into any node numbered below it.
    for (int h = 0; h < 2; ++h) {
      const Hull& hull = m.hulls[h];
      std::vector<int32_t> num(hull.nodes.size(), -1);
      std::vector<int> walk;
      std::vector<int> todo = {hull.head};
      while (!todo.empty() && !hull.nodes.empty()) {
        const int n = todo.back();
        todo.pop_back();
        if (n < 0 || num[size_t(n)] >= 0) continue;
        num[size_t(n)] = int32_t(walk.size());
        walk.push_back(n);
        todo.push_back(hull.nodes[size_t(n)].child[1]);
        todo.push_back(hull.nodes[size_t(n)].child[0]);
      }
      head[size_t(h) + 1] = int32_t(clip_total) + (hull.head < 0 ? hull.head : 0);
      for (int n : walk) {
        const ClipOut& c = hull.nodes[size_t(n)];
        clipnodes.i32(c.plane);
        for (int32_t ch : c.child)
          clipnodes.i16(ch >= 0 ? num[size_t(ch)] + int32_t(clip_total) : ch);
      }
      clip_total += uint32_t(walk.size());
    }
    heads.push_back(head);
    model_counts.push_back(
        {uint32_t(leaf_total - first_leaf), first_face, uint32_t(face_total - first_face)});
  }

  for (const NodeRec& r : node_recs) {
    nodes.i32(r.plane);
    for (int32_t ch : r.child) nodes.i16(ch);
    for (size_t k = 0; k < 3; ++k) nodes.i16(clamp16(std::floor(r.lo[k])));
    for (size_t k = 0; k < 3; ++k) nodes.i16(clamp16(std::ceil(r.hi[k])));
    nodes.u16(uint16_t(r.first_face));
    nodes.u16(uint16_t(r.faces));
  }
  // Leaf 0: every solid leaf.
  leafs.i32(-2);
  leafs.i32(-1);
  for (int k = 0; k < 6; ++k) leafs.i16(0);
  leafs.u16(0);
  leafs.u16(0);
  for (int k = 0; k < 4; ++k) leafs.u8(0);
  uint32_t mark_at = 0;
  for (const LeafRec& r : leaf_recs) {
    leafs.i32(r.contents);
    leafs.i32(-1);
    const bool any = r.lo[0] <= r.hi[0];
    for (size_t k = 0; k < 3; ++k) leafs.i16(any ? clamp16(std::floor(r.lo[k])) : 0);
    for (size_t k = 0; k < 3; ++k) leafs.i16(any ? clamp16(std::ceil(r.hi[k])) : 0);
    leafs.u16(uint16_t(mark_at));
    leafs.u16(uint16_t(r.marks.size()));
    const uint8_t water = (r.contents == -3 || r.contents == -4) ? 128 : 0;
    leafs.u8(water);
    for (int k = 0; k < 3; ++k) leafs.u8(0);
    for (int32_t m : r.marks) marksurfaces.u16(uint16_t(m));
    mark_at += uint32_t(r.marks.size());
  }

  for (size_t i = 0; i < L.models.size(); ++i) {
    const ModelOut& m = L.models[i];
    for (size_t k = 0; k < 3; ++k) models.f32(float(m.lo[k] - 1));
    for (size_t k = 0; k < 3; ++k) models.f32(float(m.hi[k] + 1));
    for (int k = 0; k < 3; ++k) models.f32(0);
    for (int32_t h : heads[i]) models.i32(h);
    models.i32(int32_t(model_counts[i][0]));
    models.i32(int32_t(model_counts[i][1]));
    models.i32(int32_t(model_counts[i][2]));
  }

  // The family's limits: 16-bit indices.
  const auto over = [&](const char* what, size_t n, size_t cap) {
    if (n <= cap) return false;
    *error = std::string("the map has ") + std::to_string(n) + " " + what + ", past the " +
             std::to_string(cap) + " a version 29 map can hold";
    return true;
  };
  if (over("nodes", node_total, 32767) || over("leaves", leaf_total, 32767) ||
      over("faces", face_total, 65535) || over("corners", corner_count, 65535) ||
      over("clip nodes", clip_total, 32767) || over("marked faces", mark_total, 65535) ||
      over("planes", L.planes->size(), 32767)) {
    return false;
  }

  Out head;
  head.i32(29);
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
  report->clip_nodes = clip_total;
  return true;
}

}  // namespace ge::brush::mapc
