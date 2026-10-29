// LAB `climate`: the air's temperature and moisture (engine/wind/climate.hpp) and what they do
// (mist.hpp, cold.hpp), read the way a world reads them, so a change shows as numbers and
// pictures: a valley with a lake through a clear, still autumn night in six panels (the air by
// its temperature, the cold pooling on the floor, the fog forming by the water and lifting in the
// morning sun); a day's temperature and dew point on the valley floor, the slope and the summit;
// the fog's visual range through the night at the shore, in the valley, on a dry slope and in a
// wind; where fog lies by the day's temperature against the humidity and against the wind; the
// fall's rain share against the air's temperature by its humidity; the new snow's density and a
// ski's friction by the temperature with the feel's bands; the breath; and lake ice through a
// winter with what it bears. Every number is a pure function, so the lab is deterministic.
//
// ge:summary Lab: temperature, dew point, fog kinds, snow or rain, the snow's feel, breath, ice
// ge:keywords lab climate temperature dew-point fog mist valley cold-pool burn-off rain-share
// ge:keywords snow-feel glide fresh-density breath lake-ice cross-section
// ge:skill water labs
// ge:test tests/wind_climate_test.cpp tests/wind_mist_test.cpp tests/wind_cold_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "wind/climate.hpp"
#include "wind/cold.hpp"
#include "wind/mist.hpp"

namespace {

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

void chart_shot(gl::Lab& lab, const std::string& id, const gl::Chart& chart,
                const std::string& caption, const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id,
            chart.title,
            caption,
            look_for,
            {{"mean", lab.arg("mean")}, {"humidity", lab.arg("humidity")}}},
           gl::to_picture(chart.plot));
}

// The night the lab is of: clear, all but still, damp, by a lake still warm from the autumn.
gw::Climate night_of(const gl::Lab& lab) {
  gw::Climate c;
  c.mean_c = lab.arg_num("mean");
  c.range_c = 12;
  c.humidity = lab.arg_num("humidity");
  c.cloud = 0.05;
  c.wind = 1;
  c.water_c = 4;
  return c;
}

// THE VALLEY: 3 km across, its ridges 600 m over a lake 500 m wide on its floor.
constexpr double kHalf = 1500, kRidge = 600, kLake = 250, kTop = 900, kBed = 60;

double ground_at(double x) {
  const double a = std::fabs(x);
  if (a < kLake) return -40 * (1 - (a / kLake) * (a / kLake));  // the lake's bed
  const double u = (a - kLake) / (kHalf - kLake);
  return kRidge * u * std::sqrt(u);
}

gw::Site site_at(double x, double y) {
  gw::Site s;
  s.y = y;
  s.above = std::max(2.0, y - std::max(ground_at(x), 0.0));
  s.hollow = std::max(0.0, kRidge - y);
  s.water_m = std::max(0.0, std::fabs(x) - kLake);
  return s;
}

gl::Rgba temp_colour(double t) {
  // -12 C deep blue, 0 C pale, +12 C orange.
  const double u = std::clamp((t + 12) / 24, 0.0, 1.0);
  const auto mix = [](double a, double b, double s) {
    return static_cast<uint8_t>(std::lround(a + (b - a) * s));
  };
  if (u < 0.5) {
    const double s = u / 0.5;
    return {mix(40, 225, s), mix(80, 232, s), mix(170, 240, s), 255};
  }
  const double s = (u - 0.5) / 0.5;
  return {mix(225, 230, s), mix(232, 130, s), mix(240, 50, s), 255};
}

