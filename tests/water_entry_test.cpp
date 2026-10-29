// engine/water/entry.hpp and the crater in wake.hpp: a body through the surface opens a crater
// that grows with its energy as its fourth root and deepens with its density; the rings it sets
// off grow with it; coming out opens no crater; the crater and its jet move no water and end in
// the time stated; `wake_at` and `ring_height` carry the crater, so a float drops into it; a
// ring without one keeps the far field's bits; and a body running in leans everything forward
// (the waves ahead higher than behind, the crater drawn out ahead, still moving no water, the
// slope the height's).
//
// ge:summary Test: a body through the surface: crater by energy and density, rings, jet, sums
// ge:keywords test water entry crater cavity jet energy density ring splash entry_of oblique lean
// ge:skill water testing
#include <cmath>

#include "check.hpp"
#include "water/entry.hpp"
#include "water/wake.hpp"

namespace {

namespace gw = ge::water;
constexpr double kPi = 3.14159265358979323846;

gw::Entry drop(double mass, double volume, double speed, double footprint = 0.15) {
  return gw::entry_of({.mass = mass, .volume = volume, .footprint = footprint, .vy = -speed}, 0, 0);
}

}  // namespace

TEST(the_crater_grows_with_the_energy_as_its_fourth_root) {
  const gw::Entry a = drop(90, 0.088, 5), b = drop(90 * 16, 0.088 * 16, 5);
  CHECK(std::fabs(b.energy / a.energy - 16) < 1e-9);
  // Sixteen times the energy, twice the radius (both wider than the footprint).
  CHECK(std::fabs(b.radius / a.radius - 2) < 1e-9);
  // Twice the speed, four times the energy, sqrt 2 the radius.
  const gw::Entry c = drop(90, 0.088, 10);
  CHECK(std::fabs(c.radius / a.radius - std::sqrt(2.0)) < 1e-9);
  // The player off a jump: a crater about a metre across, a third of a metre deep.
  const gw::Entry p = drop(90, 0.088, 5.4);
  CHECK(p.radius > 0.4 && p.radius < 0.6);
  CHECK(p.ring.depth > 0.25 && p.ring.depth < 0.35);
  CHECK(p.jet_at > 0.35 && p.jet_at < 0.55);
}

TEST(a_denser_body_digs_deeper_and_a_heavier_one_makes_higher_waves) {
  const gw::Entry light = drop(20, 0.05, 6), dense = drop(20, 0.008, 6);
  CHECK(light.radius == dense.radius);  // the same energy
  CHECK(dense.ring.depth > light.ring.depth);
  gw::Wakes w;
  double small = 0, big = 0;
  const gw::Entry pebble = drop(0.05, 2e-5, 6, 0.0008), boulder = drop(400, 0.15, 6, 0.28);
  for (int i = 5; i < 400; ++i) {
    const double tau = i * 0.01;
    small = std::max(small, gw::ring_height(w, pebble.ring, 2, 0, tau));
    big = std::max(big, gw::ring_height(w, boulder.ring, 2, 0, tau));
  }
  CHECK(big > 20 * small);
  CHECK(boulder.drops > pebble.drops);
  CHECK(boulder.speed == pebble.speed);  // the crown's speed is the entry's
}

TEST(coming_out_opens_no_crater) {
  const gw::Entry e = gw::entry_of({.mass = 5, .volume = 0.006, .vy = 2, .leaving = true}, 0, 0);
  CHECK(e.ring.crater == 0);
  CHECK(e.jet_at == 0);
  CHECK(e.ring.volume > 0);
  CHECK(e.drops > 0);
  CHECK(gw::crater_time(e.ring) == 0);
}

TEST(the_crater_and_its_jet_move_no_water_and_end_on_time) {
  const gw::Entry e = drop(90, 0.088, 5.4);
  const gw::Ring& r = e.ring;
  // The cavity's shape (1 - rho^2) exp(-rho^2) integrates to nothing over the plane.
  const double tau = 0.5 * e.jet_at;
  double volume = 0, dug = 0;
  for (int i = 0; i < static_cast<int>(8 * r.crater / 0.001); ++i) {
    const double x = 0.0005 + i * 0.001;
    const double h = gw::crater_height(r, x, 0, tau);
    volume += 2 * kPi * x * h * 0.001;
    dug += 2 * kPi * x * std::min(h, 0.0) * 0.001;
  }
  CHECK(std::fabs(volume) < 1e-3 * std::fabs(dug));
  CHECK(gw::crater_height(r, 0, 0, tau) < -0.9 * r.depth);
  // The jet stands up the middle after the crater closes, and everything is still after.
  CHECK(gw::crater_height(r, 0, 0, e.jet_at + 0.05) > 0);
  CHECK(gw::crater_height(r, 0, 0, gw::crater_time(r) + 0.01) == 0);
  CHECK(gw::crater_height(r, 0, 0, -0.1) == 0);
}

