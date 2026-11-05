// The brush world's writer: convex brushes laid out as a version 38 map with no faces, its tree
// cut where the brushes' axial sides lie, so the brush trace and point contents read it as they
// read a compiled map.
//
// - **Planes** come in pairs, 2n facing one way and 2n + 1 the other: an axial plane's even one
//   faces along +axis (type 0..2), a slanted one's along its first non-zero component (type 3 +
//   its largest component's axis). A side names whichever of the pair faces out of its brush.
// - **The tree**, model by model: a region (the model's brushes' bounds, grown by 64) is split by
//   the axial side plane strictly inside it nearest its middle, on its longest axis that has one,
//   until no brush side cuts it; nodes always name the even (positive) plane. A leaf lists every
//   brush whose bounds touch it (closed: a brush touching a leaf's face is listed), and its
//   contents are the OR of the brushes holding its centre: exact for box brushes, whose sides all
//   cut the tree; a slanted brush's leaves take it by their centres.
// - Leaf 0 is a solid leaf no node names (a reader's convention); every other solid leaf is in no
//   cluster and area 0, an open one in cluster 0 and area 1; there is no visibility lump, so
//   every cluster sees every other.
// - **Texinfo**: one a distinct (texture, flags, value), its projection zero (nothing is drawn).
//
// ge:summary Writes a brush world as a version 38 map with no faces: planes, tree, brushes, leaves
// ge:keywords write brush-world v38 brush brushside leafbrush tree split plane pair box_brush
// ge:keywords add_side fixture collision trace texinfo
// ge:skill brush
// ge:test tests/brush_trace_test.cpp
#include <array>
#include <bit>
#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "brush/bytes.hpp"
#include "brush/write.hpp"

namespace ge::brush {

namespace {

using detail::Out;

struct WPlane {
  float n[3];
  float d;
  int32_t type;
};

bool same_bits(float a, float b) {
  return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}

int axial_axis(const float n[3]) {
  for (int k = 0; k < 3; ++k) {
    if ((n[k] == 1 || n[k] == -1) && n[(k + 1) % 3] == 0 && n[(k + 2) % 3] == 0) return k;
  }
  return -1;
}

struct Compile {
  std::vector<WPlane> planes;
  struct NodeRec {
    uint32_t plane;
    int32_t child[2];
    float lo[3], hi[3];
  };
  struct LeafRec {
    int32_t contents;
    std::vector<uint32_t> brushes;
    float lo[3], hi[3];
  };
  std::vector<NodeRec> nodes;
  std::vector<LeafRec> leaves;
  // Every brush of every model, in order: its bounds and its sides as written.
  std::vector<const WorldBrush*> brushes;
  std::vector<std::array<float, 6>> bounds;  // lo xyz, hi xyz

  // The pair index of a plane; the side's own orientation is pair + (1 when it faces the odd way).
  uint32_t plane_of(const float n[3], float d) {
    const int axis = axial_axis(n);
    bool flip;
    if (axis >= 0) {
      flip = n[axis] < 0;
    } else {
      int first = 0;
      while (first < 3 && n[first] == 0) ++first;
      flip = first < 3 && n[first] < 0;
    }
    float cn[3];
    for (int k = 0; k < 3; ++k) cn[k] = flip ? -n[k] : n[k];
    const float cd = flip ? -d : d;
    for (size_t i = 0; i < planes.size(); i += 2) {
      const WPlane& p = planes[i];
      if (same_bits(p.n[0], cn[0]) && same_bits(p.n[1], cn[1]) && same_bits(p.n[2], cn[2]) &&
          same_bits(p.d, cd))
        return uint32_t(i) + (flip ? 1 : 0);
    }
    int32_t type = axis;
    if (axis < 0) {
      int big = 0;
      for (int k = 1; k < 3; ++k) {
        if (std::fabs(cn[k]) > std::fabs(cn[big])) big = k;
      }
      type = 3 + big;
    }
    const auto at = uint32_t(planes.size());
    planes.push_back({{cn[0], cn[1], cn[2]}, cd, type});
    planes.push_back({{-cn[0], -cn[1], -cn[2]}, -cd, type});
    return at + (flip ? 1 : 0);
  }