gl::Picture valley_panel(const gw::Climate& c, double hour, int w, int h, gl::Lab& lab) {
  gl::Picture p = gl::create_picture(w, h, {0, 0, 0, 255});
  const auto to_x = [&](int px) { return -kHalf + (px + 0.5) * 2 * kHalf / w; };
  const auto to_y = [&](int py) { return kTop - (py + 0.5) * (kTop + kBed) / h; };
  // The valley's fog lies level from the floor by the lake, like water, to its depth.
  const gw::Mist fog = gw::mist_at(c, site_at(kLake + 20, 0), hour);
  const double fog_top = fog.strength >= gw::kNoMist ? fog.depth : -1e9;
  const double fog_alpha = std::min(1.0, fog.strength) * 0.9;
  for (int px = 0; px < w; px += 2) {
    const double x = to_x(px);
    const double g = ground_at(x);
    for (int py = 0; py < h; py += 2) {
      const double y = to_y(py);
      gl::Rgba col;
      if (y < g) {
        col = {70, 62, 56, 255};
      } else if (y < 0) {
        col = {30, 70, 120, 255};
      } else {
        col = temp_colour(gw::air_at(c, site_at(x, y), hour).temp_c);
        if (y < fog_top) {
          col.r = static_cast<uint8_t>(std::lround(col.r + (250 - col.r) * fog_alpha));
          col.g = static_cast<uint8_t>(std::lround(col.g + (250 - col.g) * fog_alpha));
          col.b = static_cast<uint8_t>(std::lround(col.b + (252 - col.b) * fog_alpha));
        }
      }
      gl::fill_rect(p, px, py, 2, 2, col);
    }
  }
  const gw::Thermal floor_air = gw::air_at(c, site_at(kLake + 50, 0), hour);
  const gw::Mist& shore = fog;
  gl::draw_text(p, 6, 6, hour_name(hour), {20, 20, 30, 255}, 2);
  gl::draw_text(p, 6, 26,
                "FLOOR " + fixed(floor_air.temp_c, 1) + " C  DEW " + fixed(floor_air.dew_c, 1),
                {20, 20, 30, 255});
  gl::draw_text(p, 6, 38,
                shore.strength >= gw::kNoMist ? std::string(gw::mist_name(shore.kind)) + " FOG " +
                                                    fixed(shore.visibility, 0) + " M"
                                              : std::string("NO FOG"),
                {20, 20, 30, 255});
  lab.metric("valley.floor_c." + hour_name(hour), floor_air.temp_c, "C");
  lab.metric("valley.shore_visibility." + hour_name(hour), shore.visibility, "m");
  return p;
}

void valley(gl::Lab& lab) {
  const gw::Climate c = night_of(lab);
  constexpr int kW = 320, kH = 200;
  constexpr double kHours[] = {19, 22, 1, 4.5, 7, 10.5};
  gl::Picture sheet = gl::create_picture(3 * kW + 8, 2 * kH + 4, {255, 255, 255, 255});
  for (int k = 0; k < 6; ++k) {
    const gl::Picture panel = valley_panel(c, kHours[k], kW, kH, lab);
    gl::blit(sheet, panel, (k % 3) * (kW + 4), (k / 3) * (kH + 4));
  }
  lab.shot({"valley",
            "A VALLEY WITH A LAKE THROUGH A CLEAR, STILL NIGHT",
            "A 3 km valley, 600 m deep, a lake on its floor, from the evening to the next "
            "morning: the air coloured by its temperature (deep blue -12 C, pale 0 C, orange "
            "+12 C), the ground brown, the lake blue, the fog white to its top.",
            "the floor turns colder than the slopes as the night goes on (the cold pool); fog "
            "forms by the lake and fills the floor before dawn; the morning sun lifts it by "
            "10:30",
            {{"mean", lab.arg("mean")},
             {"humidity", lab.arg("humidity")},
             {"wind", "1"},
             {"cloud", "0.05"}}},
           sheet);
}

void day(gl::Lab& lab) {
  const gw::Climate c = night_of(lab);
  gl::LineChart line;
  line.frame = {"A DAY'S AIR ON THE VALLEY FLOOR, THE SLOPE AND THE SUMMIT",
                "clear and still; dashed: the dew points", "hour", "C"};
  line.spikes = false;
  struct Where {
    const char* name;
    gw::Site site;
  };
  gw::Site floor = site_at(kLake + 50, 0), slope = site_at(900, ground_at(900)),
           summit = site_at(kHalf, kRidge);
  summit.hollow = 0;
  const Where where[] = {{"floor", floor}, {"slope", slope}, {"summit", summit}};
  gl::LabTable table{
      "The air by the hour", {"hour", "where", "C", "dew C", "rh", "wet bulb C"}, {}};
  for (const Where& w : where) {
    gl::Trace t{w.name, {}, {}}, d{std::string(w.name) + " dew", {}, {}};
    for (int k = 0; k <= 96; ++k) {
      const double h = k / 4.0;
      const gw::Thermal a = gw::air_at(c, w.site, h);
      t.x.push_back(h);
      t.y.push_back(a.temp_c);
      d.x.push_back(h);
      d.y.push_back(a.dew_c);
      if (k % 16 == 0) {
        table.rows.push_back({hour_name(h), w.name, fixed(a.temp_c, 2), fixed(a.dew_c, 2),
                              fixed(a.rh, 2), fixed(a.wet_bulb_c, 2)});
        lab.metric(std::string("air.") + w.name + "." + hour_name(h), a.temp_c, "C");
      }
    }
    line.traces.push_back(t);
    line.traces.push_back(d);
  }
  line.marks = {{c.sunrise, "SUNRISE"}, {c.sunset, "SUNSET"}};
  chart_shot(lab, "day", gl::line_chart(line),
             "The air's temperature and its dew point over a day at the valley floor by the lake, "
             "on the slope and on the summit: the low at sunrise, the high two hours past noon.",
             "the floor colder than the summit before dawn (the inversion) and warmest after noon; "
             "each dew point held flat except where the air meets it at night");
  lab.table(table);
}

