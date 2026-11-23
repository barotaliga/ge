// The own lightning beam traced every frame along the aim (own_beam.hpp).
//
// ge:summary The own lightning beam traced every frame along the aim (cl_fakeshaft)
// ge:keywords own_beam fakeshaft trace body-box
// ge:skill render shooter
#include "shooters_edge/client/own_beam.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace shooters_edge::client {

namespace {

sh::Vec3f unit(sh::Vec3f v) {
  const float n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  return n > 1e-6f ? sh::Vec3f{v.x / n, v.y / n, v.z / n} : sh::Vec3f{0, 0, 0};
}

bool zero(const sh::Vec3f& v) { return v.x == 0 && v.y == 0 && v.z == 0; }

// Two hits on one face: the same plane (normals alike, and each on the other's plane).
bool same_face(const sh::Vec3f& a, const sh::Vec3f& na, const sh::Vec3f& b, const sh::Vec3f& nb) {
  const sh::Vec3f d = {b.x - a.x, b.y - a.y, b.z - a.z};
  return dot(na, nb) > kSameFaceDot && std::abs(dot(d, na)) < kSameFaceU;
}

}  // namespace

bool beam_trace(const LoadedMap* map, const sh::Vec3f& from, const sh::Vec3f& dir,
                std::span<const Body> bodies, int own, sh::Vec3f& to, sh::Vec3f& normal) {
  if (map == nullptr || map->bsp.world.models.empty()) return false;
  const sh::Vec3f d = unit(dir);
  if (zero(d)) return false;
  const sh::Vec3f far = {from.x + d.x * kOwnBeamReachU, from.y + d.y * kOwnBeamReachU,
                         from.z + d.z * kOwnBeamReachU};
  const sh::HullTrace tr = sh::trace_hull(map->bsp.world.models[0].hulls[0], from, far);
  float best = tr.startsolid ? 0 : tr.fraction;
  normal = tr.fraction < 1 ? tr.plane.normal : sh::Vec3f{-d.x, -d.y, -d.z};
  // The living bodies in the way: the nearest box the line enters before the wall.
  for (const Body& body : bodies) {
    if (!body.alive || body.entity == own) continue;
    const float lo[3] = {body.origin.x - kBodyHalfU, body.origin.y - kBodyHalfU,
                         body.origin.z - kBodyLowU};
    const float hi[3] = {body.origin.x + kBodyHalfU, body.origin.y + kBodyHalfU,
                         body.origin.z + kBodyHighU};
    const float o[3] = {from.x, from.y, from.z};
    const float v[3] = {far.x - from.x, far.y - from.y, far.z - from.z};
    float enter = 0, leave = 1;
    int axis = -1;
    bool miss = false;
    for (int a = 0; a < 3 && !miss; ++a) {
      if (std::abs(v[a]) < 1e-6f) {
        miss = o[a] < lo[a] || o[a] > hi[a];
        continue;
      }
      float t0 = (lo[a] - o[a]) / v[a], t1 = (hi[a] - o[a]) / v[a];
      if (t0 > t1) std::swap(t0, t1);
      if (t0 > enter) enter = t0, axis = a;
      leave = std::min(leave, t1);
      miss = enter > leave;
    }
    if (miss || axis < 0 || enter >= best) continue;
    best = enter;
    float nv[3] = {0, 0, 0};
    nv[axis] = v[axis] > 0 ? -1.0f : 1.0f;
    normal = {nv[0], nv[1], nv[2]};
  }
  to = {from.x + (far.x - from.x) * best, from.y + (far.y - from.y) * best,
        from.z + (far.z - from.z) * best};
  return true;
}

bool own_beam(const LoadedMap* map, const sh::Vec3f& eye, const sh::Vec3f& view_dir,
              const sh::Vec3f& server_from, const sh::Vec3f& server_to, double share,
              std::span<const Body> bodies, int own, sh::Vec3f& from, sh::Vec3f& to,
              sh::Vec3f& normal) {
  if (map == nullptr || map->bsp.world.models.empty()) return false;
  const sh::Vec3f sv =
      unit({server_to.x - server_from.x, server_to.y - server_from.y, server_to.z - server_from.z});
  const sh::Vec3f vv = unit(view_dir);
  const float k = static_cast<float>(std::clamp(share, 0.0, 1.0));
  sh::Vec3f d =
      unit({sv.x + (vv.x - sv.x) * k, sv.y + (vv.y - sv.y) * k, sv.z + (vv.z - sv.z) * k});
  if (zero(d)) d = zero(vv) ? sv : vv;
  if (zero(d)) return false;
  from = {eye.x, eye.y, eye.z - kOwnBeamDropU};
  return beam_trace(map, from, d, bodies, own, to, normal);
}

int beam_edge(const LoadedMap* map, const sh::Vec3f& from, const sh::Vec3f& was_dir,
              const sh::Vec3f& dir, std::span<const Body> bodies, int own, sh::Vec3f at[2],
              sh::Vec3f n[2]) {
  sh::Vec3f a_at, a_n, b_at, b_n;
  sh::Vec3f lo = unit(was_dir), hi = unit(dir);
  if (zero(lo) || zero(hi)) return 0;
  if (!beam_trace(map, from, lo, bodies, own, a_at, a_n) ||
      !beam_trace(map, from, hi, bodies, own, b_at, b_n))
    return 0;
  if (same_face(a_at, a_n, b_at, b_n)) return 0;
  // Halve the turn between the two aims, keeping `lo` on the first face and `hi` off it, until
  // the two hits are a few units apart: the last point on the old face and the first on the new.
  for (int i = 0; i < kEdgeSteps; ++i) {
    const sh::Vec3f mid = unit({lo.x + hi.x, lo.y + hi.y, lo.z + hi.z});
    sh::Vec3f m_at, m_n;
    if (zero(mid) || !beam_trace(map, from, mid, bodies, own, m_at, m_n)) break;
    if (same_face(a_at, a_n, m_at, m_n)) {
      lo = mid;
      a_at = m_at;
    } else {
      hi = mid;
      b_at = m_at;
      b_n = m_n;
    }
    const sh::Vec3f gap = {b_at.x - a_at.x, b_at.y - a_at.y, b_at.z - a_at.z};
    if (dot(gap, gap) < kEdgeU * kEdgeU) break;
  }
  at[0] = a_at;
  n[0] = a_n;
  at[1] = b_at;
  n[1] = b_n;
  return 2;
}

}  // namespace shooters_edge::client
