// LAB `rain-audition`: what the rain sounds like (engine/water/rain.hpp), rendered offline and
// metered, for a session with no speakers. Two scenes:
//
//   places  the rain bed and its single drops at an ear, a few seconds each: a drizzle, a light
//           rain and a downpour in the open; a light rain and a downpour by a lake; the
//           downpour under a roof; a blizzard's snow on a gale and the same snow in still air.
//           The checks a listener would make fail here (the downpour far over the drizzle, the
//           lake's light rain high, the roof's drum darker than the open, the still snow silent)
//   rate    each voice's level against the rain's rate, 0.75 to 150 mm/h
//
// ge:summary Lab: the rain's sound by its rate and place, a blizzard's hiss, metered
// ge:keywords lab rain audition sound patter wash drum drops-on-water driven-snow blizzard
// ge:keywords meter wav rate
// ge:skill water audio labs
// ge:test tests/water_rain_test.cpp
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/meter.hpp"
#include "audio/synth.hpp"
#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/plot.hpp"
#include "lab/sound.hpp"
#include "water/rain.hpp"
#include "wind/precip.hpp"

namespace {

namespace a = ge::audio;
namespace gl = ge::lab;
namespace gw = ge::water;
namespace lm = ge::core::libm;
using ge::lab::Det;

constexpr int kFps = 60;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

// The intensity on the precip dial that rains `rate` mm/h (precip.hpp: 0.64 * 232.6^i).
double dial_for(double rate) { return lm::log(rate / 0.64) / lm::log(232.6); }

struct Place {
  std::string id, what;
  ge::wind::Precip fall;
  double wind;
  gw::RainPlace place;
};

// `seconds` of the bed at a place, metered after its first second.
std::vector<float> listen(const Place& p, double seconds, gw::RainHearing& heard, int& singles) {
  a::Synth s;
  gw::RainBed bed = gw::make_rain_bed(0, 5);
  heard = gw::hear_rain(p.fall, p.wind, p.place);
  std::vector<float> out;
  singles = 0;
  const int frames = static_cast<int>(seconds * kFps);
  for (int i = 0; i < frames; ++i) {
    a::rack_apply(bed.rack, s, gw::rain_targets(heard, {}));
    singles += gw::rain_drops(s, bed.rng, heard, 1.0f / kFps, bed.rack.bus);
    std::vector<float> chunk(2 * static_cast<size_t>(a::kRate / kFps));
    s.render(chunk.data(), static_cast<int>(chunk.size() / 2));
    if (i >= kFps) out.insert(out.end(), chunk.begin(), chunk.end());
  }
  return out;
}

void places(gl::Lab& lab, double seconds) {
  const auto rain = [](double rate) { return ge::wind::Precip{dial_for(rate), 1}; };
  const std::vector<Place> list = {
      {"drizzle",
       "a drizzle (0.75 mm/h, the lightest the dial rains) on open ground",
       rain(0.75),
       2,
       {}},
      {"light", "a light rain (2.5 mm/h) on open ground", rain(2.5), 3, {}},
      {"downpour", "a downpour (50 mm/h) on open ground", rain(50), 5, {}},
      {"lake-light",
       "a light rain (2.5 mm/h) on a lake, from a jetty",
       rain(2.5),
       3,
       {.open = 1, .water = 0.9}},
      {"lake-downpour",
       "a downpour (50 mm/h) on the lake, from the jetty",
       rain(50),
       5,
       {.open = 1, .water = 0.9}},
      {"roof", "the downpour from under a roof", rain(50), 5, {.open = 0, .water = 0, .roof = 1}},
      {"blizzard", "a blizzard: heavy snow (0.85 on the dial) on a 22 m/s gale", {0.85, 0}, 22, {}},
      {"snow-still", "the same snow in a 2 m/s air", {0.85, 0}, 2, {}},
  };
  std::vector<a::Meter> meters;
  for (const Place& p : list) {
    gw::RainHearing heard;
    int singles = 0;
    const std::vector<float> out = listen(p, seconds, heard, singles);
    lab.metric(p.id + ".patter", heard.patter, "share");
    lab.metric(p.id + ".water", heard.water, "share");
    lab.metric(p.id + ".drum", heard.drum, "share");
    lab.metric(p.id + ".driven", heard.driven, "share");
    lab.metric(p.id + ".singles", singles, "count");
    meters.push_back(gl::sound_shot(
        lab,
        {.id = "rain-" + p.id,
         .title = "THE RAIN: " + p.id,
         .caption = p.what + ". Heard: patter " + fmt(heard.patter, 2) + ", wash " +
                    fmt(heard.wash, 2) + ", on water " + fmt(heard.water, 2) + ", drum " +
                    fmt(heard.drum, 2) + ", driven snow " + fmt(heard.driven, 2) + "; " +
                    std::to_string(singles) + " drops heard one by one",
         .look_for = "a steady bed with no clicks or gaps; a drizzle's single drops ticking over a "
                     "near silence; on the lake the weight in the top bands; under the roof the "
                     "weight low; the still snow silent",
         .inputs = {{"rate", fmt(ge::wind::rate_of(p.fall), 1) + " mm/h"},
                    {"wind", fmt(p.wind, 0) + " m/s"}}},
        out));
  }
  const auto at = [&](const std::string& id) {
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i].id == id) return meters[i];
    return a::Meter{};
  };
  lab.metric("downpour-over-drizzle", at("downpour").rms_db - at("drizzle").rms_db, "dB");
  if (!(at("downpour").rms_db > at("drizzle").rms_db + 20)) {
    lab.fail("the downpour is not 20 dB over the drizzle (" + fmt(at("downpour").rms_db, 1) +
                 " against " + fmt(at("drizzle").rms_db, 1) + " dBFS)",
             "Check hear_rain's levels by rain_power (engine/water/rain.cpp). Skill: water.");
  }
  if (!(at("lake-light").centroid_hz > 1.3 * at("light").centroid_hz)) {
    lab.fail("the light rain on the lake is not brighter than on the ground (" +
                 fmt(at("lake-light").centroid_hz, 0) + " Hz against " +
                 fmt(at("light").centroid_hz, 0) + ")",
             "Check rain_targets' drops on water. Skill: water.");
  }
  if (!(at("roof").centroid_hz < at("downpour").centroid_hz)) {
    lab.fail("the roof's drum is not darker than the open downpour",
             "Check rain_targets' drum. Skill: water.");
  }
  if (at("snow-still").loudest_db > -80) {
    lab.fail("snow in still air is heard (" + fmt(at("snow-still").loudest_db, 1) + " dBFS)",
             "Check hear_rain's driven snow by the wind. Skill: water.");
  }
  if (!(at("blizzard").rms_db > at("snow-still").rms_db + 30)) {
    lab.fail("the blizzard is not 30 dB over still snow (" + fmt(at("blizzard").rms_db, 1) +
                 " against " + fmt(at("snow-still").rms_db, 1) + " dBFS)",
             "Check hear_rain's driven snow by the wind. Skill: water.");
  }
}

