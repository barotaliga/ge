// The brush trace: the position test, the sweep down the node tree, clipping to a brush, and the
// contents at a point. The precision of each step is trace.hpp's; the comments here say where a
// double enters.
//
// ge:summary The brush trace's position test, sweep, brush clip and point contents
// ge:keywords trace sweep clip brush position-test point-contents epsilon offset idist
// ge:skill brush
// ge:test tests/brush_trace_test.cpp
#include "brush/trace.hpp"

namespace ge::brush {

namespace {

float dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
float mag(float x) { return x < 0 ? -x : x; }

}  // namespace

struct BrushTracer::Sweep {
  BrushTracer& t;
  const Map& m;
  float start[3], end[3], mins[3], maxs[3], extents[3];
  bool point = false;
  int32_t mask = 0;
  Trace r;
  size_t budget = 0;  // nodes and leaves the walk may still visit

  // A side's plane pushed out by the box's corner (its max on axes where the normal is negative,
  // its min elsewhere): the plane's distance less the corner's dot with the normal.
  float pushed(const Plane& p) const {
    if (point) return p.d;
    float ofs[3];
    for (int k = 0; k < 3; ++k) ofs[k] = p.n[k] < 0 ? maxs[k] : mins[k];
    return p.d - dot(ofs, p.n);
  }

  // Whether the brush is new to this trace and shares the mask (it is stamped either way).
  bool fresh(uint32_t b) {
    if (t.stamp_[b] == t.pass_) return false;
    t.stamp_[b] = t.pass_;
    return (m.brushes[b].contents & mask) != 0 && m.brushes[b].sides > 0;
  }

  void clip(const Brush& b) {
    float enter = -1, leave = 1;
    bool gets_out = false, starts_out = false;
    const BrushSide* lead = nullptr;
    for (uint32_t i = 0; i < b.sides; ++i) {
      const BrushSide& side = m.brush_sides[b.first_side + i];
      const Plane& p = m.planes[side.plane];
      const float dist = pushed(p);
      const float d1 = dot(start, p.n) - dist;
      const float d2 = dot(end, p.n) - dist;
      if (d2 > 0) gets_out = true;
      if (d1 > 0) starts_out = true;
      if (d1 > 0 && d2 >= d1) return;  // in front of this side and not coming closer
      if (d1 <= 0 && d2 <= 0) continue;
      const float span = d1 - d2;
      if (d1 > d2) {  // entering: the largest fraction
        const auto f = float((double(d1) - kTraceEpsilon) / double(span));
        if (f > enter) {
          enter = f;
          lead = &side;
        }
      } else {  // leaving: the smallest
        const auto f = float((double(d1) + kTraceEpsilon) / double(span));
        if (f < leave) leave = f;
      }
    }
    if (!starts_out) {
      r.start_solid = true;
      if (!gets_out) r.all_solid = true;
      return;
    }
    if (enter < leave && enter > -1 && enter < r.fraction && lead) {
      r.fraction = enter < 0 ? 0 : enter;
      r.plane = m.planes[lead->plane];
      r.texture = lead->texture;
      r.flags = lead->flags;
      r.value = lead->value;
      r.contents = b.contents;
    }
  }

  bool inside(const Brush& b) const {
    for (uint32_t i = 0; i < b.sides; ++i) {
      const Plane& p = m.planes[m.brush_sides[b.first_side + i].plane];
      if (dot(start, p.n) - pushed(p) > 0) return false;
    }
    return true;
  }

  void leaf_sweep(uint32_t leaf) {
    const Leaf& l = m.leaves[leaf];
    if (!(l.contents & mask)) return;
    for (uint32_t k = 0; k < l.brushes; ++k) {
      const uint32_t b = m.leaf_brushes[l.first_brush + k];
      if (!fresh(b)) continue;
      clip(m.brushes[b]);
      if (r.fraction == 0) return;
    }
  }

  void node_sweep(int32_t num, float p1f, float p2f, const float p1[3], const float p2[3]) {
    if (r.fraction <= p1f || budget == 0) return;
    --budget;
    if (num < 0) {
      leaf_sweep(uint32_t(-(num + 1)));
      return;
    }
    const Node& node = m.nodes[size_t(num)];
    const Plane& p = m.planes[node.plane];
    float t1, t2, offset;
    if (p.type < 3) {
      t1 = p1[p.type] - p.d;
      t2 = p2[p.type] - p.d;
      offset = extents[p.type];
    } else {
      t1 = dot(p1, p.n) - p.d;
      t2 = dot(p2, p.n) - p.d;
      // Each product in float, the sum in double.
      offset = point ? 0.0f
                     : float(double(mag(extents[0] * p.n[0])) + double(mag(extents[1] * p.n[1])) +
                             double(mag(extents[2] * p.n[2])));
    }
    if (t1 >= offset && t2 >= offset) {
      node_sweep(node.child[0], p1f, p2f, p1, p2);
      return;
    }
    if (t1 < -offset && t2 < -offset) {
      node_sweep(node.child[1], p1f, p2f, p1, p2);
      return;
    }
    int side;
    float near, far;
    if (t1 < t2) {
      const auto idist = float(1.0 / double(t1 - t2));
      side = 1;
      far = float((double(t1 + offset) + kTraceEpsilon) * double(idist));
      near = float((double(t1 - offset) + kTraceEpsilon) * double(idist));
    } else if (t1 > t2) {
      const auto idist = float(1.0 / double(t1 - t2));
      side = 0;
      far = float((double(t1 - offset) - kTraceEpsilon) * double(idist));
      near = float((double(t1 + offset) + kTraceEpsilon) * double(idist));
    } else {
      side = 0;
      near = 1;
      far = 0;
    }
    const auto hold = [](float f) { return f < 0 ? 0.0f : f > 1 ? 1.0f : f; };
    near = hold(near);
    far = hold(far);
    float mid[3];
    float midf = p1f + (p2f - p1f) * near;
    for (int k = 0; k < 3; ++k) mid[k] = p1[k] + near * (p2[k] - p1[k]);
    node_sweep(node.child[side], p1f, midf, p1, mid);
    midf = p1f + (p2f - p1f) * far;
    for (int k = 0; k < 3; ++k) mid[k] = p1[k] + far * (p2[k] - p1[k]);
    node_sweep(node.child[side ^ 1], midf, p2f, mid, p2);
  }

