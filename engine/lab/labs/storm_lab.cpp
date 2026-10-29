// LAB `storm`: the engine's thunderstorms (engine/wind/storm, lightning, thunder) read the way a
// game reads them, as numbers, pictures and sound: a cell's life cycle; a multicell storm's
// lightning minute by minute; one ground flash's light over milliseconds and what 60 Hz frames
// catch of it; the storm at night from the side (a ground strike under a lit base, a flash inside
// the cloud seen as sheet lightning, a bolt from the blue); a flash's frames as a filmstrip; the
// storm from above as radar sees it, with its gust fronts and strikes; thunder's envelope and
// cut-off at three distances, with the sound of each; and a table of the flashes against the
// observed bands. The night pictures light the cloud with the diffusion law the renderer's
// clouds use (docs/storms.md § The picture).
//
// ge:summary Lab: storm life cycle, lightning bands, flicker, night strikes, radar, thunder WAVs
// ge:keywords lab storm thunderstorm lightning flash flicker filmstrip strike sheet-lightning
// ge:keywords bolt-from-the-blue radar gust-front thunder rumble wav
// ge:skill water labs
// ge:test tests/wind_storm_test.cpp tests/wind_lightning_test.cpp
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "lab/sound.hpp"
#include "wind/lightning.hpp"
#include "wind/storm.hpp"
#include "wind/thunder.hpp"

namespace {

namespace gl = ge::lab;
namespace gw = ge::wind;
namespace lm = ge::core::libm;

std::string fixed(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void chart_shot(gl::Lab& lab, const std::string& id, const gl::Chart& chart,
                const std::string& caption, const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, {{"seed", lab.arg("seed")}}},
           gl::to_picture(chart.plot));
}

gw::Storm single(double peak) {
  gw::Storm s;
  s.vx = 8;
  s.cells = 1;
  s.cell[0].peak = peak;
  s.cell[0].core = 1800 + 1800 * peak;
  s.cell[0].seed = 1234;
  return s;
}

std::vector<gw::Flash> every_flash(const gw::Storm& s) {
  std::vector<gw::Flash> out;
  for (int k = 0; k < s.cells; ++k) {
    const double end = s.cell[k].born + gw::life_of(s.cell[k].peak);
    for (auto n = static_cast<int64_t>(s.cell[k].born); n < static_cast<int64_t>(end); ++n) {
      gw::Flash f;
      if (gw::flash_in_slot(s, k, n, f)) out.push_back(f);
    }
  }
  return out;
}

// ---- charts -------------------------------------------------------------------------------

void life(gl::Lab& lab) {
  gl::LineChart line;
  line.frame = {"A CELL'S LIFE: ORDINARY (0.5) AND SEVERE (1)",
                "updraft m/s, cloud top km x2, rain dial x20, flashes a minute", "t (min)",
                "value"};
  line.spikes = false;
  for (const double peak : {0.5, 1.0}) {
    const gw::Storm s = single(peak);
    const std::string tag = peak < 1 ? "ordinary " : "severe ";
    gl::Trace up{tag + "updraft", {}, {}}, top{tag + "top x2", {}, {}},
        rain{tag + "rain x20", {}, {}}, fl{tag + "flashes/min", {}, {}};
    for (int step = 0; step * 20 <= gw::life_of(1); ++step) {
      const double t = step * 20.0;
      const gw::CellAt c = gw::cell_at(s, 0, t);
      const double m = t / 60;
      up.x.push_back(m), up.y.push_back(c.updraft);
      top.x.push_back(m), top.y.push_back(c.top / 500);
      rain.x.push_back(m), rain.y.push_back(c.rain * 20);
      fl.x.push_back(m), fl.y.push_back(c.flashes);
    }
    line.traces.insert(line.traces.end(), {up, top, rain, fl});
  }
  const double t2 = gw::kTowering + gw::mature_of(0.5);
  line.bands.push_back({0, gw::kTowering / 60, "TOWERING"});
  line.bands.push_back({gw::kTowering / 60, t2 / 60, "MATURE (0.5)"});
  line.bands.push_back({t2 / 60, gw::life_of(0.5) / 60, "DISSIPATING (0.5)"});
  chart_shot(lab, "life", gl::line_chart(line),
             "One cell's life against the clock. The updraft builds through the towering "
             "(cumulus) stage while the top climbs; rain and lightning start late in it, peak "
             "while mature, and the updraft dies first; the rain lingers on as the anvil's light "
             "fall. The severe cell is taller, rains harder and flashes several times as often.",
             "updraft leads; rain and flashes lag it; the severe flash rate towers over the "
             "ordinary one; nothing after the cell's end");
  for (const double peak : {0.5, 1.0}) {
    const gw::CellAt c = gw::cell_at(single(peak), 0, gw::kTowering + 400);
    const std::string key = peak < 1 ? "ordinary" : "severe";
    lab.metric("mature." + key + ".flashes-per-min", c.flashes, "1/min");
    lab.metric("mature." + key + ".top", c.top, "m");
  }
}