TEST(wake_at_and_ring_height_carry_the_crater) {
  gw::Wakes w;
  const gw::Entry e = drop(90, 0.088, 5.4);
  gw::emit_ring(w, e.ring);
  const double tau = 0.5 * e.jet_at;
  const gw::WakeAt at = gw::wake_at(w, 0, 0.2, 0, tau);
  CHECK(at.height < -0.1);
  CHECK(at.dx > 0);  // the cavity's wall rises outward
  CHECK(std::fabs(gw::ring_height(w, e.ring, 0.2, 0, tau) - at.height) < 1e-12);
  // A ring with no crater is the far field alone, bit for bit as before.
  gw::Ring plain = e.ring;
  plain.crater = 0;
  plain.depth = 0;
  gw::Wakes far;
  gw::emit_ring(far, plain);
  CHECK(gw::wake_at(far, 0, 3, 0, 2.0).height == gw::ring_height(far, plain, 3, 0, 2.0));
}

TEST(running_in_pushes_the_water_ahead) {
  // The player at 6 m/s down, once straight and once running at 8 m/s along +x.
  const gw::Entry straight = drop(90, 0.088, 6);
  const gw::Entry run = gw::entry_of({.mass = 90, .volume = 0.088, .vx = 8, .vy = -6}, 0, 0);
  CHECK(straight.ring.lean_x == 0 && straight.ring.reach == 0);
  CHECK(run.radius == straight.radius);  // the crater's size from the speed down
  CHECK(std::fabs(run.ring.lean_x - gw::kLeanMost * 0.8) < 1e-12);
  CHECK(run.ring.lean_z == 0);
  CHECK(run.ring.reach > 0.5 * run.radius && run.ring.reach <= gw::kReachMost * run.radius);
  CHECK(run.ring.volume > straight.ring.volume);
  CHECK(run.drops > straight.drops);
  CHECK(std::fabs(run.ahead - 0.6 * 8) < 1e-12);
  // The waves 3 m ahead higher than straight in, the ones behind lower.
  gw::Wakes w;
  double ahead = 0, behind = 0, round = 0;
  for (int i = 5; i < 400; ++i) {
    const double tau = i * 0.01;
    ahead = std::max(ahead, gw::ring_height(w, run.ring, 3, 0, tau));
    behind = std::max(behind, gw::ring_height(w, run.ring, -3, 0, tau));
    round = std::max(round, gw::ring_height(w, straight.ring, 3, 0, tau));
  }
  CHECK(ahead > 1.3 * behind);
  CHECK(ahead > round);
  // The crater drawn out ahead: still deep a radius on, where straight in it is gone.
  const double tau = 0.5 * run.jet_at;
  const double r = run.radius;
  CHECK(gw::crater_height(run.ring, r, 0, tau) < 0.5 * gw::crater_height(run.ring, 0, 0, tau));
  CHECK(gw::crater_height(straight.ring, r, 0, tau) >= 0);
  CHECK(gw::crater_height(run.ring, r, 0, tau) < 0);
  // It moves no water: the cavity over the plane sums to nothing.
  double volume = 0, dug = 0;
  const double cell = 0.01;
  for (int i = -800; i < 800; ++i) {
    for (int j = -500; j < 500; ++j) {
      const double h = gw::crater_height(run.ring, (i + 0.5) * cell, (j + 0.5) * cell, tau);
      volume += h * cell * cell;
      dug += std::min(h, 0.0) * cell * cell;
    }
  }
  CHECK(std::fabs(volume) < 1e-3 * std::fabs(dug));
  // The slope `wake_at` hands out over the crater is the height's (the far field's is the
  // phase's alone, as for any ring, which is near enough a few waves out).
  gw::emit_ring(w, run.ring);
  for (const double x : {-0.6, 0.0, 0.4, 1.3}) {
    const double z = 0.3, s = 1e-5;
    const gw::WakeAt at = gw::wake_at(w, 0, x, z, tau);
    const double dx =
        (gw::wake_at(w, 0, x + s, z, tau).height - gw::wake_at(w, 0, x - s, z, tau).height) /
        (2 * s);
    const double dz =
        (gw::wake_at(w, 0, x, z + s, tau).height - gw::wake_at(w, 0, x, z - s, tau).height) /
        (2 * s);
    CHECK(std::fabs(at.dx - dx) < 1e-4 + 1e-3 * std::fabs(dx));
    CHECK(std::fabs(at.dz - dz) < 1e-4 + 1e-3 * std::fabs(dz));
  }
}
