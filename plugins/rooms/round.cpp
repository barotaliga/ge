// Curves drawn round (curves.hpp `round_curves`): each triangle of a curve's faces cut at the
// curve's angular grid, every corner moved onto the cylinder through the facets' corners and
// given the cylinder's normal.
//
// ge:summary Draws found curves round: triangles cut at an angular grid, moved onto the cylinder
// ge:keywords round rounding smooth arch pillar cylinder subdivide angular-grid normal render
// ge:skill rooms
// ge:test tests/rooms_curves_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/libm/libm.hpp"
#include "rooms/curves.hpp"

namespace ge::rooms {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Frame {
  double c[3], a[3], u[3], v[3];
  double corner = 0;  // the radius through the facets' corners
  double sign = 1;    // the normal's: out of a pillar, into an arch
};

double dot3(const double* x, const double* y) { return x[0] * y[0] + x[1] * y[1] + x[2] * y[2]; }

Frame frame_of(const Curve& c) {
  Frame f;
  for (size_t k = 0; k < 3; ++k) f.c[k] = c.centre[k], f.a[k] = c.axis[k];
  const double ref[3] = {std::fabs(f.a[1]) < 0.9 ? 0.0 : 1.0, std::fabs(f.a[1]) < 0.9 ? 1.0 : 0.0,
                         0.0};
  const double ra = dot3(ref, f.a);
  double l = 0;
  for (size_t k = 0; k < 3; ++k) f.u[k] = ref[k] - f.a[k] * ra, l += f.u[k] * f.u[k];
  l = std::sqrt(l);
  for (double& x : f.u) x /= l;
  f.v[0] = f.a[1] * f.u[2] - f.a[2] * f.u[1];
  f.v[1] = f.a[2] * f.u[0] - f.a[0] * f.u[2];
  f.v[2] = f.a[0] * f.u[1] - f.a[1] * f.u[0];
  // The turn between neighbouring facets, and the corners' radius it gives the facets' circle.
  const double step = c.sweep_deg >= 360 ? 2 * kPi / double(c.facets)
                                         : c.sweep_deg * kPi / 180 / double(c.facets - 1);
  f.corner = c.radius / core::libm::cos(step / 2);
  f.sign = c.concave ? -1 : 1;
  return f;
}

// The angle of `p` round the frame's axis.
double angle_of(const Frame& f, const double* p) {
  const double d[3] = {p[0] - f.c[0], p[1] - f.c[1], p[2] - f.c[2]};
  return core::libm::atan2(dot3(d, f.v), dot3(d, f.u));
}

struct Point {
  double p[3];
  float st[2];
};

}  // namespace

