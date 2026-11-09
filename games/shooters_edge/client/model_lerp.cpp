// The models drawn between their frames (model_lerp.hpp).
//
// ge:summary The model lerp: each entity's animation and place tracked and drawn between
// ge:keywords model-lerp lerp_models track sweep step-lerp
// ge:skill shooter render
#include "shooters_edge/client/model_lerp.hpp"

#include <algorithm>

#include "shooter/server.hpp"
#include "shooters_edge/client/demo_lerp.hpp"

namespace shooters_edge::client {

namespace {

// The time a pace's gap says its next step takes: the gap itself when it is a pace, else the last.
double pace(double gap, double last) {
  return gap >= kLeastFrameSpan && gap <= kMostFrameSpan ? gap : last;
}

sh::Vec3f mix(const sh::Vec3f& a, const sh::Vec3f& b, float f) {
  return {a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f};
}

sh::Vec3f turn(const sh::Vec3f& a, const sh::Vec3f& b, float f) {
  return {lerp_angle(a.x, b.x, f), lerp_angle(a.y, b.y, f), lerp_angle(a.z, b.z, f)};
}

bool same(const sh::Vec3f& a, const sh::Vec3f& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

float apart2(const sh::Vec3f& a, const sh::Vec3f& b) {
  const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
  return x * x + y * y + z * z;
}

// Where `tr` is drawn at `drawn_s` on the server's clock.
void drawn_place(const ModelTrack& tr, double drawn_s, sh::Vec3f& origin, sh::Vec3f& angles) {
  origin = tr.origin;
  angles = tr.angles;
  if (tr.stepping) {
    const double f = (drawn_s - tr.step_at) / tr.step_span;
    if (f < 1) {
      const auto g = static_cast<float>(std::max(f, 0.0));
      origin = mix(tr.step_origin, tr.origin, g);
      angles = turn(tr.step_angles, tr.angles, g);
    }
    return;
  }
  const double gap = tr.now_s - tr.prev_s;
  if (!(gap > 0) || gap > kMostGap) return;
  const auto f = static_cast<float>(std::clamp((drawn_s - tr.prev_s) / gap, 0.0, 1.0));
  origin = mix(tr.prev_origin, tr.origin, f);
  angles = turn(tr.prev_angles, tr.angles, f);
}

}  // namespace

void lerp_models(ModelLerp& l, std::span<LerpModel> placed, double t, const ModelLerpClock& clock,
                 const ModelLerpSwitches& on) {
  const bool moves = on.moves && clock.moves;
  // A cut: the picture's clock run back (a seek, a new demo) or the state's (a new map).
  if (t < l.last_t || (moves && clock.state_s < l.last_state)) l.tracks.clear();
  const bool fresh_state = moves && clock.state_s != l.last_state;
  const uint32_t before = l.calls++;
  for (LerpModel& p : placed) {
    if (p.entity == 0) continue;
    auto [it, made] = l.tracks.try_emplace(p.entity);
    ModelTrack& tr = it->second;
    const bool cut = made || tr.seen != before || tr.modelindex != p.modelindex || p.warping ||
                     apart2(tr.origin, p.origin) > kSnapUnits * kSnapUnits;
    tr.seen = l.calls;
    if (cut) {
      tr = ModelTrack{};
      tr.seen = l.calls;
      tr.modelindex = p.modelindex;
      tr.frame = p.frame;
      tr.origin = tr.prev_origin = p.origin;
      tr.angles = tr.prev_angles = p.angles;
      tr.now_s = tr.prev_s = tr.moved_s = clock.state_s;
      p.lerp = on.frames;
      continue;
    }
    // THE ANIMATION: a new frame drawn from the last over the time its frames last.
    if (p.frame != tr.frame) {
      if (tr.changed >= 0) tr.span = pace(t - tr.changed, tr.span);
      tr.from = tr.frame;
      tr.frame = p.frame;
      tr.changed = t;
    }
    if (on.frames) {
      p.lerp = true;
      if (tr.from >= 0) {
        const double f = (t - tr.changed) / tr.span;
        if (f < 1) {
          p.frame_from = tr.from;
          p.frame_f = static_cast<float>(std::max(f, 0.0));
        }
      }
    }
    if (!moves) {  // drawn where it stands: kept for the snap's test alone
      tr.origin = p.origin;
      tr.angles = p.angles;
      continue;
    }
    // THE PLACE: a new state kept; a still thing's move drawn over the time since its last.
    if (fresh_state) {
      const bool moved = !same(p.origin, tr.origin) || !same(p.angles, tr.angles);
      if (moved) {
        const bool still_before = tr.moved_s < tr.now_s;  // it stood through the last state
        if (p.entity > sh::kMaxClients && still_before) {
          sh::Vec3f at, angles;  // where it is drawn now (not written into the track it reads)
          drawn_place(tr, clock.drawn_s, at, angles);
          tr.step_origin = at;
          tr.step_angles = angles;
          tr.stepping = true;
          tr.step_at = clock.drawn_s;
          tr.step_span = pace(clock.state_s - tr.moved_s, tr.step_span);
        } else {
          tr.stepping = false;
        }
        tr.moved_s = clock.state_s;
      }
      tr.prev_origin = tr.origin;
      tr.prev_angles = tr.angles;
      tr.prev_s = tr.now_s;
      tr.origin = p.origin;
      tr.angles = p.angles;
      tr.now_s = clock.state_s;
    }
    drawn_place(tr, clock.drawn_s, p.origin, p.angles);
  }
  // The ones gone this picture are let go.
  for (auto it = l.tracks.begin(); it != l.tracks.end();)
    it = it->second.seen == l.calls ? std::next(it) : l.tracks.erase(it);
  l.last_t = t;
  if (moves) l.last_state = clock.state_s;
}

}  // namespace shooters_edge::client
