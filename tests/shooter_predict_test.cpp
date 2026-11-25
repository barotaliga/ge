// THE CLIENT'S PREDICTION: the protocol's copy of a player (the origin on the 1/8 grid and the
// velocity in whole units, both truncated toward zero); its own player predicted from that copy
// over the commands after it through the server's own move, landing within the grid's error of the
// server and exactly on it when the server's state was already on the grid; nothing to predict
// gives the server's state as it is; the milliseconds since a player's last command, capped at
// 255; another player's time (the arrival less its msec, against now less the latency plus 0.02,
// never in the future) and its run forward; the smoothing's window (4 to 64 units) and its decay
// rates by size. A mover's motion read off two frames, its stops where it stood or was built; a
// mover run on carries its rider, leaves the others and stops after its reach.
//
// ge:summary Tests: quantized player state, own and other player prediction, error smoothing
// ge:keywords test predict prediction quantize net-player smoothing other-player net-msec
// ge:keywords mover_track mover_motion mover_ahead advance_movers rider
// ge:skill shooter testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "shooter/client.hpp"
#include "shooter/predict.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;
using ge::test::loop::box;

namespace {

// A floor at z = 0 and a wall at x = 256..288, as the client sees them.
struct ClientMap {
  sh::ModelData data = sh::build_brush_model(
      {box({-1024, -1024, -64}, {1024, 1024, 0}), box({256, -1024, 0}, {288, 1024, 256})});
  sh::BrushModel model = data.model();
  sh::MoveWorld world;

  ClientMap() { sh::client_world(model, nullptr, 0, nullptr, 0, world); }
};

sh::MoveCmd forward_cmd(uint8_t msec, float yaw = 0) {
  sh::MoveCmd c;
  c.msec = msec;
  c.angles = {0, yaw, 0};
  c.forward = 400;
  return c;
}

float distance(sh::Vec3f a, sh::Vec3f b) { return sh::length(a - b); }

}  // namespace

TEST(the_protocol_truncates_the_origin_to_eighths_and_the_speed_to_units) {
  sh::MoveState s;
  s.origin = {1.99f, -1.99f, 24.0625f};
  s.velocity = {12.9f, -12.9f, 0.5f};
  s.ground = 0;
  s.jump_held = true;
  const sh::NetPlayer n = sh::quantize_player(s);
  CHECK(n.origin8[0] == 15 && n.origin8[1] == -15 && n.origin8[2] == 192);
  CHECK(n.velocity[0] == 12 && n.velocity[1] == -12 && n.velocity[2] == 0);
  CHECK(n.onground && n.jump_held);
  const sh::MoveState back = sh::unquantize_player(n, {});
  CHECK(back.origin.x == 1.875f && back.origin.y == -1.875f && back.origin.z == 24.0f);
  CHECK(back.ground == 0);
}

TEST(nothing_to_predict_shows_the_servers_state) {
  ClientMap m;
  sh::MoveState s;
  s.origin = {10.5f, 3.25f, 24};
  s.velocity = {100, 0, 0};
  const sh::NetPlayer n = sh::quantize_player(s);
  const sh::MoveState p = sh::predict_player(n, nullptr, 0, {}, m.world);
  CHECK(p.origin.x == 10.5f && p.origin.y == 3.25f && p.origin.z == 24.0f);
  CHECK(p.velocity.x == 100.0f);
}

