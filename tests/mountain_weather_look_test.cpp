// plugins/mountain/app/weather_look.hpp: a clear sky changes nothing; each lid covers the whole
// dome and lets an eighth of the sun or less through, a storm least; a broken sky or a veil
// covers part of it and keeps most of the sun; a fall closes the view to its own visual range and
// hides the deck overhead, and takes its share off the sun; a valley fog closes the view the more
// the thicker it is; and the look at a moment is the map's sky with the air's fall then.
//
// ge:summary Test: the mountain's weather kinds as the renderer's cloud, deck and fog
// ge:keywords test weather look overcast deck fog visibility sun-pass cover weather_look
// ge:skill mountain testing
#include <cmath>

#include "check.hpp"
#include "mountain/air/air.hpp"
#include "mountain/app/weather_look.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace ma = ge::mountain::air;
using ge::mountain::app::weather_look;

mg::Weather sky(mg::WeatherKind kind, double snowfall = 0, double fog = 0) {
  return {kind, snowfall, fog, 0, 0, false};
}

ma::Fall falling(double fall) { return {fall, ma::visibility_in(fall)}; }

}  // namespace

TEST(a_clear_sky_changes_nothing) {
  const ge::render::Weather w = weather_look(sky(mg::WeatherKind::clear), falling(0));
  const ge::render::Weather none;
  CHECK(w.sun_pass == none.sun_pass && w.cover == none.cover);
  CHECK(w.visibility_m == 0 && w.hidden == 0);
}

TEST(a_lid_covers_the_dome_and_dims_the_sun) {
  double last = 1;
  for (mg::WeatherKind k :
       {mg::WeatherKind::overcast, mg::WeatherKind::snow, mg::WeatherKind::storm}) {
    const ge::render::Weather w = weather_look(sky(k), falling(0));
    CHECK(w.cover == 1);
    CHECK(w.sun_pass <= 0.125 && w.sun_pass < last);  // a storm's the least
    CHECK(w.visibility_m > 2000 && w.visibility_m < 4000);
    CHECK(w.deck_zenith < 1);  // darker overhead
    last = w.sun_pass;
  }
}

TEST(a_broken_sky_keeps_most_of_the_sun) {
  for (mg::WeatherKind k :
       {mg::WeatherKind::fair, mg::WeatherKind::flurries, mg::WeatherKind::high}) {
    const ge::render::Weather w = weather_look(sky(k), falling(0));
    CHECK(w.cover > 0 && w.cover < 0.5);
    CHECK(w.sun_pass >= 0.5 && w.sun_pass < 1);
    CHECK(w.visibility_m == 0);
  }
}

TEST(a_fall_closes_the_view_and_hides_the_deck) {
  const mg::Weather s = sky(mg::WeatherKind::snow, 0.6);
  const ge::render::Weather light = weather_look(s, falling(0.2));
  const ge::render::Weather heavy = weather_look(s, falling(0.9));
  CHECK(std::fabs(heavy.visibility_m - ma::visibility_in(0.9)) < 1e-9);
  CHECK(heavy.visibility_m < light.visibility_m);
  CHECK(heavy.hidden > light.hidden && heavy.hidden > 0.99);
  CHECK(heavy.sun_pass < light.sun_pass);
  // A flurry's fall with no deck still closes the view.
  const ge::render::Weather flurry =
      weather_look(sky(mg::WeatherKind::flurries, 0.3), falling(0.3));
  CHECK(std::fabs(flurry.visibility_m - ma::visibility_in(0.3)) < 1e-9);
}

TEST(a_thicker_fog_closes_the_view_more) {
  const ge::render::Weather thin = weather_look(sky(mg::WeatherKind::fog, 0, 0), falling(0));
  const ge::render::Weather thick = weather_look(sky(mg::WeatherKind::fog, 0, 1), falling(0));
  CHECK(thin.visibility_m > 1000 && thick.visibility_m < 150);
  CHECK(thick.sun_pass < thin.sun_pass);
  CHECK(thin.cover == 1 && thin.hidden == 0);
}

TEST(the_look_at_a_moment_is_the_maps_sky_then) {
  mg::Level l;
  l.seed = 38;
  l.weather = sky(mg::WeatherKind::storm, 0.9);
  for (double t : {0.0, 30.0, 300.0}) {
    const ge::render::Weather a = ge::mountain::app::weather_at(l, t);
    const ge::render::Weather b = weather_look(l.weather, ma::fall_at(l, t));
    CHECK(a.sun_pass == b.sun_pass && a.visibility_m == b.visibility_m && a.hidden == b.hidden);
  }
}
