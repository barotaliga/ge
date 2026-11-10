// THE CLIENT'S PREDICTION ON THE ORIGINAL MOVE: a server running the original move per command
// (the multiplayer clock), and a client predicting its player from the protocol's copy of the
// acknowledged state through the same step on its stand-in server: within the grid's error while
// running, up a step and into a wall; to the bit from a state already on the grid; the jump the
// engine makes for a multiplayer program predicted and the one a single-player program makes not;
// a mover and another player where the client saw them block the prediction as they block the
// server; a command over 50 ms split as the server splits it.
//
// ge:summary Tests: the original move predicted on a client against the server's own step
// ge:keywords test predict prediction original-move stand-in-server sv_playermove jump step-up
// ge:skill shooter testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooter/client.hpp"
#include "shooter/frame_recording.hpp"
#include "shooter/predict.hpp"
#include "shooter/predict_original.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::box;

namespace {

// A floor at z = 0, a step 16 high from x = 160, a wall at x = 480; model *1 a block at y 200.
struct Level {
  std::vector<sh::ModelData> data;
  std::vector<sh::BrushModel> models;

  Level() {
    data.push_back(sh::build_brush_model({box({-1024, -1024, -64}, {1024, 1024, 0}),
                                          box({160, -1024, 0}, {1024, 1024, 16}),
                                          box({480, -1024, 16}, {512, 1024, 256})}));
    data.push_back(sh::build_brush_model({box({-64, 180, 0}, {64, 220, 128})}));
    for (const sh::ModelData& d : data) models.push_back(d.model());
  }
};

struct Host {
  sh::Server sv;
  explicit Host(const Level& l, bool program_jumps = false) {
    sh::ServerSettings set;
    set.maxclients = 2;
    set.max_edicts = 16;
    set.playermove = sh::PlayerMove::original;
    set.program_jumps = program_jumps;
    sh::create_server(sv, set, l.models, sh::Program{});
    sh::connect_client(sv, 0, false, true);
    sh::stand_frame_player(sv, 1, {0, 0, 24}, {});
  }
  const sh::Edict& player() const { return sv.edicts[1]; }
  // The block (*1, the server's modelindex 2) at `at`.
  void block(sh::Vec3f at) {
    const int e = sh::spawn_edict(sv);
    sv.edicts[static_cast<size_t>(e)].solid = sh::kSolidBsp;
    sv.edicts[static_cast<size_t>(e)].movetype = sh::kMovetypePush;
    sh::set_brush_model(sv, e, 2);
    sh::set_origin(sv, e, at);
  }
  // Another player's box at `at` (slot 2).
  void other(sh::Vec3f at) {
    sh::connect_client(sv, 1, false, true);
    sh::stand_frame_player(sv, 2, at, {});
  }
};

sh::MoveCmd cmd(int i, float yaw, int16_t forward = 400, uint8_t buttons = 0, uint8_t msec = 0) {
  sh::MoveCmd c;
  c.msec = msec != 0 ? msec : static_cast<uint8_t>(i % 2 == 0 ? 13 : 12);
  c.angles = {0, yaw, 0};
  c.forward = forward;
  c.buttons = buttons;
  return c;
}

uint32_t g_sequence = 0;
void run(Host& h, const sh::MoveCmd& c) {
  sh::Packet p;
  p.sequence = ++g_sequence;
  for (sh::MoveCmd& m : p.cmds) m = c;
  sh::run_packet(h.sv, p);
}

float distance(sh::Vec3f a, sh::Vec3f b) { return sh::length(a - b); }

// The server runs `cmds[0..split)`, the client is told its state, then both run the rest.
struct Race {
  float error = 0;
  sh::MoveState predicted;
  sh::Vec3f server;
};
Race race(Host& h, const Level& l, const std::vector<sh::MoveCmd>& cmds, size_t split,
          const std::vector<sh::ModelAt>& movers = {}, const std::vector<sh::Vec3f>& others = {}) {
  for (size_t i = 0; i < split; i++) run(h, cmds[i]);
  const sh::NetPlayer acked = sh::net_player_of(h.sv, 0);
  sh::OriginalPrediction p;
  sh::original_prediction_level(p, l.models, h.sv.set);
  sh::original_prediction_place(p, movers.data(), static_cast<int>(movers.size()), others.data(),
                                static_cast<int>(others.size()));
  Race r;
  r.predicted = sh::predict_player_original(p, acked, cmds.data() + split,
                                            static_cast<int>(cmds.size() - split));
  for (size_t i = split; i < cmds.size(); i++) run(h, cmds[i]);
  r.server = h.player().origin;
  r.error = distance(r.predicted.origin, r.server);
  return r;
}

std::string at(sh::Vec3f v) {
  return std::to_string(v.x) + " " + std::to_string(v.y) + " " + std::to_string(v.z);
}

}  // namespace