BrushMesh round_curves(const Level& level, const BrushMesh& mesh, std::span<const Curve> curves,
                       double segment_deg) {
  std::vector<int32_t> curve_of(level.faces.size(), -1);
  for (size_t i = 0; i < curves.size(); ++i) {
    for (const uint32_t f : curves[i].faces) curve_of[f] = int32_t(i);
  }
  std::vector<Frame> frames;
  for (const Curve& c : curves) frames.push_back(frame_of(c));
  const double seg = std::max(segment_deg, 0.5) * kPi / 180;

  BrushMesh out;
  out.pos = mesh.pos, out.nrm = mesh.nrm, out.st = mesh.st;
  out.unbounded = mesh.unbounded;
  const auto keep = [&out, &mesh](size_t t) {
    out.material.push_back(mesh.material[t]);
    out.contents.push_back(mesh.contents[t]);
    out.hidden.push_back(mesh.hidden[t]);
    out.brush.push_back(mesh.brush[t]);
    out.face.push_back(mesh.face[t]);
  };
  for (size_t t = 0; t < mesh.triangles(); ++t) {
    const int32_t ci = curve_of[mesh.face[t]];
    if (ci < 0) {
      out.idx.insert(out.idx.end(), {mesh.idx[t * 3], mesh.idx[t * 3 + 1], mesh.idx[t * 3 + 2]});
      keep(t);
      continue;
    }
    const Frame& f = frames[size_t(ci)];
    std::vector<Point> poly(3);
    double lo = 0, hi = 0, a0 = 0;
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = mesh.idx[t * 3 + k];
      for (size_t a = 0; a < 3; ++a) poly[k].p[a] = double(mesh.pos[v * 3 + a]);
      poly[k].st[0] = mesh.st[v * 2], poly[k].st[1] = mesh.st[v * 2 + 1];
      double a = angle_of(f, poly[k].p);
      if (k == 0) {
        a0 = a, lo = hi = a;
        continue;
      }
      while (a - a0 > kPi) a -= 2 * kPi;
      while (a - a0 < -kPi) a += 2 * kPi;
      lo = std::min(lo, a), hi = std::max(hi, a);
    }
    // Cut at every grid angle inside the triangle's span: the piece before each cut is done.
    std::vector<std::vector<Point>> pieces;
    // The grid's angles are whole multiples of the segment, so neighbours cut at the same ones.
    for (auto cut = int64_t(std::ceil(lo / seg + 1e-9)); double(cut) * seg < hi - 1e-9; ++cut) {
      const double g = double(cut) * seg;
      // The plane through the axis at angle g, its normal pointing to growing angles.
      const double n[3] = {-core::libm::sin(g) * f.u[0] + core::libm::cos(g) * f.v[0],
                           -core::libm::sin(g) * f.u[1] + core::libm::cos(g) * f.v[1],
                           -core::libm::sin(g) * f.u[2] + core::libm::cos(g) * f.v[2]};
      std::vector<Point> before, after;
      for (size_t k = 0; k < poly.size(); ++k) {
        const Point& p = poly[k];
        const Point& q = poly[(k + 1) % poly.size()];
        const double dp[3] = {p.p[0] - f.c[0], p.p[1] - f.c[1], p.p[2] - f.c[2]};
        const double dq[3] = {q.p[0] - f.c[0], q.p[1] - f.c[1], q.p[2] - f.c[2]};
        const double sp = dot3(dp, n), sq = dot3(dq, n);
        if (sp <= 0) before.push_back(p);
        if (sp >= 0) after.push_back(p);
        if ((sp < 0 && sq > 0) || (sp > 0 && sq < 0)) {
          const double w = sp / (sp - sq);
          Point x;
          for (size_t a = 0; a < 3; ++a) x.p[a] = p.p[a] + (q.p[a] - p.p[a]) * w;
          for (size_t a = 0; a < 2; ++a) x.st[a] = p.st[a] + float(w) * (q.st[a] - p.st[a]);
          before.push_back(x);
          after.push_back(x);
        }
      }
      if (before.size() >= 3) pieces.push_back(std::move(before));
      poly.swap(after);
      if (poly.size() < 3) break;
    }
    if (poly.size() >= 3) pieces.push_back(std::move(poly));
    for (const std::vector<Point>& piece : pieces) {
      const auto base = uint32_t(out.pos.size() / 3);
      for (const Point& c : piece) {
        const double d[3] = {c.p[0] - f.c[0], c.p[1] - f.c[1], c.p[2] - f.c[2]};
        const double along = dot3(d, f.a);
        double r[3] = {d[0] - f.a[0] * along, d[1] - f.a[1] * along, d[2] - f.a[2] * along};
        const double l = std::sqrt(dot3(r, r));
        for (double& x : r) x = l > 1e-12 ? x / l : 0;
        for (size_t a = 0; a < 3; ++a) {
          out.pos.push_back(float(f.c[a] + f.a[a] * along + r[a] * f.corner));
          out.nrm.push_back(float(r[a] * f.sign));
        }
        out.st.push_back(c.st[0]), out.st.push_back(c.st[1]);
      }
      for (size_t i = 1; i + 1 < piece.size(); ++i) {
        out.idx.insert(out.idx.end(), {base, base + uint32_t(i), base + uint32_t(i + 1)});
        keep(t);
      }
    }
  }
  return out;
}

}  // namespace ge::rooms
