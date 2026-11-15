// A built map brought back to a source (import.hpp).
//
// ge:summary Brings a built map back to a source: cells or brushes, surfaces' projections, keys
// ge:keywords import cells leaves half-space projection solve surface bucket brush-model
// ge:skill brush
// ge:test tests/brush_import_test.cpp
#include "brush/compile/import.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <tuple>
#include <vector>

#include "brush/compile/plan.hpp"

namespace ge::brush::mapc {

namespace {

// A surface's plane (from its largest triangle, facing where the triangle's winding shows it) and
// the projection its texels give.
struct SurfPlane {
  bool ok = false;
  V3 n{};
  double d = 0;
  double vecs[2][4] = {};
};

V3 pos(const Vertex& v) { return {double(v.pos[0]), double(v.pos[1]), double(v.pos[2])}; }

double det3(const V3& a, const V3& b, const V3& c) { return dot(a, cross(b, c)); }

// The axis x with x·e1 = r1, x·e2 = r2, x·n = 0 (Cramer's rule).
bool solve_axis(const V3& e1, const V3& e2, const V3& n, double r1, double r2, V3* out) {
  const double dm = det3(e1, e2, n);
  if (std::fabs(dm) < 1e-12) return false;
  // Rows e1, e2, n; the columns swapped for the right-hand side in turn.
  for (size_t c = 0; c < 3; ++c) {
    V3 a = e1, b = e2, m = n;
    a[c] = r1, b[c] = r2, m[c] = 0;
    (*out)[c] = det3(a, b, m) / dm;
  }
  return true;
}

SurfPlane surface_plane(const Map& map, const Surface& s) {
  SurfPlane sp;
  double best = 0;
  const Texture* tex = s.texture < map.textures.size() ? &map.textures[s.texture] : nullptr;
  // Texels: 29 and 38 store them; 46 stores whole textures, read here at 256 texels a texture.
  const double ks = map.st_texels ? 1 : (tex && tex->width ? double(tex->width) : 256);
  const double kt = map.st_texels ? 1 : (tex && tex->height ? double(tex->height) : 256);
  for (uint32_t t = 0; t + 2 < s.count; t += 3) {
    const Vertex* v[3];
    for (uint32_t k = 0; k < 3; ++k) v[k] = &map.vertices[map.indices[s.first + t + k]];
    const V3 p0 = pos(*v[0]), e1 = sub(pos(*v[1]), p0), e2 = sub(pos(*v[2]), p0);
    const V3 c = cross(e1, e2);
    const double a = dot(c, c);
    if (a <= best) continue;
    const V3 n = mul(c, 1 / std::sqrt(a));
    V3 sa{}, ta{};
    if (!solve_axis(e1, e2, n, double(v[1]->st[0] - v[0]->st[0]) * ks,
                    double(v[2]->st[0] - v[0]->st[0]) * ks, &sa) ||
        !solve_axis(e1, e2, n, double(v[1]->st[1] - v[0]->st[1]) * kt,
                    double(v[2]->st[1] - v[0]->st[1]) * kt, &ta))
      continue;
    best = a;
    sp.ok = true;
    sp.n = n;
    sp.d = dot(n, p0);
    for (size_t k = 0; k < 3; ++k) sp.vecs[0][k] = sa[k], sp.vecs[1][k] = ta[k];
    sp.vecs[0][3] = double(v[0]->st[0]) * ks - dot(sa, p0);
    sp.vecs[1][3] = double(v[0]->st[1]) * kt - dot(ta, p0);
  }
  return sp;
}

// A plane as a key: its normal and distance rounded (thousandths and tenths of a unit).
using Key = std::tuple<long, long, long, long>;
Key key_of(const V3& n, double d) {
  return {std::lround(n[0] * 1000), std::lround(n[1] * 1000), std::lround(n[2] * 1000),
          std::lround(d * 10)};
}

// How far `c` (on the surface's plane) is from the surface: 0 inside a triangle, else the
// distance to the nearest triangle's middle.
double distance_to_surface(const Map& map, const Surface& s, const V3& n, const V3& c) {
  double best = 1e30;
  for (uint32_t t = 0; t + 2 < s.count; t += 3) {
    V3 p[3];
    for (uint32_t k = 0; k < 3; ++k) p[k] = pos(map.vertices[map.indices[s.first + t + k]]);
    int plus = 0, minus = 0;
    for (size_t k = 0; k < 3; ++k) {
      const V3 e = sub(p[(k + 1) % 3], p[k]);
      const double el = std::max(length(e), 1e-12);
      const double sd = dot(cross(e, sub(c, p[k])), n) / el;
      plus += sd > 0.01;
      minus += sd < -0.01;
    }
    if (plus == 0 || minus == 0) return 0;
    const V3 m = mul(add(add(p[0], p[1]), p[2]), 1.0 / 3);
    best = std::min(best, length(sub(m, c)));
  }
  return best;
}

// A cell's contents in the source's terms from a leaf (the 29 family's numbers), 0 for empty.
uint32_t leaf_contents(const Leaf& l) {
  switch (l.contents) {
    case -2:
      return kSolid;
    case -3:
      return kWater;
    case -4:
      return kSlime;
    case -5:
      return kLava;
    case -6:
      return kSky;
    default:
      return 0;
  }
}

// A brush's contents bits (38, 46) in the source's terms.
uint32_t bits_contents(int32_t bits) {
  if (bits & 8) return kLava;
  if (bits & 16) return kSlime;
  if (bits & 32) return kWater;
  if (bits & 3) return kSolid;
  if (bits & 0x30000) return kClip;
  return 0;
}

const char* contents_texture(uint32_t c) {
  switch (c) {
    case kWater:
      return "*water";
    case kSlime:
      return "*slime";
    case kLava:
      return "*lava";
    case kSky:
      return "sky";
    case kClip:
      return "clip";
    default:
      return "";
  }
}

struct Step {
  uint32_t plane;
  uint8_t side;  // 0 the cell is in front of the plane, 1 behind
};

struct Cell {
  std::vector<Step> path;
  uint32_t contents;
};

int uniform_under(const Map& map, int32_t child, std::vector<int>& memo) {
  if (child < 0) {
    const uint32_t c = leaf_contents(map.leaves[size_t(-(child + 1))]);
    return c == 0 ? -1 : int(c);
  }
  int& u = memo[size_t(child)];
  if (u != -2) return u;
  const Node& n = map.nodes[size_t(child)];
  const int a = uniform_under(map, n.child[0], memo), b = uniform_under(map, n.child[1], memo);
  u = a >= 0 && a == b ? a : -1;
  return u;
}

void collect(const Map& map, int32_t child, std::vector<int>& memo, std::vector<Step>& path,
             std::vector<Cell>& cells) {
  if (child < 0) {
    const uint32_t c = leaf_contents(map.leaves[size_t(-(child + 1))]);
    if (c != 0) cells.push_back({path, c});
    return;
  }
  if (const int u = uniform_under(map, child, memo); u >= 0) {
    cells.push_back({path, uint32_t(u)});
    return;
  }
  const Node& n = map.nodes[size_t(child)];
  for (uint8_t side = 0; side < 2; ++side) {
    path.push_back({n.plane, side});
    collect(map, n.child[side], memo, path, cells);
    path.pop_back();
  }
}

void leaves_under(const Map& map, int32_t child, std::vector<uint32_t>& out) {
  if (child < 0) {
    out.push_back(uint32_t(-(child + 1)));
    return;
  }
  const Node& n = map.nodes[size_t(child)];
  leaves_under(map, n.child[0], out);
  leaves_under(map, n.child[1], out);
}

class Importer {
 public:
  explicit Importer(const Map& map) : map_(map) {
    for (uint32_t i = 0; i < map.surfaces.size(); ++i) {
      SurfPlane sp = surface_plane(map, map.surfaces[i]);
      if (!sp.ok) continue;
      buckets_[key_of(sp.n, sp.d)].push_back(i);
      planes_.emplace(i, sp);
    }
  }