void minutes(gl::Lab& lab, const gw::Storm& s) {
  const auto flashes = every_flash(s);
  const int span = 120;
  std::vector<double> cloud(span, 0), ground(span, 0);
  for (const gw::Flash& f : flashes) {
    const auto m = static_cast<int>(f.t0 / 60);
    if (m < 0 || m >= span) continue;
    (f.kind == gw::FlashKind::cloud ? cloud : ground)[m] += 1;
  }
  gl::LineChart line;
  line.frame = {"A MULTICELL STORM'S LIGHTNING, MINUTE BY MINUTE",
                std::to_string(s.cells) + " cells, a new one every 10 to 15 minutes", "t (min)",
                "flashes a minute"};
  line.spikes = false;
  gl::Trace c{"in the cloud", {}, {}}, g{"to the ground", {}, {}};
  for (int m = 0; m < span; ++m) {
    c.x.push_back(m), c.y.push_back(cloud[m]);
    g.x.push_back(m), g.y.push_back(ground[m]);
  }
  line.traces = {c, g};
  for (int k = 0; k < s.cells; ++k) {
    line.marks.push_back({s.cell[k].born / 60, "CELL " + std::to_string(k + 1)});
  }
  chart_shot(lab, "minutes", gl::line_chart(line),
             "Every flash of a storm whose cells keep rising for an hour, counted a minute at a "
             "time. Each cell's lightning starts some ten minutes after it rises (the marks), so "
             "the storm pulses as cells take over from one another; most flashes stay in the "
             "cloud.",
             "pulses that follow the cells; cloud flashes above ground flashes throughout; the "
             "storm dying away after the last cell");
  lab.metric("storm.flashes", static_cast<double>(flashes.size()), "flashes");
}

const gw::Flash* pick(const std::vector<gw::Flash>& flashes, gw::FlashKind kind, int strokes,
                      bool held, bool blue) {
  for (const gw::Flash& f : flashes) {
    if (f.kind != kind || f.strokes < strokes || f.blue != blue) continue;
    if (f.t0 < gw::kTowering + 300) continue;  // a mature cell, its anvil spread
    bool has = false;
    for (int i = 0; i < f.strokes; ++i) has = has || f.stroke[i].hold > 0;
    if (held && !has) continue;
    return &f;
  }
  return nullptr;
}

void flicker(gl::Lab& lab, const gw::Flash& f) {
  gl::LineChart line;
  line.frame = {"ONE GROUND FLASH'S LIGHT, AND WHAT 60 HZ FRAMES CATCH OF IT",
                std::to_string(f.strokes) + " return strokes, a continuing current",
                "t (ms after the leader starts)", "power (log)"};
  line.spikes = false;
  line.log_y = true;
  gl::Trace fine{"light, 0.25 ms", {}, {}}, frames{"60 Hz frames", {}, {}};
  for (int step = 0; step * 0.00025 < f.length + 0.05; ++step) {
    const double u = step * 0.00025;
    fine.x.push_back(u * 1000);
    fine.y.push_back(gw::light_over(f, f.t0 + u, f.t0 + u + 0.00025) + 1e-4);
  }
  for (int frame = 0; frame / 60.0 < f.length + 0.05; ++frame) {
    const double u = frame / 60.0;
    const double p = gw::light_over(f, f.t0 + u, f.t0 + u + 1.0 / 60) + 1e-4;
    frames.x.push_back(u * 1000), frames.y.push_back(p);
    frames.x.push_back((u + 1.0 / 60) * 1000), frames.y.push_back(p);
  }
  line.traces = {fine, frames};
  for (int i = 0; i < f.strokes; ++i) {
    line.marks.push_back({f.stroke[i].t * 1000, i == 0 ? "FIRST STROKE" : ""});
  }
  chart_shot(lab, "flicker", gl::line_chart(line),
             "A ground flash's light at a quarter of a millisecond: the faint stepped leader, then "
             "each return stroke's spike decaying in about 2 ms, tens of milliseconds apart, and "
             "a continuing current's steady glow after one of them. The steps are what a 60 Hz "
             "frame gets: each frame's mean, integrated in closed form, so no stroke falls "
             "between frames and the flash flickers on screen as it does to the eye.",
             "every spike inside a lit frame step; frames between strokes dimmer; the "
             "continuing current a plateau");
}

