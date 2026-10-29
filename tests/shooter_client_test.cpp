// THE PLAYER'S PACKET AND COMMAND: a packet replays what was lost before its newest command (two
// lost: its oldest and middle; one lost: its middle; more: the last command heard, buttons
// cleared, for each beyond two; 20 or more: nothing), late and repeated packets are dropped;
// the speed check banks the time a client is behind, up to 500 ms, and cuts a command that runs
// ahead of the clock and the bank; a command over 50 ms is halved until it is not, the impulse on
// the first piece only; the view's pitch is clamped and a forced view kept; a touch runs once a
// packet however many pieces met it; a hook cannot cancel a landing; a spectator thinks in the
// spectator's hook; a missile spawned after the move runs ahead at once; the server frame's
// clock, its first long physics step and its packet order.
//
// ge:summary Tests: run_packet's lost commands, the speed check, the chop, touches, the frame
// ge:keywords test client packet lost-commands speed-check cmd-chop split touches landing-guard
// ge:keywords spectator server_frame frametime new-missile pitch-clamp fixangle
// ge:skill shooter testing
#include <vector>

#include "check.hpp"
#include "shooter/client.hpp"
#include "shooter/frame.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;

namespace {

// A server whose program puts each player on the floor at x = 128 * slot and records every
// pre-think's step, every post-think and spectator think.
struct Game {
  Bench b;
  std::vector<float> steps;  // each pre-think's frametime
  std::vector<int> selves;   // each pre-think's self
  std::vector<float> impulses;
  int post_thinks = 0, spectator_thinks = 0, spectator_connects = 0;
  std::function<void(sh::Server&)> on_pre, on_post;

  explicit Game(sh::ServerSettings set = {}) {
    sh::Program prog;
    prog.put_client_in_server = b.add([](sh::Server& sv) {
      const int e = sv.g.self;
      sh::Edict& p = sv.edicts[static_cast<size_t>(e)];
      p.solid = sh::kSolidSlidebox;
      p.movetype = sh::kMovetypeWalk;
      p.health = 100;
      sh::set_size(sv, e, sh::kPlayerMins, sh::kPlayerMaxs);
      sh::set_origin(sv, e, {128.0f * static_cast<float>(e - 1), 0, 24});
    });
    prog.player_pre_think = b.add([this](sh::Server& sv) {
      steps.push_back(sv.g.frametime);
      selves.push_back(sv.g.self);
      sh::Edict& p = sv.edicts[static_cast<size_t>(sv.g.self)];
      impulses.push_back(p.impulse);
      p.impulse = 0;
      if (on_pre) on_pre(sv);
    });
    prog.player_post_think = b.add([this](sh::Server& sv) {
      post_thinks++;
      if (on_post) on_post(sv);
    });
    prog.spectator_connect = b.add([this](sh::Server& sv) {
      spectator_connects++;
      sh::Edict& p = sv.edicts[static_cast<size_t>(sv.g.self)];
      p.movetype = sh::kMovetypeNoclip;
      p.solid = sh::kSolidNot;
      p.health = 100;
    });
    prog.spectator_think = b.add([this](sh::Server&) { spectator_thinks++; });
    b.start({ge::test::loop::box({-1024, -1024, -64}, {1024, 1024, 0})}, {}, set, prog);
  }

  sh::Edict& player(int slot) { return b.ent(slot + 1); }
};

sh::ServerSettings unchecked() {
  sh::ServerSettings set;
  set.speedcheck = false;
  return set;
}

sh::MoveCmd cmd(int msec) {
  sh::MoveCmd c;
  c.msec = static_cast<uint8_t>(msec);
  return c;
}

sh::Packet packet(int client, uint32_t sequence, int a, int b, int c) {
  sh::Packet p;
  p.client = client;
  p.sequence = sequence;
  p.cmds[0] = cmd(a);
  p.cmds[1] = cmd(b);
  p.cmds[2] = cmd(c);
  return p;
}

std::vector<float> ms(std::initializer_list<int> list) {
  std::vector<float> v;
  for (int m : list) v.push_back(static_cast<float>(m * 0.001));
  return v;
}

}  // namespace

