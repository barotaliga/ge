// Rocket jumps read off a recorded player (rocket_jumps.hpp): the shots matched to the bursts by
// him, the push checked, the flight followed to its landing and the keys read off the move.
//
// ge:summary Rocket jumps read off a recorded player: the detector, the flight and the keys
// ge:keywords rocket-jump rj flip demo read keys wish landing
// ge:skill shooter bots
// ge:test tests/shooter_rocket_jumps_test.cpp
#include "shooter/rocket_jumps.hpp"

#include <algorithm>
#include <array>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::shooter {

namespace {

constexpr float kDeg = static_cast<float>(180.0 / core::kPi);
constexpr float kPress = 0.383f;      // a key's axis within 67.5 degrees of the wish (cos 67.5)
constexpr float kMiddle = 4;          // a player's middle over his origin (his box's), units
constexpr float kFeetLook = 60;       // within this off the flight the view looked the way it flew
constexpr float kFlipLook = 120;      // past this it looked back
constexpr float kTurnedBack = 45;     // a flip's view within this of the flight has turned back
constexpr float kFlightHeading = 50;  // the flight's heading read off its velocity above this
constexpr float kAlongView = 0.7f;    // a rocket within 45 degrees of the view flew along it
constexpr float kAlongLeast = 12;     // a rocket's way read only when it flew this far, units
constexpr float kPushAway = 0.5f;     // the push within 60 degrees of straight away from the burst
constexpr double kBeforeBurst_s = 0.03;  // the speed before a blast read this long before it, s

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

// The last frame at or before `t` (0 when none).
size_t frame_at(std::span<const TrackPoint> track, double t) {
  const auto it = std::upper_bound(track.begin(), track.end(), t,
                                   [](double v, const TrackPoint& p) { return v < p.t; });
  return it == track.begin() ? 0 : static_cast<size_t>(it - track.begin()) - 1;
}

// The frames `window` either side of `i` (clamped), none of them cut.
bool span_round(const HopReading& hops, size_t i, double window, size_t& a, size_t& b) {
  const std::vector<HopFrame>& f = hops.frames;
  a = i;
  b = i;
  while (a > 0 && f[a - 1].phase != HopPhase::cut && f[i].t - f[a - 1].t <= window) a--;
  while (b + 1 < f.size() && f[b + 1].phase != HopPhase::cut && f[b + 1].t - f[i].t <= window) b++;
  return b > a && f[a].phase != HopPhase::cut && f[b].phase != HopPhase::cut;
}

// The wish at frame `i`: the acceleration across, the ground's friction put back.
bool wish_at(const HopReading& hops, size_t i, const RocketJumpReadSettings& set, Vec3f& out) {
  size_t a = 0, b = 0;
  if (!span_round(hops, i, set.window_s, a, b)) return false;
  const HopFrame &fa = hops.frames[a], &fb = hops.frames[b], &f = hops.frames[i];
  const float dt = static_cast<float>(fb.t - fa.t);
  if (dt <= 0) return false;
  out = (fb.velocity - fa.velocity) * (1 / dt);
  out.z = 0;
  if (f.phase == HopPhase::ground && f.speed > 1) {
    const float drop = set.friction * (f.speed > set.stopspeed ? f.speed : set.stopspeed);
    const Vec3f v{f.velocity.x / f.speed, f.velocity.y / f.speed, 0};
    out = out + v * drop;
  }
  return true;
}

// The keys pressed most over frames [a, b] (ties to the fewer keys).
uint8_t most_keys(const HopReading& hops, size_t a, size_t b, const RocketJumpReadSettings& set) {
  std::array<int, 16> count{};
  for (size_t i = a; i <= b && i < hops.frames.size(); i++) {
    Vec3f w;
    if (!wish_at(hops, i, set, w)) continue;
    count[keys_of_wish(w, hops.frames[i].yaw, set.least_wish)]++;
  }
  uint8_t best = 0;
  for (uint8_t k = 1; k < 16; k++)
    if (count[k] > count[best]) best = k;
  return best;
}

// The keys over frames [a, b] in the order held, each `hold` frames or more (none left out).
KeySequence key_sequence(const HopReading& hops, size_t a, size_t b,
                         const RocketJumpReadSettings& set) {
  KeySequence out;
  uint8_t run_keys = 0;
  int run = 0;
  const auto close = [&] {
    if (run < set.hold || run_keys == 0) return;
    if (out.count > 0 && out.keys[out.count - 1] == run_keys) return;
    if (out.count < kMostKeySteps) out.keys[out.count++] = run_keys;
  };
  for (size_t i = a; i <= b && i < hops.frames.size(); i++) {
    Vec3f w;
    if (!wish_at(hops, i, set, w)) continue;
    const uint8_t k = keys_of_wish(w, hops.frames[i].yaw, set.least_wish);
    if (k == run_keys) {
      run++;
      continue;
    }
    close();
    run_keys = k;
    run = 1;
  }
  close();
  return out;
}

}  // namespace

