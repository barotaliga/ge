// plugins/shooter/aim_help: a line just missing a player's box is bent onto it within the leeway,
// on each axis alone; a line beyond the leeway, behind the eye or already through a box is left;
// the nearest turn wins; a spread's lines turn together; and the loop's half bends a traced shot
// for a granted shooter only when it is his shot (near his eye, along his view); a rocket is led
// to where a runner will be, a grenade thrown on the arc that lands on him.
//
// ge:summary Test: the aim help's bend: leeway per axis, choice, spread, the traced shot
// ge:keywords test aim-help aim_help aim_bend_dir aim_help_line leeway hitscan
// ge:skill shooter testing
#include <cmath>

#include "check.hpp"
#include "shooter/aim_help.hpp"

using namespace ge::shooter;

namespace {

// A player's box (32 wide, 56 tall) standing at `x`, `y` on the floor z = 0.
AimTarget player(int ent, float x, float y) {
  return AimTarget{ent, Vec3f{x - 16, y - 16, 0}, Vec3f{x + 16, y + 16, 56}};
}

const Vec3f kEye{0, 0, 46};
const Vec3f kForward{1, 0, 0};  // along +x, level

}  // namespace

TEST(a_line_above_a_player_is_bent_down_onto_him_within_the_vertical_leeway) {
  const AimTarget t[] = {player(1, 500, 0)};
  // Looking 40 units over his head at his distance (the line at z 96, his top 56).
  Vec3f f{500, 0, 50};
  normalize(f);
  const AimBend b = aim_help(kEye, f, 4096, t, {0, 48});
  CHECK(b.bent && b.target == 1);
  CHECK(b.aim.z == 52);  // 4 inside his top
  CHECK(b.updown < 0 && b.across == 0);
  // A leeway smaller than the miss bends nothing.
  CHECK(!aim_help(kEye, f, 4096, t, {0, 30}).bent);
}

TEST(a_line_beside_a_player_is_bent_across_only_with_a_sideways_leeway) {
  const AimTarget t[] = {player(1, 400, 30)};  // his near edge 14 to the left of the line
  CHECK(!aim_help(kEye, kForward, 4096, t, {0, 48}).bent);  // vertical help alone
  const AimBend b = aim_help(kEye, kForward, 4096, t, {24, 0});
  CHECK(b.bent && b.target == 1);
  CHECK(b.aim.y == 18);  // 4 inside his edge at y 14
  CHECK(b.updown == 0);
  CHECK(b.forward.y > 0);  // turned to the left (+y)
}

TEST(nothing_is_bent_behind_beyond_or_through_a_box) {
  const AimTarget behind[] = {player(1, -300, 10)};
  CHECK(!aim_help(kEye, kForward, 4096, behind, {64, 64}).bent);
  const AimTarget far[] = {player(1, 5000, 20)};
  CHECK(!aim_help(kEye, kForward, 4096, far, {64, 64}).bent);
  const AimTarget hit[] = {player(2, 300, -40), player(1, 300, 10)};
  const AimBend b = aim_help(kEye, kForward, 4096, hit, {64, 64});
  CHECK(!b.bent && b.direct && b.target == 1);
}

TEST(the_smallest_turn_wins) {
  const AimTarget t[] = {player(1, 200, 40), player(2, 800, 40)};  // both 8 off at their depth
  const AimBend b = aim_help(kEye, kForward, 4096, t, {32, 0});
  CHECK(b.bent && b.target == 2);  // the far one needs the smaller angle
}

TEST(a_spreads_lines_turn_together) {
  Vec3f bent{0.9f, 0.1f, 0};
  normalize(bent);
  Vec3f pellet{0.99f, -0.05f, 0.02f};
  normalize(pellet);
  const Vec3f d = aim_bend_dir(pellet, kForward, bent);
  CHECK(d.y > pellet.y);  // moved with the view
  CHECK(aim_bend_dir(kForward, kForward, bent) == bent);
}

TEST(a_rocket_is_led_to_where_a_runner_will_be) {
  // A player 800 ahead running sideways at 320; a rocket at 1000 aimed where he stands now would
  // pass behind him by 0.8 s × 320 = 256: beyond any leeway. Aimed 230 ahead of him it misses by
  // a little, and the lead sends it where he will be.
  AimMover runner{1, Vec3f{784, -16, 0}, Vec3f{816, 16, 56}, Vec3f{0, -320, 0}, false};
  LeadShot shot;
  shot.origin = kEye;
  Vec3f aim{800, -230, 28 - 46};
  normalize(aim);
  shot.velocity = aim * 1000;
  const LeadBend b = aim_lead(shot, aim, {&runner, 1}, {48, 48}, 800);
  CHECK(b.bent && b.target == 1);
  // At the time found the rocket and his box's middle meet, the rocket's speed kept.
  const Vec3f will_be{800, 0 - 320 * b.time, 28};
  const Vec3f rocket = kEye + b.velocity * b.time;
  CHECK(std::abs(rocket.y - will_be.y) < 2 && std::abs(rocket.z - will_be.z) < 2);
  CHECK(std::abs(length(b.velocity) - 1000) < 0.5f);
  // Aimed where he stands: too far behind him to help.
  Vec3f behind{800, 0, 28 - 46};
  normalize(behind);
  shot.velocity = behind * 1000;
  CHECK(!aim_lead(shot, behind, {&runner, 1}, {48, 48}, 800).bent);
}

TEST(a_grenade_is_thrown_on_an_arc_that_lands_on_him) {
  AimMover still{1, Vec3f{484, -16, 0}, Vec3f{516, 16, 56}, Vec3f{}, false};
  LeadShot shot;
  shot.origin = kEye;
  shot.velocity = Vec3f{600, 30, 350};  // a little to his left and over his head
  shot.gravity = 800;
  const LeadBend b = aim_lead(shot, kForward, {&still, 1}, {48, 48}, 800);
  CHECK(b.bent);
  const float t = b.time;
  const Vec3f lands{kEye.x + b.velocity.x * t, kEye.y + b.velocity.y * t,
                    kEye.z + b.velocity.z * t - 0.5f * 800 * t * t};
  CHECK(std::abs(lands.x - 500) < 1 && std::abs(lands.y) < 1 && std::abs(lands.z - 28) < 1);
}