  bool holds(uint32_t b, const float p[3]) const {
    for (const WorldSide& s : brushes[b]->sides) {
      if (s.n[0] * p[0] + s.n[1] * p[1] + s.n[2] * p[2] - s.d > 0) return false;
    }
    return true;
  }

  int32_t build(const std::vector<uint32_t>& in, const float lo[3], const float hi[3]) {
    // The brushes touching the region (closed bounds).
    std::vector<uint32_t> touch;
    for (uint32_t b : in) {
      const auto& bb = bounds[b];
      bool t = true;
      for (int k = 0; k < 3 && t; ++k) t = bb[size_t(k)] <= hi[k] && bb[size_t(k) + 3] >= lo[k];
      if (t) touch.push_back(b);
    }
    // The split: an axial side strictly inside, nearest the middle of the longest such axis.
    int axis = -1;
    float at = 0;
    float best_len = -1;
    for (int k = 0; k < 3; ++k) {
      const float len = hi[k] - lo[k];
      if (len <= best_len) continue;
      const float mid = lo[k] + len * 0.5f;
      bool found = false;
      float pick = 0;
      for (uint32_t b : touch) {
        for (const float c : {bounds[b][size_t(k)], bounds[b][size_t(k) + 3]}) {
          if (c > lo[k] && c < hi[k] && (!found || std::fabs(c - mid) < std::fabs(pick - mid))) {
            pick = c;
            found = true;
          }
        }
      }
      if (found) {
        axis = k;
        at = pick;
        best_len = len;
      }
    }
    if (axis < 0) {
      LeafRec leaf;
      float centre[3];
      for (int k = 0; k < 3; ++k) {
        centre[k] = lo[k] + (hi[k] - lo[k]) * 0.5f;
        leaf.lo[k] = lo[k];
        leaf.hi[k] = hi[k];
      }
      leaf.contents = 0;
      for (uint32_t b : touch) {
        if (holds(b, centre)) leaf.contents |= brushes[b]->contents;
      }
      leaf.brushes = touch;
      leaves.push_back(std::move(leaf));
      return -int32_t(leaves.size());  // the leaf just added, as a child: -(index + 1)
    }
    const float n[3] = {axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f};
    const uint32_t plane = plane_of(n, at);
    const auto index = int32_t(nodes.size());
    NodeRec rec{};
    rec.plane = plane;
    for (int k = 0; k < 3; ++k) {
      rec.lo[k] = lo[k];
      rec.hi[k] = hi[k];
    }
    nodes.push_back(rec);
    float flo[3] = {lo[0], lo[1], lo[2]}, bhi[3] = {hi[0], hi[1], hi[2]};
    flo[axis] = at;
    bhi[axis] = at;
    const int32_t front = build(touch, flo, hi);
    const int32_t back = build(touch, lo, bhi);
    nodes[size_t(index)].child[0] = front;
    nodes[size_t(index)].child[1] = back;
    return index;
  }
};

void put_bounds(Out& o, const float lo[3], const float hi[3]) {
  const auto clamp = [](float v) {
    const float r = v < -32767 ? -32767.0f : v > 32767 ? 32767.0f : v;
    return int32_t(r);
  };
  for (int k = 0; k < 3; ++k) o.i16(clamp(lo[k]));
  for (int k = 0; k < 3; ++k) o.i16(clamp(hi[k]));
}

}  // namespace

WorldBrush box_brush(const float lo[3], const float hi[3], int32_t contents,
                     const std::string& texture) {
  WorldBrush b;
  b.contents = contents;
  for (int k = 0; k < 3; ++k) {
    WorldSide pos, neg;
    pos.n[k] = 1;
    pos.d = hi[k];
    neg.n[k] = -1;
    neg.d = -lo[k];
    pos.texture = neg.texture = texture;
    b.sides.push_back(pos);
    b.sides.push_back(neg);
  }
  return b;
}

void add_side(WorldBrush* b, const float n[3], float d, const std::string& texture) {
  const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  WorldSide s;
  for (int k = 0; k < 3; ++k) s.n[k] = n[k] / len;
  s.d = d;
  s.texture = texture;
  b->sides.push_back(s);
}

bool write_brush_world(const BrushWorld& world, std::vector<uint8_t>* out, std::string* error) {
  const auto fail = [&](const std::string& what) {
    if (error) *error = "brush world: " + what;
    return false;
  };
  if (world.models.empty()) return fail("no models (model 0 is the world); next: add the world");
  Compile c;
  // Leaf 0: the solid leaf no node names.
  c.leaves.push_back({1, {}, {0, 0, 0}, {0, 0, 0}});
  for (size_t mi = 0; mi < world.models.size(); ++mi) {
    for (size_t bi = 0; bi < world.models[mi].size(); ++bi) {
      const WorldBrush& b = world.models[mi][bi];
      std::array<float, 6> bb{};
      int have = 0;
      for (const WorldSide& s : b.sides) {
        const int a = axial_axis(s.n);
        if (a < 0) continue;
        if (s.n[a] > 0) {
          bb[size_t(a) + 3] = s.d;
          have |= 1 << (a + 3);
        } else {
          bb[size_t(a)] = -s.d;
          have |= 1 << a;
        }
      }
      if (have != 63) {
        return fail("model " + std::to_string(mi) + " brush " + std::to_string(bi) +
                    " lacks one of its six axial sides; next: build it with box_brush");
      }
      c.brushes.push_back(&b);
      c.bounds.push_back(bb);
    }
  }
  std::vector<int32_t> heads;
  std::vector<std::array<float, 6>> model_bounds;
  uint32_t first = 0;
  for (const auto& model : world.models) {
    std::vector<uint32_t> list;
    float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
    for (uint32_t k = 0; k < model.size(); ++k) {
      const auto& bb = c.bounds[first + k];
      for (int a = 0; a < 3; ++a) {
        lo[a] = k == 0 ? bb[size_t(a)] : std::fmin(lo[a], bb[size_t(a)]);
        hi[a] = k == 0 ? bb[size_t(a) + 3] : std::fmax(hi[a], bb[size_t(a) + 3]);
      }
      list.push_back(first + k);
    }
    first += uint32_t(model.size());
    model_bounds.push_back({lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]});
    float glo[3], ghi[3];
    for (int a = 0; a < 3; ++a) {
      glo[a] = lo[a] - 64;
      ghi[a] = hi[a] + 64;
    }
    heads.push_back(c.build(list, glo, ghi));
  }
  // Every side's plane, in brush order.
  std::vector<uint32_t> side_plane;
  for (const WorldBrush* b : c.brushes) {
    for (const WorldSide& s : b->sides) side_plane.push_back(c.plane_of(s.n, s.d));
  }
  if (c.planes.size() > 65535 || c.brushes.size() > 65535 || side_plane.size() > 65535)
    return fail("more than 65535 planes, brushes or sides; next: build a smaller world");