const char* rocket_jump_kind_name(RocketJumpKind k) {
  switch (k) {
    case RocketJumpKind::feet:
      return "feet";
    case RocketJumpKind::side:
      return "side";
    case RocketJumpKind::flip:
      return "flip";
  }
  return "?";
}

std::string sequence_name(const KeySequence& s) {
  if (s.count == 0) return "-";
  std::string out;
  for (int i = 0; i < s.count; i++) out += (i > 0 ? ">" : "") + keys_name(s.keys[i]);
  return out;
}

std::string keys_name(uint8_t keys) {
  std::string s;
  const auto add = [&](uint8_t bit, const char* n) {
    if ((keys & bit) == 0) return;
    if (!s.empty()) s += "+";
    s += n;
  };
  add(kKeyForward, "F");
  add(kKeyBack, "B");
  add(kKeyLeft, "L");
  add(kKeyRight, "R");
  return s.empty() ? "-" : s;
}

uint8_t keys_of_wish(Vec3f wish, float yaw, float least) {
  const float len = across(wish);
  if (len < least) return 0;
  const float r = yaw / kDeg;
  const float c = static_cast<float>(core::libm::cos(static_cast<double>(r)));
  const float s = static_cast<float>(core::libm::sin(static_cast<double>(r)));
  // The view's forward (c, s) and right (s, -c), yaw counter-clockwise from +x.
  const float f = (wish.x * c + wish.y * s) / len;
  const float side = (wish.x * s - wish.y * c) / len;
  uint8_t k = 0;
  if (f > kPress) k |= kKeyForward;
  if (f < -kPress) k |= kKeyBack;
  if (side > kPress) k |= kKeyRight;
  if (side < -kPress) k |= kKeyLeft;
  return k;
}

bool read_wish(const HopReading& hops, size_t i, const RocketJumpReadSettings& set, Vec3f& out) {
  return wish_at(hops, i, set, out);
}

