// Thrown debris that bounces and settles (debris.hpp).
//
// ge:summary Thrown debris: the pool, the throw, the step's bounces, slides, rests and fades
// ge:keywords detail debris bounce slide rest fade landing
// ge:skill detail
// ge:test tests/detail_debris_test.cpp
#include "detail/debris.hpp"

#include <algorithm>
#include <cmath>

#include "detail/vec.hpp"

namespace ge::detail {

namespace {

// The piece's long axis laid in the plane of `n` (its roll about it kept), its body y as near the
// old one as the plane allows.
core::Quat laid_along(const core::Quat& turn, const Vec3& n) {
  const Vec3 long_axis = core::rotate(turn, {1, 0, 0});
  Vec3 a = sub(long_axis, mul(n, dot(long_axis, n)));
  a = dot(a, a) > 1e-12 ? normalized(a) : any_perpendicular(n);
  const Vec3 old_up = core::rotate(turn, {0, 1, 0});
  Vec3 up = sub(old_up, mul(a, dot(old_up, a)));
  up = dot(up, up) > 1e-12 ? normalized(up) : n;
  return turn_of_axes(a, up);
}

// Speed `s` along `v` taken off it (none left: zero).
Vec3 slowed(const Vec3& v, double s) {
  const double len = length(v);
  if (len <= s) return {0, 0, 0};
  return mul(v, (len - s) / len);
}

}  // namespace

core::Quat turn_of_axes(const Vec3& x, const Vec3& y) {
  const Vec3 z = cross(x, y);
  const double m00 = x.x, m10 = x.y, m20 = x.z;
  const double m01 = y.x, m11 = y.y, m21 = y.z;
  const double m02 = z.x, m12 = z.y, m22 = z.z;
  const double trace = m00 + m11 + m22;
  core::Quat q{0, 0, 0, 1};
  if (trace > 0) {
    const double s = std::sqrt(trace + 1) * 2;
    q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, s / 4};
  } else if (m00 > m11 && m00 > m22) {
    const double s = std::sqrt(1 + m00 - m11 - m22) * 2;
    q = {s / 4, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s};
  } else if (m11 > m22) {
    const double s = std::sqrt(1 + m11 - m00 - m22) * 2;
    q = {(m01 + m10) / s, s / 4, (m12 + m21) / s, (m02 - m20) / s};
  } else {
    const double s = std::sqrt(1 + m22 - m00 - m11) * 2;
    q = {(m02 + m20) / s, (m12 + m21) / s, s / 4, (m10 - m01) / s};
  }
  return core::normalize(q);
}

DebrisPool make_debris_pool(uint32_t capacity, std::span<const DebrisShape> shapes) {
  DebrisPool p;
  p.pieces.assign(capacity, Debris{});
  p.shapes.assign(shapes.begin(), shapes.end());
  if (p.shapes.empty()) p.shapes.push_back(DebrisShape{});
  return p;
}

int throw_debris(DebrisPool& pool, const DebrisThrow& t, double now_s) {
  if (pool.pieces.empty()) return -1;
  size_t slot = 0;
  for (size_t i = 0; i < pool.pieces.size(); ++i) {
    if (pool.pieces[i].serial == 0) {
      slot = i;
      break;
    }
    if (pool.pieces[i].serial < pool.pieces[slot].serial) slot = i;  // the oldest
  }
  Debris& d = pool.pieces[slot];
  d = Debris{};
  d.at = t.at;
  d.vel = t.vel;
  d.turn = t.turn;
  d.spin = t.spin;
  d.born = now_s;
  d.serial = ++pool.serial;
  if (d.serial == 0) d.serial = ++pool.serial;  // 0 is a free slot's
  d.rng = core::make_rng(d.serial * 0x9e3779b9u);
  d.kind = t.kind < pool.shapes.size() ? t.kind : 0;
  return static_cast<int>(slot);
}

