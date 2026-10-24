// plugins/mountain/app/cloud_field.hpp: a clear sky and a fog stand no layers and every lid one
// full sheet; a lid picked by hand turns the weather to a deck; each broken sky its own (heaps
// under high streaks; taller heaps under a rippled sheet; streaks over a veil), each within its
// bands over the valley floor; the same map deals the same sky and another seed another; the wind
// aloft grows and turns with height, drifts the layers downwind and ages them; and with the layers
// drawn a broken sky takes nothing off the sun on the whole.
//
// ge:summary Tests for the cloud field: the stacks, the deal, the wind aloft, the drift
// ge:keywords test mountain cloud field cloud_layers_for wind_aloft veer_aloft drift sky_at
// ge:keywords lid_under
// ge:skill mountain testing
#include <cmath>

#include "check.hpp"
#include "mountain/air/air.hpp"
#include "mountain/app/cloud_field.hpp"
#include "mountain/app/weather_look.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace app = ge::mountain::app;

mg::Weather sky(mg::WeatherKind kind, double wind = 4, double from = 0) {
  return {kind, 0, 0, wind, from, false};
}

constexpr double kPi = 3.14159265358979323846;

}  // namespace

TEST(clear_and_fog_stand_no_layers_and_a_lid_one_full_sheet) {
  for (mg::WeatherKind k : {mg::WeatherKind::clear, mg::WeatherKind::fog}) {
    CHECK(app::cloud_layers_for(sky(k), 38, 0, 45, 0).count == 0);
  }
  for (mg::WeatherKind k :
       {mg::WeatherKind::overcast, mg::WeatherKind::snow, mg::WeatherKind::storm}) {
    const auto lid = app::cloud_layers_for(sky(k), 38, 0, 45, 0);
    CHECK(lid.count == 1 && lid.layers[0].dome == 0 && lid.layers[0].cover >= 0.96);
  }
  // The snow's deck is thicker and darker than the overcast's.
  const auto grey = app::cloud_layers_for(sky(mg::WeatherKind::overcast), 38, 0, 45, 0);
  const auto deck = app::cloud_layers_for(sky(mg::WeatherKind::snow), 38, 0, 45, 0);
  CHECK(deck.layers[0].top_m - deck.layers[0].base_m >
        grey.layers[0].top_m - grey.layers[0].base_m);
}

TEST(a_lid_picked_by_hand_turns_the_weather_to_a_deck) {
  const ge::render::CloudKind* kinds[2] = {ge::render::cloud_kind_named("cumulus"),
                                           ge::render::cloud_kind_named("cirrus")};
  ge::render::Weather w;
  app::lid_under(kinds, 2, w);
  CHECK(w.cover == 0);  // broken skies are no lid
  kinds[1] = ge::render::cloud_kind_named("stratus");
  app::lid_under(kinds, 2, w);
  CHECK(w.cover == 1);
  ge::render::Weather dark;
  kinds[1] = ge::render::cloud_kind_named("nimbostratus");
  app::lid_under(kinds, 2, dark);
  CHECK(dark.cover == 1 && dark.deck_light < w.deck_light && dark.deck_zenith < w.deck_zenith);
  // A sea of cloud lying in the valley is no lid over it.
  ge::render::Weather sea;
  kinds[0] = ge::render::cloud_kind_named("inversion");
  app::lid_under(kinds, 1, sea);
  CHECK(sea.cover == 0);
}

TEST(each_broken_sky_stands_its_layers_in_their_bands) {
  const double floor_y = 200;
  const auto fair = app::cloud_layers_for(sky(mg::WeatherKind::fair), 38, floor_y, 45, 0);
  CHECK(fair.count == 2);
  CHECK(fair.layers[0].dome == 1 && fair.layers[0].base_m >= floor_y + 1500 &&
        fair.layers[0].base_m <= floor_y + 2000);
  CHECK(fair.layers[0].cover >= 0.22 && fair.layers[0].cover <= 0.36);
  CHECK(fair.layers[1].dome == 0 && fair.layers[1].base_m >= floor_y + 7600);
  const auto flurries = app::cloud_layers_for(sky(mg::WeatherKind::flurries), 38, floor_y, 45, 0);
  CHECK(flurries.count == 2);
  CHECK(flurries.layers[0].top_m - flurries.layers[0].base_m >= 1300);
  CHECK(flurries.layers[1].base_m >= floor_y + 3600 && flurries.layers[1].base_m <= floor_y + 4500);
  const auto high = app::cloud_layers_for(sky(mg::WeatherKind::high), 38, floor_y, 45, 0);
  CHECK(high.count == 2 && high.layers[0].stretch > 1 && high.layers[1].cover >= 0.75);
  for (const auto* s : {&fair, &flurries, &high})
    for (int i = 0; i < s->count; ++i)
      CHECK(s->layers[i].top_m > s->layers[i].base_m && s->layers[i].density > 0);
}

TEST(a_map_deals_its_own_sky) {
  const auto a = app::cloud_layers_for(sky(mg::WeatherKind::fair), 38, 0, 45, 0);
  const auto b = app::cloud_layers_for(sky(mg::WeatherKind::fair), 38, 0, 45, 0);
  const auto c = app::cloud_layers_for(sky(mg::WeatherKind::fair), 39, 0, 45, 0);
  CHECK(a.layers[0].base_m == b.layers[0].base_m && a.layers[0].cover == b.layers[0].cover);
  CHECK(a.layers[0].base_m != c.layers[0].base_m || a.layers[0].cover != c.layers[0].cover);
}

TEST(the_wind_aloft_grows_turns_and_carries_the_layers) {
  CHECK(app::wind_aloft(4, 2000, 0) > app::wind_aloft(4, 500, 0));
  CHECK(app::wind_aloft(0, 2000, 6) == 6);    // floored at the kind's least
  CHECK(app::wind_aloft(40, 9000, 0) == 45);  // held under a jet's
  CHECK(app::veer_aloft(2000, 45) > 0);       // clockwise in the north
  CHECK(app::veer_aloft(2000, -45) < 0);
  // Downwind of where the wind comes from, turned a little; drifting and ageing with the clock.
  const auto at0 = app::cloud_layers_for(sky(mg::WeatherKind::fair, 5, 1.0), 38, 0, 45, 0);
  const auto at60 = app::cloud_layers_for(sky(mg::WeatherKind::fair, 5, 1.0), 38, 0, 45, 60);
  const double turn = at0.layers[0].heading - (1.0 + kPi);
  CHECK(turn > 0 && turn < 0.6);
  const double moved = at60.layers[0].drift_m - at0.layers[0].drift_m;
  CHECK(std::fabs(moved - 60 * app::kCloudDrift *
                              app::wind_aloft(5, 0.5 * (at0.layers[0].base_m + at0.layers[0].top_m),
                                              4)) < 1e-6);
  CHECK(at60.layers[0].evolve > at0.layers[0].evolve);
}

TEST(drawn_layers_take_nothing_off_the_sun_on_the_whole) {
  const ge::mountain::air::Fall none{0, ge::mountain::air::visibility_in(0)};
  for (mg::WeatherKind k :
       {mg::WeatherKind::fair, mg::WeatherKind::flurries, mg::WeatherKind::high}) {
    const ge::render::Weather w = app::weather_look(sky(k), none, true);
    CHECK(w.sun_pass == 1 && w.cover == 0);
  }
  // A lid keeps its deck.
  const ge::render::Weather lid = app::weather_look(sky(mg::WeatherKind::overcast), none, true);
  CHECK(lid.cover == 1 && lid.sun_pass < 0.2);
}