TEST(the_prediction_is_the_servers_move_from_the_grid) {
  ClientMap m;
  const sh::MoveSettings set;
  // The server's player: standing on the floor, run forward for 20 commands toward the wall.
  sh::MoveState server;
  server.origin = {0, 0, 24};
  server.ground = 0;
  server.ground_normal = {0, 0, 1};
  sh::MoveCmd cmds[40];
  for (int i = 0; i < 40; i++) cmds[i] = forward_cmd(static_cast<uint8_t>(i % 2 == 0 ? 13 : 12));
  for (int i = 0; i < 20; i++) sh::run_command(server, cmds[i], set, m.world);
  const sh::NetPlayer acked = sh::quantize_player(server);  // the reply to command 20

  // The client predicts the next 20 commands from the acknowledged copy; the server runs them.
  const sh::MoveState predicted = sh::predict_player(acked, cmds + 20, 20, set, m.world);
  for (int i = 20; i < 40; i++) sh::run_command(server, cmds[i], set, m.world);
  const float err = distance(predicted.origin, server.origin);
  CHECK_MSG(err < 1.0f, "prediction off the server by " + std::to_string(err) + " units");
  CHECK(predicted.ground != -1);

  // Into the wall: both stop against it, so the error closes.
  for (int i = 0; i < 40; i++) sh::run_command(server, cmds[i], set, m.world);
  const sh::NetPlayer at_wall = sh::quantize_player(server);
  const sh::MoveState p2 = sh::predict_player(at_wall, cmds, 10, set, m.world);
  for (int i = 0; i < 10; i++) sh::run_command(server, cmds[i], set, m.world);
  CHECK_MSG(server.origin.x > 239 && server.origin.x < 240, "against the wall at 240");
  CHECK(std::abs(p2.origin.x - server.origin.x) < 0.125f);
}

TEST(a_state_already_on_the_grid_predicts_to_the_bit) {
  ClientMap m;
  const sh::MoveSettings set;
  sh::MoveState server;
  server.origin = {-100, 50, 24};
  server.velocity = {120, -40, 0};
  server.ground = 0;
  server.ground_normal = {0, 0, 1};
  server.maxspeed = set.maxspeed;
  const sh::NetPlayer n = sh::quantize_player(server);
  const sh::MoveCmd cmds[3] = {forward_cmd(13, 30), forward_cmd(12, 30), forward_cmd(13, 30)};
  const sh::MoveState p = sh::predict_player(n, cmds, 3, set, m.world);
  for (const sh::MoveCmd& c : cmds) sh::run_command(server, c, set, m.world);
  CHECK_BITS(p.origin.x, server.origin.x);
  CHECK_BITS(p.origin.y, server.origin.y);
  CHECK_BITS(p.velocity.x, server.velocity.x);
}

TEST(the_server_sends_the_milliseconds_since_a_players_last_command) {
  Bench b;
  b.start();
  sh::connect_client(b.sv, 0, false, false);
  CHECK(sh::net_msec(b.sv, 0) == 0);
  b.sv.time += 0.1;
  CHECK(sh::net_msec(b.sv, 0) == 100);
  b.sv.time += 0.5;
  CHECK(sh::net_msec(b.sv, 0) == 255);
  b.ent(1).origin = {8.06f, 0, 24};
  b.ent(1).mins = sh::kPlayerMins;
  b.ent(1).velocity = {-3.7f, 0, 0};
  const sh::NetPlayer n = sh::net_player_of(b.sv, 0);
  CHECK(n.origin8[0] == 64 && n.origin8[2] == 192);
  CHECK(n.velocity[0] == -3);
}

TEST(an_other_player_runs_from_its_state_time_to_now_less_the_latency) {
  // The state was 30 ms old when it arrived at 10.0 s; now 10.05 s with 50 ms of latency:
  // the player is drawn at 10.02 s, 50 ms past its state.
  CHECK(sh::other_msec(10.0, 30, 10.05, 0.05) == 50);
  // No latency: never in the future, so now, 80 ms past the state.
  CHECK(sh::other_msec(10.0, 30, 10.05, 0.0) == 80);
  // Long ago: capped at 255; a state newer than the player time runs nothing.
  CHECK(sh::other_msec(10.0, 30, 11.0, 0.0) == 255);
  CHECK(sh::other_msec(10.0, 0, 10.0, 0.5) == 0);

  ClientMap m;
  const sh::MoveSettings set;
  sh::MoveState s;
  s.origin = {0, 0, 24};
  s.velocity = {200, 0, 0};
  s.ground = 0;
  s.ground_normal = {0, 0, 1};
  const sh::NetPlayer n = sh::quantize_player(s);
  const sh::MoveState p = sh::predict_other(n, forward_cmd(0), 80, set, m.world);
  sh::MoveState direct = sh::unquantize_player(n, set);
  sh::run_command(direct, forward_cmd(80), set, m.world);  // split into two 40 ms halves
  CHECK_BITS(p.origin.x, direct.origin.x);
  CHECK(p.origin.x > 15);
}