  constexpr int kLumps = 19;
  std::array<Out, kLumps> L;
  Out& entities = L[0];
  Out& planes = L[1];
  Out& nodes = L[4];
  Out& texinfo = L[5];
  Out& leafs = L[8];
  Out& leafbrushes = L[10];
  Out& edges = L[11];
  Out& models = L[13];
  Out& brushes = L[14];
  Out& brushsides = L[15];
  Out& areas = L[17];

  std::string text = "{\n\"classname\" \"worldspawn\"\n}\n";
  for (const Entity& e : world.entities) {
    text += "{\n";
    for (const auto& [k, v] : e.pairs) {
      text += '"';
      text += k;
      text += "\" \"";
      text += v;
      text += "\"\n";
    }
    text += "}\n";
  }
  for (char ch : text) entities.u8(uint8_t(ch));
  entities.u8(0);

  for (const WPlane& p : c.planes) {
    for (float x : p.n) planes.f32(x);
    planes.f32(p.d);
    planes.i32(p.type);
  }

  for (const Compile::NodeRec& n : c.nodes) {
    nodes.i32(int32_t(n.plane));
    for (int32_t ch : n.child) nodes.i32(ch);
    put_bounds(nodes, n.lo, n.hi);
    nodes.u16(0);
    nodes.u16(0);
  }