TEST(a_packet_replays_what_was_lost_before_its_newest_command) {
  Game g(unchecked());
  sh::connect_client(g.b.sv, 0, false, false);
  CHECK(sh::run_packet(g.b.sv, packet(0, 1, 10, 11, 12)) == 0);
  CHECK(g.steps == ms({12}));
  g.steps.clear();
  CHECK(sh::run_packet(g.b.sv, packet(0, 3, 20, 21, 22)) == 1);  // one lost: the middle
  CHECK(g.steps == ms({21, 22}));
  g.steps.clear();
  CHECK(sh::run_packet(g.b.sv, packet(0, 6, 30, 31, 32)) == 2);  // two: the oldest and middle
  CHECK(g.steps == ms({30, 31, 32}));
  g.steps.clear();
  CHECK(sh::run_packet(g.b.sv, packet(0, 11, 40, 41, 42)) == 4);  // four: the last heard twice
  CHECK(g.steps == ms({32, 32, 40, 41, 42}));
  g.steps.clear();
  CHECK(sh::run_packet(g.b.sv, packet(0, 31, 50, 51, 52)) == 19);  // 19: still replayed
  CHECK(g.steps.size() == 17 + 1 + 2 + 2);                         // 51 and 52 ms run as halves
  g.steps.clear();
  CHECK(sh::run_packet(g.b.sv, packet(0, 52, 60, 61, 62)) == 20);  // 20: nothing replayed
  CHECK(g.steps == ms({31, 31}));  // the newest alone, 62 ms as halves
  g.steps.clear();
  CHECK(sh::run_packet(g.b.sv, packet(0, 52, 60, 61, 62)) == -1);  // repeated
  CHECK(sh::run_packet(g.b.sv, packet(0, 40, 60, 61, 62)) == -1);  // late
  CHECK(g.steps.empty());
  CHECK(g.post_thinks == 6);
  CHECK(g.b.sv.counts.commands_lost == 1 + 2 + 4 + 19);
}

TEST(the_last_command_heard_is_replayed_without_its_buttons) {
  Game g(unchecked());
  sh::connect_client(g.b.sv, 0, false, false);
  sh::Packet p = packet(0, 1, 10, 11, 12);
  p.cmds[2].buttons = sh::kButtonAttack | sh::kButtonJump;
  sh::run_packet(g.b.sv, p);
  CHECK(g.b.sv.clients[0].last_cmd.buttons == 0);
  CHECK(g.b.sv.clients[0].last_cmd.msec == 12);
}

TEST(the_speed_check_banks_the_lag_and_cuts_what_runs_ahead) {
  sh::ClientSlot c;
  c.speed_check_time = 1.0;
  CHECK(sh::speed_check(c, 1.013, 13) == 13);
  CHECK(c.speed_bank_ms == 0);
  CHECK(sh::speed_check(c, 1.026, 20) == 13);  // 7 ahead, nothing banked
  CHECK(sh::speed_check(c, 1.126, 40) == 40);  // 100 elapsed: 60 banked
  CHECK(c.speed_bank_ms == 60);
  CHECK(sh::speed_check(c, 1.136, 50) == 50);  // 40 over, paid from the bank
  CHECK(c.speed_bank_ms == 20);
  CHECK(sh::speed_check(c, 1.146, 50) == 30);  // 40 over, the bank has 20
  CHECK(c.speed_bank_ms == 0);
  CHECK(sh::speed_check(c, 3.0, 13) == 13);  // a long pause banks at most 500
  CHECK(c.speed_bank_ms == sh::kSpeedBankMaxMs);
  // Elapsed time is rounded to the millisecond: 12.6 ms is 13.
  sh::ClientSlot r;
  r.speed_check_time = 1.0;
  CHECK(sh::speed_check(r, 1.0126, 13) == 13);
}

