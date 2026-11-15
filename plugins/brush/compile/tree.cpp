// The map compiler's tree (tree.hpp): pieces, the split, portals, the flood, faces.
//
// ge:summary Defines the brush BSP build, the portals, the flood fill and the faces from portals
// ge:keywords map-compiler bsp split-plane portal flood-fill leak faces merge subdivide
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include "brush/compile/tree.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>

namespace ge::brush::mapc {

namespace {

constexpr double kBrushEps = 0.1;  // a piece reaching less than this past a plane does not cross it

void piece_bounds(Piece* p) {
  p->lo = {1e30, 1e30, 1e30};
  p->hi = {-1e30, -1e30, -1e30};
  for (const Piece::PSide& s : p->sides) bounds_add(s.w, &p->lo, &p->hi);
}

// The least and greatest distance of a piece's corners from `pl`.
void reach(const Piece& p, const PlaneD& pl, double* lo, double* hi) {
  *lo = 1e30;
  *hi = -1e30;
  for (const Piece::PSide& s : p.sides) {
    for (const V3& v : s.w) {
      const double d = dist(pl, v);
      *lo = std::min(*lo, d);
      *hi = std::max(*hi, d);
    }
  }
}

// -1 behind, 1 in front, 0 cut.
int classify(const Piece& p, const PlaneD& pl, int type) {
  if (type < 3) {
    const auto k = size_t(type);
    if (p.lo[k] > pl.d - kBrushEps) return 1;
    if (p.hi[k] < pl.d + kBrushEps) return -1;
  }
  double lo = 0, hi = 0;
  reach(p, pl, &lo, &hi);
  if (lo > -kBrushEps) return 1;
  if (hi < kBrushEps) return -1;
  return 0;
}

bool closed(const Piece& p) { return p.sides.size() >= 4; }

// `p` cut by canonical plane `index`.
void split_piece(const Piece& p, int index, const Planes& planes, Piece* front, Piece* back,
                 bool* has_front, bool* has_back) {
  const PlaneD pl = planes.canonical(index);
  double lo = 0, hi = 0;
  reach(p, pl, &lo, &hi);
  *has_front = *has_back = false;
  if (hi < kBrushEps) {
    *back = p;
    *has_back = true;
    return;
  }
  if (lo > -kBrushEps) {
    *front = p;
    *has_front = true;
    return;
  }
  Winding mid = base_winding(pl);
  for (const Piece::PSide& s : p.sides) {
    mid = clip_behind(mid, planes.oriented(s.plane), 0);
    if (mid.empty()) break;
  }
  if (tiny(mid)) {
    if (hi > -lo) {
      *front = p;
      *has_front = true;
    } else {
      *back = p;
      *has_back = true;
    }
    return;
  }
  front->sides.clear();
  back->sides.clear();
  for (Piece* q : {front, back}) {
    q->contents = p.contents;
    q->brush = p.brush;
  }
  for (const Piece::PSide& s : p.sides) {
    Winding fw, bw;
    split_winding(s.w, pl, kOnEps, &fw, &bw);
    if (!tiny(fw)) front->sides.push_back({s.plane, std::move(fw), s.texinfo, s.tested});
    if (!tiny(bw)) back->sides.push_back({s.plane, std::move(bw), s.texinfo, s.tested});
  }
  front->sides.push_back({index * 2 + 1, reversed(mid), -1, true});
  back->sides.push_back({index * 2, std::move(mid), -1, true});
  if (closed(*front)) {
    piece_bounds(front);
    *has_front = true;
  }
  if (closed(*back)) {
    piece_bounds(back);
    *has_back = true;
  }
}

int choose_split(const std::vector<Piece>& pieces, const Planes& planes) {
  for (int pass = 0; pass < 2; ++pass) {
    std::vector<int> cands;
    for (const Piece& p : pieces) {
      for (const Piece::PSide& s : p.sides) {
        if (s.tested || (pass == 0 && s.texinfo < 0)) continue;
        cands.push_back(s.plane >> 1);
      }
    }
    if (cands.empty()) continue;
    std::sort(cands.begin(), cands.end());
    cands.erase(std::unique(cands.begin(), cands.end()), cands.end());
    // Axial planes first when there are any.
    std::vector<int> axial;
    for (int c : cands) {
      if (planes.type(c) < 3) axial.push_back(c);
    }
    if (!axial.empty() && pieces.size() > 64) cands = axial;
    std::vector<int> weigh = cands;
    if (pieces.size() > 64 && cands.size() > 48) {
      weigh.clear();
      for (size_t i = 0; i < 48; ++i) weigh.push_back(cands[i * cands.size() / 48]);
    }
    int best = -1;
    int64_t best_score = 0;
    for (int c : weigh) {
      const PlaneD pl = planes.canonical(c);
      const int type = planes.type(c);
      int64_t f = 0, b = 0, cut = 0;
      for (const Piece& p : pieces) {
        const int k = classify(p, pl, type);
        if (k > 0) {
          ++f;
        } else if (k < 0) {
          ++b;
        } else {
          ++cut;
        }
      }
      int64_t score = 5 * cut + std::llabs(f - b);
      if (type >= 3) score += 5;
      if (best < 0 || score < best_score) {
        best = c;
        best_score = score;
      }
    }
    return best;
  }
  return -1;
}

uint32_t strongest(uint32_t a, uint32_t b) { return std::max(a, b); }

int build(std::vector<Piece>& pieces, V3 lo, V3 hi, Planes* planes, Tree* tree) {
  const int id = int(tree->nodes.size());
  tree->nodes.emplace_back();
  tree->nodes[size_t(id)].lo = lo;
  tree->nodes[size_t(id)].hi = hi;
  const int split = choose_split(pieces, *planes);
  if (split < 0) {
    Node& leaf = tree->nodes[size_t(id)];
    for (const Piece& p : pieces) {
      leaf.contents = strongest(leaf.contents, p.contents);
      leaf.brushes.push_back(p.brush);
    }
    std::sort(leaf.brushes.begin(), leaf.brushes.end());
    leaf.brushes.erase(std::unique(leaf.brushes.begin(), leaf.brushes.end()), leaf.brushes.end());
    return id;
  }
  std::vector<Piece> fs, bs;
  for (const Piece& p : pieces) {
    Piece f, b;
    bool hf = false, hb = false;
    split_piece(p, split, *planes, &f, &b, &hf, &hb);
    if (hf) fs.push_back(std::move(f));
    if (hb) bs.push_back(std::move(b));
  }
  pieces.clear();
  pieces.shrink_to_fit();
  for (std::vector<Piece>* list : {&fs, &bs}) {
    for (Piece& p : *list) {
      for (Piece::PSide& s : p.sides) {
        if ((s.plane >> 1) == split) s.tested = true;
      }
    }
  }
  const int type = planes->type(split);
  V3 flo = lo, fhi = hi, blo = lo, bhi = hi;
  if (type < 3) {
    const double d = planes->canonical(split).d;
    flo[size_t(type)] = std::max(lo[size_t(type)], d);
    bhi[size_t(type)] = std::min(hi[size_t(type)], d);
  }
  tree->nodes[size_t(id)].plane = split;
  const int f = build(fs, flo, fhi, planes, tree);
  const int b = build(bs, blo, bhi, planes, tree);
  tree->nodes[size_t(id)].child[0] = f;
  tree->nodes[size_t(id)].child[1] = b;
  return id;
}

void remove_portal(Node* n, int p) {
  auto it = std::find(n->portals.begin(), n->portals.end(), p);
  if (it != n->portals.end()) n->portals.erase(it);
}

void portalize(const Planes& planes, Tree* tree, int node) {
  if (tree->nodes[size_t(node)].plane < 0) return;
  const int plane = tree->nodes[size_t(node)].plane;
  const PlaneD pl = planes.canonical(plane);
  const int f = tree->nodes[size_t(node)].child[0], b = tree->nodes[size_t(node)].child[1];
  // The new portal on the node's plane, cut by the portals bounding the node.
  Winding w = base_winding(pl);
  for (int pid : tree->nodes[size_t(node)].portals) {
    const Portal& p = tree->portals[size_t(pid)];
    const PlaneD pp = planes.canonical(p.plane);
    w = clip_behind(w, p.nodes[0] == node ? flip(pp) : pp, 0);
    if (w.empty()) break;
  }
  if (!tiny(w)) {
    const int id = int(tree->portals.size());
    tree->portals.push_back({plane, std::move(w), {f, b}, node});
    tree->nodes[size_t(f)].portals.push_back(id);
    tree->nodes[size_t(b)].portals.push_back(id);
  }
  // The node's portals split between its children.
  const std::vector<int> mine = tree->nodes[size_t(node)].portals;
  tree->nodes[size_t(node)].portals.clear();
  for (int pid : mine) {
    const int side = tree->portals[size_t(pid)].nodes[0] == node ? 0 : 1;
    const int other = tree->portals[size_t(pid)].nodes[1 - side];
    remove_portal(&tree->nodes[size_t(other)], pid);
    Winding fw, bw;
    split_winding(tree->portals[size_t(pid)].w, pl, kOnEps, &fw, &bw);
    const bool keep_f = !tiny(fw), keep_b = !tiny(bw);
    if (keep_f && keep_b) {
      Portal q = tree->portals[size_t(pid)];
      q.w = std::move(bw);
      q.nodes[side] = b;
      tree->portals[size_t(pid)].w = std::move(fw);
      tree->portals[size_t(pid)].nodes[side] = f;
      tree->nodes[size_t(f)].portals.push_back(pid);
      tree->nodes[size_t(other)].portals.push_back(pid);
      const int qid = int(tree->portals.size());
      tree->portals.push_back(std::move(q));
      tree->nodes[size_t(b)].portals.push_back(qid);
      tree->nodes[size_t(other)].portals.push_back(qid);
    } else if (keep_f || keep_b) {
      const int to = keep_f ? f : b;
      tree->portals[size_t(pid)].w = keep_f ? std::move(fw) : std::move(bw);
      tree->portals[size_t(pid)].nodes[side] = to;
      tree->nodes[size_t(to)].portals.push_back(pid);
      tree->nodes[size_t(other)].portals.push_back(pid);
    } else {
      tree->portals[size_t(pid)].nodes[0] = tree->portals[size_t(pid)].nodes[1] = -1;
    }
  }
  portalize(planes, tree, f);
  portalize(planes, tree, b);
}

// The side in `leaf` on oriented plane `id` facing out of its brush, among `brushes`.
int side_texinfo(const Node& leaf, const std::vector<BrushSides>& brushes, int id) {
  for (int pass = 0; pass < 2; ++pass) {
    for (int b : leaf.brushes) {
      const BrushSides& bs = brushes[size_t(b)];
      if (pass == 0 && bs.contents != leaf.contents) continue;
      for (size_t s = 0; s < bs.planes.size(); ++s) {
        if (bs.planes[s] == id && bs.texinfos[s] >= 0) return bs.texinfos[s];
      }
    }
  }
  return -1;
}

}  // namespace

std::vector<Piece> make_pieces(const std::vector<Brush>& brushes,
                               const std::vector<std::vector<int>>& texinfos, Planes* planes,
                               std::vector<BrushSides>* sides, std::vector<std::string>* warnings) {
  std::vector<Piece> out;
  sides->assign(brushes.size(), {});
  for (size_t b = 0; b < brushes.size(); ++b) {
    const Brush& br = brushes[b];
    Piece p;
    p.contents = br.contents;
    p.brush = int(b);
    std::vector<int> ids;
    ids.reserve(br.sides.size());
    for (const Side& s : br.sides) ids.push_back(planes->find(s.plane));
    for (size_t i = 0; i < br.sides.size(); ++i) {
      bool dup = false;
      for (size_t j = 0; j < i; ++j) dup = dup || ids[j] == ids[i];
      if (dup) continue;
      Winding w = base_winding(planes->oriented(ids[i]));
      for (size_t j = 0; j < br.sides.size() && !w.empty(); ++j) {
        if (j != i && ids[j] != ids[i]) w = clip_behind(w, planes->oriented(ids[j]), 0);
      }
      if (tiny(w)) continue;
      p.sides.push_back({ids[i], std::move(w), texinfos[b][i], false});
    }
    piece_bounds(&p);
    bool open = !closed(p);
    for (size_t k = 0; k < 3 && !open; ++k) open = p.hi[k] - p.lo[k] > kHuge || p.hi[k] <= p.lo[k];
    if (open) {
      std::string at;
      if (!p.sides.empty()) {
        const V3 c = centre(p.sides[0].w);
        at = " near (" + std::to_string(int64_t(c[0])) + " " + std::to_string(int64_t(c[1])) + " " +
             std::to_string(int64_t(c[2])) + ")";
      }
      warnings->push_back(
          "brush at line " + std::to_string(br.line) + at + " (" + std::to_string(p.sides.size()) +
          " of " + std::to_string(br.sides.size()) +
          " sides) does not close: its sides leave no solid between them; left out");
      continue;
    }
    BrushSides& bs = (*sides)[b];
    bs.contents = br.contents;
    for (const Piece::PSide& s : p.sides) {
      bs.planes.push_back(s.plane);
      bs.texinfos.push_back(s.texinfo);
    }
    out.push_back(std::move(p));
  }
  return out;
}

void build_tree(std::vector<Piece> pieces, const V3& lo, const V3& hi, Planes* planes, Tree* tree) {
  tree->nodes.clear();
  tree->lo = lo;
  tree->hi = hi;
  tree->head = build(pieces, lo, hi, planes, tree);
}

void make_portals(Planes* planes, Tree* tree) {
  tree->portals.clear();
  tree->outside = int(tree->nodes.size());
  Node out;
  out.outside = true;
  tree->nodes.push_back(out);
  for (size_t k = 0; k < 3; ++k) {
    for (int s = 0; s < 2; ++s) {
      PlaneD pl;
      pl.n = {0, 0, 0};
      pl.n[k] = 1;
      pl.d = s ? tree->hi[k] : tree->lo[k];
      Winding w = base_winding(pl);
      for (size_t j = 0; j < 3; ++j) {
        if (j == k) continue;
        PlaneD a;
        a.n = {0, 0, 0};
        a.n[j] = 1;
        a.d = tree->hi[j];
        w = clip_behind(w, a, 0);
        a.d = tree->lo[j];
        w = clip_behind(w, flip(a), 0);
      }
      Portal p;
      p.plane = planes->find(pl) >> 1;
      p.w = std::move(w);
      // The region is behind its top face and in front of its bottom one.
      p.nodes[0] = s ? tree->outside : tree->head;
      p.nodes[1] = s ? tree->head : tree->outside;
      const int id = int(tree->portals.size());
      tree->portals.push_back(std::move(p));
      tree->nodes[size_t(tree->head)].portals.push_back(id);
      tree->nodes[size_t(tree->outside)].portals.push_back(id);
    }
  }
  portalize(*planes, tree, tree->head);
}

int leaf_at(const Tree& tree, const Planes& planes, const V3& p) {
  int n = tree.head;
  while (tree.nodes[size_t(n)].plane >= 0) {
    const Node& node = tree.nodes[size_t(n)];
    n = node.child[dist(planes.canonical(node.plane), p) >= 0 ? 0 : 1];
  }
  return n;
}

bool flood_fill(const std::vector<Seed>& seeds, const Planes& planes, Tree* tree, Leak* leak) {
  std::deque<int> queue;
  for (const Seed& s : seeds) {
    const int l = leaf_at(*tree, planes, s.at);
    Node& leaf = tree->nodes[size_t(l)];
    if (opaque(leaf.contents) || leaf.reached >= 0) continue;
    leaf.reached = s.entity;
    queue.push_back(l);
  }
  if (queue.empty()) {
    leak->entity = -1;
    return false;
  }
  while (!queue.empty()) {
    const int l = queue.front();
    queue.pop_front();
    for (int pid : tree->nodes[size_t(l)].portals) {
      const Portal& p = tree->portals[size_t(pid)];
      const int o = p.nodes[0] == l ? p.nodes[1] : p.nodes[0];
      if (o < 0) continue;
      Node& other = tree->nodes[size_t(o)];
      if (opaque(other.contents) || other.reached >= 0) continue;
      other.reached = tree->nodes[size_t(l)].reached;
      other.via = pid;
      if (other.outside) {
        leak->entity = other.reached;
        leak->path.clear();
        int at = o;
        while (tree->nodes[size_t(at)].via >= 0) {
          const Portal& q = tree->portals[size_t(tree->nodes[size_t(at)].via)];
          leak->path.push_back(centre(q.w));
          at = q.nodes[0] == at ? q.nodes[1] : q.nodes[0];
        }
        std::reverse(leak->path.begin(), leak->path.end());
        return false;
      }
      queue.push_back(o);
    }
  }
  for (Node& n : tree->nodes) {
    if (n.plane < 0 && !n.outside && n.reached < 0 && !opaque(n.contents)) n.contents = kSolid;
  }
  return true;
}

void make_faces(const std::vector<BrushSides>& brushes, const std::vector<TexInfo>& texinfos,
                Tree* tree, size_t* missing) {
  (void)texinfos;
  tree->faces.clear();
  for (const Portal& p : tree->portals) {
    if (p.nodes[0] < 0 || p.onnode < 0) continue;
    const Node& a = tree->nodes[size_t(p.nodes[0])];
    const Node& b = tree->nodes[size_t(p.nodes[1])];
    const uint32_t ca = a.outside ? kEmpty : a.contents, cb = b.outside ? kEmpty : b.contents;
    if (ca == cb || (opaque(ca) && opaque(cb))) continue;
    // The stronger leaf's surface; its outward side on the portal's plane.
    const bool a_strong = ca > cb;
    const int strong = a_strong ? p.nodes[0] : p.nodes[1];
    const int weak = a_strong ? p.nodes[1] : p.nodes[0];
    const int outward = p.plane * 2 + (a_strong ? 1 : 0);  // pointing from strong to weak
    int ti = side_texinfo(tree->nodes[size_t(strong)], brushes, outward);
    if (ti < 0) {
      ti = side_texinfo(tree->nodes[size_t(strong)], brushes, outward ^ 1);
      if (ti < 0) {
        for (int bi : tree->nodes[size_t(strong)].brushes) {
          for (int t : brushes[size_t(bi)].texinfos) {
            if (ti < 0 && t >= 0) ti = t;
          }
        }
      }
      ++*missing;
      if (ti < 0) continue;
    }
    // A face facing from the strong leaf into the weak one; a liquid's also back into itself.
    const auto emit = [&](bool facing_plus, int into) {
      Face f;
      f.plane = p.plane;
      f.back = !facing_plus;
      f.texinfo = ti;
      f.w = facing_plus ? p.w : reversed(p.w);
      f.node = p.onnode;
      f.leaves.push_back(into);
      tree->faces.push_back(std::move(f));
    };
    const bool strong_front = a_strong;  // the strong leaf in front: facing −normal into the weak
    emit(!strong_front, weak);
    if (!opaque(a_strong ? ca : cb)) emit(strong_front, strong);
  }
}

namespace {

// The union of two convex windings sharing an edge (a's i→i+1 is b's j+1→j), if convex.
bool try_merge(const Winding& a, const Winding& b, const V3& n, Winding* out) {
  const size_t na = a.size(), nb = b.size();
  const auto same = [](const V3& x, const V3& y) {
    return std::fabs(x[0] - y[0]) < 0.01 && std::fabs(x[1] - y[1]) < 0.01 &&
           std::fabs(x[2] - y[2]) < 0.01;
  };
  for (size_t i = 0; i < na; ++i) {
    const V3& a0 = a[i];
    const V3& a1 = a[(i + 1) % na];
    for (size_t j = 0; j < nb; ++j) {
      if (!same(b[j], a1) || !same(b[(j + 1) % nb], a0)) continue;
      // a from a1 round to a0, then b from a0 (b[j+1]) round to a1 (b[j]), less the shared ends.
      Winding m;
      for (size_t k = 1; k < na; ++k) m.push_back(a[(i + k) % na]);  // a1 .. a0
      for (size_t k = 2; k < nb; ++k) m.push_back(b[(j + k) % nb]);  // past a0 .. before a1
      // Convex and with no corner in a straight line kept.
      Winding c;
      const size_t nm = m.size();
      for (size_t k = 0; k < nm; ++k) {
        const V3& p0 = m[(k + nm - 1) % nm];
        const V3& p1 = m[k];
        const V3& p2 = m[(k + 1) % nm];
        const double turn = dot(cross(sub(p1, p0), sub(p2, p1)), n);
        const double l = length(sub(p1, p0)) * length(sub(p2, p1));
        if (turn < -1e-6 * l) return false;
        if (turn > 1e-6 * l) c.push_back(p1);
      }
      if (c.size() < 3) return false;
      *out = std::move(c);
      return true;
    }
  }
  return false;
}

}  // namespace

void merge_faces(Tree* tree, const Planes& planes) {
  std::vector<std::vector<int>> by_node(tree->nodes.size());
  for (size_t i = 0; i < tree->faces.size(); ++i) {
    by_node[size_t(tree->faces[i].node)].push_back(int(i));
  }
  std::vector<char> dead(tree->faces.size(), 0);
  for (const std::vector<int>& list : by_node) {
    bool merged = true;
    while (merged) {
      merged = false;
      for (size_t x = 0; x < list.size(); ++x) {
        const int i = list[x];
        if (dead[size_t(i)]) continue;
        for (size_t y = x + 1; y < list.size(); ++y) {
          const int j = list[y];
          if (dead[size_t(j)]) continue;
          Face& fi = tree->faces[size_t(i)];
          const Face& fj = tree->faces[size_t(j)];
          if (fi.texinfo != fj.texinfo || fi.back != fj.back) continue;
          V3 n = planes.canonical(fi.plane).n;
          if (fi.back) n = mul(n, -1);
          Winding m;
          if (!try_merge(fi.w, fj.w, n, &m)) continue;
          fi.w = std::move(m);
          for (int l : fj.leaves) {
            if (std::find(fi.leaves.begin(), fi.leaves.end(), l) == fi.leaves.end()) {
              fi.leaves.push_back(l);
            }
          }
          dead[size_t(j)] = 1;
          merged = true;
        }
      }
    }
  }
  std::vector<Face> kept;
  for (size_t i = 0; i < tree->faces.size(); ++i) {
    if (!dead[i]) kept.push_back(std::move(tree->faces[i]));
  }
  tree->faces = std::move(kept);
}

void subdivide_faces(Tree* tree, const Planes& planes, const std::vector<TexInfo>& texinfos) {
  (void)planes;
  constexpr double kSpan = 240;
  std::vector<Face> out;
  std::vector<Face> work = std::move(tree->faces);
  while (!work.empty()) {
    Face f = std::move(work.back());
    work.pop_back();
    const TexInfo& ti = texinfos[size_t(f.texinfo)];
    bool cut = false;
    if (!(ti.flags & 1)) {
      for (int a = 0; a < 2; ++a) {
        const V3 v = {ti.vecs[a][0], ti.vecs[a][1], ti.vecs[a][2]};
        const double len = length(v);
        if (!(len > 0)) continue;
        double lo = 1e30, hi = -1e30;
        for (const V3& p : f.w) {
          const double s = dot(v, p) + ti.vecs[a][3];
          lo = std::min(lo, s);
          hi = std::max(hi, s);
        }
        if (hi - lo <= kSpan) continue;
        // Cut at lo + 240 texels, on a 16-texel line so the halves' lightmaps line up.
        const double at = std::floor((lo + kSpan) / 16) * 16;
        PlaneD pl;
        pl.n = mul(v, 1 / len);
        pl.d = (at - ti.vecs[a][3]) / len;
        Winding fw, bw;
        split_winding(f.w, pl, kOnEps, &fw, &bw);
        if (tiny(fw) || tiny(bw)) continue;
        Face g = f;
        Face h = f;
        h.w = std::move(fw);
        g.w = std::move(bw);
        work.push_back(std::move(h));
        work.push_back(std::move(g));
        cut = true;
        break;
      }
    }
    if (!cut) out.push_back(std::move(f));
  }
  // Back in a stable order: by node, then by first corner.
  std::stable_sort(out.begin(), out.end(), [](const Face& a, const Face& b) {
    if (a.node != b.node) return a.node < b.node;
    return a.w[0] < b.w[0];
  });
  tree->faces = std::move(out);
}

void finish_tree(Tree* tree) {
  for (Node& n : tree->nodes) {
    if (n.plane < 0) n.faces.clear();
  }
  for (size_t i = 0; i < tree->faces.size(); ++i) {
    for (int l : tree->faces[i].leaves) tree->nodes[size_t(l)].faces.push_back(int(i));
  }
  // Leaf bounds from their portals; a node's from its children (post-order: children are built
  // after their parent, so walk backwards).
  for (Node& n : tree->nodes) {
    n.lo = {1e30, 1e30, 1e30};
    n.hi = {-1e30, -1e30, -1e30};
    if (n.plane >= 0 || opaque(n.contents)) continue;
    for (int pid : n.portals) bounds_add(tree->portals[size_t(pid)].w, &n.lo, &n.hi);
  }
  for (size_t i = tree->nodes.size(); i-- > 0;) {
    Node& n = tree->nodes[i];
    if (n.plane < 0) continue;
    for (int c : n.child) {
      const Node& ch = tree->nodes[size_t(c)];
      for (size_t k = 0; k < 3; ++k) {
        n.lo[k] = std::min(n.lo[k], ch.lo[k]);
        n.hi[k] = std::max(n.hi[k], ch.hi[k]);
      }
    }
  }
}

}  // namespace ge::brush::mapc
