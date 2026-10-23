// LAB `moon`: THE REAL MOON (core/moon.hpp's place, phase, face and shadow; render/moon.hpp's
// albedo map; render/night.hpp's shadow light), drawn on the CPU. The digest holds the moon's
// numbers; the pictures and the eclipse's disc light read render/night.hpp's shadow light, which
// is the platform's <cmath>, and are measured. Reading (the default) decodes the albedo map and
// checks it (the seas darker than the highlands by the real moon's ratio, the near side's mean the
// scale's), then draws:
//
// - `map`: the albedo map, the whole sphere, the near side in the middle;
// - `phases`: a lunar month from the new moon of 18 January 2026, eight discs as the eye at 46 N
//   sees them at the hour each stands highest, the zenith up: the bright limb toward the sun, the
//   earthlit part, the seas where they are, the face tilted and rocked by the libration;
// - `year`: the moon's height over 2026 at that latitude: each night's highest (by its lit
//   share) beside the noon sun's, so the full moon rides high in winter and low in summer;
// - `paths`: each full moon of 2026 across the sky from its rising to its setting;
// - `eclipse`: the total eclipse of 3 March 2026 as seen from 20 N 155 W, eight moments from the
//   penumbra's first touch to the umbra's last, drawn as the eye sees a faint part beside a
//   bright one, the umbra the air's red.
//
// BAKING (`--bake=<png>`) is the offline half that writes the table: it reads a colour PNG of an
// albedo mosaic of the moon (equirectangular, twice as wide as high, 512 or a multiple of it
// high; the public-domain colour map of a lunar orbiter's wide-angle camera mosaic, as NASA's
// visualization studio publishes it), takes its linear luminance, and with `--write` (from the
// repository's root) writes render/moon_data.cpp. Built only with GE_GPU (the table lives in
// ge_render).
//
// ge:summary Lab: the real moon's map, a month of phases, its height and paths over a year, an
// ge:summary eclipse; and the albedo table's bake
// ge:keywords lab moon phases libration albedo moon-map eclipse moon-path full-moon bake picture
// ge:skill render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/moon.hpp"
#include "core/solar.hpp"
#include "lab/lab.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"
#include "render/moon.hpp"
#include "render/night.hpp"

namespace {

namespace gc = ge::core;
namespace libm = ge::core::libm;
namespace gr = ge::render;
using ge::lab::Det;
using ge::lab::Picture;
using ge::lab::Rgba;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
// The umbra's red (render/sky.wgsl's kUmbraTint).
constexpr double kUmbraTint[3] = {2.24, 0.70, 0.36};

// The place the pictures are seen from.
constexpr double kLat = 46.0, kLon = 7.0;

std::string fixed(double v, int places) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*f", places, v);
  return buf;
}

uint8_t byte(double v) {
  // Linear light to the display's sRGB.
  const double c = std::clamp(v, 0.0, 1.0);
  const double s = c <= 0.0031308 ? c * 12.92 : 1.055 * libm::pow(c, 1 / 2.4) - 0.055;
  return static_cast<uint8_t>(std::lround(s * 255));
}