void rate(gl::Lab& lab) {
  gl::Trace patter{"patter (open ground)", {}, {}}, drum{"drum (under a roof)", {}, {}};
  gl::Trace power{"root of the rain's power over a downpour's", {}, {}};
  gl::LabTable table{"The rain heard by its rate",
                     {"rate mm/h", "median drop mm", "drops >1 mm /m2/s", "power W/m2", "patter"},
                     {}};
  for (const double r : {0.75, 1.0, 2.5, 5.0, 10.0, 25.0, 50.0, 100.0, 150.0}) {
    const ge::wind::Precip p{dial_for(r), 1};
    const gw::RainHearing open = gw::hear_rain(p, 3, {});
    const gw::RainHearing roof = gw::hear_rain(p, 3, {.open = 0, .water = 0, .roof = 1});
    patter.x.push_back(lm::log(r) / lm::log(10.0));
    patter.y.push_back(open.patter);
    drum.x.push_back(lm::log(r) / lm::log(10.0));
    drum.y.push_back(roof.drum);
    power.x.push_back(lm::log(r) / lm::log(10.0));
    power.y.push_back(std::sqrt(gw::rain_power(r) / gw::rain_power(gw::kFullRain)));
    table.rows.push_back({fmt(r, 1), fmt(ge::wind::median_drop(r), 2),
                          fmt(ge::wind::drops_per_m2s(r, 1), 0), fmt(gw::rain_power(r), 4),
                          fmt(open.patter, 3)});
    lab.metric("rate." + fmt(r, 2) + ".patter", open.patter, "share");
  }
  lab.table(table);
  gl::LineChart lc;
  lc.frame = {"THE RAIN HEARD BY ITS RATE", "each voice's level, the root of its power",
              "rain rate (log10 mm/h)", "level (share of a downpour's)"};
  lc.traces = {patter, drum, power};
  lc.rules = {1.0};
  lc.spikes = false;
  const gl::Chart c = gl::line_chart(lc);
  const std::string svg = gl::to_svg(c.plot);
  lab.attach("rain-rate.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot(
      {"rain-rate",
       c.title,
       "Each voice's level against the rate, from a drizzle to a cloudburst: the root of the "
       "kinetic energy the drops bring down (1/2 rho R v^2) over a downpour's (50 mm/h).",
       "every line rising with the rate, held at 1.5; the patter full (1) at 50 mm/h, the roof's "
       "drum a fifth louder; a drizzle near nothing",
       {}},
      gl::to_picture(c.plot));
}

void run(gl::Lab& lab) {
  const std::string only = lab.arg("scene");
  const double seconds = lab.arg_num("seconds");
  if (only == "all" || only == "places") places(lab, seconds);
  if (only == "all" || only == "rate") rate(lab);
}

const gl::LabSpec kSpec{
    "rain-audition",
    gl::LabKind::sim,
    "engine",
    "engine/water/rain (the rain's bed by its rate, its single drops, the driven snow)",
    "water",
    "What does the rain sound like by its rate, in the open, on a lake and under a roof, and a "
    "storm's snow on the wind?",
    {
        {"scene", "all", "all, places or rate"},
        {"seconds", "4", "seconds each place is heard (the first settles the layers)"},
    },
    "--seconds=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