TEST(smoothing_folds_a_middle_error_and_decays_it_by_its_size) {
  sh::Smoothing s;
  sh::smoothing_frame(s, {2, 0, 0}, {0, 0, 0});  // under 4: taken at once
  CHECK(sh::length(s.correction) == 0);
  sh::smoothing_frame(s, {10, 0, 0}, {0, 0, 0});  // 10 units: folded in
  CHECK(s.correction.x == 10.0f);
  CHECK(sh::smoothed(s, {0, 0, 0}).x == 10.0f);
  sh::smoothing_decay(s, 0.004);  // under 16: 500 units/s
  CHECK(std::abs(s.correction.x - 8.0f) < 1e-4f);
  sh::smoothing_decay(s, 0.01);  // 8 is not under 8: still 500 units/s
  CHECK(std::abs(s.correction.x - 3.0f) < 1e-4f);
  sh::smoothing_decay(s, 0.01);  // under 8: 200 units/s
  CHECK(std::abs(s.correction.x - 1.0f) < 1e-4f);
  sh::smoothing_decay(s, 1.0);
  CHECK(s.correction.x == 0.0f);

  sh::smoothing_frame(s, {40, 0, 0}, {0, 0, 0});  // 32 and above: 1400 units/s
  sh::smoothing_decay(s, 0.005);
  CHECK(std::abs(s.correction.x - 33.0f) < 1e-3f);
  s.scale = 2;  // the smoothing setting scales the rate
  sh::smoothing_decay(s, 0.005);
  CHECK(std::abs(s.correction.x - 19.0f) < 1e-3f);

  sh::smoothing_frame(s, {100, 0, 0}, {0, 0, 0});  // over 64: a teleport snaps
  CHECK(sh::length(s.correction) == 0);
}

TEST(a_client_may_cut_the_error_by_its_latency_drop_a_small_one_or_turn_it_off) {
  sh::Smoothing s;
  sh::smoothing_frame(s, {10, 0, 0}, {0, 0, 0}, 0.052);  // a quarter explained by a step's timing
  CHECK(std::abs(s.correction.x - 7.5f) < 1e-3f);
  sh::smoothing_frame(s, {10, 0, 0}, {0, 0, 0}, 0.01);  // under a step: taken at once
  CHECK(std::abs(s.correction.x - 7.5f) < 1e-3f);
  for (int i = 0; i < 10; i++) sh::smoothing_frame(s, {60, 0, 0}, {0, 0, 0}, 1);
  CHECK(std::abs(sh::length(s.correction) - sh::kSmoothMax) < 1e-3f);  // capped
  s.correction = {5, 0, 0};
  s.drop = 220;
  sh::smoothing_decay(s, 0.02);  // 200 * 0.02 = 4 off leaves 1, under 220 * 0.02: dropped
  CHECK(sh::length(s.correction) == 0);
  s.off = true;
  sh::smoothing_frame(s, {10, 0, 0}, {0, 0, 0});
  CHECK(sh::length(s.correction) == 0);
}