  // Each side's texture and projection from the surface lying on it; the rest as `fill` or the
  // brush's most shown texture. Sides that leave no polygon are dropped. False when the brush
  // holds no volume.
  bool finish(Brush* b, const char* fill, ImportReport* rep) {
    if (!tidy_brush(b)) return false;
    std::map<std::string, int> shown;
    std::vector<bool> lay(b->sides.size(), false);
    for (size_t i = 0; i < b->sides.size(); ++i) {
      Side& s = b->sides[i];
      Winding w = base_winding(s.plane);
      for (size_t j = 0; j < b->sides.size() && !w.empty(); ++j)
        if (j != i) w = clip_behind(w, b->sides[j].plane);
      if (w.size() < 3) continue;
      const V3 c = centre(w);
      const auto it = buckets_.find(key_of(s.plane.n, s.plane.d));
      if (it == buckets_.end()) continue;
      double best = 1e30;
      uint32_t found = 0;
      for (uint32_t si : it->second) {
        const double dd = distance_to_surface(map_, map_.surfaces[si], s.plane.n, c);
        if (dd < best) best = dd, found = si;
      }
      // A surface on the plane but far from this side lies on another brush.
      if (best > 0.5 * std::sqrt(area(w)) + 1) continue;
      const Surface& sf = map_.surfaces[found];
      if (sf.texture >= map_.textures.size()) continue;
      if (s.texture.empty()) s.texture = map_.textures[sf.texture].name;
      const SurfPlane& sp = planes_.at(found);
      for (int r = 0; r < 2; ++r)
        for (int k = 0; k < 4; ++k) s.vecs[r][k] = sp.vecs[r][k];
      lay[i] = true;
      ++shown[s.texture];
      ++rep->textured_sides;
    }
    std::string most = fill;
    if (most.empty()) {
      int n = 0;
      for (const auto& [name, count] : shown)
        if (count > n) n = count, most = name;
      if (most.empty()) most = map_.textures.empty() ? "wall" : map_.textures[0].name;
    }
    for (size_t i = 0; i < b->sides.size(); ++i) {
      Side& s = b->sides[i];
      if (!lay[i] && s.texture.empty()) {
        s.texture = most;
        axial_vecs(s.plane.n, 0, 0, 0, 1, 1, s.vecs);
      }
    }
    rep->brushes++;
    rep->sides += int(b->sides.size());
    return true;
  }

