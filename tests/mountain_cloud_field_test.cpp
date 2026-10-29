// plugins/mountain/app/cloud_field.hpp: a clear sky and a fog stand no layers and every lid one
// full sheet; a lid picked by hand turns the weather to a deck; each broken sky its own (heaps
// under high streaks; taller heaps under a rippled sheet; streaks over a veil), each within its
// bands over the valley floor; the same map deals the same sky and another seed another; the wind
// aloft grows and turns with height, drifts the layers downwind and ages them; and with the layers
// drawn a broken sky takes nothing off the sun on the whole; a layer carries its kind's light round
// the disc (its particles, its halo, its deck); dusk winds up the low-level jet and sends scud
// racing under the sky; a sky picked by hand stands at its height and drifts at its speed.
//
// ge:summary Tests for the cloud field: the stacks, the deal, the wind aloft, the drift
// ge:keywords test mountain cloud field cloud_layers_for wind_aloft veer_aloft drift sky_at
// ge:keywords lid_under dusk_of jet_at low-level-jet scud fractus aloft
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

TEST(a_layer_carries_its_kinds_light_round_the_disc) {
  const ge::render::CloudKind* kinds[2] = {ge::render::cloud_kind_named("altostratus"),
                                           ge::render::cloud_kind_named("veil")};
  const ge::render::Clouds c =
      app::cloud_layers_of(kinds, 2, sky(mg::WeatherKind::fair), 7, 1000, 46, 0);
  CHECK(c.count == 2);
  CHECK(c.layers[0].drop_um == kinds[0]->drop && c.layers[0].drop_spread == kinds[0]->spread);
  CHECK(c.layers[0].deck == 1 && c.layers[0].halo == 0);
  CHECK(c.layers[1].halo == 1 && c.layers[1].deck == 0 && c.layers[1].drop_um > 20);
}

TEST(a_layer_carries_its_kinds_thickness_and_a_sky_its_own_lean) {
  const ge::render::CloudKind* kinds[2] = {ge::render::cloud_kind_named("altostratus"),
                                           ge::render::cloud_kind_named("stratus")};
  int thinner = 0, thicker = 0;
  for (int n = 1; n <= 40; ++n) {
    const double seed = n;
    const ge::render::Clouds c =
        app::cloud_layers_of(kinds, 2, sky(mg::WeatherKind::fair), seed, 1000, 46, 0);
    for (int i = 0; i < 2; ++i) {
      CHECK(c.layers[i].thin == kinds[i]->thin && c.layers[i].thick_cells == kinds[i]->thick_cells);
      CHECK(c.layers[i].thick_bias >= -0.4 && c.layers[i].thick_bias <= 0.4);
    }
    if (c.layers[0].thick_bias < -0.1) ++thinner;
    if (c.layers[0].thick_bias > 0.1) ++thicker;
  }
  CHECK(thinner > 5 && thicker > 5);
  // A middle veil the sun shows through in places is no lid over the valley.
  ge::render::Weather w;
  app::lid_under(kinds, 1, w);
  CHECK(w.cover == 0);
  app::lid_under(kinds + 1, 1, w);
  CHECK(w.cover == 1);
}

TEST(dusk_winds_up_the_low_level_jet_and_sends_scud_racing_under_the_sky) {
  constexpr double kDeg = kPi / 180;
  CHECK(app::dusk_of(10 * kDeg) == 0 && app::dusk_of(-10 * kDeg) == 1);
  CHECK(app::dusk_of(0) > 0.4 && app::dusk_of(0) < 0.6);
  // By day the wind is the day's at every height; at night it is up to 1.7 times it a few
  // hundred metres up and the day's again kilometres up.
  CHECK(app::jet_at(400, 0) == 1);
  CHECK(app::jet_at(400, 1) > 1.65 && app::jet_at(400, 1) <= 1.7);
  CHECK(app::jet_at(3000, 1) < 1.01);
  // By day a fair sky is its two layers, bit for bit what it was before dusk was read.
  const mg::Weather fair = sky(mg::WeatherKind::fair, 6);
  const auto day = app::cloud_layers_for(fair, 38, 1000, 45, 60);
  CHECK(day.count == 2);
  // At night scud races under it, the stack dealt as by day, and low and near it crosses the sky
  // many times faster than the heaps over it.
  const auto night = app::cloud_layers_for(fair, 38, 1000, 45, 60, {.dusk = 1});
  CHECK(night.count == 3);
  const ge::render::CloudLayer& scud = night.layers[0];
  CHECK(scud.base_m >= 1200 && scud.base_m <= 1450 && scud.cover > 0.1);
  CHECK(night.layers[1].base_m == day.layers[0].base_m &&
        night.layers[1].cover == day.layers[0].cover);
  const auto later = app::cloud_layers_for(fair, 38, 1000, 45, 61, {.dusk = 1});
  auto across = [&](int i) {  // radians of sky a layer's base crosses overhead in a second
    const double h = night.layers[i].base_m - 1000;
    return (later.layers[i].drift_m - night.layers[i].drift_m) / h;
  };
  CHECK(across(0) > 3 * across(1));
  // Twilight brings less of it; a high, a clear and a still sky none.
  CHECK(app::cloud_layers_for(fair, 38, 1000, 45, 60, {.dusk = 0.3}).layers[0].cover < scud.cover);
  CHECK(app::cloud_layers_for(sky(mg::WeatherKind::high, 6), 38, 1000, 45, 60, {.dusk = 1}).count ==
        2);
  CHECK(
      app::cloud_layers_for(sky(mg::WeatherKind::clear, 6), 38, 1000, 45, 60, {.dusk = 1}).count ==
      0);
  CHECK(app::cloud_layers_for(sky(mg::WeatherKind::fair, 1), 38, 1000, 45, 60, {.dusk = 1}).count ==
        2);
}

