// THE PLAYER'S OWN MISSILES THROWN AHEAD OF THE SERVER (cl_predict_projectiles): the launch by
// Quake's rules (the nail's barrel, the rocket's midair speed, the grenade's arc), dated back by
// the shot's age, shown and ended by the round trip; a wall stops a nail; a grenade bounces off
// the floor; a server copy near a stand-in about to end takes it over; at most 96 fly; none off,
// or with no ping; drawn only with the server's model.
//
// ge:summary Tests: the own missiles thrown ahead: launch, age, wall, bounce, handover, drawn
// ge:keywords test own_missiles cl_predict_projectiles handover grenade nail rocket
// ge:skill net shooter testing
#include <cmath>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/own_missiles.hpp"

namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

std::unique_ptr<cl::LoadedMap> halls() {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error), error);
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(bytes, "halls", error);
  CHECK_MSG(map != nullptr, error);
  return map;
}

bool near(float a, float b, float tol = 0.01f) { return std::abs(a - b) <= tol; }

constexpr sh::Vec3f kAt = {128, 192, 24};  // the halls' spot, the player's origin

}  // namespace

TEST(a_nail_leaves_its_barrel_and_flies_at_once_dated_back_by_its_age) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.kind = cl::OwnMissileKind::nail;
  shot.origin = kAt;
  shot.angles = {0, 0, 0};  // due east (+x)
  shot.side = 1;            // the right barrel: -y facing +x
  CHECK(cl::own_missile_fire(m, shot, 10, 0.1, 100, kAt));
  CHECK(m.flying.size() == 1);
  const cl::OwnMissile& o = m.flying[0];
  CHECK(near(o.origin.x, kAt.x + 8) && near(o.origin.y, kAt.y - 4) && near(o.origin.z, kAt.z + 16));
  CHECK(near(o.velocity.x, cl::kNailSpeedU) && near(o.velocity.y, 0) && near(o.velocity.z, 0));
  CHECK(std::abs(o.end_s - (10 + 0.1 + cl::kOwnMissileLeadS + cl::kOwnMissileWaitS)) < 1e-9);
  CHECK(o.show_s <= 10);  // the server measured the same round trip: at once
  // Fired 0.02 s ago: flown 20 units by now.
  shot.side = -1;
  shot.age_s = 0.02;
  CHECK(cl::own_missile_fire(m, shot, 10, 0.1, 100, kAt));
  cl::own_missiles_step(m, nullptr, 800, 10);
  CHECK(near(m.flying[1].origin.x, kAt.x + 8 + 20, 0.05f));
  CHECK(near(m.flying[1].origin.y, kAt.y + 4));  // the left barrel
}

TEST(a_rocket_flies_twice_as_fast_under_the_midair_rule) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.kind = cl::OwnMissileKind::rocket;
  shot.origin = kAt;
  shot.midair = true;
  CHECK(cl::own_missile_fire(m, shot, 0, 0.05, 50, kAt));
  CHECK(near(m.flying[0].velocity.x, cl::kMidairRocketSpeedU));
  shot.midair = false;
  shot.side = 1;  // a rocket leaves from the middle whatever the barrel
  CHECK(cl::own_missile_fire(m, shot, 0, 0.05, 50, kAt));
  CHECK(near(m.flying[1].velocity.x, cl::kRocketSpeedU) && near(m.flying[1].origin.y, kAt.y));
}

TEST(none_off_or_with_no_ping) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.origin = kAt;
  CHECK(!cl::own_missile_fire(m, shot, 0, 0.1, 0, kAt));  // a game on this machine
  m.on = false;
  CHECK(!cl::own_missile_fire(m, shot, 0, 0.1, 100, kAt));
  CHECK(m.flying.empty() && m.thrown == 0);
}

TEST(it_shows_when_the_server_s_copy_is_fired_and_ends_when_it_is_drawn) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.kind = cl::OwnMissileKind::grenade;
  shot.origin = kAt;
  // The server measures 20 ms where the round trip is 100: its copy is 0.067 s later than ours.
  CHECK(cl::own_missile_fire(m, shot, 5, 0.1, 20, kAt));
  CHECK(std::abs(m.flying[0].show_s - (5 + 0.1 - cl::kOwnMissileLeadS - 0.02)) < 1e-9);
  // A grenade: the round trip and the wait alone.
  CHECK(std::abs(m.flying[0].end_s - (5.1 + cl::kOwnMissileWaitS)) < 1e-9);
  cl::DemoState s;
  s.models = {"", "progs/player.mdl", "progs/grenade.mdl"};
  cl::own_missiles_draw(m, s, 5.01);
  CHECK(s.own_missiles.empty());  // not yet
  cl::own_missiles_draw(m, s, 5.08);
  CHECK(s.own_missiles.size() == 1 && s.own_missiles[0].model == 2);
  CHECK(s.own_missile_ids.size() == 1 && s.own_missile_ids[0] == m.flying[0].id);
  cl::own_missiles_step(m, nullptr, 800, 5.1 + cl::kOwnMissileWaitS);
  CHECK(m.flying.empty() && m.ended == 1);
}