  // The 29 family: a model's tree cells as brushes.
  void cells(const Model& m, bool world, std::vector<Brush>* out, ImportReport* rep) {
    std::vector<int> memo(map_.nodes.size(), -2);
    std::vector<Step> path;
    std::vector<Cell> cs;
    if (m.head_node < 0 || size_t(m.head_node) >= map_.nodes.size()) return;
    collect(map_, m.head_node, memo, path, cs);
    const double pad = world ? 64 : 1;
    for (const Cell& cell : cs) {
      Brush b;
      for (const Step& st : cell.path) {
        const Plane& p = map_.planes[st.plane];
        PlaneD pl{{double(p.n[0]), double(p.n[1]), double(p.n[2])}, double(p.d)};
        Side s;
        s.plane = st.side == 0 ? flip(pl) : pl;  // facing out of the cell
        b.sides.push_back(s);
      }
      // The model's box, a little out, closes the cells on its rim.
      for (size_t k = 0; k < 3; ++k) {
        Side lo, hi;
        lo.plane.n = {0, 0, 0}, hi.plane.n = {0, 0, 0};
        lo.plane.n[k] = -1, lo.plane.d = -(double(m.mins[k]) - pad);
        hi.plane.n[k] = 1, hi.plane.d = double(m.maxs[k]) + pad;
        b.sides.push_back(lo);
        b.sides.push_back(hi);
      }
      b.contents = cell.contents;
      if (finish(&b, contents_texture(cell.contents == kSolid ? 0 : cell.contents), rep))
        out->push_back(std::move(b));
    }
  }

