// ge:summary The hop reader: velocity by differences, the ground traced, hops chained, the summary
// ge:keywords read_hops hops chain takeoff turn-share view-lag ground-frames percentile
// ge:skill shooter bots
// ge:test tests/shooter_hops_test.cpp
#include "shooter/hops.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"

namespace ge::shooter {

namespace {

constexpr float kDeg = static_cast<float>(180.0 / core::kPi);
constexpr float kRise = 1.5f;    // a takeoff rises this much into the next frame, units
constexpr float kMoving = 10;    // the speed's percentiles count frames above this, units/s
constexpr float kFloorZ = 0.7f;  // a plane this flat is ground (the move's own rule)

float heading_of(Vec3f v) {
  return static_cast<float>(core::libm::atan2(static_cast<double>(v.y), static_cast<double>(v.x))) *
         kDeg;
}

float wrap(float d) {
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return d;
}

float across(Vec3f v) { return root_of(v.x * v.x + v.y * v.y); }

// The q-quantile of `v` (sorted in place); 0 when empty.
float quantile(std::vector<float>& v, float q) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  const auto i = static_cast<size_t>(ge::core::num::round(q * static_cast<double>(v.size() - 1)));
  return v[std::min(i, v.size() - 1)];
}

}  // namespace

float max_turn(float speed, double cmd_hz) {
  if (speed <= 0) return 0;
  return static_cast<float>(core::libm::atan2(30.0, static_cast<double>(speed)) * cmd_hz) * kDeg;
}

