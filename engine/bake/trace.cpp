// The BVH's build (binned surface-area splits over centroids, stable partitions so the tree is
// the same on every standard library) and its two traversals.
//
// ge:summary Defines the bake's BVH build and its nearest-hit and occlusion traversals
// ge:keywords bake bvh sah binned build traversal slab moller-trumbore
// ge:skill render perf
// ge:test tests/bake_trace_test.cpp
#include "bake/trace.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ge::bake {

namespace {

constexpr int kBins = 16;
constexpr uint32_t kLeafMost = 4;
constexpr int kStack = 64;

struct Box {
  float lo[3] = {1e30f, 1e30f, 1e30f};
  float hi[3] = {-1e30f, -1e30f, -1e30f};
  void grow(const float p[3]) {
    for (int k = 0; k < 3; k++) {
      lo[k] = std::min(lo[k], p[k]);
      hi[k] = std::max(hi[k], p[k]);
    }
  }
  void grow(const Box& b) {
    grow(b.lo);
    grow(b.hi);
  }
  float area() const {
    const float x = hi[0] - lo[0], y = hi[1] - lo[1], z = hi[2] - lo[2];
    return x < 0 ? 0 : 2 * (x * y + y * z + z * x);
  }
};

struct Build {
  const Scene* scene;
  std::vector<Box> boxes;
  std::vector<float> centre;  // 3 a triangle
  std::vector<uint32_t> order;
};

// Slab test: the entry distance, or +inf when the box is missed within (t_min, t_max).
float enter(const BvhNode& n, const float o[3], const float inv[3], float t_min, float t_max) {
  float t0 = t_min, t1 = t_max;
  for (int k = 0; k < 3; k++) {
    float a = (n.lo[k] - o[k]) * inv[k], b = (n.hi[k] - o[k]) * inv[k];
    if (a > b) std::swap(a, b);
    t0 = a > t0 ? a : t0;  // a NaN (origin on a slab of a flat box) leaves t0 as it was
    t1 = b < t1 ? b : t1;
  }
  return t0 <= t1 ? t0 : INFINITY;
}

// Moller-Trumbore against leaf triangle `i`; true with t, b1, b2 and the side on a hit.
bool intersect(const float* p, const float o[3], const float d[3], float t_min, float t_max,
               float* t, float* b1, float* b2, bool* front) {
  const float* v0 = p;
  const float* e1 = p + 3;
  const float* e2 = p + 6;
  const float q[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2],
                      d[0] * e2[1] - d[1] * e2[0]};
  const float det = e1[0] * q[0] + e1[1] * q[1] + e1[2] * q[2];
  if (det > -1e-12f && det < 1e-12f) return false;
  const float inv = 1.0f / det;
  const float s[3] = {o[0] - v0[0], o[1] - v0[1], o[2] - v0[2]};
  const float u = (s[0] * q[0] + s[1] * q[1] + s[2] * q[2]) * inv;
  if (u < 0 || u > 1) return false;
  const float r[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2],
                      s[0] * e1[1] - s[1] * e1[0]};
  const float v = (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) * inv;
  if (v < 0 || u + v > 1) return false;
  const float tt = (e2[0] * r[0] + e2[1] * r[1] + e2[2] * r[2]) * inv;
  if (!(tt > t_min && tt < t_max)) return false;
  *t = tt;
  *b1 = u;
  *b2 = v;
  *front = det > 0;  // det = −d · (e1 × e2): positive when the ray meets the front
  return true;
}

void inverse(const float d[3], float inv[3]) {
  for (int k = 0; k < 3; k++) inv[k] = 1.0f / d[k];
}

}  // namespace