  // Which sides of plane `p` the box [lo, hi] touches: 1 front, 2 back, 3 both.
  static int box_side(const Plane& p, const float lo[3], const float hi[3]) {
    if (p.type < 3) {
      if (p.d <= lo[p.type]) return 1;
      if (p.d >= hi[p.type]) return 2;
      return 3;
    }
    float a[3], b[3];  // the corners farthest along the normal and against it
    for (int k = 0; k < 3; ++k) {
      a[k] = p.n[k] < 0 ? lo[k] : hi[k];
      b[k] = p.n[k] < 0 ? hi[k] : lo[k];
    }
    int s = 0;
    if (dot(a, p.n) >= p.d) s |= 1;
    if (dot(b, p.n) < p.d) s |= 2;
    return s;
  }

  void position_test(int32_t head) {
    float lo[3], hi[3];
    for (int k = 0; k < 3; ++k) {
      lo[k] = start[k] + mins[k] - 1;
      hi[k] = start[k] + maxs[k] + 1;
    }
    std::vector<int32_t>& stack = t.stack_;
    stack.clear();
    stack.push_back(head);
    while (!stack.empty() && budget > 0) {
      --budget;
      const int32_t num = stack.back();
      stack.pop_back();
      if (num < 0) {
        const Leaf& l = m.leaves[size_t(-(num + 1))];
        if (!(l.contents & mask)) continue;
        for (uint32_t k = 0; k < l.brushes; ++k) {
          const uint32_t b = m.leaf_brushes[l.first_brush + k];
          if (!fresh(b) || !inside(m.brushes[b])) continue;
          r.start_solid = r.all_solid = true;
          r.fraction = 0;
          r.contents = m.brushes[b].contents;
          return;
        }
        continue;
      }
      const Node& node = m.nodes[size_t(num)];
      const int s = box_side(m.planes[node.plane], lo, hi);
      // The front child is visited first: pushed last.
      if (s & 2) stack.push_back(node.child[1]);
      if (s & 1) stack.push_back(node.child[0]);
    }
  }
};

BrushTracer::BrushTracer(const Map& map) : map_(&map), stamp_(map.brushes.size(), 0) {
  stack_.reserve(64);
}

Trace BrushTracer::trace(const float start[3], const float end[3], const float mins[3],
                         const float maxs[3], int32_t mask, uint32_t model) {
  const Map& m = *map_;
  if (++pass_ == 0) {  // the stamps wrapped: start them again
    for (uint32_t& s : stamp_) s = 0;
    pass_ = 1;
  }
  Sweep s{*this, m, {}, {}, {}, {}, {}, false, mask, {}, m.nodes.size() + m.leaves.size()};
  bool point = true;
  for (int k = 0; k < 3; ++k) {
    s.start[k] = start[k];
    s.end[k] = end[k];
    s.mins[k] = mins[k];
    s.maxs[k] = maxs[k];
    s.extents[k] = -mins[k] > maxs[k] ? -mins[k] : maxs[k];
    point = point && mins[k] == 0 && maxs[k] == 0;
  }
  s.point = point;
  if (point) {
    for (float& e : s.extents) e = 0;
  }
  if (model >= m.models.size() || m.brushes.empty()) {
    for (int k = 0; k < 3; ++k) s.r.end[k] = end[k];
    return s.r;
  }
  const int32_t head = m.models[model].head_node;
  if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2]) {
    s.position_test(head);
    for (int k = 0; k < 3; ++k) s.r.end[k] = start[k];
    return s.r;
  }
  s.node_sweep(head, 0, 1, s.start, s.end);
  if (s.r.fraction == 1) {
    for (int k = 0; k < 3; ++k) s.r.end[k] = end[k];
  } else {
    for (int k = 0; k < 3; ++k) s.r.end[k] = start[k] + s.r.fraction * (end[k] - start[k]);
  }
  return s.r;
}

int32_t BrushTracer::contents(const float p[3], uint32_t model) const {
  const Map& m = *map_;
  if (model >= m.models.size() || m.leaves.empty()) return 0;
  int32_t at = m.models[model].head_node;
  for (size_t steps = 0; at >= 0 && steps <= m.nodes.size(); ++steps) {
    const Node& n = m.nodes[size_t(at)];
    const Plane& pl = m.planes[n.plane];
    const float d = pl.type < 3 ? p[pl.type] - pl.d : dot(p, pl.n) - pl.d;
    at = n.child[d < 0 ? 1 : 0];
  }
  if (at >= 0) return 0;
  return m.leaves[size_t(-(at + 1))].contents;
}

}  // namespace ge::brush