TEST(the_state_after_each_command_and_the_jump_it_took) {
  ClientMap m;
  const sh::MoveSettings set;
  sh::MoveState stand;
  stand.origin = {0, 0, 24};
  stand.ground = 0;
  stand.ground_normal = {0, 0, 1};
  sh::MoveCmd cmds[6];
  for (int i = 0; i < 6; i++) cmds[i] = forward_cmd(13);
  cmds[2].buttons |= sh::kButtonJump;
  cmds[3].buttons |= sh::kButtonJump;  // still held: one jump
  const sh::NetPlayer acked = sh::quantize_player(stand);
  sh::MoveState each[6];
  const sh::MoveState last = sh::predict_player(acked, cmds, 6, set, m.world, each);
  CHECK(each[5].origin.x == last.origin.x && each[5].origin.z == last.origin.z);
  int jumps = 0, at = -1;
  sh::MoveState before = sh::unquantize_player(acked, set);
  for (int i = 0; i < 6; i++) {
    if (sh::move_jumped(before, each[i])) {
      jumps++;
      at = i;
    }
    before = each[i];
  }
  CHECK(jumps == 1 && at == 2);
  CHECK(each[1].origin.x < each[2].origin.x);
}

TEST(a_movers_motion_is_read_off_two_frames_and_it_stops_where_it_stood) {
  sh::MoverTrack t;
  const auto frame = [&](double s, float z) {
    sh::mover_track_frame(t, s);
    sh::mover_track_seen(t, 5, 2, {0, 0, z});
  };
  frame(0.0, 60);
  frame(0.1, 60);  // standing at 60: a stop
  frame(0.2, 50);
  frame(0.3, 40);
  sh::Vec3f v;
  float reach = 0;
  sh::mover_motion(t, 5, v, reach);
  CHECK(std::fabs(v.z + 100) < 0.01f && v.x == 0);
  CHECK(std::fabs(reach - 40) < 0.01f);  // down to where it was built (0); 60 is behind it
  CHECK(sh::mover_ahead({0, 0, 40}, v, reach, 1).z == 0);
  CHECK(std::fabs(sh::mover_ahead({0, 0, 40}, v, reach, 0.1f).z - 30) < 0.01f);
  frame(0.4, 0);
  frame(0.5, 0);
  frame(0.6, 10);  // on its way back up: 60 is ahead
  sh::mover_motion(t, 5, v, reach);
  CHECK(v.z > 0 && std::fabs(reach - 50) < 0.01f);
  // A jump (a train's teleport) or an entity not in the last frame has no motion.
  frame(0.7, 400);
  sh::mover_motion(t, 5, v, reach);
  CHECK(v.z == 0 && reach > 1e29f);
  sh::mover_motion(t, 9, v, reach);
  CHECK(v.z == 0);
}

TEST(a_mover_run_on_carries_its_rider_and_leaves_the_others) {
  const ClientMap map;
  sh::ModelData plat_data = sh::build_brush_model({box({-64, -64, -16}, {64, 64, 0})});
  sh::BrushModel plat = plat_data.model();
  sh::ClientModel mover{&plat, {0, 0, 100}, {0, 0, 100}, 15};
  sh::MoveWorld world;
  sh::client_world(map.model, &mover, 1, nullptr, 0, world);
  sh::MoveState rider;
  rider.origin = {0, 0, 124};
  rider.ground = 1;  // standing on the mover's solid
  sh::MoveState other;
  other.origin = {-500, 0, 24};
  other.ground = 0;
  sh::advance_movers(world, rider, &mover, 1, 0.1f);
  CHECK(std::fabs(world.solids[1].origin.z - 110) < 0.01f);
  CHECK(std::fabs(rider.origin.z - 134) < 0.01f);
  sh::MoveWorld world2;
  sh::client_world(map.model, &mover, 1, nullptr, 0, world2);
  sh::advance_movers(world2, other, &mover, 1, 0.1f);
  CHECK(other.origin.z == 24);
  // It stops after its reach: 15 units, however long it runs.
  sh::advance_movers(world, rider, &mover, 1, 1.0f);
  CHECK(std::fabs(world.solids[1].origin.z - 115) < 0.01f &&
        std::fabs(rider.origin.z - 139) < 0.01f);
}