double dot(const gc::Vec3d& a, const gc::Vec3d& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
gc::Vec3d axpy(const gc::Vec3d& a, double s, const gc::Vec3d& b) {
  return {a.x + s * b.x, a.y + s * b.y, a.z + s * b.z};
}
gc::Vec3d unit(const gc::Vec3d& a) {
  const double l = std::sqrt(dot(a, a));
  return {a.x / l, a.y / l, a.z / l};
}

// THE DISC as the eye sees it, centred at (cx, cy) with radius `radius` px, the zenith up: the
// lit part by Lommel-Seeliger's law over the albedo, the earthlit part beside it and the Earth's
// shadow on it, each against the disc's brightest point and its faint parts as the eye sees
// them (the earthshine's ratio to the power 0.35, the shadow's 0.5, as render/sky.wgsl draws
// them). `gain` is the display's
// white.
void draw_disc(Picture& pic, double cx, double cy, double radius, const gc::MoonSky& m,
               double gain) {
  const gc::Vec3d t = m.toward;
  // The sky's up and right at the moon.
  const gc::Vec3d up = unit(axpy({0, 1, 0}, -t.y, t));
  const gc::Vec3d side = {libm::cos(m.azimuth), 0, -libm::sin(m.azimuth)};
  const gc::Vec3d right = unit(axpy(side, -dot(side, t), t));
  const double toward[3] = {t.x, t.y, t.z}, shadow[3] = {m.shadow.x, m.shadow.y, m.shadow.z};
  const gr::Eclipse e = gr::eclipse_of(toward, m.angular_radius, shadow, m.umbra, m.penumbra);
  const double surge = gr::surface_surge(m.phase_angle);
  const double earthlit =
      libm::pow(gr::kEarthshine * (1 - m.lit) / (surge * std::max(e.peak, 1e-9)), 0.35);
  const int x0 = static_cast<int>(cx - radius - 1), x1 = static_cast<int>(cx + radius + 1);
  const int y0 = static_cast<int>(cy - radius - 1), y1 = static_cast<int>(cy + radius + 1);
  for (int py = y0; py <= y1; ++py) {
    for (int px = x0; px <= x1; ++px) {
      const double u = (px + 0.5 - cx) / radius, v = (cy - (py + 0.5)) / radius;
      const double r2 = u * u + v * v;
      if (r2 >= 1) continue;
      const double mu = std::sqrt(1 - r2);
      const gc::Vec3d n = axpy(axpy({-t.x * mu, -t.y * mu, -t.z * mu}, u, right), v, up);
      const double lat = libm::asin(std::clamp(dot(n, m.pole), -1.0, 1.0)) / kDeg;
      const double lon = libm::atan2(dot(n, m.east), dot(n, m.prime)) / kDeg;
      const double albedo = gr::albedo_at(lon, lat);
      const double mu0 = dot(n, m.sun);
      const double lit = mu0 > 0 ? 2 * mu0 / (mu0 + mu) : 0;
      // The direction to this point of the disc, and the shadow there.
      const gc::Vec3d d =
          unit(axpy(axpy(t, u * m.angular_radius, right), v * m.angular_radius, up));
      const double s = libm::acos(std::clamp(dot(d, m.shadow), -1.0, 1.0));
      const gr::ShadowLight sl = gr::shadow_light(s, m.umbra, m.penumbra);
      // Its brightness as seen against the disc's brightest point, its colour as it is.
      const double level = std::max(sl.sun + sl.air, 1e-30);
      const double seen = libm::pow(level / std::max(e.peak, 1e-9), 0.5);
      double rgb[3];
      for (int k = 0; k < 3; ++k) {
        const double colour = (sl.sun + sl.air * kUmbraTint[k]) / level;
        rgb[k] = gain * albedo * (lit * colour * seen + earthlit);
      }
      const double edge = std::clamp((1 - std::sqrt(r2)) * radius, 0.0, 1.0);
      const size_t at =
          (static_cast<size_t>(py) * static_cast<size_t>(pic.width) + static_cast<size_t>(px)) * 4;
      if (px < 0 || py < 0 || px >= pic.width || py >= pic.height) continue;
      for (int k = 0; k < 3; ++k) {
        const double under = pic.rgba[at + static_cast<size_t>(k)];
        pic.rgba[at + static_cast<size_t>(k)] =
            static_cast<uint8_t>(std::lround(under * (1 - edge) + byte(rgb[k]) * edge));
      }
    }
  }
}

// The UT Julian day the moon stands highest on the night starting on `jd0`'s date at `lon`.
double highest(double jd0, double lat, double lon) {
  double best = jd0, top = -10;
  for (int k = 0; k < 96; ++k) {
    const double jd = jd0 + 0.5 - lon / 360.0 + k / 96.0 - 0.25;
    const double el = gc::moon_sky(jd, lat, lon).elevation;
    if (el > top) {
      top = el;
      best = jd;
    }
  }
  return best;
}

std::string generated_file(const std::string& table, double scale) {
  std::string s =
      "// GENERATED by the `moon` lab (`make lab LAB=moon ARGS=\"--bake=<png> --write\"`): never\n"
      "// edit by hand. The moon's albedo map render/moon.hpp decodes: " +
      std::to_string(gr::kMoonMapWidth) + " x " + std::to_string(gr::kMoonMapHeight) +
      " bytes of linear\n"
      "// luminance over the brightest texel, base64, and the scale to the near side's mean.\n"
      "// Its source is the colour map of the lunar orbiter's wide-angle camera mosaic (its poles\n"
      "// from the orbiter's laser altimeter's reflectance) as NASA's Scientific Visualization\n"
      "// Studio publishes it in the public domain (its CGI Moon Kit, 2019's 2048 x 1024 map).\n"
      "// One literal a line, so no literal passes a compiler's length limit.\n"
      "// agents:allow-large-file: a generated data table (half a megabyte of texels).\n"
      "//\n"
      "// ge:summary The moon's albedo table (generated)\n"
      "// ge:keywords moon albedo moon-map generated table\n"
      "// ge:skill render\n"
      "// ge:test tests/render_moon_test.cpp\n"
      "namespace ge::render {\n\n"
      "extern const double kMoonTableScale;\n"
      "const double kMoonTableScale = " +
      fixed(scale, 9) +
      ";\n\n"
      "extern const char* const kMoonTable[];\n"
      "const char* const kMoonTable[] = {\n";
  constexpr size_t kLine = 92;
  for (size_t i = 0; i < table.size(); i += kLine) {
    s += "    \"" + table.substr(i, kLine) + "\",\n";
  }
  s += "    nullptr,\n};\n\n}  // namespace ge::render\n";
  return s;
}

void bake(ge::lab::Lab& lab, const std::string& path) {
  const auto png = ge::lab::read_png(path);
  if (!png) {
    lab.fail("cannot read a PNG at " + path,
             "Pass --bake= a PNG of the moon's colour map (convert the published TIFF with any "
             "image tool). Skill: render.");
    return;
  }
  std::vector<double> lin(static_cast<size_t>(png->width) * static_cast<size_t>(png->height));
  for (size_t i = 0; i < lin.size(); ++i) {
    const uint8_t* p = &png->rgba[i * 4];
    lin[i] = gr::linear_luminance(p[0] / 255.0, p[1] / 255.0, p[2] / 255.0);
  }
  std::vector<uint8_t> bytes;
  double scale = 0;
  if (!gr::bake_moon_map(lin, png->width, png->height, bytes, scale)) {
    lab.fail("the map is " + std::to_string(png->width) + " x " + std::to_string(png->height) +
                 "; it must be twice as wide as high and a multiple of " +
                 std::to_string(gr::kMoonMapHeight) + " high",
             "Resize the map, or use the 2048 x 1024 one. Skill: render.");
    return;
  }
  lab.metric("bake.scale", scale, "");
  const std::string table = gr::encode_moon_map(bytes);
  std::vector<uint8_t> back;
  if (!gr::decode_moon_map(table, back) || back != bytes) {
    lab.fail("the baked table does not decode to the map it was made from",
             "Check encode_moon_map and decode_moon_map in engine/render/moon.cpp. Skill: render.");
    return;
  }
  if (lab.arg_bool("write")) {
    const std::filesystem::path out = "engine/render/moon_data.cpp";
    if (!std::filesystem::exists(out.parent_path())) {
      lab.fail("--write must run from the repository's root",
               "cd to the checkout and run the lab again. Skill: labs.");
      return;
    }
    std::ofstream f(out, std::ios::binary);
    f << generated_file(table, scale);
    lab.note("wrote " + out.string());
  }
}

void shot_map(ge::lab::Lab& lab, int width) {
  const int w = width, h = width / 2;
  Picture pic = ge::lab::create_picture(w, h, {0, 0, 0, 255});
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const double lon = ((x + 0.5) / w - 0.5) * 360, lat = (0.5 - (y + 0.5) / h) * 180;
      const uint8_t g = byte(0.32 * gr::albedo_at(lon, lat));
      ge::lab::set_pixel(pic, x, y, {g, g, g, 255});
    }
  }
  for (int k = 1; k < 4; ++k) {
    ge::lab::draw_line(pic, w * k / 4.0, 0, w * k / 4.0, h, {90, 60, 40, 255});
  }
  lab.shot({"map",
            "THE MOON'S ALBEDO MAP",
            "render/moon.hpp's map over the whole sphere: selenographic longitude -180 to 180 "
            "east left to right (a line every 90 degrees, the near side between the outer "
            "two), latitude 90 N at the top; linear albedo drawn at a third of the display's "
            "white.",
            "the near side's dark seas in the middle (the round sea of crises at 60 E 17 N "
            "right of centre, the ocean of storms to the left), the rayed crater at 11 W 43 S, "
            "the far side bright and nearly sea-less at the edges",
            {{"width", std::to_string(w)}}},
           pic, Det::measured);
}

