// engine/wind/cold.hpp: a clear night's cold surface gathers dew above freezing, hoar frost
// below it and rime in a windy freezing fog, and the frost goes as the surface warms; rain on a
// frozen surface glazes it; the breath shows in the cold and not in a warm, dry air; lake ice
// grows by the root of the frost's degree-days and bears a walker at 10 cm; new snow falls lighter
// in colder air and is continuous through 0 C; a ski glides best a few degrees under freezing; the
// feel names powder, squeak, sticky, slush and crust where they belong; and every number keeps its
// bits in tests/golden/wind_cold.txt.
//
// A deliberate change moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/release/tests/wind_cold_test`, and put the `climate` lab's charts
// before and after in the PR.
//
// ge:summary Test: dew, frost, rime, glaze, breath, lake ice, the snow's feel and glide; golden
// ge:keywords test frost dew rime glaze breath ice stefan snow-feel glide fresh-density golden
// ge:skill water testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "wind/cold.hpp"

namespace {

namespace gw = ge::wind;

constexpr const char* kGolden = "tests/golden/wind_cold.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

gw::Thermal air(double t, double rh) {
  gw::Thermal a;
  a.temp_c = t;
  a.rh = rh;
  a.dew_c = gw::dew_point_of(t, rh);
  a.frost_c = t;  // tests that read it set it
  a.wet_bulb_c = gw::wet_bulb_of(t, rh);
  return a;
}

}  // namespace

TEST(a_cold_surface_gathers_dew_frost_or_rime) {
  const gw::Mist none;
  gw::Thermal mild = air(6, 0.9);
  CHECK(gw::deposit_on(mild, 2, none, 1).kind == gw::Deposit::dew);
  CHECK(gw::deposit_on(mild, 8, none, 1).kind == gw::Deposit::none);
  // A clear night's air at the site: its frost point from air_at.
  gw::Climate c;
  c.mean_c = -2;
  c.humidity = 0.85;
  c.cloud = 0;
  c.wind = 1;
  const gw::Site open;
  const gw::Thermal night = gw::air_at(c, open, 5);
  const double surface = gw::surface_at(c, open, 5);
  CHECK(surface < night.temp_c - 1);
  const gw::Settling frost = gw::deposit_on(night, surface, none, c.wind);
  CHECK(frost.kind == gw::Deposit::frost);
  CHECK(frost.rate > 0 && frost.rate < 0.3);
  gw::Mist fog;
  fog.strength = 0.8;
  fog.freezing = true;
  CHECK(gw::deposit_on(night, surface, fog, 6).kind == gw::Deposit::rime);
  CHECK(gw::deposit_on(night, surface, fog, 0.5).kind == gw::Deposit::frost);
}

TEST(frost_goes_as_the_surface_warms) {
  gw::Thermal a = air(-6, 0.7);
  a.frost_c = -9;
  // Still growing under the frost point: nothing lost.
  CHECK(gw::frost_loss(a, -11) == 0);
  // Over it: sublimed at the deposit's own rate.
  CHECK(std::fabs(gw::frost_loss(a, -7) - 0.06) < 1e-12);
  // Over 0 C: melted, far faster.
  CHECK(gw::frost_loss(a, 2) > 0.6 + gw::frost_loss(a, -1));
}

TEST(rain_on_a_frozen_surface_glazes_it) {
  CHECK(gw::glaze_on(-3, 1) == 1);
  CHECK(gw::glaze_on(1, 1) == 0);
  CHECK(gw::glaze_on(-3, 0) == 0);
  CHECK(gw::glaze_on(-1, 1) > 0 && gw::glaze_on(-1, 1) < 1);
}

TEST(the_breath_shows_in_the_cold_and_not_in_warm_dry_air) {
  CHECK(std::fabs(gw::breath_of(air(-10, 0.8)) - 1) < 1e-12);
  CHECK(gw::breath_of(air(-25, 0.8)) > gw::breath_of(air(-10, 0.8)));
  CHECK(gw::breath_of(air(0, 0.8)) > 0.4);
  CHECK(gw::breath_of(air(25, 0.3)) == 0);
  CHECK(gw::breath_of(air(6, 0.95)) > gw::breath_of(air(6, 0.4)));
  CHECK(gw::breath_of(air(12, 0.95)) == 0);
}

