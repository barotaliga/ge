// games/shooters_edge/client/shells.hpp: each game's guns throw the cases they should (Quake's
// shotguns hulls, a pump one and a break two later; Quake II's machine guns brass at once, the
// chain gun as many as its flash says, its soldiers' shotguns hulls and its tanks' guns heavy
// brass; the guns that throw none, none); a case leaves the gun's port to its right and up; the
// model angles a case is drawn at turn its axes as its flight does; the sounds' names are the
// cases'.
//
// ge:summary Test: the guns' spent shells per game, the throw, the drawn angles, the sounds' names
// ge:keywords test shells spent-case eject ejection angles_of_turn ce_shells
// ge:skill shooter testing
#include <cmath>

#include "check.hpp"
#include "classicenhance/shells.hpp"
#include "core/quat.hpp"
#include "shooters_edge/app/classic_models.hpp"
#include "shooters_edge/client/shells.hpp"
#include "shooters_edge/view/weapons.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace ce = ge::classicenhance;

}  // namespace

TEST(each_games_guns_throw_their_own_cases) {
  const cl::Ejection* pump = cl::held_ejection("progs/v_shot.mdl");
  const cl::Ejection* brk = cl::held_ejection("progs/v_shot2.mdl");
  CHECK(pump != nullptr && brk != nullptr);
  if (pump == nullptr || brk == nullptr) return;
  CHECK(pump->casing == ce::Casing::hull && pump->count == 1 && pump->delay_s > 0);
  CHECK(brk->count == 2 && brk->delay_s > pump->delay_s);
  CHECK(cl::held_ejection("progs/v_nail.mdl") == nullptr);
  CHECK(cl::held_ejection("progs/v_rock.mdl") == nullptr);
  const cl::Ejection* mg = cl::held_ejection("models/weapons/v_machn/tris.md2");
  CHECK(mg != nullptr && mg->casing == ce::Casing::brass && mg->delay_s == 0);
  CHECK(cl::held_ejection("models/weapons/v_blast/tris.md2") == nullptr);
  CHECK(cl::own_set_ejection(int(shooters_edge::view::Weapon::shotgun)) != nullptr);
  CHECK(cl::own_set_ejection(int(shooters_edge::view::Weapon::rocket_launcher)) == nullptr);
  // Quake: another player's shotgun's firing frames; a soldier's gun, never an enforcer's laser.
  CHECK(cl::q1_player_ejection("shotatt1") != nullptr);
  CHECK(cl::q1_player_ejection("nailatt1") == nullptr);
  CHECK(cl::q1_monster_ejection("progs/soldier.mdl") != nullptr);
  CHECK(cl::q1_monster_ejection("progs/enforcer.mdl") == nullptr);
  // Quake II's players: the machine gun, the shotguns, the chain gun's one to three rounds.
  CHECK(cl::q2_flash_ejection(1, false)->casing == ce::Casing::brass);
  CHECK(cl::q2_flash_ejection(1 | 128, false)->casing == ce::Casing::brass);  // silenced
  CHECK(cl::q2_flash_ejection(2, false)->casing == ce::Casing::hull);
  CHECK(cl::q2_flash_ejection(13, false)->count == 2);
  for (int v = 3; v <= 5; ++v) CHECK(cl::q2_flash_ejection(v, false)->count == v - 2);
  CHECK(cl::q2_flash_ejection(0, false) == nullptr);  // the blaster
  CHECK(cl::q2_flash_ejection(7, false) == nullptr);  // the rocket launcher
  // Quake II's monsters: a soldier's shotgun, his machine gun, his blaster; a tank's chain gun.
  CHECK(cl::q2_flash_ejection(41, true)->casing == ce::Casing::hull);
  CHECK(cl::q2_flash_ejection(43, true)->casing == ce::Casing::brass);
  CHECK(cl::q2_flash_ejection(39, true) == nullptr);
  CHECK(cl::q2_flash_ejection(4, true)->casing == ce::Casing::heavy);
  CHECK(cl::q2_flash_ejection(23, true) == nullptr);  // the tank's rocket
}

TEST(a_case_leaves_the_port_to_the_guns_right_and_up) {
  cl::Cases c;
  const cl::Ejection& e = *cl::held_ejection("models/weapons/v_machn/tris.md2");
  // A gun pointing along +z (heading 0), its top up: its right is +x.
  const double at[3] = {1, 1.5, 2}, fwd[3] = {0, 0, 1}, up[3] = {0, 1, 0}, still[3] = {0, 0, 0};
  double port[3];
  cl::case_port(e, at, fwd, up, port);
  CHECK(port[0] > at[0] && port[1] < at[1] && port[2] < at[2]);
  cl::throw_case(c, e, port, fwd, up, still, 0);
  CHECK(ge::detail::debris_count(c.pool) == 1);
  const ge::detail::Debris& d = c.pool.pieces[0];
  CHECK(d.at.x == port[0] && d.vel.x > 0 && d.vel.y > 0 && d.vel.z < 0);
  CHECK(d.kind == uint16_t(ce::Casing::brass));
  // Its long axis along the gun.
  const ge::core::Vec3 x = ge::core::rotate(d.turn, {1, 0, 0});
  CHECK(std::fabs(x.z - 1) < 1e-12);
  // Owed: two hulls of a break at their moments.
  const cl::Ejection& b = *cl::held_ejection("progs/v_shot2.mdl");
  cl::owe_cases(c, b, port, fwd, up, true, 10);
  CHECK(c.due.size() == 2 && c.due[0].held && std::fabs(c.due[0].due - (10 + b.delay_s)) < 1e-6 &&
        c.due[1].due > c.due[0].due);
}

TEST(a_cases_drawn_angles_turn_it_as_its_flight_does) {
  const ge::core::Quat turns[] = {
      ge::core::identity(),
      ge::core::from_axis_angle(0, 1, 0, 0.7),
      ge::core::from_axis_angle(0, 0, 1, -0.4),
      ge::core::from_axis_angle(1, 0, 0, 1.1),
      ge::core::from_euler(2.1, 0.3, -0.8),
      ge::core::from_euler(-0.9, -1.2, 2.5),
  };
  for (const ge::core::Quat& q : turns) {
    const ge::shooter::Vec3f a = cl::angles_of_turn(q);
    const float ang[3] = {a.x, a.y, a.z};
    float r[4];
    shooters_edge::models::rotation_of(ang, true, r);
    const double dot = r[0] * q.x + r[1] * q.y + r[2] * q.z + r[3] * q.w;
    CHECK_MSG(
        std::fabs(std::fabs(dot) - 1) < 1e-5,
        "angles " + std::to_string(a.x) + " " + std::to_string(a.y) + " " + std::to_string(a.z));
  }
}

TEST(the_cases_sounds_are_named_as_classic_enhance_makes_them) {
  const char* n = cl::case_sound_name(ce::Casing::heavy, ge::detail::SurfaceClass::wood, 5);
  CHECK(std::string(n) ==
        ce::casing_sound_name(ce::Casing::heavy, ge::detail::SurfaceClass::wood, 5));
  // Kept: the same text each time.
  CHECK(n == cl::case_sound_name(ce::Casing::heavy, ge::detail::SurfaceClass::wood, 1));
}