TEST(a_checked_command_is_cut_and_a_bots_is_not) {
  Game g;
  sh::connect_client(g.b.sv, 0, false, false);
  sh::connect_client(g.b.sv, 1, false, true);
  g.b.sv.time += 0.013;
  sh::run_player_command(g.b.sv, 0, cmd(40));
  sh::run_player_command(g.b.sv, 1, cmd(40));
  CHECK(g.steps == ms({13, 40}));
  CHECK(g.b.sv.counts.speed_cuts == 1);
}

TEST(a_long_command_is_halved_until_it_fits_the_impulse_first) {
  Game g(unchecked());
  sh::connect_client(g.b.sv, 0, false, false);
  sh::MoveCmd c = cmd(120);
  c.impulse = 9;
  sh::run_player_command(g.b.sv, 0, c);
  CHECK(g.steps == ms({30, 30, 30, 30}));
  CHECK((g.impulses == std::vector<float>{9, 0, 0, 0}));
  g.steps.clear();
  sh::run_player_command(g.b.sv, 0, cmd(51));  // a millisecond is lost to the halving
  CHECK(g.steps == ms({25, 25}));
  g.steps.clear();
  sh::run_player_command(g.b.sv, 0, cmd(50));
  CHECK(g.steps == ms({50}));
}

TEST(the_pitch_is_clamped_and_a_forced_view_kept) {
  Game g(unchecked());
  sh::connect_client(g.b.sv, 0, false, false);
  sh::MoveCmd c = cmd(13);
  c.angles = {89, 45, 0};
  sh::run_player_command(g.b.sv, 0, c);
  CHECK(g.player(0).v_angle.x == 80);
  CHECK(g.player(0).v_angle.y == 45);
  CHECK(g.player(0).angles.x == -80.0f / 3);
  c.angles = {-89, 10, 0};
  sh::run_player_command(g.b.sv, 0, c);
  CHECK(g.player(0).v_angle.x == -70);
  g.player(0).fixangle = true;
  c.angles = {0, 90, 0};
  sh::run_player_command(g.b.sv, 0, c);
  CHECK(g.player(0).v_angle.y == 10);
}

TEST(a_touch_runs_once_a_packet_however_many_pieces_meet_it) {
  Game g(unchecked());
  int touches = 0;
  const sh::Func touch = g.b.add([&](sh::Server& sv) {
    touches++;
    CHECK(sv.g.other == 1);
  });
  sh::connect_client(g.b.sv, 0, false, false);
  const int wall =
      g.b.spawn_box({40, 0, 24}, {-16, -16, -24}, {16, 16, 32}, sh::kSolidBbox, sh::kMovetypeNone);
  g.b.ent(wall).touch = touch;
  g.player(0).velocity = {320, 0, 0};
  sh::Packet p = packet(0, 1, 13, 13, 200);  // 200 ms: four pieces, each into the box
  p.cmds[2].forward = 400;
  sh::run_packet(g.b.sv, p);
  CHECK(touches == 1);
  p.sequence = 2;
  sh::run_packet(g.b.sv, p);
  CHECK(touches == 2);
  CHECK(g.player(0).origin.x < 8.01f);
}

TEST(a_hook_cannot_cancel_a_landing) {
  Game g(unchecked());
  std::vector<float> seen;
  const sh::Func think = g.b.add([&](sh::Server& sv) {
    seen.push_back(sv.edicts[static_cast<size_t>(sv.g.self)].velocity.z);
  });
  sh::connect_client(g.b.sv, 0, false, false);
  bool also_x = false;
  g.on_pre = [&](sh::Server& sv) {
    sh::Edict& p = sv.edicts[1];
    p.velocity.z = 0;
    if (also_x) p.velocity.x = 5;
  };
  for (bool x : {false, true}) {
    also_x = x;
    sh::Edict& p = g.player(0);
    p.flags |= sh::kFlagOnGround;
    p.velocity = {0, 0, -50};
    p.think = think;
    p.nextthink = static_cast<float>(g.b.sv.time);
    sh::run_player_command(g.b.sv, 0, cmd(13), false);
  }
  CHECK(seen.size() == 2);
  CHECK(seen[0] == -50);  // put back
  CHECK(seen[1] == 0);    // the hook changed more than the fall: left as it set it
}

