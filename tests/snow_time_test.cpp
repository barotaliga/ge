// Snow over time (engine/snow/time.hpp): the schedules' integrals are exact on their pieces; a
// layer with no weather reads the static layer bit for bit; new snow settles over days with its
// mass kept; a ski track laid in a 5 cm/h storm fills in about two hours; a groomer's swath keeps
// the mass to the unit and lays corduroy that the next press wipes; a night's crust bears a
// standing boot and breaks under a carving skier; and the mass the tiles hold plus what spilled is
// what they were given, the weather's included.
//
// ge:summary Tests: snow over time (fall, settling, filling, grooming, crust, the mass ledger)
// ge:keywords test snow time weather schedule settling filling groom corduroy crust mass
// ge:skill snow testing
// ge:test tests/snow_time_test.cpp
#include <cmath>
#include <cstdint>

#include "check.hpp"
#include "snow/layer.hpp"
#include "snow/press.hpp"
#include "snow/reference.hpp"
#include "snow/time.hpp"

namespace sn = ge::snow;

namespace {

constexpr double kHour = 3600;

sn::LayerSpec flat(double depth, const sn::Weather* w, sn::Kind kind = sn::Kind::powder) {
  sn::LayerSpec s;
  s.region = {-10, -10, 10, 10};
  s.cover.depth = depth;
  s.cover.kind = kind;
  s.weather = w;
  return s;
}

// A ski's groove along +z through the origin, carved at `t`.
void carve(sn::Layer& layer, double t) {
  const sn::ReferenceContact rc = sn::reference_contact(0);
  for (int i = 0; i < 24; i++) sn::press(layer, sn::place(rc.imprint, 0, -3 + 0.25 * i, 0), t);
}

bool ledger(const sn::Layer& l) { return sn::mass_units(l) + l.spilled == l.mass_in; }

}  // namespace

TEST(schedules_integrate_exactly_on_their_pieces) {
  sn::Schedule s{{0, 10, 20}, {0, 10, 0}};
  CHECK(sn::value_at(s, 5) == 5);
  CHECK(sn::value_at(s, -3) == 0 && sn::value_at(s, 40) == 0);
  CHECK(std::abs(sn::integral(s, 0, 20) - 100) < 1e-9);
  CHECK(std::abs(sn::integral(s, 5, 15) - 75) < 1e-9);
  CHECK(std::abs(sn::integral_above(s, 5, 0, 20) - 25) < 1e-9);
  CHECK(std::abs(sn::integral_below(s, 5, 0, 20) - 25) < 1e-9);
  CHECK(std::abs(sn::last_above(s, 5, 30) - 15) < 1e-9);
  CHECK(sn::last_above(s, 50, 30) < -1e299);
  const sn::Schedule none;
  CHECK(sn::value_at(none, 3) == 0 && sn::integral(none, 0, 9) == 0);
}

TEST(a_layer_with_no_weather_reads_the_static_layer_bit_for_bit) {
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, flat(0.4, nullptr));
  carve(layer, 0);
  for (int i = 0; i <= 54; i++) {
    const double x = -1 + 0.037 * i;
    const sn::Column a = sn::sample(layer, x, 0.3);
    const sn::Column b = sn::sample(layer, x, 0.3, 5 * kHour);
    CHECK(a.depth == b.depth && a.density == b.density && a.packed == b.packed);
    CHECK(a.kind == b.kind && a.drop == b.drop && a.touched == b.touched);
  }
}

TEST(new_snow_settles_over_days_and_keeps_its_mass) {
  sn::Weather w;
  w.fall = {{0, 6 * kHour}, {10.0 / 3600, 10.0 / 3600}};  // 10 mm/h of water for six hours
  w.fall.t.push_back(6 * kHour + 1);
  w.fall.v.push_back(0);
  w.air = {{0}, {-5}};
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, flat(0.2, &w));
  const sn::Column base = sn::sample(layer, 0, 0, 0);
  const sn::Column fresh = sn::sample(layer, 0, 0, 6 * kHour);
  const sn::Column later = sn::sample(layer, 0, 0, 6 * kHour + 3 * 86400);
  const double mass_fresh = fresh.depth * fresh.density, mass_later = later.depth * later.density;
  CHECK(std::abs(mass_fresh - (base.depth * base.density + 60)) < 0.01);
  CHECK(std::abs(mass_later - mass_fresh) < 0.01);  // the fall's last second ramps to 0
  CHECK(fresh.depth > base.depth + 0.4);            // 60 kg/m2 at about 100 kg/m3
  CHECK(later.depth < fresh.depth - 0.15);
  CHECK(later.density > fresh.density * 1.4);
  CHECK(fresh.kind == sn::Kind::powder);
}