void shot_phases(ge::lab::Lab& lab, int width) {
  const int cell = width / 4, w = cell * 4, h = cell * 2 + 24;
  Picture pic = ge::lab::create_picture(w, h, {4, 6, 14, 255});
  // From the new moon of 18 January 2026 (19:52 UT): a thin crescent, a crescent, the first
  // quarter, a gibbous moon, the full, a gibbous, the last quarter, a crescent.
  constexpr double kAges[] = {2.5, 5.0, 7.4, 11.0, 14.8, 18.5, 22.1, 26.0};
  const double new_moon = gc::julian_day(2026, 1, 18) + 19.87 / 24;
  for (int i = 0; i < 8; ++i) {
    const double day = new_moon + kAges[i];
    const double jd = highest(std::floor(day - 0.5) + 0.5, kLat, kLon);
    const gc::MoonSky m = gc::moon_sky(jd, kLat, kLon);
    const int row = i / 4;  // the grid's row: four discs a row
    const double cx = (i % 4 + 0.5) * cell, cy = row * (cell + 12) + cell * 0.5 + 4;
    draw_disc(pic, cx, cy, cell * 0.42, m, 0.30);
    const std::string label =
        fixed(m.lit * 100, 0) + "% LIT, " + fixed(m.elevation / kDeg, 0) + " DEG UP";
    ge::lab::draw_text(pic, cx - ge::lab::text_width(label) / 2.0, cy + cell * 0.45, label,
                       {170, 180, 200, 255});
    lab.metric("phase-" + std::to_string(i) + ".lit", m.lit * 100, "%");
    lab.metric("phase-" + std::to_string(i) + ".libration-lon", m.libration_longitude, "deg");
    lab.metric("phase-" + std::to_string(i) + ".libration-lat", m.libration_latitude, "deg");
  }
  lab.shot({"phases",
            "A LUNAR MONTH FROM 46 N",
            "From the new moon of 18 January 2026, eight discs 2.5 to 26 days past it, each at "
            "the hour it stands highest over 46 N 7 E that day, the zenith up: the lit part by "
            "Lommel-Seeliger's law over the albedo map, the earthlit part as the eye sees it "
            "beside the lit, the face turned by core/moon.hpp's pole and prime meridian.",
            "the bright limb toward the sun (the right in the evening's waxing moon, the left in "
            "the morning's waning one, seen from the north); the dark seas in the same places "
            "disc to disc, rocked a few degrees by the libration; a faint ashen dark part in the "
            "crescents",
            {{"latitude", fixed(kLat, 1)}, {"longitude", fixed(kLon, 1)}, {"year", "2026"}}},
           pic, Det::measured);
}