void thunder(gl::Lab& lab, const gw::Flash& f, double seconds) {
  auto ch = std::make_unique<gw::Channel>();
  gw::build_channel(f, *ch);
  gl::LineChart line;
  line.frame = {"THUNDER FROM ONE STRIKE AT 1, 4 AND 12 KM",
                "intensity in dB (50 ms means) against the time since the flash", "t (s)", "dB"};
  line.spikes = false;
  gl::LabTable table{"Thunder by distance",
                     {"listener", "first sound s", "length s", "peak dB", "mean cut-off Hz"},
                     {}};
  for (const double km : {1.0, 4.0, 12.0}) {
    auto th = std::make_unique<gw::Thunder>();
    gw::hear_thunder(f, *ch, f.ex + km * 1000, f.ey, f.ez, gw::sound_speed(15), *th);
    gl::Trace tr{fixed(km, 0) + " km", {}, {}};
    double cut = 0, w = 0;
    for (int b = 0; b < th->bins; ++b) {
      cut += th->cutoff[b] * th->loud[b];
      w += th->loud[b];
    }
    // Drawn in 50 ms windows (the ear's own smoothing is longer still), each its mean.
    for (int b = 0; b < th->bins; b += 5) {
      double sum = 0;
      for (int i = b; i < b + 5 && i < th->bins; ++i) sum += th->loud[i];
      tr.x.push_back(th->first + (b + 2.5) * gw::kThunderBin);
      tr.y.push_back(10 * lm::log(sum / 5 + 1e-6) / lm::log(10.0));
    }
    line.traces.push_back(tr);
    const double peak_db = 10 * lm::log(th->peak + 1e-6) / lm::log(10.0);
    table.rows.push_back({fixed(km, 0) + " km", fixed(th->first, 2), fixed(th->length, 2),
                          fixed(peak_db, 1), fixed(w > 0 ? cut / w : 0, 0)});
    lab.metric("thunder." + fixed(km, 0) + "km.first", th->first, "s");
    lab.metric("thunder." + fixed(km, 0) + "km.length", th->length, "s");
    // The sound from its first arrival, one gain for all three so the distances compare.
    const auto n = static_cast<size_t>(seconds * 48000);
    std::vector<float> mono(n), stereo(2 * n);
    gw::render_thunder(*th, 48000, 77, 0.8, mono);
    for (size_t i = 0; i < n; ++i) stereo[2 * i] = stereo[2 * i + 1] = mono[i];
    gl::sound_shot(lab,
                   {"thunder-" + fixed(km, 0) + "km",
                    "THUNDER AT " + fixed(km, 0) + " KM, FROM ITS FIRST SOUND",
                    "The strike's thunder heard " + fixed(km, 0) +
                        " km off, starting at its first sound (" + fixed(th->first, 1) +
                        " s after the flash): the nearest channel's clap, then the rumble of the "
                        "rest arriving from farther along it.",
                    "near: a sharp crack then a long rumble; far: a low, soft rumble with "
                    "the crack gone",
                    {{"distance", fixed(km, 0) + " km"}}},
                   stereo);
  }
  chart_shot(lab, "thunder", gl::line_chart(line),
             "The same strike's thunder envelope heard at three distances. Each stretch of channel "
             "is heard at its distance over the speed of sound, so the thunder lasts as long as "
             "the channel's spread in distance; the bends and branches make the lumps. Farther "
             "off it starts later, is far quieter and loses its highs.",
             "1 km starting near 3 s, 12 km near 35 s; each farther line lower");
  lab.table(table);
}