void night_fog(gl::Lab& lab) {
  const gw::Climate c = night_of(lab);
  gw::Climate windy = c;
  windy.wind = 7;
  gl::LineChart line;
  line.frame = {"THE FOG'S VISUAL RANGE THROUGH THE NIGHT",
                "clear and damp; dashed: 1 km (fog's edge) and 200 m", "hour (from 16:00)",
                "visibility (m)"};
  line.log_y = true;
  line.spikes = false;
  line.rules = {1000, 200};
  struct Where {
    const char* name;
    const gw::Climate* climate;
    gw::Site site;
  };
  gw::Site dry = site_at(900, ground_at(900));
  dry.water_m = 1e9;
  dry.hollow = 0;
  const Where where[] = {{"shore", &c, site_at(kLake + 20, 0)},
                         {"valley floor", &c, site_at(kLake + 300, ground_at(kLake + 300))},
                         {"dry slope", &c, dry},
                         {"shore, 7 m/s wind", &windy, site_at(kLake + 20, 0)}};
  for (const Where& w : where) {
    gl::Trace t{w.name, {}, {}};
    for (int k = 0; k <= 96; ++k) {
      const double h = 16 + k / 4.0;
      t.x.push_back(16 + k / 4.0);
      t.y.push_back(gw::mist_at(*w.climate, w.site, h).visibility);
    }
    line.traces.push_back(t);
    lab.metric(std::string("fog.") + w.name + ".05:00",
               gw::mist_at(*w.climate, w.site, 5).visibility, "m");
  }
  chart_shot(lab, "night-fog", gl::line_chart(line),
             "How far one sees through the fog from 16:00 to the next afternoon (log scale), at "
             "the lake's shore, on the valley floor, on a dry slope and at the shore in a 7 m/s "
             "wind.",
             "the shore and the floor sink under 1 km in the night and under 200 m before dawn, "
             "then clear by late morning; the dry slope and the windy shore keep at most a thin "
             "mist over 700 m");
}

void where_fog_lies(gl::Lab& lab) {
  // Rows: the day's mean temperature; columns: the humidity, then the wind. The site: a lake's
  // shore on open, level ground (no hollow to pool the cold).
  gw::Site shore = site_at(kLake + 20, 0);
  shore.hollow = 0;
  std::vector<std::string> rows;
  std::vector<double> by_humidity, by_wind;
  for (int t = 15; t >= -15; t -= 3) {
    rows.push_back(fixed(t, 0) + " C");
    for (int k = 0; k <= 10; ++k) {
      gw::Climate c;
      c.mean_c = t;
      c.range_c = 12;
      c.cloud = 0.05;
      c.wind = 1;
      c.water_c = std::max(4.0, t + 3.0);
      c.humidity = 0.5 + 0.05 * k;
      by_humidity.push_back(gw::mist_at(c, shore, 5).strength);
      c.humidity = 0.85;
      c.wind = k;
      by_wind.push_back(gw::mist_at(c, shore, 5).strength);
    }
  }
  const auto heat = [&](const std::string& id, const std::string& title, const std::string& x,
                        const std::vector<double>& v, double x0, double x1,
                        const std::string& caption, const std::string& look_for) {
    gl::ChartFrame f{title, "fog's strength at 05:00 on a level lakeshore, clear sky", x,
                     "day's mean"};
    chart_shot(lab, id, gl::heatmap_chart(f, rows, v, x0, x1), caption, look_for);
  };
  heat("fog-by-humidity", "WHERE FOG LIES: THE DAY'S TEMPERATURE AGAINST THE HUMIDITY",
       "humidity at the mean", by_humidity, 0.5, 1.0,
       "Fog's strength at dawn on a level lakeshore under a clear, still sky: the day's mean "
       "temperature down the side, the humidity at the mean across (light none, dark thick).",
       "radiation fog from a humidity of about 0.75 up, thick past 0.85, from 0 C up; below -6 C "
       "the lake (kept at 4 C) steams whatever the humidity");
  heat("fog-by-wind", "WHERE FOG LIES: THE DAY'S TEMPERATURE AGAINST THE WIND", "wind (m/s)",
       by_wind, 0, 10,
       "Fog's strength at dawn by a lake under a clear sky, the humidity 0.85: the day's mean "
       "down the side, the wind across.",
       "thick in still air and a light breeze, thinning past 4 m/s and gone past 7 (mixed away); "
       "at the coldest rows the steam off the warmer lake survives the wind");
}