void shot_year(ge::lab::Lab& lab, int width) {
  const int w = width, h = width / 2, pad = 34;
  Picture pic = ge::lab::create_picture(w, h, {8, 10, 20, 255});
  const double plot_w = w - 2 * pad, plot_h = h - 2 * pad;
  auto px = [&](double day) { return pad + day / 365 * plot_w; };
  auto py = [&](double deg) { return pad + (1 - (deg + 10) / 90) * plot_h; };
  for (int deg = 0; deg <= 80; deg += 20) {
    ge::lab::draw_line(pic, pad, py(deg), w - pad, py(deg), {30, 34, 52, 255});
    ge::lab::draw_text(pic, 4, py(deg) - 4, std::to_string(deg), {120, 126, 150, 255});
  }
  const char* months = "JFMAMJJASOND";
  for (int m = 0; m < 12; ++m) {
    const double x = px(m * 365.0 / 12);
    ge::lab::draw_line(pic, x, pad, x, h - pad, {24, 28, 44, 255});
    ge::lab::draw_text(pic, x + plot_w / 24 - 3, h - pad + 8, std::string(1, months[m]),
                       {120, 126, 150, 255});
  }
  double winter_full = 0, summer_full = 90;
  const double jan1 = gc::julian_day(2026, 1, 1);
  const double dec = 23.44;
  for (int d = 0; d < 365; ++d) {
    // The noon sun, and the moon's highest that night (the sun 6 degrees under or more).
    const double sun_noon = 90 - kLat + dec * libm::sin(2 * kPi * (d - 80) / 365.25);
    ge::lab::fill_disc(pic, px(d), py(sun_noon), 1.2, {200, 150, 60, 255});
    double top = -90, lit = 0;
    for (int k = 0; k < 48; ++k) {
      const double hour = 12 + k * 0.5;
      const double jd0 = jan1 + d;
      const gc::DatedSunPlace sun = gc::sun_on_date(hour, kLat, kLon, jd0);
      if (sun.true_elevation > -6 * kDeg) continue;
      const gc::MoonSky m = gc::moon_on_date(hour, kLat, kLon, jd0);
      if (m.elevation / kDeg > top) {
        top = m.elevation / kDeg;
        lit = m.lit;
      }
    }
    if (top < -5) continue;
    const auto g = static_cast<uint8_t>(60 + 195 * lit);
    ge::lab::fill_disc(pic, px(d), py(top), 1.0 + 2.2 * lit,
                       {g, g, static_cast<uint8_t>(g / 2 + 128), 255});
    if (lit > 0.98) {
      if (d < 45 || d > 330) winter_full = std::max(winter_full, top);
      if (d > 150 && d < 220) summer_full = std::min(summer_full, top);
    }
  }
  lab.metric("year.winter-full-moon-height", winter_full, "deg");
  lab.metric("year.summer-full-moon-height", summer_full, "deg");
  if (winter_full < summer_full + 20) {
    lab.fail("the winter's full moon rides " + fixed(winter_full, 1) +
                 " degrees up and the summer's " + fixed(summer_full, 1) +
                 "; the winter's should be far higher",
             "Check core/moon.hpp's place (equatorial_to_world, the hour's UT). Skill: render.");
  }
  ge::lab::draw_text(pic, pad, 8,
                     "MOON'S HIGHEST EACH NIGHT (DOT SIZE: LIT SHARE); NOON SUN (ORANGE)",
                     {200, 206, 226, 255});
  lab.shot({"year",
            "THE MOON'S HEIGHT OVER 2026 AT 46 N",
            "Each night of 2026 at 46 N 7 E, the moon's greatest height (degrees) while the sun is "
            "6 degrees or more under, its dot bigger and whiter the more of it is lit; the noon "
            "sun's height in orange.",
            "the full moons (the big white dots) high in winter, near 70 degrees in December and "
            "January, and low in summer, near 15 in June: the full moon stands opposite the sun, "
            "and 2026 is near the moon's major standstill, when its path swings widest; each "
            "month a sweep from crescents low to the full moon's height and back",
            {{"latitude", fixed(kLat, 1)}, {"year", "2026"}}},
           pic, Det::measured);
}