TEST(a_ski_track_fills_in_a_storm_in_about_two_hours) {
  sn::Weather w;
  w.fall = {{0}, {5.0 / 3600}};  // 5 cm/h of snow at 100 kg/m3
  w.air = {{0}, {-8}};
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, flat(0.4, &w));
  carve(layer, 0);
  const double groove = sn::sample(layer, 0, 0, 0).drop;
  CHECK(groove > 0.15);
  CHECK(sn::sample(layer, 0, 0, 1 * kHour).drop > 0.03);
  CHECK(sn::sample(layer, 0, 0, 3 * kHour).drop < 0.002);
  // Filled to the cover and no higher; the berm beside it is buried with the rest.
  CHECK(sn::sample(layer, 0, 0, 6 * kHour).drop > -0.002);
}

TEST(a_groomer_keeps_the_mass_and_lays_corduroy_the_next_press_wipes) {
  sn::Weather w;
  w.air = {{0}, {-6}};
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, flat(0.4, &w));
  carve(layer, 0);
  const uint64_t before = sn::mass_units(layer);
  const sn::Groomed g = sn::groom(layer, sn::capsule(0, -4, 0, 4, 2.0), 600);
  CHECK(g.samples > 0 && g.refused == 0);
  CHECK(sn::mass_units(layer) == before);
  CHECK(ledger(layer));
  const sn::Column flat_now = sn::sample(layer, 0, 0, 600);
  CHECK(flat_now.kind == sn::Kind::groomed);
  CHECK(std::abs(sn::sample(layer, 0.5, 1, 600).depth - flat_now.depth) < 0.002);
  const sn::Cell* cell = sn::cell_at(layer, 100, 100);
  CHECK(cell && (cell->flags & sn::kCorduroy));
  sn::Imprint step = sn::reference_contact(2).imprint;
  step.sink =
      flat_now.drop + 0.02;  // a footprint 2 cm into the corduroy (sinks count from the cover)
  sn::press(layer, step, 900);
  CHECK(!(sn::cell_at(layer, 100, 100)->flags & sn::kCorduroy));
  CHECK(ledger(layer));
}

TEST(a_nights_crust_bears_a_boot_and_breaks_under_a_carving_skier) {
  sn::Weather w;
  w.air = {{0, 12 * kHour, 13 * kHour}, {4, 4, -5}};  // a warm day, then a frost
  const double night = 23 * kHour;
  const double lid = sn::crust_load(w, night);
  CHECK(lid > 785 && lid < 1570);
  CHECK(sn::crust_load(w, 6 * kHour) == 0);  // thawing: no lid
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, flat(1.0, &w));
  CHECK(sn::sample(layer, 0, 0, night).kind == sn::Kind::crust);
  sn::Imprint boot = sn::reference_contact(2).imprint;
  const sn::Pressed stood = sn::press(layer, boot, night);
  CHECK(!stood.broke && stood.samples == 0);
  sn::Imprint skier = boot;  // the same sole under twice the weight: a 2 g turn
  skier.load = 1570;
  const sn::Pressed broke = sn::press(layer, sn::place(skier, 3, 0, 0), night);
  CHECK(broke.broke && broke.sink > 0.02);
  // A crust with no thaw behind it is the game's own and keeps its row's sink.
  sn::Weather cold;
  cold.air = {{0}, {-5}};
  sn::Layer& own = sn::add_layer(world, flat(0.4, &cold, sn::Kind::crust));
  CHECK(sn::crust_load(cold, night) == INFINITY);
  CHECK(!sn::press(own, boot, night).broke);
  CHECK(ledger(layer) && ledger(own));
}

TEST(the_ledger_holds_with_weather) {
  sn::Weather w;
  w.fall = {{0, 2 * kHour, 4 * kHour}, {4.0 / 3600, 0, 3.0 / 3600}};
  w.air = {{0, 6 * kHour, 9 * kHour}, {-4, 2, -6}};
  w.wind = {{0}, {9}};
  sn::SnowWorld world;
  sn::Layer& layer = sn::add_layer(world, flat(0.35, &w));
  for (int k = 0; k < 6; k++) {
    for (int i = 0; i < 6; i++) {
      const sn::ReferenceContact rc = sn::reference_contact(i);
      sn::press(layer, sn::place(rc.imprint, -6 + 2.0 * i, -2 + 0.7 * k, 0.3 * k), k * 1.7 * kHour);
    }
    if (k == 3) sn::groom(layer, sn::capsule(-8, 0, 8, 0, 1.5), k * 1.7 * kHour + 60);
    CHECK_MSG(ledger(layer), "the tiles plus the spill are what they were given");
  }
}