  // Texinfo: one a distinct surface.
  std::map<std::tuple<std::string, int32_t, int32_t>, int32_t> surfaces;
  std::vector<int32_t> side_texinfo;
  for (const WorldBrush* b : c.brushes) {
    for (const WorldSide& s : b->sides) {
      if (s.texture.empty()) {
        side_texinfo.push_back(-1);
        continue;
      }
      const auto [it, added] =
          surfaces.emplace(std::make_tuple(s.texture, s.flags, s.value), int32_t(surfaces.size()));
      if (added) {
        for (int k = 0; k < 8; ++k) texinfo.f32(0);
        texinfo.i32(s.flags);
        texinfo.i32(s.value);
        texinfo.name(s.texture, 32);
        texinfo.i32(-1);
      }
      side_texinfo.push_back(it->second);
    }
  }

  uint32_t leafbrush_count = 0;
  for (size_t i = 0; i < c.leaves.size(); ++i) {
    const Compile::LeafRec& l = c.leaves[i];
    const bool solid = (l.contents & 1) != 0;
    leafs.i32(l.contents);
    leafs.i16(solid ? -1 : 0);
    leafs.i16(solid ? 0 : 1);
    put_bounds(leafs, l.lo, l.hi);
    leafs.u16(0);
    leafs.u16(0);
    leafs.u16(uint16_t(leafbrush_count));
    leafs.u16(uint16_t(l.brushes.size()));
    for (uint32_t b : l.brushes) leafbrushes.u16(uint16_t(b));
    leafbrush_count += uint32_t(l.brushes.size());
  }
  if (leafbrush_count > 65535 || c.leaves.size() > 65535)
    return fail("more than 65535 leaves or leaf brushes; next: build a smaller world");

  for (size_t mi = 0; mi < world.models.size(); ++mi) {
    const auto& bb = model_bounds[mi];
    for (float x : bb) models.f32(x);
    for (int k = 0; k < 3; ++k) models.f32(0);
    models.i32(heads[mi]);
    models.i32(0);
    models.i32(0);
  }

  uint32_t side_at = 0;
  for (const WorldBrush* b : c.brushes) {
    brushes.i32(int32_t(side_at));
    brushes.i32(int32_t(b->sides.size()));
    brushes.i32(b->contents);
    for (size_t k = 0; k < b->sides.size(); ++k) {
      brushsides.u16(uint16_t(side_plane[side_at + k]));
      brushsides.i16(side_texinfo[side_at + k]);
    }
    side_at += uint32_t(b->sides.size());
  }

  // Edge 0 is never named; it and its vertex are there because a reader checks every edge.
  edges.u32(0);
  for (int k = 0; k < 3; ++k) L[2].f32(0);
  for (int k = 0; k < 4; ++k) areas.i32(0);

  Out file;
  file.name("IBSP", 4);
  file.i32(38);
  const size_t dir = file.b.size();
  for (int k = 0; k < kLumps; ++k) {
    file.i32(0);
    file.i32(0);
  }
  for (int k = 0; k < kLumps; ++k) {
    file.align4();
    file.put_i32(dir + size_t(k) * 8, int32_t(file.b.size()));
    file.put_i32(dir + size_t(k) * 8 + 4, int32_t(L[size_t(k)].b.size()));
    file.bytes(L[size_t(k)].b);
  }
  *out = std::move(file.b);
  return true;
}

}  // namespace ge::brush