TEST(not_drawn_without_the_server_s_model) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.origin = kAt;
  CHECK(cl::own_missile_fire(m, shot, 0, 0.1, 100, kAt));
  cl::DemoState s;
  s.models = {"", "progs/player.mdl"};
  cl::own_missiles_draw(m, s, 0);
  CHECK(s.own_missiles.empty());
}

TEST(a_wall_stops_a_nail_and_a_grenade_bounces_off_the_floor) {
  const auto map = halls();
  if (!map) return;
  const sh::Hull* world = &map->bsp.world.models[0].hulls[0];
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.origin = kAt;
  // Every way along the floor ends at a wall within 4000 units.
  for (float yaw : {0.0f, 90.0f, 180.0f, 270.0f}) {
    shot.angles = {0, yaw, 0};
    CHECK(cl::own_missile_fire(m, shot, 0, 10, 10000, kAt));
  }
  cl::own_missiles_step(m, world, 800, 4);
  CHECK(m.walled == 4 && m.flying.empty());
  // A grenade lobbed down at the floor: falling, then rising off it.
  shot.kind = cl::OwnMissileKind::grenade;
  shot.angles = {60, 0, 0};  // pitched down
  CHECK(cl::own_missile_fire(m, shot, 0, 10, 10000, kAt));
  float lowest = 1e9f;
  bool rose = false;
  for (int i = 1; i <= 50; i++) {
    cl::own_missiles_step(m, world, 800, i * 0.01);
    if (m.flying.empty()) break;
    lowest = std::min(lowest, m.flying[0].origin.z);
    if (m.flying[0].velocity.z > 0) rose = true;
  }
  CHECK(rose);
  CHECK(lowest >= -0.01f);  // never through the floor
}

TEST(a_server_copy_on_a_stand_in_s_path_adopts_it_and_is_hidden_till_it_is_gone) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.kind = cl::OwnMissileKind::rocket;
  shot.origin = kAt;
  CHECK(cl::own_missile_fire(m, shot, 0, 0.1, 100, kAt));
  for (int i = 1; i <= 10; i++) cl::own_missiles_step(m, nullptr, 800, i * 0.02);
  const sh::Vec3f ahead = m.flying[0].origin;  // 200 units on
  cl::DemoState s;
  s.format = cl::DemoFormat::client;
  s.models = {"", "progs/player.mdl", "progs/missile.mdl", "progs/grenade.mdl"};
  s.players.resize(2);
  s.entities.resize(5);
  cl::DemoEntity& e = s.entities[3];
  e.present = true;
  e.model = 2;
  // The server's copy, drawn a round trip behind: 100 units back on the path, 10 off it.
  e.origin = ahead - sh::Vec3f{100, -10, 0};
  cl::DemoEntity& g = s.entities[4];  // a grenade there: another model, never adopted
  g = e;
  g.model = 3;
  cl::own_missiles_handover(m, s);
  CHECK(m.handed == 1 && m.flying[0].adopted == 3);
  CHECK(!s.entities[3].present && s.entities[4].present);
  // Adopted, it flies on past its end, the copy hidden.
  cl::own_missiles_step(m, nullptr, 800, 1);
  CHECK(m.flying.size() == 1 && m.ended == 0);
  s.entities[3].present = true;
  cl::own_missiles_handover(m, s);
  CHECK(m.flying.size() == 1 && !s.entities[3].present);
  // The copy gone (it hit): the stand-in goes with it.
  cl::own_missiles_handover(m, s);
  CHECK(m.flying.empty());
}

TEST(a_copy_far_from_every_path_is_not_adopted) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.kind = cl::OwnMissileKind::rocket;
  shot.origin = kAt;
  CHECK(cl::own_missile_fire(m, shot, 0, 0.1, 100, kAt));
  cl::own_missiles_step(m, nullptr, 800, 0.05);
  cl::DemoState s;
  s.format = cl::DemoFormat::client;
  s.models = {"", "progs/player.mdl", "progs/missile.mdl"};
  s.players.resize(2);
  s.entities.resize(4);
  s.entities[3].present = true;
  s.entities[3].model = 2;
  s.entities[3].origin = kAt + sh::Vec3f{0, 300, 0};
  cl::own_missiles_handover(m, s);
  CHECK(m.handed == 0 && s.entities[3].present);
}

