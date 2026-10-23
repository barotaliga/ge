// The brushes' planes and corners as they are added, and the swept-box clip against each one.
//
// ge:summary Box world's brush building (boxes, ramps, stairs) and the swept-box clip
// ge:keywords move box-world brush clip sweep enter leave fraction skin
// ge:skill move
// ge:test tests/move_box_world_test.cpp
#include "move/box_world.hpp"

#include <cmath>

namespace ge::move {

namespace {

void begin(BoxWorld& w, const Vec3& lo, const Vec3& hi) {
  Brush b;
  b.first_plane = static_cast<uint32_t>(w.planes.size());
  b.first_corner = static_cast<uint32_t>(w.corners.size());
  b.lo = lo;
  b.hi = hi;
  w.brushes.push_back(b);
}

void plane(BoxWorld& w, const Vec3& n, double d) {
  w.planes.push_back({n, d});
  w.brushes.back().planes++;
}

void corner(BoxWorld& w, const Vec3& p) {
  w.corners.push_back(p);
  w.brushes.back().corners++;
}

void axial_planes(BoxWorld& w, const Vec3& lo, const Vec3& hi) {
  plane(w, {1, 0, 0}, hi.x);
  plane(w, {-1, 0, 0}, -lo.x);
  plane(w, {0, 1, 0}, hi.y);
  plane(w, {0, -1, 0}, -lo.y);
  plane(w, {0, 0, 1}, hi.z);
  plane(w, {0, 0, -1}, -lo.z);
}

}  // namespace

void add_box(BoxWorld& w, const Vec3& lo, const Vec3& hi) {
  begin(w, lo, hi);
  axial_planes(w, lo, hi);
  for (int i = 0; i < 8; i++) {
    corner(w, {(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z});
  }
}

void add_ramp(BoxWorld& w, const Vec3& lo, const Vec3& hi, Rise rise) {
  begin(w, lo, hi);
  axial_planes(w, lo, hi);  // the bounds, and the bevels the slanted face needs
  const double h = hi.y - lo.y;
  // The slanted face: up = (h / run) along the rise. Its normal points up and back down the rise.
  const bool along_x = rise == Rise::px || rise == Rise::nx;
  const double run = along_x ? hi.x - lo.x : hi.z - lo.z;
  const double sign = (rise == Rise::px || rise == Rise::pz) ? 1.0 : -1.0;
  const double len = std::sqrt(run * run + h * h);
  Vec3 n{0, run / len, 0};
  if (along_x) {
    n.x = -sign * h / len;
  } else {
    n.z = -sign * h / len;
  }
  // The low edge lies on the face: at the rise's start, at lo.y.
  Vec3 low = lo;
  if (sign < 0) {
    if (along_x) {
      low.x = hi.x;
    } else {
      low.z = hi.z;
    }
  }
  plane(w, n, dot(n, low));
  // Corners: the four on the floor, the two along the top edge at the rise's end.
  for (int i = 0; i < 4; i++) corner(w, {(i & 1) ? hi.x : lo.x, lo.y, (i & 2) ? hi.z : lo.z});
  for (int i = 0; i < 2; i++) {
    Vec3 c{0, hi.y, 0};
    if (along_x) {
      c.x = sign > 0 ? hi.x : lo.x;
      c.z = i ? hi.z : lo.z;
    } else {
      c.z = sign > 0 ? hi.z : lo.z;
      c.x = i ? hi.x : lo.x;
    }
    corner(w, c);
  }
}

void add_stairs(BoxWorld& w, const Vec3& start, int count, double rise, double tread,
                double width) {
  for (int i = 0; i < count; i++) {
    const Vec3 lo{start.x, start.y, start.z + tread * i};
    const Vec3 hi{start.x + width, start.y + rise * (i + 1), start.z + tread * (i + 1)};
    add_box(w, lo, hi);
  }
}

Hit trace_box_world(const BoxWorld& w, const Box& box, const Vec3& from, const Vec3& to) {
  // The box's centre swept, each plane pushed out by the box's half extents along its normal.
  const Vec3 mid = scale(add(box.mins, box.maxs), 0.5);
  const Vec3 ext = scale(sub(box.maxs, box.mins), 0.5);
  const Vec3 c0 = add(from, mid);
  const Vec3 c1 = add(to, mid);
  Hit best;
  best.end = to;
  for (const Brush& b : w.brushes) {
    double enter = -1, leave = 1;
    bool starts_out = false, gets_out = false, misses = false;
    Vec3 normal{0, 1, 0};
    for (uint32_t i = 0; i < b.planes; i++) {
      const Plane& p = w.planes[b.first_plane + i];
      const double reach =
          std::fabs(p.n.x) * ext.x + std::fabs(p.n.y) * ext.y + std::fabs(p.n.z) * ext.z;
      const double d1 = dot(c0, p.n) - (p.d + reach);
      const double d2 = dot(c1, p.n) - (p.d + reach);
      if (d2 > 0) gets_out = true;
      if (d1 > 0) starts_out = true;
      if (d1 > 0 && d2 >= d1) {
        misses = true;  // in front of this plane all the way
        break;
      }
      if (d1 <= 0 && d2 <= 0) continue;
      if (d1 > d2) {  // entering through this plane
        const double f = (d1 - w.skin) / (d1 - d2);
        if (f > enter) {
          enter = f;
          normal = p.n;
        }
      } else {  // leaving through it
        const double f = (d1 + w.skin) / (d1 - d2);
        if (f < leave) leave = f;
      }
    }
    if (misses) continue;
    if (!starts_out) {
      best.start_solid = true;
      if (!gets_out) {
        best.all_solid = true;
        best.fraction = 0;
        best.end = from;
        return best;
      }
      continue;
    }
    if (enter < leave && enter > -1 && enter < best.fraction) {
      best.fraction = enter < 0 ? 0 : enter;
      best.normal = normal;
    }
  }
  if (best.fraction < 1) best.end = add_scaled(from, sub(to, from), best.fraction);
  return best;
}

namespace {
Hit trace_thunk(const void* ctx, const Box& box, const Vec3& from, const Vec3& to) {
  return trace_box_world(*static_cast<const BoxWorld*>(ctx), box, from, to);
}
}  // namespace

World as_world(const BoxWorld& w) { return World{&w, trace_thunk}; }

}  // namespace ge::move
