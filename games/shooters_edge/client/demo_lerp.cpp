// A demo drawn between its last two frames (demo_lerp.hpp).
//
// ge:summary A demo drawn between its last two frames: the frames kept, the rules, the state drawn
// ge:keywords lerp_frame lerp_at lerp_fraction lerp_angle lerp_player lerp_entity interpolation
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_lerp_test.cpp
#include "shooters_edge/client/demo_lerp.hpp"

#include <algorithm>
#include <utility>

namespace shooters_edge::client {

namespace {

float distance2(const sh::Vec3f& a, const sh::Vec3f& b) {
  const float x = b.x - a.x, y = b.y - a.y, z = b.z - a.z;
  return x * x + y * y + z * z;
}

sh::Vec3f lerp3(const sh::Vec3f& a, const sh::Vec3f& b, float f) {
  return {a.x + f * (b.x - a.x), a.y + f * (b.y - a.y), a.z + f * (b.z - a.z)};
}

sh::Vec3f lerp_angles(const sh::Vec3f& a, const sh::Vec3f& b, float f) {
  return {lerp_angle(a.x, b.x, f), lerp_angle(a.y, b.y, f), lerp_angle(a.z, b.z, f)};
}

// Where `e` (now at `to`) is drawn at `time` by its stride, or false when it is not stepping.
bool stride_at(const DemoStride& st, const DemoEntity& to, double time, sh::Vec3f& origin,
               sh::Vec3f& angles) {
  if (!st.stepping) return false;
  const double g = (time - st.at) / st.span;
  if (g >= 1) return false;
  const auto f = static_cast<float>(std::max(g, 0.0));
  origin = lerp3(st.origin, to.origin, f);
  angles = lerp_angles(st.angles, to.angles, f);
  return true;
}

// The first entity number that is not a player's: a single recording's players are its entities
// 1 to `max_clients`; the multiplayer formats keep their players apart.
size_t first_thing(const DemoState& s) {
  return s.format == DemoFormat::single ? static_cast<size_t>(std::max(s.max_clients, 0)) + 1 : 1;
}

}  // namespace

float lerp_angle(float a, float b, float f) {
  float move = b - a;
  // Whole turns off first (a reader may hand angles past 360), then the short way round.
  while (move >= 360) move -= 360;
  while (move <= -360) move += 360;
  if (move >= 180)
    move -= 360;
  else if (move <= -180)
    move += 360;
  return a + f * move;
}

bool lerp_player(const DemoEntity& a, const DemoEntity& b, float f, bool angles, DemoEntity& out) {
  out = b;
  if (!a.present || !b.present) return false;
  if (distance2(a.origin, b.origin) > kPlayerSnapUnits * kPlayerSnapUnits) return false;
  if (a.dead && !b.dead) return false;            // a respawn
  if (b.gib && !(a.dead || a.gib)) return false;  // the first gibbed frame
  out.origin = lerp3(a.origin, b.origin, f);
  if (angles) out.angles = lerp_angles(a.angles, b.angles, f);
  return true;
}

bool lerp_entity(const DemoEntity& a, const DemoEntity& b, float f, DemoEntity& out) {
  out = b;
  if (!a.present || !b.present || a.model != b.model) return false;
  if (distance2(a.origin, b.origin) > kEntitySnapUnits * kEntitySnapUnits) return false;
  out.origin = lerp3(a.origin, b.origin, f);
  out.angles = lerp_angles(a.angles, b.angles, f);
  return true;
}

void lerp_frame(DemoLerp& l, const DemoState& s, bool cut) {
  std::swap(l.from, l.to);
  DemoPose& p = l.to;
  p.time = s.time;
  p.levels = s.levels;
  p.player = s.player;
  p.intermission = s.intermission;
  p.set_angles = s.set_angles;
  p.eye = s.eye;
  p.angles = s.angles;
  p.entities.assign(s.entities.begin(), s.entities.end());
  p.players.assign(s.players.begin(), s.players.end());
  l.between = !cut && s.frames > 1 && l.from.levels == p.levels && p.time > l.from.time;
  // The strides: a thing still through the frame before that moves now steps from where it was
  // drawn when the frame before stood, over the time since its last move.
  const DemoPose& a = l.from;
  l.stride.resize(p.entities.size());
  for (size_t n = 0; n < p.entities.size(); n++) {
    DemoStride& st = l.stride[n];
    const DemoEntity& to = p.entities[n];
    const bool kept =
        l.strides && l.between && n >= first_thing(s) && n < a.entities.size() &&
        a.entities[n].present && to.present && a.entities[n].model == to.model &&
        distance2(a.entities[n].origin, to.origin) <= kEntitySnapUnits * kEntitySnapUnits;
    const DemoEntity& was = n < a.entities.size() ? a.entities[n] : to;
    if (!kept) {
      st = DemoStride{};
      st.moved = p.time;
      continue;
    }
    const bool moved = distance2(was.origin, to.origin) > 0 || was.angles.x != to.angles.x ||
                       was.angles.y != to.angles.y || was.angles.z != to.angles.z;
    if (!moved) continue;
    if (st.moved < a.time) {  // still through the frame before: a stride
      sh::Vec3f origin = was.origin, angles = was.angles;
      stride_at(st, was, a.time, origin, angles);
      st.span = frame_pace(p.time - st.moved, st.span);
      st.origin = origin;
      st.angles = angles;
      st.at = a.time;
      st.stepping = true;
    } else {
      st.stepping = false;
    }
    st.moved = p.time;
  }
}

double lerp_fraction(const DemoLerp& l, double time) {
  if (!l.between) return 1;
  const double span = l.to.time - l.from.time;
  if (!(span > 0) || span > kMostLerpGap) return 1;
  return std::clamp((time - l.from.time) / span, 0.0, 1.0);
}

const DemoState& lerp_at(DemoLerp& l, const DemoState& s, double time) {
  DemoState& d = l.drawn;
  d = s;
  l.f = lerp_fraction(l, time);
  l.snapped = 0;
  // A stride drawn over its time, whatever the two frames' fraction.
  const size_t strides =
      std::min(l.stride.size(), std::min(l.to.entities.size(), d.entities.size()));
  for (size_t n = 0; n < strides && l.between; n++)
    if (l.to.entities[n].present)
      stride_at(l.stride[n], l.to.entities[n], time, d.entities[n].origin, d.entities[n].angles);
  if (l.f >= 1) return d;
  const float f = static_cast<float>(l.f);
  const DemoPose& a = l.from;
  const DemoPose& b = l.to;
  const size_t players = std::min(a.players.size(), b.players.size());
  for (size_t k = 0; k < players; k++) {
    if (!b.players[k].present) continue;
    const bool turn = (b.set_angles >> (k & 31u) & 1u) == 0;
    if (!lerp_player(a.players[k], b.players[k], f, turn, d.players[k])) l.snapped++;
  }
  const size_t entities = std::min(a.entities.size(), b.entities.size());
  for (size_t n = 0; n < entities; n++) {
    if (!b.entities[n].present ||
        (n < l.stride.size() && l.stride[n].stepping &&
         stride_at(l.stride[n], b.entities[n], time, d.entities[n].origin, d.entities[n].angles)))
      continue;
    if (!lerp_entity(a.entities[n], b.entities[n], f, d.entities[n])) l.snapped++;
  }
  if (s.intermission || a.intermission || a.player != b.player) return d;
  // The eye rides the tracked one's drawn origin, as the reader puts it on his read one.
  bool moved = false;
  if (s.format == DemoFormat::single) {
    const size_t v = static_cast<size_t>(s.view_entity);
    if (s.view_entity >= 0 && v < entities && b.entities[v].present) {
      moved = a.entities[v].present &&
              distance2(a.entities[v].origin, b.entities[v].origin) <=
                  kEntitySnapUnits * kEntitySnapUnits &&
              a.entities[v].model == b.entities[v].model;
      d.eye = d.entities[v].origin;
      d.eye.z += s.view_height;
    }
  } else if (s.player >= 0 && static_cast<size_t>(s.player) < players &&
             b.players[static_cast<size_t>(s.player)].present) {
    const DemoEntity& from = a.players[static_cast<size_t>(s.player)];
    const DemoEntity& to = b.players[static_cast<size_t>(s.player)];
    DemoEntity scratch;
    moved = lerp_player(from, to, f, true, scratch);
    d.eye = d.players[static_cast<size_t>(s.player)].origin;
    d.eye.z += s.view_height;
  }
  const bool set = s.player >= 0 && s.player < 32 && (b.set_angles >> s.player & 1u) != 0;
  if (moved && !set) d.angles = lerp_angles(a.angles, b.angles, f);
  return d;
}

}  // namespace shooters_edge::client