TEST(a_sky_picked_by_hand_stands_at_its_height_and_drifts_at_its_speed) {
  const mg::Weather fair = sky(mg::WeatherKind::fair, 6);
  const auto own = app::cloud_layers_for(fair, 38, 1000, 45, 0);
  const auto low = app::cloud_layers_for(fair, 38, 1000, 45, 0, {.base = 400});
  CHECK(std::fabs(low.layers[0].base_m - 1400) < 1e-9);
  // The layers over it in the same proportion, each as deep as its kind deals it.
  const double k = 400 / (own.layers[0].base_m - 1000);
  CHECK(std::fabs(low.layers[1].base_m - (1000 + (own.layers[1].base_m - 1000) * k)) < 1e-6);
  CHECK(std::fabs((low.layers[1].top_m - low.layers[1].base_m) -
                  (own.layers[1].top_m - own.layers[1].base_m)) < 1e-9);
  // The game's own is the true wind, and a quarter of it a quarter as far.
  const auto own60 = app::cloud_layers_for(fair, 38, 1000, 45, 60);
  const auto slow0 = app::cloud_layers_for(fair, 38, 1000, 45, 0, {.speed = 0.25});
  const auto slow60 = app::cloud_layers_for(fair, 38, 1000, 45, 60, {.speed = 0.25});
  const double own_moved = own60.layers[0].drift_m - own.layers[0].drift_m;
  const double slow_moved = slow60.layers[0].drift_m - slow0.layers[0].drift_m;
  CHECK(app::kCloudDrift == 1 && std::fabs(4 * slow_moved - own_moved) < 1e-6);
}

TEST(a_layer_is_as_tall_as_its_thickness_and_the_next_starts_over_it) {
  const mg::Weather fair = sky(mg::WeatherKind::fair, 6);
  const auto own = app::cloud_layers_for(fair, 38, 1000, 45, 0);
  // Picked by hand: the lowest layer 3 km tall, as dense a metre as its kind deals it, so its
  // column hides the more of the sun; the next layer starts at its top.
  const auto tall = app::cloud_layers_for(fair, 38, 1000, 45, 0, {.base = 400, .thickness = 3000});
  CHECK(std::fabs(tall.layers[0].base_m - 1400) < 1e-9);
  CHECK(std::fabs(tall.layers[0].top_m - 4400) < 1e-9);
  CHECK(tall.layers[0].density == own.layers[0].density);
  const double own_depth = own.layers[0].top_m - own.layers[0].base_m;
  CHECK(std::fabs(tall.layers[0].shadow_tau - own.layers[0].shadow_tau * 3000 / own_depth) < 1e-9);
  CHECK(tall.layers[1].base_m >= tall.layers[0].top_m);
  // Thin: the next layer keeps its own height.
  const auto thin = app::cloud_layers_for(fair, 38, 1000, 45, 0, {.thickness = 50});
  CHECK(std::fabs(thin.layers[0].top_m - thin.layers[0].base_m - 50) < 1e-9);
  CHECK(thin.layers[1].base_m == own.layers[1].base_m);
  // In every sky, picked by hand or dealt, with the scud under it at dusk or not, no two layers
  // share a slab of the air.
  for (const mg::WeatherKind k : {mg::WeatherKind::fair, mg::WeatherKind::flurries,
                                  mg::WeatherKind::high, mg::WeatherKind::overcast}) {
    for (const app::Aloft aloft : {app::Aloft{}, app::Aloft{.dusk = 1}, app::Aloft{.base = 300},
                                   app::Aloft{.base = 200, .thickness = 5000}}) {
      for (const double seed : {7.0, 38.0, 91.0}) {
        const auto s = app::cloud_layers_for(sky(k, 6), seed, 1000, 45, 0, aloft);
        for (int i = 1; i < s.count; ++i) CHECK(s.layers[i].base_m >= s.layers[i - 1].top_m);
      }
    }
  }
}
