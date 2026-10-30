// LAB `classic-climate`: the classic levels' air (plugins/classicenhance/climate.hpp) read through
// the engine's temperature and humidity (engine/wind/climate.hpp, mist.hpp, cold.hpp): a card for
// every level at the hour it is played (the open yard's temperature and dew point, whether a fall
// there is snow, sleet or rain, the steam off its pools, the breath in its halls, frost on its
// stone), and each level's yard through the night with its hour marked. Only this repository's
// table is read, never the pack, so the pictures may be published.
//
// ge:summary Lab: the classic levels' air at their hours: temperature, fall, steam, breath, frost
// ge:keywords lab classicenhance climate level temperature steam breath frost snow rain night
// ge:skill classicenhance water labs
// ge:test tests/classicenhance_climate_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "classicenhance/climate.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "wind/cold.hpp"
#include "wind/mist.hpp"

namespace {

namespace ce = ge::classicenhance;
namespace gl = ge::lab;
namespace gw = ge::wind;

std::string fixed(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::string hour_name(double h) {
  char b[16];
  const int m = static_cast<int>(std::lround(h * 60)) % (24 * 60);
  std::snprintf(b, sizeof b, "%02d:%02d", m / 60, m % 60);
  return b;
}

const char* fall_word(double rain) {
  if (rain < 0.15) return "SNOW";
  if (rain > 0.85) return "RAIN";
  return "SLEET";
}

gl::Rgba temp_colour(double t) {
  const double u = std::clamp((t + 15) / 30, 0.0, 1.0);
  const auto mix = [](double a, double b, double s) {
    return static_cast<uint8_t>(std::lround(a + (b - a) * s));
  };
  if (u < 0.5) {
    const double s = u / 0.5;
    return {mix(52, 196, s), mix(86, 210, s), mix(160, 226, s), 255};
  }
  const double s = (u - 0.5) / 0.5;
  return {mix(196, 214, s), mix(210, 120, s), mix(226, 60, s), 255};
}

// What a level's air does at its hour: the open yard, a hall, a pool in a hall, a stone wall.
struct Reading {
  gw::Thermal yard, hall;
  gw::Mist pool;
  double rain = 0, breath = 0;
  gw::Settling frost;
};

Reading read(const ce::LevelAir& l) {
  gw::Site yard, hall, pool;
  hall.sky = 0;
  pool.sky = 0;
  pool.water_m = 0;
  Reading r;
  r.yard = gw::air_at(l.climate, yard, l.hour);
  r.hall = gw::air_at(l.climate, hall, l.hour);
  r.pool = gw::mist_at(l.climate, pool, l.hour);
  r.rain = gw::rain_share_of(r.yard);
  r.breath = gw::breath_of(r.hall);
  r.frost = gw::deposit_on(r.yard, gw::surface_at(l.climate, yard, l.hour),
                           gw::mist_at(l.climate, yard, l.hour), l.climate.wind);
  return r;
}

void cards(gl::Lab& lab) {
  constexpr int kW = 300, kH = 132, kCols = 3;
  const auto levels = ce::level_airs();
  const int rows = static_cast<int>((levels.size() + kCols - 1) / kCols);
  gl::Picture sheet = gl::create_picture(kCols * (kW + 4), rows * (kH + 4), {255, 255, 255, 255});
  gl::LabTable table{
      "The levels' air at their hours",
      {"map", "hour", "yard C", "dew C", "rh", "fall", "pool fog", "pool m", "breath", "on stone"},
      {}};
  for (size_t k = 0; k < levels.size(); ++k) {
    const ce::LevelAir& l = levels[k];
    const Reading r = read(l);
    gl::Picture card = gl::create_picture(kW, kH, temp_colour(r.yard.temp_c));
    const gl::Rgba ink{20, 22, 30, 255};
    const std::string map(l.map);
    gl::draw_text(card, 8, 8, map + "  " + hour_name(l.hour), ink, 2);
    gl::draw_text(card, 8, 30, std::string(l.what).substr(0, 48), ink);
    gl::draw_text(card, 8, 46,
                  "YARD " + fixed(r.yard.temp_c, 1) + " C  DEW " + fixed(r.yard.dew_c, 1) +
                      "  RH " + fixed(r.yard.rh, 2),
                  ink);
    gl::draw_text(card, 8, 60,
                  "WIND " + fixed(l.climate.wind, 0) + " M/S  CLOUD " + fixed(l.climate.cloud, 2),
                  ink);
    gl::draw_text(card, 8, 74,
                  std::string("A FALL IS ") + fall_word(r.rain) + " (" + fixed(r.rain, 2) + ")",
                  ink);
    gl::draw_text(card, 8, 88,
                  r.pool.strength >= gw::kNoMist
                      ? "POOLS (" + fixed(l.climate.water_c, 0) + " C) STEAM, " +
                            fixed(r.pool.visibility, 0) + " M"
                      : "POOLS (" + fixed(l.climate.water_c, 0) + " C) CLEAR",
                  ink);
    gl::draw_text(
        card, 8, 102,
        r.breath > 0.1 ? "BREATH SHOWS (" + fixed(r.breath, 2) + ")" : std::string("NO BREATH"),
        ink);
    gl::draw_text(card, 8, 116,
                  std::string("ON STONE: ") + gw::deposit_name(r.frost.kind) +
                      (r.frost.rate > 0 ? " " + fixed(r.frost.rate, 3) + " MM/H" : ""),
                  ink);
    gl::blit(sheet, card, static_cast<int>(k % kCols) * (kW + 4),
             static_cast<int>(k / kCols) * (kH + 4));
    table.rows.push_back({map, hour_name(l.hour), fixed(r.yard.temp_c, 2), fixed(r.yard.dew_c, 2),
                          fixed(r.yard.rh, 2), fall_word(r.rain), gw::mist_name(r.pool.kind),
                          fixed(r.pool.visibility, 0), fixed(r.breath, 2),
                          gw::deposit_name(r.frost.kind)});
    lab.metric("yard_c." + map, r.yard.temp_c, "C");
    lab.metric("pool_visibility." + map, r.pool.visibility, "m");
    lab.metric("breath." + map, r.breath, "share");
  }
  lab.shot({"levels",
            "THE CLASSIC LEVELS' AIR AT THEIR HOURS",
            "A card for every level of the classic pack at the hour it is played, coloured by "
            "the open yard's temperature (blue -15 C, pale 0 C, orange +15 C): the yard's air, "
            "the wind and the low cloud, what a fall there is, the steam off its pools, the "
            "breath in its halls and what settles on its stone.",
            "every level after sunset, windy, under cloud; the cold levels' warm pools steam and "
            "their halls show the breath; the lava levels are warm and dry; the castle and the "
            "keep snow",
            {}},
           sheet);
  lab.table(table);
}

void nights(gl::Lab& lab) {
  gl::LineChart line;
  line.frame = {"EACH LEVEL'S OPEN YARD THROUGH THE NIGHT",
                "the open air under each level's own wind and cloud", "hour (from 17:00)", "C"};
  line.spikes = false;
  for (const ce::LevelAir& l : ce::level_airs()) {
    gl::Trace t{std::string(l.map), {}, {}};
    for (int k = 0; k <= 56; ++k) {
      const double h = 17 + k / 4.0;
      t.x.push_back(h);
      t.y.push_back(gw::air_at(l.climate, gw::Site{}, h).temp_c);
    }
    line.traces.push_back(t);
  }
  line.rules = {0};
  const gl::Chart chart = gl::line_chart(line);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach("nights.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({"nights",
            chart.title,
            "Each level's open-yard temperature from 17:00 to 07:00 under its own wind and cloud: "
            "the low cloud and the wind keep each night's fall to a degree or two.",
            "flat, gently falling lines; the castle, the keep and the secret level under 0 C "
            "(the dashed line), the lava halls the warmest",
            {}},
           gl::to_picture(chart.plot));
}

void run(gl::Lab& lab) {
  cards(lab);
  nights(lab);
}

const gl::LabSpec kSpec{
    "classic-climate",
    gl::LabKind::picture,
    "plugin:classicenhance",
    "plugins/classicenhance/climate.hpp",
    "classicenhance",
    "Does every classic level have its air, after dark, windy and under low cloud, and do its "
    "pools steam, its fall snow or rain and its breath show where its temperature says?",
    {},
    "",
    run};
GE_LAB(kSpec);

}  // namespace
