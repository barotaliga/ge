// WHERE A SHOT MEETS A MOVING PLAYER (plugins/shooter/intercept.hpp): the intercept's flight is
// the shot's own time to the place it names, and the target is there then; a target running at
// the shooter is met sooner than the plain distance says; one in the air comes down on his arc to
// his floor; and a grenade thrown at the solved pitch, stepped through its flight, passes through
// the point.
//
// ge:summary Tests: the intercept by flight time, a target's arc, the grenade's lob pitch
// ge:keywords test intercept lob_pitch mover_at grenade rocket midair
// ge:skill shooter testing
#include <cmath>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "shooter/intercept.hpp"

namespace sh = ge::shooter;

namespace {

float dist(sh::Vec3f a, sh::Vec3f b) {
  const sh::Vec3f d = a - b;
  return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

}  // namespace

TEST(a_rocket_is_fired_where_its_target_will_be) {
  sh::Mover m;
  m.at = {1000, 0, 0};
  m.velocity = {0, 320, 0};  // straight across
  sh::Shot rocket;
  const sh::Intercept ic = sh::intercept({0, 0, 0}, m, 0, rocket, 3);
  // The flight is the rocket's own time to the point, and he is there then.
  CHECK(std::fabs(dist({0, 0, 0}, ic.at) / rocket.speed - ic.flight_s) < 0.002);
  CHECK(dist(sh::mover_at(m, ic.flight_s), ic.at) < 1);
  CHECK(ic.at.y > 320);  // led past where a whole second of his run takes him

  // Running at the shooter: met sooner than the distance he stands at says.
  m.velocity = {-320, 0, 0};
  const sh::Intercept at_me = sh::intercept({0, 0, 0}, m, 0, rocket, 3);
  CHECK(at_me.flight_s < 1.0 - 0.2);
  const sh::Intercept plain = sh::intercept({0, 0, 0}, m, 0, rocket, 1);
  CHECK(at_me.flight_s < plain.flight_s);
}

TEST(a_player_in_the_air_comes_down_on_his_arc) {
  sh::Mover m;
  m.at = {0, 0, 200};
  m.velocity = {100, 0, 270};  // a jump's rise
  m.airborne = true;
  m.floor_z = 24;
  const sh::Vec3f top = sh::mover_at(m, 270.0 / sh::kServerGravity);
  CHECK(top.z > 240 && top.z < 250);  // 200 + 270^2 / 1600
  const sh::Vec3f late = sh::mover_at(m, 3);
  CHECK(late.z == 24);                     // landed
  CHECK(std::fabs(late.x - 300) < 0.01f);  // and on across
}

TEST(a_grenade_at_the_solved_pitch_passes_through_the_point) {
  const sh::Shot g{600, true, 200, sh::kServerGravity};
  const sh::Vec3f from{0, 0, 0};
  for (const sh::Vec3f to :
       {sh::Vec3f{300, 0, 0}, sh::Vec3f{200, 200, -100}, sh::Vec3f{350, 0, 80}}) {
    float pitch = 0;
    double flight = 0;
    CHECK(sh::lob_pitch(from, to, g, pitch, flight));
    // Thrown: v = 600 along the view, 200 along its up; stepped through its flight.
    const double up = -pitch * ge::core::kPi / 180;
    const double across =
        std::sqrt(static_cast<double>(to.x) * to.x + static_cast<double>(to.y) * to.y);
    const double vx = 600 * ge::core::libm::cos(up) - 200 * ge::core::libm::sin(up);
    const double vz = 600 * ge::core::libm::sin(up) + 200 * ge::core::libm::cos(up);
    double x = 0, z = 0, t = 0, vzz = vz;
    const double dt = 1.0 / 1000;
    while (x < across && t < 3) {
      x += vx * dt;
      z += vzz * dt;
      vzz -= sh::kServerGravity * dt;
      t += dt;
    }
    CHECK(std::fabs(z - to.z) < 3);
    CHECK(std::fabs(t - flight) < 0.01);
  }
  // Past the longest throw: false, the longest throw's pitch.
  float pitch = 0;
  double flight = 0;
  CHECK(!sh::lob_pitch(from, {3000, 0, 0}, g, pitch, flight));
}
