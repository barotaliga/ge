// The high-lag teleport (lag_turn.hpp states the rules).
//
// ge:summary The high-lag teleport: in-flight moves turned on the server and in the prediction
// ge:keywords turn_move lag_turn_moves lag_turn_client lag_turn_sent setangle teleport
// ge:skill classic net shooter
// ge:test tests/classic_lag_turn_test.cpp
#include "classic/protocol/lag_turn.hpp"

#include "shooter/vec.hpp"

namespace ge::classic::protocol {

namespace {

namespace sh = ge::shooter;

int16_t whole(float v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return static_cast<int16_t>(v);  // toward zero
}

}  // namespace

void turn_move(sh::MoveCmd& c, float yaw_deg) {
  // The turned move under the old view pushes as the move would under the new one: forward'
  // = f cos d + s sin d, side' = s cos d - f sin d.
  const sh::ViewAxes a = sh::angle_vectors({0, yaw_deg, 0});
  const float cs = a.forward.x, sn = a.forward.y;
  const float f = c.forward, s = c.side;
  c.forward = whole(f * cs + s * sn);
  c.side = whole(s * cs - f * sn);
}

void turn_command(sh::MoveCmd& c, TurnKind kind, float yaw_deg, float new_yaw) {
  if (kind == TurnKind::teleport)
    turn_move(c, yaw_deg);
  else if (kind == TurnKind::respawn)
    c.angles.y = new_yaw;
}

void lag_turn_sent(LagTurn& t, TurnKind kind, uint32_t out_seq, uint32_t in_seq, float last_yaw,
                   float new_yaw, sh::MoveCmd* last) {
  t.on = kind == TurnKind::teleport || kind == TurnKind::respawn;
  if (!t.on) return;
  t.kind = kind;
  t.out_seq = out_seq;
  t.in_seq = in_seq;
  t.yaw = new_yaw - last_yaw;
  t.new_yaw = new_yaw;
  if (last != nullptr) turn_command(*last, kind, t.yaw, new_yaw);
}

int lag_turn_moves(LagTurn& t, uint32_t seq, uint32_t ack, sh::MoveCmd cmds[3]) {
  if (!t.on) return 0;
  if (static_cast<int32_t>(ack - t.out_seq) >= 0) {  // he has seen it: his moves are his own
    t.on = false;
    return 0;
  }
  int turned = 0;
  for (int i = 0; i < 3; i++) {
    // cmds[2] is the packet's own, cmds[1] the one before (seq - 1), cmds[0] seq - 2.
    const uint32_t its = seq - static_cast<uint32_t>(2 - i);
    if (i < 2 && static_cast<int32_t>(its - t.in_seq) <= 0) continue;
    turn_command(cmds[i], t.kind, t.yaw, t.new_yaw);
    turned++;
  }
  return turned;
}

void lag_turn_client(TurnKind kind, const sh::Vec3f& angles, uint32_t acked, uint32_t next,
                     sh::MoveCmd sent[64], const uint32_t sent_seq[64], sh::Vec3f& view) {
  if (kind != TurnKind::teleport && kind != TurnKind::respawn) {
    view = angles;
    return;
  }
  const bool known = sent_seq[acked & 63] == acked;
  const float from = known ? sent[acked & 63].angles.y : view.y;
  const float yaw = angles.y - from;
  for (uint32_t q = acked + 1; q != next && static_cast<int32_t>(next - q) > 0; q++) {
    if (static_cast<int32_t>(next - q) >= 64) continue;  // fallen out of the buffer
    if (sent_seq[q & 63] != q) continue;
    turn_command(sent[q & 63], kind, yaw, angles.y);
  }
  // A teleport keeps the turning he did since the command that touched it: his view as it is now,
  // turned (the newest command's yaw and after); the pitch and roll are the server's.
  const float now = view.y;
  view = angles;
  if (kind == TurnKind::teleport) view.y = now + yaw;
}

}  // namespace ge::classic::protocol