void shot_paths(ge::lab::Lab& lab, int width) {
  const int w = width, h = width / 2, pad = 34;
  Picture pic = ge::lab::create_picture(w, h, {8, 10, 20, 255});
  const double plot_w = w - 2 * pad, plot_h = h - 2 * pad;
  // Azimuth 45 (north-east) .. 315 (north-west) across, height 0..75 up.
  auto px = [&](double az) { return pad + (az - 45) / 270 * plot_w; };
  auto py = [&](double el) { return pad + (1 - el / 75) * plot_h; };
  for (int az = 90; az <= 270; az += 45) {
    ge::lab::draw_line(pic, px(az), pad, px(az), h - pad, {30, 34, 52, 255});
  }
  ge::lab::draw_text(pic, px(90) - 3, h - pad + 8, "E", {140, 146, 170, 255});
  ge::lab::draw_text(pic, px(180) - 3, h - pad + 8, "S", {140, 146, 170, 255});
  ge::lab::draw_text(pic, px(270) - 3, h - pad + 8, "W", {140, 146, 170, 255});
  ge::lab::draw_line(pic, pad, py(0), w - pad, py(0), {80, 84, 100, 255});
  // Each full moon: the night the lit share peaks, every 10 minutes from 12 h before to after.
  const double jan1 = gc::julian_day(2026, 1, 1);
  int full = 0;
  double prev = 0, prev2 = 0;
  for (int d = 0; d < 368; ++d) {
    const double lit = gc::moon_sky(jan1 + d, kLat, kLon).lit;
    if (d >= 2 && prev > prev2 && prev >= lit && prev > 0.97) {
      const double peak = jan1 + d - 1;
      const double t = (d - 1) / 365.0;
      const Rgba c = {static_cast<uint8_t>(90 + 165 * libm::sin(kPi * t)),
                      static_cast<uint8_t>(140 + 60 * libm::sin(kPi * t)),
                      static_cast<uint8_t>(255 - 175 * libm::sin(kPi * t)), 255};
      double lx = -1, ly = -1;
      for (int k = 0; k <= 24 * 6 * 2; ++k) {
        const gc::MoonSky m = gc::moon_sky(peak - 1 + k / 144.0 - kLon / 360.0, kLat, kLon);
        const double el = m.apparent_elevation / kDeg;
        if (el < 0) {
          lx = -1;
          continue;
        }
        const double x = px(m.azimuth / kDeg), y = py(el);
        if (lx >= 0) ge::lab::draw_line(pic, lx, ly, x, y, c);
        lx = x;
        ly = y;
      }
      ++full;
    }
    prev2 = prev;
    prev = lit;
  }
  lab.metric("paths.full-moons", full, "count");
  ge::lab::draw_text(pic, pad, 8, "FULL MOONS OF 2026: JANUARY BLUE, JULY ORANGE, DECEMBER BLUE",
                     {200, 206, 226, 255});
  lab.shot({"paths",
            "EVERY FULL MOON OF 2026 ACROSS THE SKY AT 46 N",
            "Each full moon of 2026 from 46 N 7 E across the sky from its rising to its setting "
            "two days round its fullness, azimuth from north-east (left) to north-west (right) "
            "and height up to 75 degrees, coloured from blue in January through orange in July.",
            "the winter's arcs rising far in the north-east, high in the south, the summer's "
            "low and short in the south-east to south-west",
            {{"latitude", fixed(kLat, 1)}, {"year", "2026"}}},
           pic, Det::measured);
}

