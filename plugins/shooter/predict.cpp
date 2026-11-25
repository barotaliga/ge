// The client's prediction (predict.hpp states the rules).
//
// ge:summary The client's prediction: quantizing, replaying commands, smoothing, other players
// ge:keywords quantize_player unquantize_player net_player_of net_msec client_world
// ge:keywords predict_player predict_other other_msec smoothing_frame smoothing_decay
// ge:skill shooter net
// ge:test tests/shooter_predict_test.cpp
#include "shooter/predict.hpp"

#include <algorithm>

#include "core/num.hpp"
#include "shooter/client.hpp"

namespace ge::shooter {

namespace {

constexpr int kMaxNetMsec = 255;

int32_t trunc8(float v) { return static_cast<int32_t>(v * 8.0f); }

int16_t trunc_speed(float v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);
}

}  // namespace

NetPlayer quantize_player(const MoveState& s) {
  NetPlayer n;
  for (int i = 0; i < 3; i++) {
    n.origin8[i] = trunc8(s.origin[i]);
    n.velocity[i] = trunc_speed(s.velocity[i]);
  }
  n.type = s.type;
  n.jump_held = s.jump_held;
  n.onground = s.ground != -1;
  return n;
}

MoveState unquantize_player(const NetPlayer& n, const MoveSettings& set) {
  MoveState s;
  for (int i = 0; i < 3; i++) {
    s.origin[i] = static_cast<float>(n.origin8[i]) * 0.125f;
    s.velocity[i] = static_cast<float>(n.velocity[i]);
  }
  s.type = n.type;
  s.jump_held = n.jump_held;
  s.ground = n.onground ? 0 : -1;
  s.ground_normal = {0, 0, 1};
  s.maxspeed = set.maxspeed;
  return s;
}

NetPlayer net_player_of(const Server& sv, int slot) {
  const ClientSlot& c = sv.clients[static_cast<size_t>(slot)];
  const Edict& ed = sv.edicts[static_cast<size_t>(slot) + 1];
  MoveState s;
  s.origin = ed.origin + (ed.mins - kPlayerMins);
  s.velocity = ed.velocity;
  s.type = move_type_of(ed, c.old_spectator);
  // The original move keeps the jump in the player's flags; the predicted move in the slot.
  s.jump_held =
      sv.set.playermove == PlayerMove::original ? (ed.flags & kFlagJumpReleased) == 0 : c.jump_held;
  s.ground = (ed.flags & kFlagOnGround) != 0 ? 0 : -1;
  return quantize_player(s);
}

int net_msec(const Server& sv, int slot) {
  const ClientSlot& c = sv.clients[static_cast<size_t>(slot)];
  const double ms = core::num::round((sv.time - c.last_cmd_time) * 1000);
  return ms > kMaxNetMsec ? kMaxNetMsec : ms < 0 ? 0 : static_cast<int>(ms);
}

void client_world(const BrushModel& world, const ClientModel* models, int model_count,
                  const Vec3f* players, int player_count, MoveWorld& out) {
  out.count = 0;
  Solid& w = out.solids[out.count++];
  w.kind = SolidKind::brush;
  w.origin = {};
  w.mins = world.mins;
  w.maxs = world.maxs;
  w.point_hull = world.hulls[0];
  w.clip_hull = world.hulls[1];
  for (int i = 0; i < model_count && out.count < kMaxSolids; i++) {
    Solid& s = out.solids[out.count++];
    s.kind = SolidKind::brush;
    s.origin = models[i].origin;
    s.mins = models[i].model->mins;
    s.maxs = models[i].model->maxs;
    s.point_hull = models[i].model->hulls[0];
    s.clip_hull = models[i].model->hulls[1];
  }
  for (int i = 0; i < player_count && out.count < kMaxSolids; i++) {
    Solid& s = out.solids[out.count++];
    s.kind = SolidKind::box;
    s.origin = players[i];
    s.mins = kPlayerMins;
    s.maxs = kPlayerMaxs;
  }
}

void advance_movers(MoveWorld& world, MoveState& s, const ClientModel* models, int count,
                    float dt) {
  for (int i = 0; i < count && 1 + i < world.count; i++) {
    const Vec3f v = models[i].velocity;
    if (v.x == 0 && v.y == 0 && v.z == 0) continue;
    Solid& m = world.solids[1 + i];
    const float speed = length(v);
    const float gone = length(m.origin - models[i].origin);
    const float step = std::min(speed * dt, models[i].reach - gone);
    if (step <= 0) continue;
    const Vec3f move = v * (step / speed);
    m.origin = m.origin + move;
    if (s.ground != 1 + i && box_free(world, s.origin)) continue;  // neither on it nor hit by it
    if (box_free(world, s.origin + move)) s.origin = s.origin + move;
  }
}