void read_rocket_jumps(std::span<const TrackPoint> track, const HopReading& hops,
                       std::span<const RocketShot> shots, const RocketJumpReadSettings& set,
                       std::vector<RocketJump>& out) {
  const size_t n = track.size();
  if (n < 3 || hops.frames.size() != n) return;
  std::vector<RocketShot> order(shots.begin(), shots.end());
  std::sort(order.begin(), order.end(),
            [](const RocketShot& a, const RocketShot& b) { return a.t0 < b.t0; });
  double busy_until = -1;  // one jump at a time: shots inside a flight read are its own
  for (const RocketShot& r : order) {
    if (r.t0 < busy_until || r.at_another) continue;
    // His own rocket flies along his view at the shot (one whose shooter was misread, fired by
    // another near him, flies elsewhere).
    {
      const TrackPoint& v = track[frame_at(track, r.t0)];
      const Vec3f fwd = angle_vectors({v.pitch, v.yaw, 0}).forward;
      const Vec3f way = r.to - r.from;
      const float wl = root_of(dot(way, way));
      if (wl > kAlongLeast && dot(way, fwd) < kAlongView * wl) continue;
    }
    const size_t e = frame_at(track, r.t1);
    if (track[e].dead || hops.frames[e].phase == HopPhase::cut) continue;
    const Vec3f mid = track[e].origin + Vec3f{0, 0, kMiddle};
    const Vec3f d = r.to - mid;
    const float off = root_of(dot(d, d));
    if (off > set.feet || r.to.z > mid.z) continue;  // by him, and below his middle
    // His speed before the blast and the most just after it.
    // Read before the burst's own frame, whose velocity (across its neighbours) holds the push.
    const HopFrame& before = hops.frames[frame_at(track, r.t1 - kBeforeBurst_s)];
    const float v0 = root_of(dot(before.velocity, before.velocity));
    size_t peak = e;
    float v1 = v0;
    for (size_t k = e + 1; k < n && track[k].t - r.t1 <= set.gain_s; k++) {
      const HopFrame& f = hops.frames[k];
      if (f.phase == HopPhase::cut) break;
      const float v = root_of(dot(f.velocity, f.velocity));
      if (v > v1) {
        v1 = v;
        peak = k;
      }
    }
    if (v1 - v0 < set.gain) continue;
    // The push away from his own burst (another's rocket from behind throws him toward it).
    {
      const Vec3f dv = hops.frames[peak].velocity - before.velocity;
      const Vec3f away = mid - r.to;
      const float dl = root_of(dot(dv, dv)), al = root_of(dot(away, away));
      if (dl > 0 && al > 1 && dot(dv, away) < kPushAway * dl * al) continue;
    }
    RocketJump j;
    j.shot = static_cast<int32_t>(frame_at(track, r.t0));
    j.burst = static_cast<int32_t>(e);
    j.t = track[static_cast<size_t>(j.shot)].t;
    j.burst_off = off;
    const TrackPoint& at = track[static_cast<size_t>(j.shot)];
    const HopFrame& run = hops.frames[static_cast<size_t>(j.shot)];
    const HopFrame& after = hops.frames[peak];
    j.from = at.origin;
    j.pitch = at.pitch;
    j.run_speed = run.speed;
    j.speed_after = after.speed;
    j.rise = after.velocity.z;
    // The flight's heading: its velocity just after, or failing that the way to where it lands.
    float flight = after.speed > kFlightHeading ? after.heading : at.yaw;
    // The landing: the first frame on the ground after the blast.
    size_t l = peak;
    float top = at.origin.z;
    while (l < n && hops.frames[l].phase == HopPhase::air && track[l].t - r.t1 <= set.land_s) {
      if (track[l].origin.z > top) top = track[l].origin.z;
      l++;
    }
    const bool landed = l < n && hops.frames[l].phase == HopPhase::ground;
    if (after.velocity.z < set.least_rise) continue;
    if (track[landed ? l : (l > 0 ? l - 1 : 0)].t - track[e].t < set.least_air_s) continue;
    const size_t last = landed ? l : (l > 0 ? l - 1 : 0);
    j.land = landed ? static_cast<int32_t>(l) : -1;
    j.to = track[last].origin;
    const Vec3f went = j.to - j.from;
    j.height = went.z;
    j.across = across(went);
    j.air_s = static_cast<float>(track[last].t - track[e].t);
    j.apex = top - at.origin.z;
    if (after.speed <= kFlightHeading && j.across > 16) flight = heading_of(went);
    j.view_off = wrap(at.yaw - flight);
    const float look = j.view_off < 0 ? -j.view_off : j.view_off;
    j.kind = look <= kFeetLook   ? RocketJumpKind::feet
             : look >= kFlipLook ? RocketJumpKind::flip
                                 : RocketJumpKind::side;
    j.standing = j.run_speed < set.standing;
    j.run_off = j.standing ? 0 : wrap(run.heading - flight);
    // The view in the air: its turn, and a flip's time to come back round.
    for (size_t k = e; k + 1 <= last && k + 1 < n; k++)
      j.turned += wrap(track[k + 1].yaw - track[k].yaw);
    if (j.kind == RocketJumpKind::flip)
      for (size_t k = e; k <= last; k++) {
        const float o = wrap(track[k].yaw - flight);
        if (o < kTurnedBack && o > -kTurnedBack) {
          j.turn_back_s = static_cast<float>(track[k].t - j.t);
          break;
        }
      }
    // The keys: the run's before the shot, the flight's after the blast.
    size_t ra = static_cast<size_t>(j.shot);
    while (ra > 0 && j.t - track[ra - 1].t <= set.run_s) ra--;
    j.keys_run = key_sequence(hops, ra, static_cast<size_t>(j.shot), set);
    size_t sa = static_cast<size_t>(j.shot);
    while (sa > 0 && j.t - track[sa - 1].t <= set.shot_s) sa--;
    j.keys_shot = most_keys(hops, sa, static_cast<size_t>(j.shot), set);
    if (last > peak) j.keys_air = key_sequence(hops, peak, last, set);
    busy_until = track[last].t;
    out.push_back(j);
  }
}

}  // namespace ge::shooter