TEST(lake_ice_grows_by_the_root_of_the_frost_and_bears_a_walker_at_ten_cm) {
  const double month = gw::ice_grown(0, -5, 30), two = gw::ice_grown(0, -5, 120);
  CHECK(std::fabs(two / month - 2) < 1e-12);
  CHECK(std::fabs(gw::ice_grown(month, -5, 90) - two) < 1e-12);
  CHECK(gw::ice_grown(0.2, 4, 5) < 0.2);
  CHECK(gw::ice_grown(0.01, 10, 30) == 0);
  CHECK(gw::ice_bears(0.10) > 150 && gw::ice_bears(0.10) < 200);
  CHECK(gw::ice_bears(0.25) > 1000);
}

TEST(new_snow_falls_lighter_in_colder_air_and_is_continuous_through_zero) {
  CHECK(gw::fresh_density(-20) < 70);
  CHECK(gw::fresh_density(-20) < gw::fresh_density(-5));
  CHECK(gw::fresh_density(-5) < gw::fresh_density(0));
  CHECK(std::fabs(gw::fresh_density(-1e-9) - gw::fresh_density(1e-9)) < 1e-3);
  CHECK(gw::fresh_density(30) == 200);
}

TEST(a_ski_glides_best_a_few_degrees_under_freezing) {
  const double best = gw::glide_of(-3, 0);
  CHECK(gw::glide_of(-20, 0) > 2 * best);
  CHECK(gw::glide_of(0, 0) > best);
  CHECK(gw::glide_of(0, 1) > gw::glide_of(0, 0));
  for (int k = -60; k <= 0; ++k) CHECK(gw::glide_of(k / 2.0, 0) >= best);
}

TEST(the_feel_names_what_the_snow_is) {
  CHECK(gw::snow_feel(-12, -12, 0.2, false) == gw::Feel::powder);
  CHECK(gw::snow_feel(-1, -1, 0.2, false) == gw::Feel::sticky);
  CHECK(gw::snow_feel(-15, -15, 0, false) == gw::Feel::squeaky);
  CHECK(gw::squeaks(-15) && !gw::squeaks(-5));
  CHECK(gw::snow_feel(0, 3, 0, false) == gw::Feel::slush);
  CHECK(gw::snow_feel(-5, -6, 0, true) == gw::Feel::crust);
  CHECK(gw::snow_feel(-5, -6, 0, false) == gw::Feel::soft);
  CHECK(std::string(gw::feel_name(gw::Feel::crust)) == "crust");
  CHECK(std::string(gw::deposit_name(gw::Deposit::rime)) == "rime");
}

TEST(every_number_keeps_its_golden) {
  const bool write = env_set("GE_GOLDEN_WRITE");
  const auto want = ge::test::golden_rows(kGolden);
  if (!write) CHECK_MSG(!want.empty(), std::string(kGolden) + " is empty; see the header");
  std::vector<std::vector<uint64_t>> got;
  for (const double t : {-25.0, -10.0, -3.0, 0.0, 4.0, 12.0}) {
    for (const double rh : {0.4, 0.95}) {
      const gw::Thermal a = air(t, rh);
      got.push_back({ge::test::bits(gw::breath_of(a)), ge::test::bits(gw::fresh_density(t)),
                     ge::test::bits(gw::glide_of(t, rh - 0.4))});
    }
  }
  for (const double h : {0.0, 0.05, 0.3}) {
    for (const double mean : {-8.0, -1.0, 3.0}) {
      const double grown = gw::ice_grown(h, mean, 21);
      got.push_back({ge::test::bits(grown), ge::test::bits(gw::ice_bears(grown))});
    }
  }
  if (write) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# breath, fresh density, glide per air; ice and what it bears per start and mean "
           "(wind_cold_test)\n";
    for (const auto& row : got) {
      std::string line;
      for (const uint64_t word : row) {
        char b[24];
        std::snprintf(b, sizeof b, "%llx", static_cast<unsigned long long>(word));
        line += (line.empty() ? "" : " ") + std::string(b);
      }
      out << line << "\n";
    }
    return;
  }
  CHECK_MSG(got == want, "the cold's marks moved; if the move is meant, rewrite " +
                             std::string(kGolden) +
                             " with GE_GOLDEN_WRITE=1 on the release build and put the climate "
                             "lab's charts before and after in the PR (skill: water)");
}