void bands(gl::Lab& lab, const std::vector<gw::Flash>& flashes) {
  int ground = 0, single = 0, strokes = 0, held = 0, gaps = 0, blue = 0;
  double log_gap = 0, length = 0;
  for (const gw::Flash& f : flashes) {
    length += f.length;
    if (f.kind != gw::FlashKind::ground) continue;
    ++ground;
    strokes += f.strokes;
    single += f.strokes == 1 ? 1 : 0;
    blue += f.blue ? 1 : 0;
    bool has = false;
    for (int i = 0; i < f.strokes; ++i) {
      has = has || f.stroke[i].hold > 0;
      if (i > 0) {
        log_gap += lm::log(f.stroke[i].t - f.stroke[i - 1].t);
        ++gaps;
      }
    }
    held += has ? 1 : 0;
  }
  const double all = static_cast<double>(flashes.size()), g = ground;
  const double cloud = 1 - g / all, mean = strokes / g, one = single / g,
               gm = lm::exp(log_gap / gaps) * 1000, cc = held / g;
  gl::LabTable table{"A severe cell's flashes against the observed bands",
                     {"quantity", "the engine", "observed"},
                     {{"flashes", fixed(all, 0), ""},
                      {"in the cloud", fixed(cloud * 100, 0) + " %", "70 to 90 %"},
                      {"strokes a ground flash", fixed(mean, 2), "3 to 5"},
                      {"single-stroke ground flashes", fixed(one * 100, 0) + " %", "15 to 30 %"},
                      {"between strokes (geometric mean)", fixed(gm, 0) + " ms", "about 60 ms"},
                      {"with a continuing current", fixed(cc * 100, 0) + " %", "30 to 50 %"},
                      {"bolts from the blue", fixed(blue / g * 100, 1) + " %", "a few %"},
                      {"mean flash", fixed(length / all * 1000, 0) + " ms", "0.2 to 1 s"}}};
  lab.table(table);
  lab.metric("bands.in-cloud", cloud, "share");
  lab.metric("bands.strokes", mean, "strokes");
  lab.metric("bands.interstroke", gm, "ms");
  auto band = [&](const char* what, double v, double lo, double hi) {
    if (v >= lo && v <= hi) return;
    lab.fail(std::string("the storm's ") + what + " is " + fixed(v, 3) + ", outside " +
                 fixed(lo, 3) + " to " + fixed(hi, 3),
             "Read the stroke draws in engine/wind/lightning.cpp (flash_in_slot) against "
             "docs/storms.md, then rerun `make lab LAB=storm` (skill: water).");
  };
  band("in-cloud share", cloud, 0.7, 0.92);
  band("strokes a ground flash", mean, 3, 5);
  band("interstroke mean (ms)", gm, 45, 80);
}

// ---- the night pictures -------------------------------------------------------------------

struct Hdr {
  int w, h;
  std::vector<float> rgb;
  void add(int x, int y, float r, float g, float b) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    float* p = &rgb[(static_cast<size_t>(y) * w + x) * 3];
    p[0] += r, p[1] += g, p[2] += b;
  }
};

// The cell's cloud from the side, 0 .. 1: a tower of overlapping puffs under a flat base, and the
// anvil's flattened puffs spread downwind (+x) under the top. Puffs are hashed off the cell's seed.
struct Puffs {
  int n = 0;
  double x[160], y[160], r[160], squash[160];
};

