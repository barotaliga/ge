// Tests: the high-lag teleport (plugins/classic/protocol/lag_turn.hpp). A move turned by the
// teleport's turn pushes, under the old view, the way the untouched move pushes under the new one
// (forward into a quarter turn left is a step to the left, whole numbers); a respawn takes the new
// yaw and leaves the move. On the server, a packet's commands are turned while the setangle's
// packet is unacknowledged (the newest always, the older two only when they come after the turn)
// and left once it is; on the client, the commands in flight are turned by the same turn and the
// view keeps the player's own turning since.
//
// ge:summary Tests: the high-lag teleport's turned moves on the server and in the prediction
// ge:keywords test lag-teleport turn_move lag_turn_moves lag_turn_client setangle teleport
// ge:skill classic net testing
#include <cmath>
#include <cstdint>

#include "check.hpp"
#include "classic/protocol/lag_turn.hpp"
#include "shooter/vec.hpp"

namespace cp = ge::classic::protocol;
namespace sh = ge::shooter;

namespace {

sh::MoveCmd walk(float yaw, int16_t forward, int16_t side) {
  sh::MoveCmd c;
  c.angles = {0, yaw, 0};
  c.forward = forward;
  c.side = side;
  c.msec = 13;
  return c;
}

// The flat push of a command: its move along its own view's forward and right.
sh::Vec3f push(const sh::MoveCmd& c) {
  const sh::ViewAxes a = sh::angle_vectors({0, c.angles.y, 0});
  return {a.forward.x * c.forward + a.right.x * c.side,
          a.forward.y * c.forward + a.right.y * c.side, 0};
}

}  // namespace

TEST(a_teleports_turn_makes_the_old_views_move_push_the_new_way) {
  // Forward under yaw 0 after a quarter turn left: under the old view that is a step left.
  sh::MoveCmd c = walk(0, 400, 0);
  cp::turn_move(c, 90);
  CHECK(c.forward == 0 && c.side == -400);
  CHECK(c.angles.y == 0);  // the angles are the command's own
  // Any turn: the turned move under the old view pushes as the move does under the new one.
  for (float turn : {35.f, -120.f, 180.f, 270.f}) {
    sh::MoveCmd old_view = walk(10, 320, -200);
    sh::MoveCmd new_view = old_view;
    new_view.angles.y += turn;
    cp::turn_move(old_view, turn);
    const sh::Vec3f a = push(old_view), b = push(new_view);
    CHECK_MSG(std::fabs(a.x - b.x) < 2 && std::fabs(a.y - b.y) < 2,
              "turn " + std::to_string(turn) + ": " + std::to_string(a.x) + "," +
                  std::to_string(a.y) + " against " + std::to_string(b.x) + "," +
                  std::to_string(b.y));
  }
  // A respawn takes the new yaw and leaves the move.
  sh::MoveCmd r = walk(0, 400, 100);
  cp::turn_command(r, cp::TurnKind::respawn, 90, 250);
  CHECK(r.angles.y == 250 && r.forward == 400 && r.side == 100);
}

TEST(the_server_turns_the_moves_made_before_the_turn_reached_him_and_no_others) {
  cp::LagTurn t;
  // The setangle went in packet 100; the client's last packet then was 40, his yaw 0, turned to 90.
  cp::lag_turn_sent(t, cp::TurnKind::teleport, 100, 40, 0, 90, nullptr);
  CHECK(t.on && t.yaw == 90);
  // Packet 41 (acknowledging 99): only its newest came after the turn (40 is the command that
  // touched the teleporter, already run); packet 42: 41 and 42.
  sh::MoveCmd cmds[3] = {walk(0, 400, 0), walk(0, 400, 0), walk(0, 400, 0)};
  CHECK(cp::lag_turn_moves(t, 41, 99, cmds) == 1);
  CHECK(cmds[2].side == -400 && cmds[1].forward == 400 && cmds[0].forward == 400);
  sh::MoveCmd more[3] = {walk(0, 400, 0), walk(0, 400, 0), walk(0, 400, 0)};
  CHECK(cp::lag_turn_moves(t, 42, 99, more) == 2);
  CHECK(more[2].side == -400 && more[1].side == -400 && more[0].forward == 400);
  // Packet 43 acknowledges 100: he has seen it, his moves are his own and the turn is over.
  sh::MoveCmd own[3] = {walk(90, 400, 0), walk(90, 400, 0), walk(90, 400, 0)};
  CHECK(cp::lag_turn_moves(t, 43, 100, own) == 0);
  CHECK(!t.on && own[2].forward == 400);
  // Nothing in flight: nothing turned.
  CHECK(cp::lag_turn_moves(t, 44, 101, own) == 0);
}

TEST(the_client_turns_its_moves_in_flight_and_keeps_the_players_own_turning) {
  sh::MoveCmd sent[64];
  uint32_t seq[64] = {};
  // Commands 10..14 sent under yaw 0, 2 and 4 degrees of mouse after 10; 10 is acknowledged.
  for (uint32_t q = 10; q < 15; q++) {
    sent[q & 63] = walk(q == 10 ? 0.f : static_cast<float>(q - 10), 400, 0);
    seq[q & 63] = q;
  }
  sh::Vec3f view = {0, 5, 0};  // where his mouse has the view now
  cp::lag_turn_client(cp::TurnKind::teleport, {0, 90, 0}, 10, 15, sent, seq, view);
  // The turn is 90 (the server's 90 less the acknowledged command's 0): his own 5 kept on top.
  CHECK_MSG(std::fabs(view.y - 95) < 1e-4f, "the view's yaw: " + std::to_string(view.y));
  // The acknowledged command is left; those in flight pushed the new way.
  CHECK(sent[10].forward == 400 && sent[10].side == 0);
  for (uint32_t q = 11; q < 15; q++) {
    sh::MoveCmd want = walk(sent[q & 63].angles.y + 90, 400, 0);
    const sh::Vec3f a = push(sent[q & 63]), b = push(want);
    CHECK(std::fabs(a.x - b.x) < 2 && std::fabs(a.y - b.y) < 2);
  }
  // A respawn: the server's angles, the moves' yaw taken.
  sh::Vec3f v2 = {10, 33, 0};
  cp::lag_turn_client(cp::TurnKind::respawn, {0, 180, 0}, 10, 15, sent, seq, v2);
  CHECK(v2.y == 180 && v2.x == 0 && sent[12].angles.y == 180);
}