TEST(a_stand_in_that_hits_a_wall_first_keeps_its_copy_hidden_till_it_is_gone) {
  const auto map = halls();
  if (!map) return;
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.kind = cl::OwnMissileKind::rocket;
  shot.origin = kAt;
  CHECK(cl::own_missile_fire(m, shot, 0, 10, 10000, kAt));
  cl::own_missiles_step(m, nullptr, 800, 0.02);
  cl::DemoState s;
  s.format = cl::DemoFormat::client;
  s.models = {"", "progs/player.mdl", "progs/missile.mdl"};
  s.players.resize(2);
  s.entities.resize(4);
  s.entities[3].present = true;
  s.entities[3].model = 2;
  s.entities[3].origin = m.flying[0].origin;
  cl::own_missiles_handover(m, s);
  CHECK(m.flying[0].adopted == 3);
  cl::own_missiles_step(m, &map->bsp.world.models[0].hulls[0], 800, 8);  // into the far wall
  CHECK(m.flying.empty() && m.walled == 1 && m.hidden.size() == 1);
  s.entities[3].present = true;
  cl::own_missiles_handover(m, s);
  CHECK(!s.entities[3].present);
  cl::own_missiles_handover(m, s);  // gone now
  CHECK(m.hidden.empty());
}

TEST(at_most_96_fly_and_a_new_one_replaces_the_farthest) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  for (int i = 0; i < cl::kOwnMissilesMost; i++) {
    shot.origin = {kAt.x + static_cast<float>(i), kAt.y, kAt.z};
    CHECK(cl::own_missile_fire(m, shot, 0, 0.1, 100, kAt));
  }
  shot.origin = kAt;
  CHECK(cl::own_missile_fire(m, shot, 0, 0.1, 100, kAt));
  CHECK(static_cast<int>(m.flying.size()) == cl::kOwnMissilesMost);
  float farthest = 0;
  for (const cl::OwnMissile& o : m.flying) farthest = std::max(farthest, o.origin.x - kAt.x);
  CHECK(farthest < cl::kOwnMissilesMost - 1 + 8 - 0.5f);  // the one at +95 went
}

TEST(the_packs_shots_leave_as_their_rules_launch_them) {
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.origin = kAt;
  shot.angles = {0, 0, 0};  // due east (+x)
  // Scourge of Armagon's twin laser bolts: 12 ahead, 1.8 up, 4.2 to each side, at 1000.
  shot.kind = cl::OwnMissileKind::laser;
  shot.side = 1;
  CHECK(cl::own_missile_fire(m, shot, 10, 0.1, 100, kAt));
  const cl::OwnMissile& laser = m.flying.back();
  CHECK(near(laser.origin.x, kAt.x + 12) && near(laser.origin.y, kAt.y - 4.2f) &&
        near(laser.origin.z, kAt.z + 1.8f));
  CHECK(near(laser.velocity.x, 1000));
  // Dissolution of Eternity's multi rockets: in a deathmatch turned 3.3 and 6.6 degrees each way.
  shot.kind = cl::OwnMissileKind::multi_rocket;
  shot.spread = 0;
  CHECK(cl::own_missile_fire(m, shot, 10, 0.1, 100, kAt));
  const cl::OwnMissile& rocket = m.flying.back();
  CHECK(near(rocket.origin.x, kAt.x + 8) && near(rocket.origin.z, kAt.z + 16));
  CHECK(rocket.velocity.y < -100 && rocket.velocity.y > -130);  // 1000 sin 6.6 to the right
  // In single play with 80 units/s to the right instead.
  shot.deathmatch = false;
  CHECK(cl::own_missile_fire(m, shot, 10, 0.1, 100, kAt));
  CHECK(near(m.flying.back().velocity.x, 1000) && near(m.flying.back().velocity.y, -80));
  // The plasma ball waits 0.1 s 24 ahead and 16 up, then flies at 1250.
  shot.kind = cl::OwnMissileKind::plasma;
  CHECK(cl::own_missile_fire(m, shot, 10, 0.1, 100, kAt));
  cl::own_missiles_step(m, nullptr, 800, 10.05);
  CHECK(near(m.flying.back().origin.x, kAt.x + 24));
  cl::own_missiles_step(m, nullptr, 800, 10.1 + 0.02);
  CHECK(near(m.flying.back().origin.x, kAt.x + 24 + 25, 0.1f));
}

TEST(a_proximity_grenade_sticks_where_it_lands) {
  std::unique_ptr<cl::LoadedMap> map = halls();
  const sh::Hull* world = &map->bsp.world.models[0].hulls[0];
  cl::OwnMissiles m;
  cl::OwnShot shot;
  shot.kind = cl::OwnMissileKind::prox;
  shot.origin = kAt;
  shot.angles = {0, 0, 0};
  CHECK(cl::own_missile_fire(m, shot, 10, 3, 100, kAt));
  cl::own_missiles_step(m, world, 800, 11);
  CHECK(m.flying.size() == 1 && m.flying[0].stuck);
  const sh::Vec3f at = m.flying[0].origin;
  cl::own_missiles_step(m, world, 800, 11.3);
  CHECK(m.flying.size() == 1 && sh::length(m.flying[0].origin - at) < 0.01f);
}
