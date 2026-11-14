// THE MAP COMPILER'S GEOMETRY: points and planes in doubles (map units, z up), convex polygons
// (windings) cut by planes, and the plane pool every compiled plane is drawn from.
//
// - A plane holds the points `n·p = d`; its FRONT is `n·p > d`. A brush side's plane faces out of
//   the brush: the solid is behind it.
// - A winding is a convex polygon wound counter-clockwise seen from the front of its plane.
// - The pool (`Planes`) keeps each plane once, canonical: an axial plane faces along +axis (the
//   families' readers test `p[axis] − d` for types 0..2), any other along its largest component
//   positive. `Planes::find` returns an ORIENTED id, `index * 2 + flipped`, so a side keeps which
//   way it faces. Normals within 1e-6 of an axis are snapped onto it, distances within 0.01 of a
//   whole unit onto it, so planes typed as whole numbers compile to whole numbers.
//
// Everything here is + − × ÷ and square roots: the same bits on every platform.
//
// ge:summary The map compiler's geometry: points, planes, windings cut by planes, the plane pool
// ge:keywords map-compiler geometry winding polygon plane-pool canonical-plane split clip
// ge:keywords oriented-plane snap base-winding
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <vector>

namespace ge::brush::mapc {

using V3 = std::array<double, 3>;

inline V3 add(const V3& a, const V3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
inline V3 sub(const V3& a, const V3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
inline V3 mul(const V3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
inline double dot(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline V3 cross(const V3& a, const V3& b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(const V3& a);
V3 normalized(const V3& a);

struct PlaneD {
  V3 n = {0, 0, 1};
  double d = 0;
};
inline double dist(const PlaneD& p, const V3& x) { return dot(p.n, x) - p.d; }
inline PlaneD flip(const PlaneD& p) { return {{-p.n[0], -p.n[1], -p.n[2]}, -p.d}; }

// The plane through three points, facing where (b − a) × (c − a) points; false when they are in a
// line.
bool plane_of(const V3& a, const V3& b, const V3& c, PlaneD* out);

using Winding = std::vector<V3>;

inline constexpr double kHuge = 131072;  // a base winding's half size: past any map's bounds
inline constexpr double kOnEps = 0.01;   // a point this near a plane is on it

// A square on `p`, `kHuge` from its centre each way, wound counter-clockwise seen from its front.
Winding base_winding(const PlaneD& p);
// `w` cut by `p`: the parts in front and behind (either may be empty). Points within `eps` are on
// the plane and go to both.
void split_winding(const Winding& w, const PlaneD& p, double eps, Winding* front, Winding* back);
// `w` with the part in front of `p` cut away (what is behind `p` stays).
Winding clip_behind(const Winding& w, const PlaneD& p, double eps = kOnEps);
Winding reversed(const Winding& w);
double area(const Winding& w);
V3 centre(const Winding& w);
// True when `w` has fewer than three corners or no area worth keeping.
bool tiny(const Winding& w);
void bounds_add(const Winding& w, V3* lo, V3* hi);

// The plane pool. Planes are canonical; an oriented id is `index * 2 + flipped`.
class Planes {
 public:
  int find(PlaneD p);  // the oriented id of `p` (added when new)
  const PlaneD& canonical(int index) const { return planes_[size_t(index)]; }
  PlaneD oriented(int id) const {
    const PlaneD& c = planes_[size_t(id >> 1)];
    return (id & 1) ? flip(c) : c;
  }
  // 0..2 axial (normal +axis), 3..5 otherwise by the largest component's axis.
  int type(int index) const;
  size_t size() const { return planes_.size(); }

 private:
  std::vector<PlaneD> planes_;
  std::map<int64_t, std::vector<int>> by_d_;  // round(d) → indices
};

// A plane's normal and distance snapped (onto an axis, onto a whole unit) and made canonical;
// `*flipped` says whether it was turned round.
PlaneD canonical_plane(PlaneD p, bool* flipped);

}  // namespace ge::brush::mapc