void step_debris(DebrisPool& pool, const Receivers* r, ReceiverScratch& scratch, double now_s,
                 double dt, std::vector<DebrisLanding>* landings, double gravity_mps2,
                 double step_s) {
  if (dt <= 0) return;
  const int steps = std::max(1, static_cast<int>(std::ceil(dt / step_s)));
  const double h = dt / steps;
  for (Debris& d : pool.pieces) {
    if (d.serial == 0) continue;
    const DebrisShape& shape = pool.shapes[d.kind];
    // Past its life (lying, or in the air too long): let go.
    if ((d.still >= 0 && now_s - d.still >= shape.life_s) ||
        (d.still < 0 && now_s - d.born >= shape.fly_s)) {
      d.serial = 0;
      continue;
    }
    if (d.still >= 0) continue;
    for (int k = 0; k < steps && d.still < 0; ++k) {
      const double at_s = now_s - dt + h * (k + 1);
      d.vel.y -= gravity_mps2 * h;
      const Vec3 move = mul(d.vel, h);
      const double len = length(move);
      Hit hit;
      if (r != nullptr && len > 0)
        hit = raycast(*r, d.at, mul(move, 1 / len), len + shape.radius_m, scratch);
      if (!hit.hit) {
        d.at = add(d.at, move);
        d.lying = false;
      } else {
        const Vec3 dir = mul(move, 1 / len);
        Vec3 n = normal_of(*r->mesh, hit.tri);
        if (dot(n, dir) > 0) n = mul(n, -1);
        d.at = mad(hit.at, n, shape.radius_m);
        const double vn = dot(d.vel, n);
        if (vn < 0) {
          const double blow = -vn;
          const Vec3 along = sub(d.vel, mul(n, vn));
          const bool floor = n.y >= kFloorUp;
          if (blow >= shape.heard_mps || !floor) {
            // A bounce: some of the blow back, some of the slide lost, a tumble of its own.
            d.vel = sub(mul(along, 1 - shape.friction), mul(n, vn * shape.restitution));
            const double kick = shape.tumble * blow;
            d.spin = {d.spin.x * shape.spin_keep + core::range(d.rng, -kick, kick),
                      d.spin.y * shape.spin_keep + core::range(d.rng, -kick, kick),
                      d.spin.z * shape.spin_keep + core::range(d.rng, -kick, kick)};
            d.lying = false;
            if (d.bounces < 255) ++d.bounces;
            if (landings != nullptr && blow >= shape.heard_mps)
              landings->push_back({d.at, n, hit.tri, blow, d.kind, d.serial, d.bounces});
          } else {
            // On a floor, too slow to bounce: laid along it, sliding and rolling about its axis.
            d.vel = slowed(along, shape.slide_mps2 * h);
            d.turn = laid_along(d.turn, n);
            d.lying = true;
            const Vec3 long_axis = core::rotate(d.turn, {1, 0, 0});
            const Vec3 roll_world = mul(cross(n, d.vel), 1 / shape.radius_m);
            d.spin = {dot(roll_world, long_axis), 0, 0};
            if (length(d.vel) < shape.rest_mps) {
              d.vel = {0, 0, 0};
              d.spin = {0, 0, 0};
              d.still = at_s;
            }
          }
        }
      }
      if (d.still < 0) d.turn = core::integrate(d.turn, d.spin.x, d.spin.y, d.spin.z, h);
    }
  }
}

double debris_shown(const DebrisPool& pool, const Debris& d, double now_s) {
  if (d.serial == 0) return 0;
  if (d.still < 0) return 1;
  const DebrisShape& shape = pool.shapes[d.kind];
  const double left = shape.life_s - (now_s - d.still);
  if (left <= 0) return 0;
  if (shape.fade_s <= 0 || left >= shape.fade_s) return 1;
  return left / shape.fade_s;
}

size_t debris_count(const DebrisPool& pool) {
  size_t n = 0;
  for (const Debris& d : pool.pieces) n += d.serial != 0 ? 1 : 0;
  return n;
}

}  // namespace ge::detail