float mover_reach(Vec3f origin, Vec3f velocity, const Vec3f* rests, int count) {
  const float speed = length(velocity);
  float best = 1e30f;
  if (speed <= 0) return best;
  const Vec3f way = velocity * (1 / speed);
  for (int i = 0; i < count; i++) {
    const Vec3f to = rests[i] - origin;
    const float ahead = dot(to, way);
    if (ahead < 0 || length(to - way * ahead) > 1) continue;  // behind it, or off its line
    best = std::min(best, ahead);
  }
  return best;
}

Vec3f mover_ahead(Vec3f origin, Vec3f velocity, float reach, float seconds) {
  const float speed = length(velocity);
  if (speed <= 0 || seconds <= 0) return origin;
  const float go = std::min(speed * seconds, reach);
  return origin + velocity * (go / speed);
}

void mover_track_frame(MoverTrack& t, double time_s) {
  for (MoverTrack::One& o : t.ents) {
    o.prev = o.last;
    o.prev_model = o.last_model;
    o.last_model = -1;
  }
  t.prev_s = t.last_s;
  t.last_s = time_s;
}

void mover_track_seen(MoverTrack& t, size_t entity, int model, Vec3f origin) {
  if (t.ents.size() <= entity) t.ents.resize(entity + 1);
  MoverTrack::One& o = t.ents[entity];
  if (o.prev_model >= 0 && o.prev_model != model) o.rest_count = o.rest_next = 0;  // another one
  o.last = origin;
  o.last_model = model;
  if (o.prev_model != model || !(o.prev == origin)) return;
  for (int i = 0; i < o.rest_count; i++)
    if (o.rests[i] == origin) return;
  o.rests[o.rest_next] = origin;
  o.rest_next = (o.rest_next + 1) % 4;
  o.rest_count = std::min(o.rest_count + 1, 4);
}

void mover_motion(const MoverTrack& t, size_t entity, Vec3f& velocity, float& reach) {
  velocity = {};
  reach = 1e30f;
  if (entity >= t.ents.size()) return;
  const MoverTrack::One& o = t.ents[entity];
  const double gap = t.last_s - t.prev_s;
  if (o.last_model < 0 || o.prev_model != o.last_model || t.prev_s < 0 || gap <= 0) return;
  const Vec3f d = o.last - o.prev;
  if (length(d) > kMoverJump) return;
  velocity = d * static_cast<float>(1 / gap);
  Vec3f stops[5] = {};  // where it was built, then where it was seen standing
  for (int i = 0; i < o.rest_count; i++) stops[1 + i] = o.rests[i];
  reach = mover_reach(o.last, velocity, stops, 1 + o.rest_count);
}

MoveState predict_player(const NetPlayer& acked, const MoveCmd* cmds, int count,
                         const MoveSettings& set, const MoveWorld& world, MoveState* each) {
  MoveState s = unquantize_player(acked, set);
  for (int i = 0; i < count; i++) {
    run_command(s, cmds[i], set, world);
    if (each != nullptr) each[i] = s;
  }
  return s;
}

int other_msec(double arrival_s, int state_msec, double now_s, double latency_s) {
  const double state_time = arrival_s - state_msec * 0.001;
  double player_time = now_s - latency_s + kOtherLead;
  if (player_time > now_s) player_time = now_s;
  const double ms = core::num::round((player_time - state_time) * 1000);
  return ms < 0 ? 0 : ms > kMaxNetMsec ? kMaxNetMsec : static_cast<int>(ms);
}

MoveState predict_other(const NetPlayer& state, MoveCmd last_cmd, int msec, const MoveSettings& set,
                        const MoveWorld& world) {
  MoveState s = unquantize_player(state, set);
  last_cmd.msec = static_cast<uint8_t>(msec);
  run_command(s, last_cmd, set, world);
  return s;
}

void smoothing_frame(Smoothing& s, Vec3f before, Vec3f after, double latency_s) {
  const Vec3f err = before - after;
  const float d = length(err);
  if (s.off || d > kSmoothMax) {
    s.correction = {};  // off, or a teleport: snap
  } else if (d >= kSmoothMin) {
    const float share =
        latency_s <= 0 ? 1.0f : static_cast<float>(1 - std::min(1.0, kSmoothStepS / latency_s));
    s.correction = s.correction + err * share;
    const float c = length(s.correction);  // a client that cuts the error caps the sum too
    if (latency_s > 0 && c > kSmoothMax) s.correction = s.correction * (kSmoothMax / c);
  }
}

void smoothing_decay(Smoothing& s, double dt) {
  const float c = length(s.correction);
  if (c == 0) return;
  const float rate = c < 8 ? 200.0f : c < 16 ? 500.0f : c < 32 ? 800.0f : 1400.0f;
  float left = c - static_cast<float>(rate * s.scale * dt);
  if (left < 0 || (s.drop > 0 && left < static_cast<float>(s.drop * s.scale * dt))) left = 0;
  s.correction = s.correction * (left / c);
}

}  // namespace ge::shooter