Puffs puffs_of(const gw::Storm& s, const gw::CellAt& c, int k) {
  Puffs p;
  const double core = s.cell[k].core, span = c.top - s.base;
  const uint32_t seed = s.cell[k].seed;
  for (int i = 0; i < 34; ++i) {
    const double h = i / 33.0;
    p.x[p.n] = c.x + core * 0.35 * (gw::storm_hash(seed, 40, i) - 0.5) + 900 * h;
    p.y[p.n] = s.base + span * 0.88 * h + 300;
    p.r[p.n] = core * (0.45 + 0.3 * gw::storm_hash(seed, 41, i)) * (0.8 + 0.4 * h);
    p.squash[p.n++] = 1;
  }
  for (int i = 0; i < 26; ++i) {
    const double u = i / 25.0;
    p.x[p.n] = c.x - 1.3 * core + 6.5 * core * u + 600 * (gw::storm_hash(seed, 42, i) - 0.5);
    p.y[p.n] = c.top - 700 - 500 * u * gw::storm_hash(seed, 43, i);
    p.r[p.n] = (2300 - 1000 * u) * (0.8 + 0.4 * gw::storm_hash(seed, 44, i));
    p.squash[p.n++] = 2.1;
  }
  // The cauliflower: small turrets round the tower's edges and its overshooting top.
  for (int i = 0; i < 90; ++i) {
    const double h = gw::storm_hash(seed, 45, i);
    const double side = gw::storm_hash(seed, 46, i) < 0.5 ? -1 : 1;
    const double width = core * (0.55 + 0.35 * h);
    p.x[p.n] = c.x + 900 * h + side * width * (0.75 + 0.3 * gw::storm_hash(seed, 47, i));
    p.y[p.n] = s.base + span * 0.85 * h + 400;
    p.r[p.n] = core * (0.12 + 0.18 * gw::storm_hash(seed, 48, i));
    p.squash[p.n++] = 1;
  }
  for (int i = 0; i < 8; ++i) {
    p.x[p.n] = c.x + 900 + core * 0.5 * (gw::storm_hash(seed, 49, i) - 0.5);
    p.y[p.n] = c.top - 200 + 500 * gw::storm_hash(seed, 50, i);
    p.r[p.n] = core * (0.2 + 0.15 * gw::storm_hash(seed, 51, i));
    p.squash[p.n++] = 1;
  }
  return p;
}

double cloud_at(const Puffs& p, double base, double x, double y) {
  if (y < base) return 0;
  double d = 0;
  for (int i = 0; i < p.n; ++i) {
    const double dx = x - p.x[i], dy = (y - p.y[i]) * p.squash[i];
    const double q = (dx * dx + dy * dy) / (p.r[i] * p.r[i]);
    if (q < 1) d += (1 - q) * (1 - q);
  }
  const double flat = (y - base) / 150;  // the base's soft underside
  const double v = d * (flat < 1 ? flat : 1) * 1.6;
  return v > 1 ? 1 : v;
}

// The storm at night from the side over [x0, x0 + span] m, lit by the light `f` gave over [a, b):
// the cloud and the rain shaft by every stretch of the channel near them (diffusing farther in
// the cloud than in the clear air), the channel itself a core and a halo, dimmed to a glow where
// it runs inside the cloud.
gl::Picture night(const gw::Storm& s, const gw::Flash& f, const gw::Channel& ch, double a, double b,
                  double x0, double span, int w, int h, bool show_rain) {
  const gw::CellAt c = gw::cell_at(s, f.cell, f.t0);
  const Puffs puffs = puffs_of(s, c, f.cell);
  const double m_px = span / w;
  const double power = gw::light_over(f, a, b);
  Hdr img{w, h, std::vector<float>(static_cast<size_t>(w) * h * 3, 0)};
  std::vector<float> dens(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      dens[static_cast<size_t>(y) * w + x] =
          static_cast<float>(cloud_at(puffs, s.base, x0 + x * m_px, (h - 1 - y) * m_px));
    }
  }
  // Up to 160 lights along the channel, the branches lighting only on the first stroke's frame.
  struct Lamp {
    double x, y, e;
  };
  std::vector<Lamp> lamps;
  const int every = ch.count / 160 + 1;
  for (int i = 0; i < ch.count; i += every) {
    const gw::Segment& g = ch.seg[i];
    lamps.push_back(
        {0.5 * (g.x0 + g.x1), 0.5 * (g.y0 + g.y1), static_cast<double>(g.glow) * every});
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const double wx = x0 + x * m_px, wy = (h - 1 - y) * m_px;
      const double cl = dens[static_cast<size_t>(y) * w + x];
      double rain = 0;
      if (show_rain && wy < s.base)
        rain = gw::rain_at(s, wx, c.z, f.t0) * (0.6 + 0.4 * wy / s.base);
      const double ground = wy < 3 * m_px ? 1 : 0;
      const double reach = cl > 0.2 ? 1400 : 600;
      double sum = 0;
      for (const Lamp& l : lamps) {
        const double dx = wx - l.x, dy = wy - l.y;
        sum += l.e / (1 + (dx * dx + dy * dy) / (reach * reach));
      }
      const double lit = power * sum * 0.02;
      const double scatter = cl + 1.4 * rain + 0.8 * ground + 0.05;
      const double amb = 0.006 + 0.02 * cl + 0.01 * rain;
      img.add(x, y, static_cast<float>(amb * 0.6 + lit * scatter * 0.8),
              static_cast<float>(amb * 0.65 + lit * scatter * 0.8),
              static_cast<float>(amb * 0.9 + lit * scatter * 0.98));
    }
  }
  for (int i = 0; i < ch.count; ++i) {
    const gw::Segment& g = ch.seg[i];
    const double x0p = (g.x0 - x0) / m_px, y0p = h - 1 - g.y0 / m_px;
    const double x1p = (g.x1 - x0) / m_px, y1p = h - 1 - g.y1 / m_px;
    const double len =
        std::fmax(1, std::sqrt((x1p - x0p) * (x1p - x0p) + (y1p - y0p) * (y1p - y0p)));
    for (int step = 0; step * 0.5 <= len; ++step) {
      const double u = step * 0.5;
      const double px = x0p + (x1p - x0p) * u / len, py = y0p + (y1p - y0p) * u / len;
      const int ix = static_cast<int>(px), iy = static_cast<int>(py);
      if (ix < 0 || iy < 0 || ix >= w || iy >= h) continue;
      const double cl = dens[static_cast<size_t>(iy) * w + ix];
      const double e = power * g.glow * (cl > 0.3 ? 0.03 : 1 - 2 * cl);
      if (e < 1e-3) continue;
      for (int oy = -3; oy <= 3; ++oy) {
        for (int ox = -3; ox <= 3; ++ox) {
          const double d2 = ox * ox + oy * oy;
          const double k = d2 == 0 ? 2.5 : (d2 <= 1 ? 0.6 : 0.12 / d2);
          const auto q = static_cast<float>(e * k * 0.5);
          img.add(ix + ox, iy + oy, q * 0.88f, q * 0.88f, q);
        }
      }
    }
  }
  gl::Picture p = gl::create_picture(w, h, {0, 0, 0});
  for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
    for (int k = 0; k < 3; ++k) {
      const double v = img.rgb[i * 3 + k];
      p.rgba[i * 4 + k] = static_cast<uint8_t>(std::lround(std::sqrt(v / (1 + v)) * 255));
    }
  }
  return p;
}