void phase(gl::Lab& lab) {
  gl::LineChart line;
  line.frame = {"SNOW OR RAIN BY THE AIR'S TEMPERATURE",
                "the fall's rain share by the wet bulb, at three humidities", "air (C)",
                "rain share"};
  line.spikes = false;
  for (const double rh : {0.5, 0.75, 1.0}) {
    gl::Trace t{"rh " + fixed(rh, 2), {}, {}};
    for (int k = -40; k <= 80; ++k) {
      const double a = k / 10.0;
      gw::Thermal air;
      air.temp_c = a;
      air.wet_bulb_c = gw::wet_bulb_of(a, rh);
      t.x.push_back(a);
      t.y.push_back(gw::rain_share_of(air));
    }
    line.traces.push_back(t);
  }
  chart_shot(lab, "phase", gl::line_chart(line),
             "The share of a fall that is rain against the air's temperature at the ground, for a "
             "dry, a moderate and a saturated air: the phase follows the wet bulb.",
             "saturated air turns to rain round +1 C; dry air (0.5) still snows at +3 C and rains "
             "only past about +6 C");
  gl::LabTable table{"The snow level against the freezing level",
                     {"mean C", "rh", "freezing m", "snow level m"},
                     {}};
  for (const double mean : {2.0, 6.0}) {
    for (const double rh : {0.5, 0.8, 0.98}) {
      gw::Climate c;
      c.mean_c = mean;
      c.humidity = rh;
      table.rows.push_back({fixed(mean, 0), fixed(rh, 2), fixed(gw::freezing_level(c, 14), 0),
                            fixed(gw::snow_level(c, 14), 0)});
      lab.metric("snow_level." + fixed(mean, 0) + "." + fixed(rh, 2), gw::snow_level(c, 14), "m");
    }
  }
  lab.table(table);
}

