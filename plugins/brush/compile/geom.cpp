// The map compiler's geometry (geom.hpp).
//
// ge:summary Defines windings cut by planes, base windings and the canonical plane pool
// ge:keywords map-compiler winding split clip plane-pool canonical snap
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#include "brush/compile/geom.hpp"

#include <cmath>
#include <cstdlib>

namespace ge::brush::mapc {

double length(const V3& a) { return std::sqrt(dot(a, a)); }

V3 normalized(const V3& a) {
  const double l = length(a);
  return l > 0 ? mul(a, 1.0 / l) : V3{0, 0, 0};
}

bool plane_of(const V3& a, const V3& b, const V3& c, PlaneD* out) {
  const V3 n = cross(sub(b, a), sub(c, a));
  const double l = length(n);
  if (!(l > 1e-9)) return false;
  out->n = mul(n, 1.0 / l);
  out->d = dot(out->n, a);
  return true;
}

Winding base_winding(const PlaneD& p) {
  // The axis the normal leans on least gives the square's first edge.
  int major = 0;
  for (int k = 1; k < 3; ++k) {
    if (std::fabs(p.n[size_t(k)]) > std::fabs(p.n[size_t(major)])) major = k;
  }
  V3 up = {0, 0, 0};
  up[major == 2 ? 0 : 2] = 1;
  // up made perpendicular to n.
  up = normalized(sub(up, mul(p.n, dot(up, p.n))));
  const V3 right = cross(up, p.n);
  const V3 o = mul(p.n, p.d);
  const V3 u = mul(up, kHuge), r = mul(right, kHuge);
  // Counter-clockwise seen from the front: right × up = n.
  return {sub(sub(o, r), u), sub(add(o, r), u), add(add(o, r), u), add(sub(o, r), u)};
}

void split_winding(const Winding& w, const PlaneD& p, double eps, Winding* front, Winding* back) {
  front->clear();
  back->clear();
  const size_t n = w.size();
  std::vector<double> d(n);
  std::vector<int> side(n);
  int counts[3] = {0, 0, 0};
  for (size_t i = 0; i < n; ++i) {
    d[i] = dist(p, w[i]);
    side[i] = d[i] > eps ? 0 : d[i] < -eps ? 1 : 2;
    ++counts[side[i]];
  }
  if (counts[1] == 0) {
    *front = w;
    if (counts[0] == 0) *back = w;  // on the plane: both
    return;
  }
  if (counts[0] == 0) {
    *back = w;
    return;
  }
  for (size_t i = 0; i < n; ++i) {
    const V3& a = w[i];
    if (side[i] == 2) {
      front->push_back(a);
      back->push_back(a);
      continue;
    }
    (side[i] == 0 ? front : back)->push_back(a);
    const size_t j = (i + 1) % n;
    if (side[j] == 2 || side[j] == side[i]) continue;
    const double t = d[i] / (d[i] - d[j]);
    V3 m;
    for (size_t k = 0; k < 3; ++k) {
      // Exact on an axial plane, so whole-unit planes give whole-unit corners.
      if (p.n[k] == 1) {
        m[k] = p.d;
      } else if (p.n[k] == -1) {
        m[k] = -p.d;
      } else {
        m[k] = a[k] + t * (w[j][k] - a[k]);
      }
    }
    front->push_back(m);
    back->push_back(m);
  }
}

Winding clip_behind(const Winding& w, const PlaneD& p, double eps) {
  Winding f, b;
  split_winding(w, p, eps, &f, &b);
  // A winding lying on the plane is kept (the caller asked what is not in front).
  return b;
}

Winding reversed(const Winding& w) { return Winding(w.rbegin(), w.rend()); }

double area(const Winding& w) {
  if (w.size() < 3) return 0;
  V3 s = {0, 0, 0};
  for (size_t i = 1; i + 1 < w.size(); ++i) s = add(s, cross(sub(w[i], w[0]), sub(w[i + 1], w[0])));
  return 0.5 * length(s);
}

V3 centre(const Winding& w) {
  V3 c = {0, 0, 0};
  for (const V3& p : w) c = add(c, p);
  return w.empty() ? c : mul(c, 1.0 / double(w.size()));
}

bool tiny(const Winding& w) {
  if (w.size() < 3) return true;
  // Fewer than three edges longer than a fifth of a unit.
  int edges = 0;
  for (size_t i = 0; i < w.size(); ++i) {
    if (length(sub(w[(i + 1) % w.size()], w[i])) > 0.2) ++edges;
  }
  return edges < 3;
}

void bounds_add(const Winding& w, V3* lo, V3* hi) {
  for (const V3& p : w) {
    for (size_t k = 0; k < 3; ++k) {
      if (p[k] < (*lo)[k]) (*lo)[k] = p[k];
      if (p[k] > (*hi)[k]) (*hi)[k] = p[k];
    }
  }
}

PlaneD canonical_plane(PlaneD p, bool* flipped) {
  for (size_t k = 0; k < 3; ++k) {
    if (std::fabs(p.n[k]) > 1 - 1e-6) {
      const double s = p.n[k] > 0 ? 1 : -1;
      p.n = {0, 0, 0};
      p.n[k] = s;
      break;
    }
  }
  const double r = std::nearbyint(p.d);
  if (std::fabs(p.d - r) < 0.01) p.d = r;
  if (p.d == 0) p.d = 0;  // no negative zero
  // Canonical: the largest component positive (an axial plane faces +axis).
  size_t major = 0;
  for (size_t k = 1; k < 3; ++k) {
    if (std::fabs(p.n[k]) > std::fabs(p.n[major])) major = k;
  }
  *flipped = p.n[major] < 0;
  if (*flipped) p = flip(p);
  for (double& c : p.n) {
    if (c == 0) c = 0;
  }
  if (p.d == 0) p.d = 0;
  return p;
}

int Planes::find(PlaneD p) {
  bool flipped = false;
  p = canonical_plane(p, &flipped);
  const auto key = int64_t(std::floor(p.d));
  for (int64_t k = key - 1; k <= key + 1; ++k) {
    const auto it = by_d_.find(k);
    if (it == by_d_.end()) continue;
    for (int i : it->second) {
      const PlaneD& q = planes_[size_t(i)];
      if (std::fabs(q.d - p.d) < 0.01 && std::fabs(q.n[0] - p.n[0]) < 1e-6 &&
          std::fabs(q.n[1] - p.n[1]) < 1e-6 && std::fabs(q.n[2] - p.n[2]) < 1e-6) {
        return i * 2 + (flipped ? 1 : 0);
      }
    }
  }
  const int index = int(planes_.size());
  planes_.push_back(p);
  by_d_[key].push_back(index);
  return index * 2 + (flipped ? 1 : 0);
}

int Planes::type(int index) const {
  const PlaneD& p = planes_[size_t(index)];
  for (int k = 0; k < 3; ++k) {
    if (p.n[size_t(k)] == 1) return k;
  }
  int major = 0;
  for (int k = 1; k < 3; ++k) {
    if (std::fabs(p.n[size_t(k)]) > std::fabs(p.n[size_t(major)])) major = k;
  }
  return 3 + major;
}

}  // namespace ge::brush::mapc