TEST(the_original_move_predicts_the_servers_run_up_a_step_and_into_a_wall) {
  const Level l;
  for (const size_t split : {size_t{10}, size_t{30}, size_t{60}}) {
    Host h(l);
    std::vector<sh::MoveCmd> cmds;
    cmds.reserve(160);
    for (int i = 0; i < 120; i++) cmds.push_back(cmd(i, 0));
    const Race r = race(h, l, cmds, split);
    CHECK_MSG(r.error < 1.0f, "split " + std::to_string(split) + ": predicted " +
                                  at(r.predicted.origin) + " server " + at(r.server));
    CHECK(r.predicted.ground != -1);
  }
  // The whole run: up the step (z 40, the original walk resting a hair above) and stopped by the
  // wall (x under 464).
  Host h(l);
  std::vector<sh::MoveCmd> cmds;
  cmds.reserve(160);
  for (int i = 0; i < 160; i++) cmds.push_back(cmd(i, 0));
  const Race r = race(h, l, cmds, 100);
  CHECK_MSG(r.server.z > 39.9f && r.server.z < 40.1f && r.server.x > 463 && r.server.x < 464,
            at(r.server));
  CHECK_MSG(r.error < 0.125f, "predicted " + at(r.predicted.origin) + " server " + at(r.server));
}

TEST(a_state_on_the_grid_predicts_the_original_move_to_the_bit) {
  const Level l;
  Host h(l);
  sh::stand_frame_player(h.sv, 1, {-100, 50, 24}, {120, -40, 0});
  h.sv.edicts[1].flags |= sh::kFlagOnGround;
  std::vector<sh::MoveCmd> cmds = {cmd(0, 30), cmd(1, 30), cmd(2, 30), cmd(3, 30)};
  const Race r = race(h, l, cmds, 0);
  CHECK_BITS(r.predicted.origin.x, r.server.x);
  CHECK_BITS(r.predicted.origin.y, r.server.y);
  CHECK_BITS(r.predicted.velocity.x, h.player().velocity.x);
}

TEST(the_engines_jump_is_predicted_and_a_programs_is_not) {
  const Level l;
  for (const bool program_jumps : {false, true}) {
    Host h(l, program_jumps);
    std::vector<sh::MoveCmd> cmds;
    cmds.reserve(160);
    for (int i = 0; i < 20; i++) cmds.push_back(cmd(i, 90, 0));
    for (int i = 20; i < 24; i++) cmds.push_back(cmd(i, 90, 0, sh::kButtonJump));
    for (int i = 24; i < 40; i++) cmds.push_back(cmd(i, 90, 0));
    const Race r = race(h, l, cmds, 20);
    CHECK_MSG(r.error < 0.125f, "predicted " + at(r.predicted.origin) + " server " + at(r.server));
    CHECK_MSG((r.server.z > 30) == !program_jumps, at(r.server));
  }
}

TEST(a_mover_and_another_player_block_the_prediction_as_they_block_the_server) {
  const Level l;
  {
    Host h(l);
    h.block({0, 0, 0});
    std::vector<sh::MoveCmd> cmds;
    cmds.reserve(160);
    for (int i = 0; i < 80; i++) cmds.push_back(cmd(i, 90));
    const Race r = race(h, l, cmds, 10, {{1, {0, 0, 0}}});
    CHECK_MSG(r.server.y < 165, at(r.server));
    CHECK_MSG(r.error < 0.125f, "predicted " + at(r.predicted.origin) + " server " + at(r.server));
    // Without the mover the client runs through it.
    Host h2(l);
    h2.block({0, 0, 0});
    const Race blind = race(h2, l, cmds, 10);
    CHECK(blind.predicted.origin.y > 220);
  }
  {
    Host h(l);
    h.other({0, -120, 24});
    std::vector<sh::MoveCmd> cmds;
    cmds.reserve(160);
    for (int i = 0; i < 60; i++) cmds.push_back(cmd(i, -90));
    const Race r = race(h, l, cmds, 5, {}, {{0, -120, 24}});
    CHECK_MSG(r.server.y > -89, at(r.server));
    CHECK_MSG(r.error < 0.125f, "predicted " + at(r.predicted.origin) + " server " + at(r.server));
  }
}

TEST(a_long_command_is_split_as_the_server_splits_it) {
  const Level l;
  Host h(l);
  std::vector<sh::MoveCmd> cmds;
  cmds.reserve(160);
  for (int i = 0; i < 10; i++) cmds.push_back(cmd(i, 45, 400, 0, 120));
  const Race r = race(h, l, cmds, 3);
  CHECK_MSG(r.error < 1.0f, "predicted " + at(r.predicted.origin) + " server " + at(r.server));
  CHECK(r.server.x > 100);
}
