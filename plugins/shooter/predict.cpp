// The client's prediction (predict.hpp states the rules).
//
// ge:summary The client's prediction: quantizing, replaying commands, smoothing, other players
// ge:keywords quantize_player unquantize_player net_player_of net_msec client_world
// ge:keywords predict_player predict_other other_msec smoothing_frame smoothing_decay
// ge:skill shooter net
// ge:test tests/shooter_predict_test.cpp
#include "shooter/predict.hpp"

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
  const Edict& ed = sv.edicts[static_cast<size_t>(slot + 1)];
  MoveState s;
  s.origin = ed.origin + (ed.mins - kPlayerMins);
  s.velocity = ed.velocity;
  s.type = move_type_of(ed, c.old_spectator);
  s.jump_held = c.jump_held;
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

MoveState predict_player(const NetPlayer& acked, const MoveCmd* cmds, int count,
                         const MoveSettings& set, const MoveWorld& world) {
  MoveState s = unquantize_player(acked, set);
  for (int i = 0; i < count; i++) run_command(s, cmds[i], set, world);
  return s;
}

int other_msec(double arrival_s, int state_msec, double now_s, double latency_s) {
  const double state_time = arrival_s - state_msec * 0.001;
  double player_time = now_s - latency_s + kOtherLead;
  if (player_time > now_s) player_time = now_s;
  const double ms = core::num::round((player_time - state_time) * 1000);
  return ms < 0 ? 0 : ms > kMaxNetMsec ? kMaxNetMsec : static_cast<int>(ms);
}

MoveState predict_other(const NetPlayer& state, MoveCmd last_cmd, int msec,
                        const MoveSettings& set, const MoveWorld& world) {
  MoveState s = unquantize_player(state, set);
  last_cmd.msec = static_cast<uint8_t>(msec);
  run_command(s, last_cmd, set, world);
  return s;
}

void smoothing_frame(Smoothing& s, Vec3f before, Vec3f after) {
  const Vec3f err = before - after;
  const float d = length(err);
  if (d > kSmoothMax) {
    s.correction = {};  // a teleport: snap
  } else if (d >= kSmoothMin) {
    s.correction = s.correction + err;
  }
}

void smoothing_decay(Smoothing& s, double dt) {
  const float c = length(s.correction);
  if (c == 0) return;
  const float rate = c < 8 ? 200.0f : c < 16 ? 500.0f : c < 32 ? 800.0f : 1400.0f;
  float left = c - static_cast<float>(rate * s.scale * dt);
  if (left < 0) left = 0;
  s.correction = s.correction * (left / c);
}

}  // namespace ge::shooter