void read_hops(std::span<const TrackPoint> track, const MoveWorld& world, const HopSettings& set,
               HopReading& out) {
  out.frames.assign(track.size(), HopFrame{});
  out.hops.clear();
  out.summary = HopSummary{};
  const size_t n = track.size();
  if (n == 0) return;
  // Cuts: a frame is cut when it or the step into it or out of it breaks.
  std::vector<uint8_t> breaks(n, 0);  // the step from i - 1 into i breaks
  for (size_t i = 1; i < n; i++) {
    const Vec3f d = track[i].origin - track[i - 1].origin;
    const double dt = track[i].t - track[i - 1].t;
    breaks[i] =
        across(d) > set.teleport || dt <= 0 || dt > set.gap_s || track[i].dead || track[i - 1].dead;
  }
  for (size_t i = 0; i < n; i++) {
    HopFrame& f = out.frames[i];
    const TrackPoint& p = track[i];
    f.t = p.t;
    f.origin = p.origin;
    f.yaw = p.yaw;
    const bool cut = p.dead || i == 0 || i + 1 >= n || breaks[i] || breaks[i + 1];
    if (cut) continue;
    const float dt = static_cast<float>(track[i + 1].t - track[i - 1].t);
    f.velocity = (track[i + 1].origin - track[i - 1].origin) * (1 / dt);
    f.speed = across(f.velocity);
    f.heading = heading_of(f.velocity);
    f.lag = wrap(f.yaw - f.heading);
    const HullTrace tr =
        player_trace(world, p.origin, {p.origin.x, p.origin.y, p.origin.z - set.ground_probe});
    const bool ground = !tr.startsolid && tr.fraction < 1 && tr.plane.normal.z >= kFloorZ;
    f.phase = ground ? HopPhase::ground : HopPhase::air;
  }
  // The turn: the heading's change across the frame's neighbours, where all three are read.
  for (size_t i = 1; i + 1 < n; i++) {
    HopFrame& f = out.frames[i];
    const HopFrame &a = out.frames[i - 1], &b = out.frames[i + 1];
    if (f.phase == HopPhase::cut || a.phase == HopPhase::cut || b.phase == HopPhase::cut) continue;
    if (a.speed < kMoving || b.speed < kMoving) continue;
    f.turn = wrap(b.heading - a.heading) / static_cast<float>(b.t - a.t);
  }
  // Takeoffs and hops.
  int32_t last_land = -1;
  for (size_t i = 1; i + 1 < n; i++) {
    HopFrame& f = out.frames[i];
    const HopFrame& prev = out.frames[i - 1];
    if (f.phase != HopPhase::air || prev.phase != HopPhase::ground) continue;
    if (track[i + 1].origin.z - track[i].origin.z <= kRise) continue;
    f.takeoff = true;
    Hop h;
    h.takeoff = static_cast<int32_t>(i);
    size_t l = i;
    int8_t side = 0;
    while (l < n && out.frames[l].phase == HopPhase::air) {
      const float tr = out.frames[l].turn;
      const int8_t s = tr > 15 ? 1 : tr < -15 ? -1 : 0;
      if (s != 0 && side != 0 && s != side) h.switches++;
      if (s != 0) side = s;
      if (l + 1 < n && out.frames[l + 1].phase == HopPhase::air)
        h.turned += wrap(out.frames[l + 1].heading - out.frames[l].heading);
      l++;
    }
    if (l >= n || out.frames[l].phase != HopPhase::ground) continue;  // cut in the air
    h.land = static_cast<int32_t>(l);
    h.air_s = static_cast<float>(track[l].t - track[i].t);
    h.speed_off = out.frames[i + 1].speed;
    h.speed_land = out.frames[l - 1].speed;
    // Frames stood since the last landing, when nothing cut it.
    if (last_land >= 0) {
      bool clean = true;
      for (int32_t k = last_land; k < h.takeoff && clean; k++)
        clean = out.frames[static_cast<size_t>(k)].phase == HopPhase::ground;
      if (clean) h.ground_before = h.takeoff - last_land;
    }
    last_land = h.land;
    out.hops.push_back(h);
  }

  // The summary.
  HopSummary& s = out.summary;
  s.seconds = track[n - 1].t - track[0].t;
  s.frames = static_cast<int32_t>(n);
  s.jumps = static_cast<int32_t>(out.hops.size());
  int32_t chain = 0, run = 0;
  std::vector<float> gains;
  for (size_t k = 0; k < out.hops.size(); k++) {
    Hop& h = out.hops[k];
    const bool chained = h.ground_before >= 0 && h.ground_before <= set.chain_frames &&
                         h.air_s <= static_cast<float>(set.chain_gap_s);
    if (h.ground_before >= 0)
      s.ground_before[static_cast<size_t>(std::min(h.ground_before, kGroundBins - 1))]++;
    if (chained) {
      s.chained++;
      run++;
      if (h.air_s > 0.2f) gains.push_back((h.speed_land - h.speed_off) / h.air_s);
    } else {
      chain++;
      run = 1;
    }
    h.chain = chain;
    s.longest_chain = std::max(s.longest_chain, run);
  }
  s.chains = chain;
  int32_t read = 0, air = 0;
  std::vector<float> speeds, shares, lags;
  for (const HopFrame& f : out.frames) {
    if (f.phase == HopPhase::cut) continue;
    read++;
    if (f.speed > kMoving) speeds.push_back(f.speed);
    if (f.phase != HopPhase::air) continue;
    air++;
    if (f.speed <= set.fast || f.turn == 0) continue;
    shares.push_back((f.turn < 0 ? -f.turn : f.turn) / max_turn(f.speed, set.cmd_hz));
    lags.push_back(f.lag < 0 ? -f.lag : f.lag);
    const int bin = static_cast<int>((f.lag + 180) / 10);
    s.lag[static_cast<size_t>(std::clamp(bin, 0, kLagBins - 1))]++;
  }
  s.air_share = read > 0 ? static_cast<float>(air) / static_cast<float>(read) : 0;
  s.speed_top = speeds.empty() ? 0 : *std::max_element(speeds.begin(), speeds.end());
  s.speed_p90 = quantile(speeds, 0.9f);
  s.speed_p50 = quantile(speeds, 0.5f);
  s.turn_share_p50 = quantile(shares, 0.5f);
  s.lag_p50 = quantile(lags, 0.5f);
  s.gain_per_s = quantile(gains, 0.5f);
}

}  // namespace ge::shooter