void shot_eclipse(ge::lab::Lab& lab, int width) {
  const int cell = width / 4, w = cell * 4, h = cell * 2 + 24;
  Picture pic = ge::lab::create_picture(w, h, {4, 6, 14, 255});
  constexpr double kHours[] = {9.6, 10.25, 10.75, 11.15, 11.55, 12.1, 12.7, 13.3};
  const double day = gc::julian_day(2026, 3, 3);
  double darkest = 1;
  for (int i = 0; i < 8; ++i) {
    const gc::MoonSky m = gc::moon_sky(day + kHours[i] / 24, 20.0, -155.0);
    const int row = i / 4;  // the grid's row: four discs a row
    const double cx = (i % 4 + 0.5) * cell, cy = row * (cell + 12) + cell * 0.5 + 4;
    draw_disc(pic, cx, cy, cell * 0.42, m, 0.30);
    const double toward[3] = {m.toward.x, m.toward.y, m.toward.z};
    const double shadow[3] = {m.shadow.x, m.shadow.y, m.shadow.z};
    const gr::Eclipse e = gr::eclipse_of(toward, m.angular_radius, shadow, m.umbra, m.penumbra);
    darkest = std::min(darkest, e.mean);
    const int hh = static_cast<int>(kHours[i]);
    const int mm = static_cast<int>(std::lround((kHours[i] - hh) * 60));
    char clock[32];
    std::snprintf(clock, sizeof clock, "%02d:%02d UT, LIGHT %s%%", hh, mm,
                  fixed(e.mean * 100, e.mean < 0.01 ? 2 : 0).c_str());
    ge::lab::draw_text(pic, cx - ge::lab::text_width(clock) / 2.0, cy + cell * 0.45, clock,
                       {170, 180, 200, 255});
  }
  lab.metric("eclipse.darkest", darkest * 100, "%", Det::measured);
  if (darkest > 0.01) {
    lab.fail("the eclipse's darkest moment leaves " + fixed(darkest * 100, 2) +
                 "% of the moon's light; a total eclipse leaves under 1%",
             "Check core/moon.hpp's shadow and render/night.hpp's shadow_light. Skill: render.");
  }
  lab.shot({"eclipse",
            "THE TOTAL ECLIPSE OF 3 MARCH 2026",
            "The moon seen from 20 N 155 W through the eclipse, eight moments in UT, the zenith "
            "up, each disc against its own brightest point as the eye sees a faint part beside "
            "a bright one; under each, its light against a full moon's.",
            "the penumbra's faint dimming on one limb first, the umbra's dark bite moving across, "
            "the whole disc a dim copper red in totality (11:04 to 12:03), then the bite leaving "
            "by the other limb",
            {{"latitude", "20"}, {"longitude", "-155"}, {"date", "2026-03-03"}}},
           pic, Det::measured);
}

