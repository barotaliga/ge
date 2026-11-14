// The aim spread (aim_spread.hpp).
//
// ge:summary The aim spread: samples where a firing player's line crosses a box, their spread
// ge:keywords aim-spread aimbot bot-likeness sample spread
// ge:skill shooter
// ge:test tests/shooter_aim_spread_test.cpp
#include "shooter/aim_spread.hpp"

#include <cmath>

namespace ge::shooter {

namespace {

bool alive(const Edict& e) { return !e.free && e.solid != kSolidNot && e.health > 0; }

}  // namespace

bool aim_spread_sample(AimSpread& s, Vec3f eye, Vec3f forward, std::span<const AimBox> boxes) {
  // The view's sideways axis in the level's plan (none when looking straight up or down).
  Vec3f side{forward.y, -forward.x, 0};
  if (normalize(side) == 0) return false;
  bool found = false;
  float best_t = 0, best_x = 0, best_y = 0;
  for (const AimBox& b : boxes) {
    const Vec3f mid = (b.lo + b.hi) * 0.5f;
    const float half_w = (b.hi.x - b.lo.x) * 0.5f, half_h = (b.hi.z - b.lo.z) * 0.5f;
    if (half_w <= 0 || half_h <= 0) continue;
    const Vec3f to = mid - eye;
    const float t = dot(to, forward);
    if (t <= 0) continue;  // behind the eye
    const Vec3f miss = eye + forward * t - mid;
    const float x = dot(miss, side) / half_w, y = miss.z / half_h;
    if (x < -1 || x > 1 || y < -1 || y > 1) continue;  // the line passes beside the box
    if (found && t >= best_t) continue;
    found = true;
    best_t = t;
    best_x = x;
    best_y = y;
  }
  if (!found) return false;
  s.samples++;
  s.sum_x += best_x;
  s.sum_y += best_y;
  s.sum_xx += static_cast<double>(best_x) * best_x;
  s.sum_yy += static_cast<double>(best_y) * best_y;
  return true;
}

void aim_spread_frame(const Server& sv, std::span<AimSpread* const> spreads) {
  AimBox boxes[kMaxClients];
  int box_ent[kMaxClients];
  int n = 0;
  for (int i = 0; i < static_cast<int>(sv.clients.size()) && i < kMaxClients; i++) {
    const int e = i + 1;
    if (!sv.clients[static_cast<size_t>(i)].active || e >= static_cast<int>(sv.edicts.size()))
      continue;
    const Edict& p = sv.edicts[static_cast<size_t>(e)];
    if (!alive(p)) continue;
    boxes[n] = {p.origin + p.mins, p.origin + p.maxs};
    box_ent[n] = e;
    n++;
  }
  for (int i = 0; i < static_cast<int>(sv.clients.size()) && i < static_cast<int>(spreads.size());
       i++) {
    const int e = i + 1;
    if (!sv.clients[static_cast<size_t>(i)].active || e >= static_cast<int>(sv.edicts.size()))
      continue;
    const Edict& p = sv.edicts[static_cast<size_t>(e)];
    AimSpread* into = spreads[static_cast<size_t>(i)];
    if (into == nullptr || !alive(p) || p.button0 == 0) continue;
    AimBox others[kMaxClients];
    int m = 0;
    for (int k = 0; k < n; k++)
      if (box_ent[k] != e) others[m++] = boxes[k];
    const Vec3f forward = angle_vectors(p.v_angle).forward;
    aim_spread_sample(*into, p.origin + p.view_ofs, forward,
                      std::span<const AimBox>(others, static_cast<size_t>(m)));
  }
}

double aim_spread_rms(const AimSpread& s) {
  if (s.samples < kAimSpreadLeast) return -1;
  const auto n = static_cast<double>(s.samples);
  const double mx = s.sum_x / n, my = s.sum_y / n;
  const double var = s.sum_xx / n - mx * mx + s.sum_yy / n - my * my;
  return var > 0 ? std::sqrt(var) : 0;
}

}  // namespace ge::shooter