TEST(a_spectator_thinks_in_the_spectators_hook) {
  Game g(unchecked());
  sh::connect_client(g.b.sv, 0, true, false);
  CHECK(g.spectator_connects == 1);
  sh::run_packet(g.b.sv, packet(0, 1, 13, 13, 13));
  CHECK(g.steps.empty());
  CHECK(g.post_thinks == 0);
  CHECK(g.spectator_thinks == 1);
  CHECK(sh::move_type_of(g.player(0), false) == sh::MoveType::spectator);
  CHECK(sh::move_type_of(g.player(0), true) == sh::MoveType::old_spectator);
}

TEST(a_missile_spawned_after_the_move_runs_ahead_at_once) {
  Game g(unchecked());
  int missile = 0;
  g.on_post = [&](sh::Server& sv) {
    missile = sh::spawn_edict(sv);
    sh::Edict& m = sv.edicts[static_cast<size_t>(missile)];
    m.solid = sh::kSolidBbox;
    m.movetype = sh::kMovetypeFlyMissile;
    m.velocity = {1000, 0, 0};
    m.owner = sv.g.self;
    sh::set_origin(sv, missile, {0, 0, 40});
    sv.g.newmis = missile;
  };
  sh::connect_client(g.b.sv, 0, false, false);
  sh::run_packet(g.b.sv, packet(0, 1, 13, 13, 13));
  CHECK(g.b.sv.counts.new_missiles == 1);
  CHECK(g.b.ent(missile).origin.x == 1000 * static_cast<float>(0.05));
}

TEST(the_server_frame_keeps_its_clock_and_runs_packets_in_their_order) {
  Game g(unchecked());
  sh::connect_client(g.b.sv, 0, false, false);
  sh::connect_client(g.b.sv, 1, false, false);
  const sh::Packet packets[3] = {packet(1, 1, 13, 13, 13), packet(0, 1, 13, 13, 13),
                                 packet(1, 2, 13, 13, 13)};
  sh::server_frame(g.b.sv, packets, 3);
  CHECK((g.selves == std::vector<int>{2, 1, 2}));
  CHECK_BITS(g.b.sv.time, 1.0 + 1 * 0.013);
  CHECK_BITS(g.b.sv.g.frametime, 0.1f);  // the first physics step is the longest
  sh::server_frame(g.b.sv, nullptr, 0);
  CHECK_BITS(g.b.sv.time, 1.0 + 2 * 0.013);
  CHECK_BITS(g.b.sv.g.frametime, 0.013f);
  for (int i = 0; i < 98; i++) sh::server_frame(g.b.sv, nullptr, 0);
  CHECK_BITS(g.b.sv.time, 1.0 + 100 * 0.013);  // no drift: the time is counted, not summed
  CHECK(g.b.sv.counts.physics_frames == 100);
}

TEST(the_moves_solids_are_the_world_and_what_is_near) {
  Game g(unchecked());
  sh::connect_client(g.b.sv, 0, false, false);
  const int near =
      g.b.spawn_box({100, 0, 24}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  g.b.spawn_box({600, 0, 24}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  const int owned =
      g.b.spawn_box({50, 0, 24}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  g.b.ent(owned).owner = 1;
  g.b.spawn_box({60, 0, 24}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidTrigger, sh::kMovetypeNone);
  sh::MoveWorld w;
  int ents[sh::kMaxSolids];
  sh::build_player_world(g.b.sv, 1, g.player(0).origin, w, ents);
  CHECK(w.count == 2);
  CHECK(ents[0] == 0 && ents[1] == near);
}
