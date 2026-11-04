// The definitions behind osm_shapes.hpp.
//
// ge:summary Lab tooling: lines and rings on a real face's map, thinned, clipped and joined
// ge:keywords osm-shapes douglas-peucker clip-to-map join-rings
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include "mountain/labs/osm_shapes.hpp"

#include <algorithm>
#include <cmath>

#include "core/math.hpp"
#include "mountain/mapgen/real_face.hpp"

namespace ge::mountain::labs {

namespace {
constexpr double kSize = mapgen::kFaceSize;
}  // namespace

bool on_map(const Pt& p) { return p.x >= 0 && p.x <= kSize && p.z >= 0 && p.z <= kSize; }

double length_of(const std::vector<Pt>& pts) {
  double s = 0;
  for (size_t i = 1; i < pts.size(); i++) {
    s += ge::core::hypot(pts[i].x - pts[i - 1].x, pts[i].z - pts[i - 1].z);
  }
  return s;
}

// A line thinned to the bends that keep it within `tol` m (Douglas–Peucker).
std::vector<Pt> thin(const std::vector<Pt>& pts, double tol) {
  if (pts.size() < 3) return pts;
  const Pt a = pts.front();
  const Pt b = pts.back();
  const double dx = b.x - a.x;
  const double dz = b.z - a.z;
  const double len = std::max(1e-9, ge::core::hypot(dx, dz));
  double worst = 0;
  size_t at = 0;
  for (size_t i = 1; i + 1 < pts.size(); i++) {
    const double d = std::abs((pts[i].x - a.x) * dz - (pts[i].z - a.z) * dx) / len;
    if (d > worst) {
      worst = d;
      at = i;
    }
  }
  if (worst <= tol) return {a, b};
  std::vector<Pt> left = thin({pts.begin(), pts.begin() + long(at) + 1}, tol);
  const std::vector<Pt> right = thin({pts.begin() + long(at), pts.end()}, tol);
  left.pop_back();
  left.insert(left.end(), right.begin(), right.end());
  return left;
}

// The longest run of a line's points that stay on the map.
std::vector<Pt> longest_on_map(const std::vector<Pt>& pts) {
  std::vector<Pt> best;
  std::vector<Pt> cur;
  for (const Pt& p : pts) {
    if (on_map(p)) {
      cur.push_back(p);
      continue;
    }
    if (cur.size() > best.size()) best = cur;
    cur.clear();
  }
  return cur.size() > best.size() ? cur : best;
}

bool inside(const Ring& ring, double x, double z) {
  bool odd = false;
  for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
    const Pt a = ring[i];
    const Pt b = ring[j];
    if ((a.z > z) != (b.z > z) && x < (b.x - a.x) * (z - a.z) / (b.z - a.z) + a.x) odd = !odd;
  }
  return odd;
}

// The area a ring holds, m² (its sign its winding).
double signed_area(const Ring& ring) {
  double a = 0;
  for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
    a += ring[j].x * ring[i].z - ring[i].x * ring[j].z;
  }
  return a / 2;
}

// The ring clipped to the map's square (Sutherland–Hodgman, one edge of the square at a time).
Ring clip_to_map(const Ring& ring) {
  Ring out = ring;
  const auto pass = [&](auto in_side, auto cut) {
    Ring next;
    for (size_t i = 0; i < out.size(); i++) {
      const Pt a = out[(i + out.size() - 1) % out.size()];
      const Pt b = out[i];
      if (in_side(b)) {
        if (!in_side(a)) next.push_back(cut(a, b));
        next.push_back(b);
      } else if (in_side(a)) {
        next.push_back(cut(a, b));
      }
    }
    out = std::move(next);
  };
  const auto at_x = [](double x) {
    return [x](Pt a, Pt b) { return Pt{x, a.z + (b.z - a.z) * (x - a.x) / (b.x - a.x)}; };
  };
  const auto at_z = [](double z) {
    return [z](Pt a, Pt b) { return Pt{a.x + (b.x - a.x) * (z - a.z) / (b.z - a.z), z}; };
  };
  pass([](Pt p) { return p.x >= 0; }, at_x(0));
  if (!out.empty()) pass([](Pt p) { return p.x <= kSize; }, at_x(kSize));
  if (!out.empty()) pass([](Pt p) { return p.z >= 0; }, at_z(0));
  if (!out.empty()) pass([](Pt p) { return p.z <= kSize; }, at_z(kSize));
  return out;
}

// A closed ring thinned (Douglas–Peucker on its two halves, split at the point farthest from its
// first), unclosed.
Ring thin_ring(const Ring& ring, double tol) {
  if (ring.size() < 4) return ring;
  size_t far = 0;
  double most = -1;
  for (size_t i = 1; i < ring.size(); i++) {
    const double d = ge::core::hypot(ring[i].x - ring[0].x, ring[i].z - ring[0].z);
    if (d > most) {
      most = d;
      far = i;
    }
  }
  std::vector<Pt> a = thin({ring.begin(), ring.begin() + long(far) + 1}, tol);
  std::vector<Pt> second(ring.begin() + long(far), ring.end());
  second.push_back(ring[0]);
  const std::vector<Pt> b = thin(second, tol);
  a.pop_back();
  a.insert(a.end(), b.begin(), b.end() - 1);
  return a;
}

// Ways' node lists joined end to end into closed rings; a chain that never closes is dropped.
std::vector<std::vector<int64_t>> join_rings(std::vector<std::vector<int64_t>> parts) {
  std::vector<std::vector<int64_t>> rings;
  while (!parts.empty()) {
    std::vector<int64_t> cur = std::move(parts.back());
    parts.pop_back();
    bool grew = true;
    while (cur.size() > 1 && cur.front() != cur.back() && grew) {
      grew = false;
      for (size_t i = 0; i < parts.size(); i++) {
        std::vector<int64_t>& p = parts[i];
        if (p.empty()) continue;
        if (p.back() == cur.back()) std::reverse(p.begin(), p.end());
        if (p.front() != cur.back()) continue;
        cur.insert(cur.end(), p.begin() + 1, p.end());
        parts.erase(parts.begin() + long(i));
        grew = true;
        break;
      }
    }
    if (cur.size() >= 4 && cur.front() == cur.back()) rings.push_back(std::move(cur));
  }
  return rings;
}

// The way's points on the map, the nodes the data has.
std::vector<Pt> points_of(const mapgen::FaceCrop& f, const OsmData& d, const OsmWay& w) {
  std::vector<Pt> out;
  for (const int64_t r : w.refs) {
    const auto it = d.nodes.find(r);
    if (it == d.nodes.end()) continue;
    const auto [x, z] = mapgen::map_at(f, it->second.first, it->second.second);
    out.push_back({x, z});
  }
  return out;
}

}  // namespace ge::mountain::labs