// The left edge of a side view `span` m across framing a flash and its cell.
double left_of(const gw::Storm& s, const gw::Flash& f, double span) {
  const gw::CellAt c = gw::cell_at(s, f.cell, f.t0);
  const double lo = std::fmin(c.x - s.cell[f.cell].core, std::fmin(f.x, f.ex));
  const double hi = std::fmax(c.x + s.cell[f.cell].core, std::fmax(f.x, f.ex));
  return 0.5 * (lo + hi) - 0.5 * span;
}

// The first stroke's frame of a flash (or, for a cloud flash, its brightest frame).
double bright_frame(const gw::Flash& f) {
  double best = 0, at = f.t0;
  for (int step = 0; step / 240.0 < f.length; ++step) {
    const double u = step / 240.0;
    const double p = gw::light_over(f, f.t0 + u, f.t0 + u + 1.0 / 60);
    if (p > best) best = p, at = f.t0 + u;
  }
  return at;
}

void nights(gl::Lab& lab, const gw::Storm& s, const std::vector<gw::Flash>& flashes, int w) {
  const int h = w * 9 / 16;
  auto ch = std::make_unique<gw::Channel>();
  struct Pick {
    const char* id;
    const char* title;
    const char* caption;
    const char* look;
    const gw::Flash* f;
    double span;  // m across
  };
  const Pick picks[] = {
      {"strike", "A GROUND STRIKE AT NIGHT, FROM THE SIDE",
       "The storm 16 km across at night, seen from the side at the frame of a ground flash's "
       "first return stroke: the main channel from the cloud's lower charge region to the ground "
       "with its branches, the cloud lit from inside where the channel leaves it, the rain shaft "
       "and the ground lit around the strike.",
       "a jagged channel, brightest at its core, branches dimmer and downward; the cloud base "
       "glowing over it; the rain shaft a lit curtain",
       pick(flashes, gw::FlashKind::ground, 3, false, false), 16000},
      {"sheet", "A FLASH INSIDE THE CLOUD: SHEET LIGHTNING",
       "A cloud flash at its brightest frame: the channel runs between the charge layers inside "
       "the tower and the anvil, so it is hardly seen; its light diffuses through the cloud and "
       "lights the tower and the anvil from within. Seen from under the cloud, that glow is "
       "sheet lightning.",
       "the cloud's body glowing softly, brightest round the hidden channel; no sharp bolt",
       pick(flashes, gw::FlashKind::cloud, 4, false, false), 34000},
      {"blue", "A BOLT FROM THE BLUE",
       "A ground flash out of the anvil's top, striking far out ahead of the storm, where the "
       "sky above the strike can be clear.",
       "a long channel from the anvil's top down to the ground well clear of the rain",
       pick(flashes, gw::FlashKind::ground, 1, false, true), 34000},
  };
  for (const Pick& p : picks) {
    if (p.f == nullptr) {
      lab.fail(std::string("no flash for the ") + p.id + " picture",
               "Widen pick() in engine/lab/labs/storm_lab.cpp, then run `make lab LAB=storm` "
               "(skill: water).");
      continue;
    }
    gw::build_channel(*p.f, *ch);
    const double at = bright_frame(*p.f);
    lab.shot({p.id,
              p.title,
              p.caption,
              p.look,
              {{"kind", gw::flash_kind_name(p.f->kind)},
               {"strokes", std::to_string(p.f->strokes)},
               {"frame", "1/60 s"}}},
             night(s, *p.f, *ch, at, at + 1.0 / 60, left_of(s, *p.f, p.span), p.span, w, h, true));
  }

  // The filmstrip: a held ground flash frame by frame at 60 Hz.
  const gw::Flash* f = pick(flashes, gw::FlashKind::ground, 4, true, false);
  if (f == nullptr) return;
  gw::build_channel(*f, *ch);
  const int cols = 5, rows = 3, fw = w / cols, fh = fw * 9 / 16;
  gl::Picture strip = gl::create_picture(fw * cols, fh * rows, {0, 0, 0});
  for (int i = 0; i < cols * rows; ++i) {
    const double a = f->t0 + i / 60.0;
    gl::Picture cell =
        night(s, *f, *ch, a, a + 1.0 / 60, left_of(s, *f, 16000), 16000, fw, fh, true);
    const std::string label = std::to_string(i * 1000 / 60) + " MS";
    gl::draw_text(cell, 4, 4, label, {200, 200, 220}, 1);
    gl::blit(strip, cell, (i % cols) * fw, (i / cols) * fh);
  }
  lab.shot({"filmstrip",
            "ONE FLASH, FRAME BY FRAME AT 60 HZ",
            "Fifteen frames of a " + std::to_string(f->strokes) +
                "-stroke ground flash with a continuing current, each lit by the light that fell "
                "in its 1/60 s: dark while the leader steps down, a full strike on each return "
                "stroke, dimmer between, and a held glow through the continuing current.",
            "bright and dark frames alternating (the flicker); a steady run of lit frames where "
            "the current holds; dark at the end",
            {{"strokes", std::to_string(f->strokes)}}},
           strip);
}

