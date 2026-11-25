// Tests: teleports a client foresees (plugins/shooter/teleport_ahead.hpp). The box test grows the
// player's box as the server does; a replay that walks into a teleporter comes out at its far end
// with its speed, the hit naming the command; a guess turns the view once, keeping the turning
// made since; the server's state settles it (kept when he is there, undone two commands past it
// when he is not, that teleporter then left out while he stands in it), and the server's own turn
// to the same view is the guess's. A push sets the speed in the replay without a teleport, and the
// tables keep the teleporters first.
//
// ge:summary Tests: the client's foreseen teleports: the box, the replay, the guess and its answers
// ge:keywords test teleport-prediction teleporter_at tele_guess predict_player_teleports
// ge:keywords pusher_at tele-tables
// ge:skill shooter testing
#include <cmath>

#include "check.hpp"
#include "shooter/teleport_ahead.hpp"

namespace sh = ge::shooter;

namespace {

sh::Teleporters one_pad() {
  sh::Teleporter t;
  t.lo = {100, -50, -10};
  t.hi = {140, 50, 80};
  t.dest = {1000, 1000, 50};
  t.angles = {0, 90, 0};
  t.velocity = {0, 300, 0};
  return {t};
}

}  // namespace

TEST(the_players_box_reaches_a_teleporter_a_unit_early_as_the_server_grows_it) {
  const sh::Teleporters t = one_pad();
  // His box reaches x = origin + 16; the pad starts at 100: at 83 the grown box touches it.
  CHECK(sh::teleporter_at(t, {83.5f, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, 1) == 0);
  CHECK(sh::teleporter_at(t, {82, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, 1) == -1);
  CHECK(sh::teleporter_at(t, {120, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, 1, 0) == -1);
}

TEST(a_replay_walking_into_a_teleporter_comes_out_at_its_far_end) {
  const sh::Teleporters t = one_pad();
  sh::MoveWorld world;  // nothing to stand on: in the air, as from a jump
  sh::MoveState start;
  start.origin = {60, 0, 24};
  start.velocity = {320, 0, 0};
  const sh::NetPlayer n = sh::quantize_player(start);
  sh::MoveCmd cmds[10];
  for (sh::MoveCmd& c : cmds) {
    c.msec = 13;
    c.forward = 400;
  }
  sh::TeleHit hit;
  sh::MoveState each[10];
  const sh::MoveState s =
      sh::predict_player_teleports(n, cmds, 10, sh::MoveSettings{}, world, each, &t, -1, &hit);
  CHECK(hit.index == 0 && hit.cmd >= 0 && hit.cmd < 10);
  // The command that hit left him at the destination with its speed; the later ones from there.
  CHECK(each[hit.cmd].origin.x == 1000 && each[hit.cmd].velocity.y == 300);
  CHECK(s.origin.y > 1000);
  // Without the table, he walks on through the box.
  const sh::MoveState plain = sh::predict_player_teleports(n, cmds, 10, sh::MoveSettings{}, world,
                                                           nullptr, nullptr, -1, &hit);
  CHECK(hit.cmd == -1 && plain.origin.x > 100 && plain.origin.x < 200);
}

TEST(a_guess_turns_the_view_once_and_the_server_settles_it) {
  const sh::Teleporters t = one_pad();
  sh::TeleGuess g;
  // Walked in on command 50 facing 0; the mouse has since turned 5 more and pitched 10 down.
  sh::Vec3f view{10, 5, 0};
  const sh::Vec3f turn = sh::tele_guess_hit(g, t, 50, 0, {0, 0, 0}, view);
  CHECK(view.y == 95 && view.x == 0 && turn.y == 90);
  // Found again on the next prediction (or a command later): no second turn.
  sh::tele_guess_hit(g, t, 51, 0, {0, 0, 0}, view);
  CHECK(view.y == 95 && g.seq == 51);
  // The server answers command 51 with him at the far end: kept.
  sh::tele_guess_acked(g, t, 51, {1000, 1010, 50}, sh::kPlayerMins, sh::kPlayerMaxs, view);
  CHECK(g.seq == 0 && view.y == 95);

  // Another walk in, but the server leaves him by the pad (a teleporter not yet switched on).
  sh::TeleGuess w;
  sh::Vec3f v2{0, 0, 0};
  sh::tele_guess_hit(w, t, 80, 0, {0, 0, 0}, v2);
  v2.y += 3;  // his own turning since
  sh::tele_guess_acked(w, t, 80, {110, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, v2);
  CHECK(v2.y == 93);  // one command is not enough to believe a no
  sh::tele_guess_acked(w, t, 82, {112, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, v2);
  CHECK(v2.y == 3 && w.undone && w.skip == 0);
  // Left out while the server has him in its box, back once he is out.
  sh::tele_guess_acked(w, t, 83, {115, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, v2);
  CHECK(w.skip == 0);
  // The server's turn after all (the teleport came a few commands late): turned again.
  CHECK(sh::tele_guess_setangle(w, t, {0, 90, 0}, v2));
  CHECK(v2.y == 93 && !w.undone && w.skip == -1);
  sh::tele_guess_acked(w, t, 90, {0, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, v2);
  CHECK(w.skip == -1);
}

TEST(the_servers_turn_is_the_guesss_only_when_it_faces_the_same_way) {
  const sh::Teleporters t = one_pad();
  sh::TeleGuess g;
  sh::Vec3f view{0, 0, 0};
  sh::tele_guess_hit(g, t, 10, 0, {0, 0, 0}, view);
  view.y += 4;
  sh::Vec3f kept = view;
  CHECK(sh::tele_guess_setangle(g, t, {0, 90.5f, 0}, kept));
  CHECK(kept.y == view.y && g.index == -1);
  // A respawn (another facing) while a guess stands: not the guess's, which is dropped.
  sh::TeleGuess h;
  sh::Vec3f v{0, 0, 0};
  sh::tele_guess_hit(h, t, 20, 0, {0, 0, 0}, v);
  CHECK(!sh::tele_guess_setangle(h, t, {0, 270, 0}, v));
  CHECK(h.seq == 0 && h.index == -1);
  // Nothing foreseen: never the guess's.
  sh::TeleGuess none;
  CHECK(!sh::tele_guess_setangle(none, t, {0, 90, 0}, v));
}

TEST(a_push_sets_the_speed_in_the_replay_and_the_tables_keep_teleporters_first) {
  sh::Teleporter push;
  push.lo = {100, -50, -10};
  push.hi = {140, 50, 80};
  push.velocity = {0, 0, 500};
  push.push = true;
  sh::TeleTables tables;
  tables.fill(one_pad(), {push});
  CHECK(tables.with_push.size() == 2 && !tables.with_push[0].push && tables.with_push[1].push);
  CHECK(tables.table(true, true) == &tables.with_push && tables.table(true, false) == &tables.tele);
  CHECK(tables.table(false, true) == &tables.push_only && tables.table(false, false) == nullptr);
  // A push is no teleporter: the box test for one passes over it.
  CHECK(sh::teleporter_at(tables.push_only, {120, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, 1) ==
        -1);
  CHECK(sh::pusher_at(tables.push_only, {120, 0, 24}, sh::kPlayerMins, sh::kPlayerMaxs, 1) == 0);
  sh::MoveWorld world;
  sh::MoveState start;
  start.origin = {60, 0, 24};
  start.velocity = {320, 0, 0};
  sh::MoveCmd cmds[10];
  for (sh::MoveCmd& c : cmds) {
    c.msec = 13;
    c.forward = 400;
  }
  sh::MoveState each[10];
  sh::TeleHit hit;
  sh::predict_player_teleports(sh::quantize_player(start), cmds, 10, sh::MoveSettings{}, world,
                               each, &tables.push_only, -1, &hit);
  int thrown = -1;
  for (int i = 0; i < 10 && thrown < 0; i++)
    if (each[i].velocity.z > 400) thrown = i;
  CHECK(thrown >= 0 && hit.cmd == -1);  // thrown up, and no teleport named
}