void run(ge::lab::Lab& lab) {
  if (!lab.arg("bake").empty()) {
    bake(lab, lab.arg("bake"));
    return;
  }
  // The map: the seas darker than the highlands, the near side's mean the scale's.
  const double sea = gr::albedo_at(31.4, 8.5);         // the sea of tranquillity
  const double highland = gr::albedo_at(10.0, -25.0);  // the southern highlands
  lab.metric("map.scale", gr::moon_albedo_scale(), "");
  lab.metric("map.sea", sea, "");
  lab.metric("map.highland", highland, "");
  if (highland < 1.4 * sea) {
    lab.fail("the highlands are only " + fixed(highland / sea, 2) +
                 " times as bright as the seas; on the real moon nearly twice",
             "Bake the map again from the published colour map. Skill: render.");
  }
  const int width = static_cast<int>(lab.arg_num("width"));
  shot_map(lab, width);
  shot_phases(lab, width);
  shot_year(lab, width);
  shot_paths(lab, width);
  shot_eclipse(lab, width);
}

const ge::lab::LabSpec kSpec{
    "moon",
    ge::lab::LabKind::picture,
    "engine",
    "core/moon.hpp, render/moon.hpp, render/night.hpp: the real moon's place, phase, face, "
    "shadow and albedo map, and the map's bake",
    "render",
    "Is the moon the real one: its seas where they are, its phases and libration through a "
    "month, its height through a year, an eclipse in its shadow?",
    {
        {"width", "1024", "a picture's width, px"},
        {"bake", "", "bake the albedo table off this PNG of the moon's map instead of reading"},
        {"write", "", "with --bake: write render/moon_data.cpp (from the repository's root)", true},
    },
    "--width=256",
    run,
};
GE_LAB(kSpec);

}  // namespace