void snow(gl::Lab& lab) {
  gl::LineChart density, glide;
  density.frame = {"NEW SNOW'S DENSITY BY THE AIR IT FALLS THROUGH", "kg/m3", "air (C)",
                   "density (kg/m3)"};
  glide.frame = {"A SKI'S FRICTION BY THE SNOW'S TEMPERATURE", "dry and wet snow; the feel's bands",
                 "snow (C)", "friction"};
  density.spikes = glide.spikes = false;
  gl::Trace d{"fresh", {}, {}}, dry{"dry", {}, {}}, wet{"wet (0.5)", {}, {}};
  for (int k = -300; k <= 40; ++k) {
    const double t = k / 10.0;
    d.x.push_back(t);
    d.y.push_back(gw::fresh_density(t));
    if (t <= 0) {
      dry.x.push_back(t);
      dry.y.push_back(gw::glide_of(t, 0));
      wet.x.push_back(t);
      wet.y.push_back(gw::glide_of(t, 0.5));
    }
  }
  density.traces = {d};
  glide.traces = {dry, wet};
  glide.bands = {{-30, -10, "SQUEAKY"}, {-10, -2, "SOFT"}, {-2, 0, "STICKY"}};
  chart_shot(lab, "fresh-density", gl::line_chart(density),
             "New snow's density against the air's temperature as it falls: feather-light below "
             "-10 C, wet and heavy round 0 C, continuous through it.",
             "flat near 68 kg/m3 in the cold, rising steeply past -5 C to 119 at 0 C and on");
  chart_shot(lab, "glide", gl::line_chart(glide),
             "A ski's kinetic friction on snow against the snow's temperature, dry and half wet, "
             "with the feel's bands: squeaky under -10 C, soft, sticky near 0.",
             "the dry curve lowest round -3 C, climbing steeply into the cold; wet snow higher "
             "everywhere");
  gl::LabTable table{"The feel", {"snow C", "air C", "new m", "thawed", "feel", "squeaks"}, {}};
  struct Case {
    double snow, air, fresh;
    bool thawed;
  };
  constexpr Case kCases[] = {{-14, -14, 0.2, false}, {-15, -15, 0, false}, {-4, -4, 0.2, false},
                             {-1, -1, 0.2, false},   {-5, -6, 0, false},   {-5, -6, 0, true},
                             {0, 3, 0, false}};
  for (const Case& k : kCases) {
    table.rows.push_back({fixed(k.snow, 0), fixed(k.air, 0), fixed(k.fresh, 2),
                          k.thawed ? "yes" : "no",
                          gw::feel_name(gw::snow_feel(k.snow, k.air, k.fresh, k.thawed)),
                          gw::squeaks(k.snow) ? "yes" : "no"});
  }
  lab.table(table);
}

void breath_and_ice(gl::Lab& lab) {
  gl::LineChart breath, ice;
  breath.frame = {"THE BREATH'S CLOUD BY THE AIR",
                  "1 = a -10 C winter's plume; dashed: 0.1, a wisp", "air (C)", "breath"};
  breath.spikes = false;
  breath.rules = {0.1};
  for (const double rh : {0.4, 0.7, 0.95}) {
    gl::Trace t{"rh " + fixed(rh, 2), {}, {}};
    for (int k = -30; k <= 25; ++k) {
      gw::Thermal a;
      a.temp_c = k;
      a.dew_c = gw::dew_point_of(k, rh);
      t.x.push_back(k);
      t.y.push_back(gw::breath_of(a));
    }
    breath.traces.push_back(t);
  }
  chart_shot(lab, "breath", gl::line_chart(breath),
             "How thick the breath's cloud shows against the air's temperature, at three "
             "humidities: the most water any mix of breath and air condenses.",
             "every line rises into the cold; a damp air shows the breath below 10 C, a dry one "
             "only below about 5 C");
  ice.frame = {"LAKE ICE THROUGH A WINTER", "dashed: 10 cm bears a walker, 25 cm a car", "day",
               "ice (m)"};
  ice.spikes = false;
  ice.rules = {0.10, 0.25};
  for (const double mean : {-3.0, -8.0, -15.0}) {
    gl::Trace t{"mean " + fixed(mean, 0) + " C", {}, {}};
    double h = 0;
    for (int d = 0; d <= 90; ++d) {
      t.x.push_back(d);
      t.y.push_back(h);
      h = gw::ice_grown(h, mean, 1);
    }
    ice.traces.push_back(t);
    lab.metric("ice.after90." + fixed(mean, 0), h, "m");
  }
  chart_shot(lab, "ice", gl::line_chart(ice),
             "Ice on a lake over 90 days at three daily means (Stefan's law), with what it bears.",
             "each curve the root of the days; at -15 C a walker's ice in under a week and a "
             "car's in about three");
}

void run(gl::Lab& lab) {
  valley(lab);
  day(lab);
  night_fog(lab);
  where_fog_lies(lab);
  phase(lab);
  snow(lab);
  breath_and_ice(lab);
}

const gl::LabSpec kSpec{
    "climate",
    gl::LabKind::picture,
    "engine",
    "engine/wind/climate.hpp",
    "water",
    "Does the air cool and pool where it should, does fog form by water on a clear, still night "
    "near freezing and lift in the sun, does a fall turn to rain at the wet bulb's 1 C, and do the "
    "snow, the breath and the ice follow the temperature?",
    {{"mean", "1", "the day's mean temperature, C, of the valley's night"},
     {"humidity", "0.8", "the humidity at the mean, 0..1"}},
    "",
    run};
GE_LAB(kSpec);

}  // namespace
