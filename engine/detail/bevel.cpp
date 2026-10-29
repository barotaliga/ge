// Bevels: the edges found off the faces, each judged (turn, depth, exposure) and cut by its round's
// tangent planes; and the solids' grid that answers "what else is solid" for a whole level.
//
// ge:summary Bevels' edge walk, verdicts and tangent planes; the level's solid grid
// ge:keywords detail bevel edge verdict tangent-plane fillet slerp solid-index grid probe
// ge:skill detail
// ge:test tests/detail_bevel_test.cpp
#include "detail/bevel.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::detail {
namespace {

constexpr double kDeg = core::kPi / 180;
constexpr int kMostSegments = 8;
// Where along an edge the exposure probes sit, as shares of its length.
constexpr double kProbeAlong[] = {0.08, 0.3, 0.5, 0.7, 0.92};

bool on_plane(const Plane& p, const Vec3& v) {
  const double d = dot(p.n, v) - p.d;
  return d < 1e-6 && d > -1e-6;
}

// The unit direction inside face `a`'s plane, at right angles to the edge, pointing away from the
// edge into the face.
Vec3 into_face(const Plane& a, const Plane& b, const Vec3& along) {
  Vec3 u = normalized(cross(a.n, along));
  if (dot(u, b.n) > 0) u = mul(u, -1);
  return u;
}

// How far face `poly` reaches from the edge at `p0` along `u`, m.
double depth_of(const Polygon& poly, const Vec3& p0, const Vec3& u) {
  double most = 0;
  for (const Vec3& v : poly.loop) most = std::max(most, dot(sub(v, p0), u));
  return most;
}

const Polygon* face_of(std::span<const Polygon> polys, int plane) {
  for (const Polygon& p : polys) {
    if (p.plane == plane) return &p;
  }
  return nullptr;
}

struct IndexCtx {
  const SolidIndex* index;
  int self;
  double margin;
};

bool index_at(const void* ctx, const Vec3& p) {
  const auto* c = static_cast<const IndexCtx*>(ctx);
  return solid_at(*c->index, p, c->self, c->margin) >= 0;
}

}  // namespace

std::string_view verdict_name(EdgeVerdict v) {
  switch (v) {
    case EdgeVerdict::bevelled:
      return "bevelled";
    case EdgeVerdict::flat:
      return "flat";
    case EdgeVerdict::knife:
      return "knife";
    case EdgeVerdict::thin:
      return "thin";
    case EdgeVerdict::buried:
      return "buried";
  }
  return "?";
}

std::vector<BevelEdge> edges_of(const Solid& s, std::span<const Polygon> polys) {
  std::vector<BevelEdge> out;
  for (const Polygon& poly : polys) {
    const size_t n = poly.loop.size();
    for (size_t i = 0; i < n; ++i) {
      const Vec3& a = poly.loop[i];
      const Vec3& b = poly.loop[(i + 1) % n];
      int other = -1;
      for (const Polygon& q : polys) {
        if (q.plane == poly.plane) continue;
        const Plane& qp = s.planes[static_cast<size_t>(q.plane)];
        if (on_plane(qp, a) && on_plane(qp, b)) {
          other = q.plane;
          break;
        }
      }
      // Each edge is met twice, once from either face; keep it from its lower plane.
      if (other < 0 || other < poly.plane) continue;
      BevelEdge e;
      e.plane_a = poly.plane;
      e.plane_b = other;
      e.p0 = a;
      e.p1 = b;
      const double c =
          dot(s.planes[static_cast<size_t>(poly.plane)].n, s.planes[static_cast<size_t>(other)].n);
      e.turn_rad = core::libm::acos(core::clamp(c, -1, 1));
      out.push_back(e);
    }
  }
  return out;
}

BevelResult bevel(const Solid& s, const BevelSpec& spec, SolidAt at, const void* ctx) {
  BevelResult r;
  r.solid = s;
  const std::vector<Polygon> polys = polygons_of(s);
  r.edges = edges_of(s, polys);
  const int segments = std::clamp(spec.segments, 1, kMostSegments);
  for (BevelEdge& e : r.edges) {
    const Plane& a = s.planes[static_cast<size_t>(e.plane_a)];
    const Plane& b = s.planes[static_cast<size_t>(e.plane_b)];
    if (e.turn_rad < spec.min_turn_deg * kDeg) {
      e.verdict = EdgeVerdict::flat;
      continue;
    }
    if (e.turn_rad > spec.max_turn_deg * kDeg) {
      e.verdict = EdgeVerdict::knife;
      continue;
    }
    const Vec3 along = normalized(sub(e.p1, e.p0));
    const Vec3 ua = into_face(a, b, along);
    const Vec3 ub = into_face(b, a, along);
    const double half_tan = core::libm::tan(e.turn_rad * 0.5);
    double setback = spec.radius_m * half_tan;
    const Polygon* fa = face_of(polys, e.plane_a);
    const Polygon* fb = face_of(polys, e.plane_b);
    const double room = spec.max_share * std::min(depth_of(*fa, e.p0, ua), depth_of(*fb, e.p0, ub));
    setback = std::min(setback, room);
    const double radius = setback / half_tan;
    if (radius < spec.radius_m * 0.25) {
      e.verdict = EdgeVerdict::thin;
      continue;
    }
    if (at) {
      // A neighbour against either face near the edge, or filling the corner beyond it.
      const double off = core::clamp(radius * 0.25, 0.002, 0.02);
      const Vec3 out = normalized(add(a.n, b.n));
      bool buried = false;
      for (double k : kProbeAlong) {
        const Vec3 m = mad(e.p0, sub(e.p1, e.p0), k);
        if (at(ctx, mad(mad(m, ua, setback * 0.5), a.n, off)) ||
            at(ctx, mad(mad(m, ub, setback * 0.5), b.n, off)) || at(ctx, mad(m, out, off))) {
          buried = true;
          break;
        }
      }
      if (buried) {
        e.verdict = EdgeVerdict::buried;
        continue;
      }
    }
    e.verdict = EdgeVerdict::bevelled;
    e.radius_m = radius;
    // The round's centre: `radius` inside both faces, off the edge's line.
    const double cos_t = dot(a.n, b.n);
    const Vec3 centre = mad(e.p0, add(a.n, b.n), -radius / (1 + cos_t));
    const double sin_t = core::libm::sin(e.turn_rad);
    // The face the bevel takes its material from: the steeper one, so a floor's tag does not
    // wrap down a step's nose.
    const int tag = std::fabs(a.n.y) <= std::fabs(b.n.y) ? a.tag : b.tag;
    for (int m = 0; m < segments; ++m) {
      const double phi = e.turn_rad * (m + 0.5) / segments;
      const Vec3 n = normalized(add(mul(a.n, core::libm::sin(e.turn_rad - phi) / sin_t),
                                    mul(b.n, core::libm::sin(phi) / sin_t)));
      r.solid.planes.push_back({n, dot(n, centre) + radius, tag});
    }
  }
  return r;
}

