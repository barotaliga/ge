// engine/water/entry.hpp and the crater in wake.hpp: a body through the surface opens a crater
// that grows with its energy as its fourth root and deepens with its density; the rings it sets
// off grow with it; coming out opens no crater; the crater and its jet move no water and end in
// the time stated; `wake_at` and `ring_height` carry the crater, so a float drops into it; and a
// ring without one keeps the far field's bits.
//
// ge:summary Test: a body through the surface: crater by energy and density, rings, jet, sums
// ge:keywords test water entry crater cavity jet energy density ring splash entry_of
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
    small = std::max(small, gw::ring_height(w, pebble.ring, 2, tau));
    big = std::max(big, gw::ring_height(w, boulder.ring, 2, tau));
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
    const double h = gw::crater_height(r, x, tau);
    volume += 2 * kPi * x * h * 0.001;
    dug += 2 * kPi * x * std::min(h, 0.0) * 0.001;
  }
  CHECK(std::fabs(volume) < 1e-3 * std::fabs(dug));
  CHECK(gw::crater_height(r, 0, tau) < -0.9 * r.depth);
  // The jet stands up the middle after the crater closes, and everything is still after.
  CHECK(gw::crater_height(r, 0, e.jet_at + 0.05) > 0);
  CHECK(gw::crater_height(r, 0, gw::crater_time(r) + 0.01) == 0);
  CHECK(gw::crater_height(r, 0, -0.1) == 0);
}

TEST(wake_at_and_ring_height_carry_the_crater) {
  gw::Wakes w;
  const gw::Entry e = drop(90, 0.088, 5.4);
  gw::emit_ring(w, e.ring);
  const double tau = 0.5 * e.jet_at;
  const gw::WakeAt at = gw::wake_at(w, 0, 0.2, 0, tau);
  CHECK(at.height < -0.1);
  CHECK(at.dx > 0);  // the cavity's wall rises outward
  CHECK(std::fabs(gw::ring_height(w, e.ring, 0.2, tau) - at.height) < 1e-12);
  // A ring with no crater is the far field alone, bit for bit as before.
  gw::Ring plain = e.ring;
  plain.crater = 0;
  plain.depth = 0;
  gw::Wakes far;
  gw::emit_ring(far, plain);
  CHECK(gw::wake_at(far, 0, 3, 0, 2.0).height == gw::ring_height(far, plain, 3, 2.0));
}
