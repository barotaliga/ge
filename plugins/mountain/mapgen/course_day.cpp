// The definitions behind course_day.hpp.
//
// ge:summary A course's day: the draws off the course's own stream, held to the face
// ge:keywords course-day
// ge:skill mountain
// ge:test tests/mountain_resort_level_test.cpp
#include "mountain/mapgen/course_day.hpp"

#include <string>

#include "core/num.hpp"
#include "core/prng.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/sun.hpp"
#include "mountain/mapgen/weather.hpp"

namespace ge::mountain::mapgen {

namespace num = ge::core::num;

namespace {

constexpr uint32_t kDaySalt = 0x0dae5a1;
constexpr int kDraws = 48;
// The hour past noon the fallback day starts at: the hour the face was turned to.
constexpr double kNoon = 12.5;

}  // namespace

CourseDay course_day(uint32_t sub, const Region& region, double latitude, double facing,
                     const std::string& id) {
  const uint32_t own =
      sub ^ static_cast<uint32_t>(num::imul(static_cast<uint32_t>(std::stoi(id)), kDaySalt));
  ge::core::Rng rng = ge::core::make_rng(own);
  const Band days = region.sun.day_of_year;
  for (int t = 0; t < kDraws; t++) {
    const double day = num::round(in_band(rng, days));
    const auto w = sun_window(latitude, day);
    if (!w) continue;
    const double hour = ge::core::range(rng, w->min, w->max);
    const DealtWeather sky =
        deal_weather(own + static_cast<uint32_t>(t), Sun{hour, day, latitude, std::nullopt});
    const Sun sun{sky.hour, day, latitude, facing};
    if (sun_off_face(sun) <= rules::sun::facing) return {sun, sky.weather};
  }
  const double day = num::round((days.min + days.max) / 2);
  DealtWeather sky = deal_weather(own, Sun{kNoon, day, latitude, std::nullopt});
  sky.weather.evening = false;
  return {Sun{kNoon, day, latitude, facing}, sky.weather};
}

}  // namespace ge::mountain::mapgen