SolidIndex index_solids(std::span<const Solid> solids, double cell_m) {
  SolidIndex ix;
  ix.solids = solids;
  ix.cell_m = cell_m;
  Vec3 lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
  for (const Solid& s : solids) {
    Vec3 slo{1e300, 1e300, 1e300}, shi{-1e300, -1e300, -1e300};
    for (const Polygon& p : polygons_of(s)) {
      for (const Vec3& v : p.loop) {
        slo = {std::min(slo.x, v.x), std::min(slo.y, v.y), std::min(slo.z, v.z)};
        shi = {std::max(shi.x, v.x), std::max(shi.y, v.y), std::max(shi.z, v.z)};
      }
    }
    ix.lo.push_back(slo);
    ix.hi.push_back(shi);
    lo = {std::min(lo.x, slo.x), std::min(lo.y, slo.y), std::min(lo.z, slo.z)};
    hi = {std::max(hi.x, shi.x), std::max(hi.y, shi.y), std::max(hi.z, shi.z)};
  }
  if (solids.empty()) return ix;
  ix.origin = lo;
  ix.nx = static_cast<int64_t>(std::floor((hi.x - lo.x) / cell_m)) + 1;
  ix.ny = static_cast<int64_t>(std::floor((hi.y - lo.y) / cell_m)) + 1;
  ix.nz = static_cast<int64_t>(std::floor((hi.z - lo.z) / cell_m)) + 1;
  const auto cells = static_cast<size_t>(ix.nx * ix.ny * ix.nz);
  std::vector<uint32_t> count(cells + 1, 0);
  auto each_cell = [&](size_t s, auto&& fn) {
    const auto c0 = [&](double v, double o) {
      return static_cast<int64_t>(std::floor((v - o) / cell_m));
    };
    for (int64_t z = c0(ix.lo[s].z, lo.z); z <= c0(ix.hi[s].z, lo.z); ++z) {
      for (int64_t y = c0(ix.lo[s].y, lo.y); y <= c0(ix.hi[s].y, lo.y); ++y) {
        for (int64_t x = c0(ix.lo[s].x, lo.x); x <= c0(ix.hi[s].x, lo.x); ++x) {
          fn(static_cast<size_t>((z * ix.ny + y) * ix.nx + x));
        }
      }
    }
  };
  for (size_t s = 0; s < solids.size(); ++s) each_cell(s, [&](size_t c) { ++count[c + 1]; });
  for (size_t c = 0; c < cells; ++c) count[c + 1] += count[c];
  ix.start = count;
  ix.ids.resize(count[cells]);
  std::vector<uint32_t> fill(count.begin(), count.end() - 1);
  for (size_t s = 0; s < solids.size(); ++s) {
    each_cell(s, [&](size_t c) { ix.ids[fill[c]++] = static_cast<uint32_t>(s); });
  }
  return ix;
}

int solid_at(const SolidIndex& ix, const Vec3& p, int skip, double margin) {
  if (ix.start.empty()) return -1;
  const auto x = static_cast<int64_t>(std::floor((p.x - ix.origin.x) / ix.cell_m));
  const auto y = static_cast<int64_t>(std::floor((p.y - ix.origin.y) / ix.cell_m));
  const auto z = static_cast<int64_t>(std::floor((p.z - ix.origin.z) / ix.cell_m));
  if (x < 0 || y < 0 || z < 0 || x >= ix.nx || y >= ix.ny || z >= ix.nz) return -1;
  const auto c = static_cast<size_t>((z * ix.ny + y) * ix.nx + x);
  for (uint32_t k = ix.start[c]; k < ix.start[c + 1]; ++k) {
    const uint32_t s = ix.ids[k];
    if (static_cast<int>(s) == skip) continue;
    if (contains(ix.solids[s], p, margin)) return static_cast<int>(s);
  }
  return -1;
}

std::vector<BevelResult> bevel_all(std::span<const Solid> solids, const BevelSpec& spec) {
  const SolidIndex ix = index_solids(solids);
  std::vector<BevelResult> out;
  out.reserve(solids.size());
  for (size_t s = 0; s < solids.size(); ++s) {
    // A probe sits `off` (at least 2 mm) past a face; a flush neighbour holds it by that much.
    const IndexCtx ctx{&ix, static_cast<int>(s), 0.0005};
    out.push_back(bevel(solids[s], spec, index_at, &ctx));
  }
  return out;
}

}  // namespace ge::detail