  // The 38 and 46 families: a model's brushes as they were.
  void brushes(const Model& m, std::vector<Brush>* out, std::vector<bool>& taken,
               ImportReport* rep) {
    std::vector<uint32_t> leaves;
    if (m.head_node >= 0 && size_t(m.head_node) < map_.nodes.size())
      leaves_under(map_, m.head_node, leaves);
    for (uint32_t li : leaves) {
      const Leaf& l = map_.leaves[li];
      for (uint32_t k = 0; k < l.brushes; ++k) {
        const uint32_t bi = map_.leaf_brushes[l.first_brush + k];
        if (bi >= map_.brushes.size() || taken[bi]) continue;
        taken[bi] = true;
        const ge::brush::Brush& src = map_.brushes[bi];
        Brush b;
        b.contents = bits_contents(src.contents);
        if (b.contents == 0) continue;
        bool sky = false;
        for (uint32_t s = 0; s < src.sides; ++s) {
          const BrushSide& bs = map_.brush_sides[src.first_side + s];
          const Plane& p = map_.planes[bs.plane];
          Side side;
          side.plane = {{double(p.n[0]), double(p.n[1]), double(p.n[2])}, double(p.d)};
          if (bs.texture >= 0 && size_t(bs.texture) < map_.textures.size()) {
            side.texture = map_.textures[size_t(bs.texture)].name;
            sky = sky || map_.textures[size_t(bs.texture)].sky;
          }
          side.flags = bs.flags;
          side.value = bs.value;
          b.sides.push_back(side);
        }
        if (sky && b.contents == kSolid) b.contents = kSky;
        if (finish(&b, "", rep)) out->push_back(std::move(b));
      }
    }
  }

 private:
  const Map& map_;
  std::map<Key, std::vector<uint32_t>> buckets_;
  std::map<uint32_t, SurfPlane> planes_;
};

}  // namespace

bool import_map(const Map& map, Source* out, ImportReport* report, std::string* error) {
  ImportReport local;
  ImportReport* rep = report ? report : &local;
  *rep = {};
  out->entities.clear();
  if (map.models.empty()) {
    *error = "the map has no world model; next: pass a map the reader takes whole";
    return false;
  }
  Importer im(map);
  const bool has_brushes = !map.brushes.empty();
  std::vector<bool> taken(map.brushes.size(), false);
  const auto model_brushes = [&](size_t mi, std::vector<Brush>* bs) {
    if (has_brushes)
      im.brushes(map.models[mi], bs, taken, rep);
    else
      im.cells(map.models[mi], mi == 0, bs, rep);
  };
  // The world first: the entity text's first block, else one made.
  Entity world;
  size_t first = 0;
  if (!map.entities.empty() && map.entities[0].get("classname") == "worldspawn") {
    for (const auto& kv : map.entities[0].pairs) world.keys.push_back(kv);
    first = 1;
  } else {
    world.keys.emplace_back("classname", "worldspawn");
  }
  model_brushes(0, &world.brushes);
  out->entities.push_back(std::move(world));
  for (size_t i = first; i < map.entities.size(); ++i) {
    Entity e;
    size_t model = 0;
    for (const auto& [k, v] : map.entities[i].pairs) {
      if (k == "model" && v.size() > 1 && v[0] == '*') {
        model = size_t(std::strtoul(v.c_str() + 1, nullptr, 10));
        continue;
      }
      e.keys.emplace_back(k, v);
    }
    if (model > 0 && model < map.models.size()) {
      model_brushes(model, &e.brushes);
      rep->models++;
    }
    out->entities.push_back(std::move(e));
  }
  return true;
}

}  // namespace ge::brush::mapc