// ---- the radar view -----------------------------------------------------------------------

void plan(gl::Lab& lab, const gw::Storm& s, double t) {
  const int size = 600;
  const double m_px = 80;
  double cx = 0, cz = 0;
  int live = 0;
  for (int k = 0; k < s.cells; ++k) {
    const gw::CellAt c = gw::cell_at(s, k, t);
    if (c.stage == gw::Stage::none) continue;
    cx += c.x, cz += c.z, ++live;
  }
  if (live > 0) cx /= live, cz /= live;
  const double x0 = cx - size * m_px / 2, z0 = cz - size * m_px / 2;
  gl::Picture p = gl::create_picture(size, size, {18, 22, 30});
  for (int py = 0; py < size; py += 2) {
    for (int px = 0; px < size; px += 2) {
      const double x = x0 + px * m_px, z = z0 + (size - py) * m_px;
      const double r = gw::rain_at(s, x, z, t);
      if (r < 0.05) continue;
      gl::Rgba col = r < 0.3    ? gl::Rgba{40, 140, 60}
                     : r < 0.55 ? gl::Rgba{60, 200, 70}
                     : r < 0.7  ? gl::Rgba{230, 210, 40}
                     : r < 0.85 ? gl::Rgba{230, 90, 30}
                                : gl::Rgba{220, 40, 160};
      gl::fill_rect(p, px, py, 2, 2, col);
    }
  }
  for (int py = 20; py < size; py += 40) {
    for (int px = 20; px < size; px += 40) {
      const double x = x0 + px * m_px, z = z0 + (size - py) * m_px;
      const gw::Air a = gw::outflow_at(s, {x, 10, z, 10}, t);
      const double v = std::sqrt(a.x * a.x + a.z * a.z);
      if (v < 1) continue;
      const double l = std::fmin(18, v);
      gl::draw_line(p, px, py, px + a.x / v * l, py - a.z / v * l, {230, 230, 240});
      gl::fill_disc(p, px + a.x / v * l, py - a.z / v * l, 1.5, {230, 230, 240});
    }
  }
  std::vector<gw::Flash> fl(4096);
  const int n = gw::flashes_between(s, t - 300, t, fl.data(), 4096);
  for (int i = 0; i < n && i < 4096; ++i) {
    if (fl[i].kind != gw::FlashKind::ground) continue;
    const double px = (fl[i].ex - x0) / m_px, py = size - (fl[i].ez - z0) / m_px;
    const gl::Rgba c = fl[i].blue ? gl::Rgba{90, 220, 255} : gl::Rgba{255, 255, 120};
    gl::draw_line(p, px - 3, py, px + 3, py, c);
    gl::draw_line(p, px, py - 3, px, py + 3, c);
  }
  for (int k = 0; k < s.cells; ++k) {
    const gw::CellAt c = gw::cell_at(s, k, t);
    if (c.stage == gw::Stage::none) continue;
    const double px = (c.x - x0) / m_px, py = size - (c.z - z0) / m_px;
    if (c.front > 0 && c.outflow > 3) {
      gl::draw_ring(p, px, py, c.front / m_px, {120, 200, 255});
    }
    gl::fill_disc(p, px, py, 3, {255, 255, 255});
    gl::draw_text(p, px + 5, py - 3, gw::stage_name(c.stage), {255, 255, 255}, 1);
  }
  gl::draw_text(p, 8, 8, "48 KM, NORTH UP, T = " + fixed(t / 60, 0) + " MIN", {220, 220, 230}, 1);
  lab.shot({"radar",
            "THE STORM FROM ABOVE: RAIN, GUST FRONTS AND STRIKES",
            "The multicell storm 48 km across at minute " + fixed(t / 60, 0) +
                ", drifting east. Colours are the rain on the precipitation dial as radar shows "
                "it (green light through magenta torrential); white dots are cells with their "
                "stage; blue rings their gust fronts; arrows the outflow 10 m up; crosses the "
                "ground strikes of the last five minutes (cyan: bolts from the blue).",
            "the newest cells to the south-east of the old; strikes clustered under the cores; "
            "outflow arrows strongest just inside each ring and on its east side",
            {{"t", fixed(t, 0) + " s"}}},
           p);
}