Bvh build_bvh(const Scene& scene) {
  Bvh bvh;
  const auto count = static_cast<uint32_t>(scene.triangles());
  Build b{&scene, std::vector<Box>(count), std::vector<float>(size_t{count} * 3), {}};
  b.order.resize(count);
  std::iota(b.order.begin(), b.order.end(), 0u);
  for (uint32_t t = 0; t < count; t++) {
    for (int c = 0; c < 3; c++)
      b.boxes[t].grow(&scene.positions[size_t{scene.indices[t * 3 + c]} * 3]);
    for (int k = 0; k < 3; k++) b.centre[t * 3 + k] = (b.boxes[t].lo[k] + b.boxes[t].hi[k]) * 0.5f;
  }
  struct Task {
    uint32_t node, first, count;
  };
  bvh.nodes.reserve(count > 0 ? size_t{count} * 2 : 1);
  bvh.nodes.push_back({});
  std::vector<Task> todo{{0, 0, count}};
  while (!todo.empty()) {
    const Task task = todo.back();
    todo.pop_back();
    Box box, cbox;
    for (uint32_t i = task.first; i < task.first + task.count; i++) {
      box.grow(b.boxes[b.order[i]]);
      cbox.grow(&b.centre[size_t{b.order[i]} * 3]);
    }
    BvhNode& node = bvh.nodes[task.node];
    for (int k = 0; k < 3; k++) {
      node.lo[k] = box.lo[k];
      node.hi[k] = box.hi[k];
    }
    node.first = task.first;
    node.count = task.count;
    if (task.count <= kLeafMost) continue;
    int axis = 0;
    for (int k = 1; k < 3; k++) {
      if (cbox.hi[k] - cbox.lo[k] > cbox.hi[axis] - cbox.lo[axis]) axis = k;
    }
    const float lo = cbox.lo[axis], span = cbox.hi[axis] - cbox.lo[axis];
    if (!(span > 0)) continue;  // every centre in one place: a leaf, however many
    auto bin_of = [&](uint32_t t) {
      const int k = static_cast<int>((b.centre[size_t{t} * 3 + axis] - lo) / span * kBins);
      return std::clamp(k, 0, kBins - 1);
    };
    Box bins[kBins];
    uint32_t counts[kBins] = {};
    for (uint32_t i = task.first; i < task.first + task.count; i++) {
      const int k = bin_of(b.order[i]);
      bins[k].grow(b.boxes[b.order[i]]);
      counts[k]++;
    }
    // The split after bin s: the left's area × count plus the right's, the cheapest wins.
    float right_area[kBins] = {};
    uint32_t right_count[kBins] = {};
    Box acc;
    uint32_t n = 0;
    for (int s = kBins - 1; s > 0; s--) {
      acc.grow(bins[s]);
      n += counts[s];
      right_area[s] = acc.area();
      right_count[s] = n;
    }
    Box left;
    uint32_t ln = 0;
    float best = INFINITY;
    int split = -1;
    for (int s = 0; s < kBins - 1; s++) {
      left.grow(bins[s]);
      ln += counts[s];
      if (ln == 0 || right_count[s + 1] == 0) continue;
      const float cost = left.area() * static_cast<float>(ln) +
                         right_area[s + 1] * static_cast<float>(right_count[s + 1]);
      if (cost < best) {
        best = cost;
        split = s;
      }
    }
    if (split < 0 || best >= box.area() * static_cast<float>(task.count)) {
      if (task.count <= kLeafMost * 4) continue;  // splitting costs more than it saves
      split = kBins / 2 - 1;
    }
    auto* begin = b.order.data() + task.first;
    auto* mid = std::stable_partition(begin, begin + task.count,
                                      [&](uint32_t t) { return bin_of(t) <= split; });
    auto left_count = static_cast<uint32_t>(mid - begin);
    if (left_count == 0 || left_count == task.count) left_count = task.count / 2;
    const auto child = static_cast<uint32_t>(bvh.nodes.size());
    bvh.nodes[task.node].first = child;
    bvh.nodes[task.node].count = 0;
    bvh.nodes.push_back({});
    bvh.nodes.push_back({});
    todo.push_back({child + 1, task.first + left_count, task.count - left_count});
    todo.push_back({child, task.first, left_count});
  }
  bvh.tris = b.order;
  bvh.prim.resize(size_t{count} * 9);
  for (uint32_t i = 0; i < count; i++) {
    const uint32_t t = bvh.tris[i];
    const float* v[3];
    for (int c = 0; c < 3; c++) v[c] = &scene.positions[size_t{scene.indices[t * 3 + c]} * 3];
    float* p = &bvh.prim[size_t{i} * 9];
    for (int k = 0; k < 3; k++) {
      p[k] = v[0][k];
      p[3 + k] = v[1][k] - v[0][k];
      p[6 + k] = v[2][k] - v[0][k];
    }
  }
  return bvh;
}

Hit closest_hit(const Bvh& bvh, const Ray& ray) {
  Hit hit;
  if (bvh.tris.empty()) return hit;
  float inv[3];
  inverse(ray.d, inv);
  float t_max = ray.t_max;
  uint32_t stack[kStack];
  int top = 0;
  if (enter(bvh.nodes[0], ray.o, inv, ray.t_min, t_max) == INFINITY) return hit;
  stack[top++] = 0;
  while (top > 0) {
    const BvhNode& n = bvh.nodes[stack[--top]];
    if (n.count > 0) {
      for (uint32_t i = n.first; i < n.first + n.count; i++) {
        float t, b1, b2;
        bool front;
        if (intersect(&bvh.prim[size_t{i} * 9], ray.o, ray.d, ray.t_min, t_max, &t, &b1, &b2,
                      &front)) {
          t_max = t;
          hit = {bvh.tris[i], t, b1, b2, front};
        }
      }
      continue;
    }
    const float a = enter(bvh.nodes[n.first], ray.o, inv, ray.t_min, t_max);
    const float c = enter(bvh.nodes[n.first + 1], ray.o, inv, ray.t_min, t_max);
    // The nearer child on top, so it is searched first and shortens t_max for the other.
    const uint32_t near = a <= c ? n.first : n.first + 1;
    const uint32_t far = a <= c ? n.first + 1 : n.first;
    const float tf = a <= c ? c : a, tn = a <= c ? a : c;
    if (tf != INFINITY && top < kStack) stack[top++] = far;
    if (tn != INFINITY && top < kStack) stack[top++] = near;
  }
  return hit;
}

bool occluded(const Bvh& bvh, const Ray& ray) {
  if (bvh.tris.empty()) return false;
  float inv[3];
  inverse(ray.d, inv);
  uint32_t stack[kStack];
  int top = 0;
  stack[top++] = 0;
  while (top > 0) {
    const BvhNode& n = bvh.nodes[stack[--top]];
    if (enter(n, ray.o, inv, ray.t_min, ray.t_max) == INFINITY) continue;
    if (n.count > 0) {
      for (uint32_t i = n.first; i < n.first + n.count; i++) {
        float t, b1, b2;
        bool front;
        if (intersect(&bvh.prim[size_t{i} * 9], ray.o, ray.d, ray.t_min, ray.t_max, &t, &b1, &b2,
                      &front)) {
          return true;
        }
      }
      continue;
    }
    if (top + 2 <= kStack) {
      stack[top++] = n.first + 1;
      stack[top++] = n.first;
    }
  }
  return false;
}

}  // namespace ge::bake