void run(gl::Lab& lab) {
  const int width = static_cast<int>(lab.arg_num("width"));
  const double seconds = lab.arg_num("seconds");
  gw::StormSpec spec;
  spec.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  spec.renew = 3600;
  spec.severity = 0.6;
  spec.vx = 9;
  spec.vz = 3;
  const gw::Storm storm = gw::lay_storm(spec);
  const gw::Storm severe = single(1);
  const auto many = every_flash(severe);

  life(lab);
  minutes(lab, storm);
  const gw::Flash* held = pick(many, gw::FlashKind::ground, 4, true, false);
  if (held != nullptr) flicker(lab, *held);
  bands(lab, many);
  nights(lab, severe, many, width);
  plan(lab, storm, lab.arg_num("minute") * 60);
  const gw::Flash* strike = pick(many, gw::FlashKind::ground, 3, false, false);
  if (strike != nullptr) thunder(lab, *strike, seconds);
}

const gl::LabSpec kSpec{
    "storm",
    gl::LabKind::picture,
    "engine",
    "engine/wind/storm.hpp, engine/wind/lightning.hpp, engine/wind/thunder.hpp",
    "water",
    "Do the engine's thunderstorms live, flash, light the cloud, flicker, branch and thunder the "
    "way real ones do?",
    {{"seed", "38", "the multicell storm's seed"},
     {"width", "960", "the night pictures' width, px"},
     {"minute", "50", "the radar view's moment, min"},
     {"seconds", "14", "how much of each thunder is rendered, s"}},
    "--width=320 --seconds=2",
    run};
GE_LAB(kSpec);

}  // namespace
