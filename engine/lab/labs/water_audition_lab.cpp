// LAB `water-audition`: everything the water sounds like (engine/water/sound.hpp), rendered
// offline and metered, for a session with no speakers. Five scenes:
//
//   bank      every one-shot in the water's bank alone, held to the band a bank lives in
//   splashes  a pebble to a boulder going in, and a body coming out: bigger is darker and
//             longer, faster is louder, each with the bubbles it takes down
//   places    the bed at an ear by the water, four seconds of it with its bubbles: a calm and a
//             fresh lake from its shore, from a boat and from under it, a gale on a beach, a
//             slow river and a mountain stream from their banks, a waterfall from 15 and 60 m
//             and from under its pool, and nowhere near water; the checks a listener would make
//             fail here (the fresh shore over the calm, the beach over the lake, the far fall
//             quieter and darker, under darker than over, silence away from the water)
//   distance  the waterfall's roar against the distance, beside 1 / r
//   swim      a swimmer on the fresh lake: in, four strokes with a breath every other, under
//             for two with bubbles, the gasp coming up, the bed muffled while his ears are under
//
// ge:summary Lab: the water's bank, splashes, places, the roar by distance and a swim, metered
// ge:keywords lab water audition sound splash bubble lapping surf river rush waterfall roar
// ge:keywords under-water muffle swim stroke breath gasp meter wav
// ge:skill water audio labs
// ge:test tests/water_sound_test.cpp
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/meter.hpp"
#include "audio/sound.hpp"
#include "audio/synth.hpp"
#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/plot.hpp"
#include "lab/sound.hpp"
#include "water/flow.hpp"
#include "water/sound.hpp"
#include "water/world.hpp"

namespace {

namespace a = ge::audio;
namespace gl = ge::lab;
namespace gw = ge::water;
namespace lm = ge::core::libm;
using ge::lab::Det;

constexpr double kPi = 3.14159265358979323846;
constexpr int kFps = 60;
// A bank's band and a bed's, dBFS of the loudest 10 ms. The bed's floor is a calm lake and a
// slow river from their banks: quiet, as they are, and heard only on a still day.
constexpr float kBankQuietest = -40, kBankLoudest = -16;
constexpr float kBedQuietest = -58, kBedLoudest = -12;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<float> render(a::Synth& s, double seconds) {
  std::vector<float> out(2 * static_cast<size_t>(seconds * a::kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  return out;
}

// A round lake 300 m across round the origin, its surface at 0 and its bed shelving to a beach
// all round; the wind from the north (+z), so its waves run onto the south shore.
double beach(const void*, double x, double z) {
  const double r = std::sqrt(x * x + z * z);
  return -8 * (1 - (r / 150) * (r / 150));
}
gw::World lake(double wind) {
  ge::wind::Field field;
  field.mean = wind;
  field.from = 0;
  gw::World world(field);
  std::vector<gw::Point> shore;
  for (int i = 0; i < 64; ++i) {
    const double t = 2 * kPi * i / 64;
    shore.push_back({150 * lm::cos(t), 150 * lm::sin(t)});
  }
  gw::add_lake(world, shore, 0, {}, {beach, nullptr});
  return world;
}

// The sea on a beach at z = 0 shelving 1 in 20 out to the south, the wind onshore.
double shelf(const void*, double, double z) { return 0.05 * z; }
gw::World sea(double wind) {
  ge::wind::Field field;
  field.mean = wind;
  field.from = kPi;
  gw::World world(field);
  gw::add_ocean(world, 0, {.look = "adriatic", .density = 1025}, {shelf, nullptr});
  return world;
}

// A straight channel down +z with its surface at 0.
gw::World river(double discharge, double width, double depth) {
  gw::World world;
  std::vector<gw::ChannelPoint> line;
  for (int i = 0; i <= 40; ++i) line.push_back({0, -200 + 10.0 * i, 0, width, depth});
  gw::add_river(world, line, discharge);
  return world;
}

// A shelf 12 m over a pool 3 m deep (a weir at its far end holds it), 3 m^3/s poured onto the
// shelf and over its lip at x = 16 m, run for 30 s until the sheet is steady.
gw::World falls() {
  gw::World world;
  gw::Flow f = gw::make_flow({.nx = 48, .nz = 16, .cell = 1, .open = gw::kEast});
  gw::set_bed(f, 0, 0, 16, 16, 12);
  gw::set_bed(f, 16, 0, 44, 16, 0);
  gw::set_bed(f, 44, 0, 48, 16, 2.6);
  gw::fill_to(f, 16, 0, 44, 16, 3);
  f.sources.push_back({2, 8, 3});
  gw::add_flow(world, std::move(f));
  for (int i = 0; i < 30 * 30; ++i) gw::step_flows(world, 1.0 / 30);
  return world;
}

const gw::Fall* loudest_fall(const gw::World& world) {
  const gw::Fall* best = nullptr;
  for (const gw::Flow& f : world.flows) {
    for (const gw::Fall& fall : f.falls) {
      if (!best || fall.discharge > best->discharge) best = &fall;
    }
  }
  return best;
}

void bank(gl::Lab& lab) {
  for (const a::SoundDef& def : gw::kWaterBank) {
    const std::string id(def.id);
    a::Synth s;
    a::play_def(s, def);
    const a::Meter m = gl::sound_shot(
        lab,
        {.id = "bank-" + id,
         .title = "WATER BANK: " + id,
         .caption = std::string(def.description),
         .look_for = "the sentence in the picture; the loudest 10 ms between " +
                     fmt(kBankQuietest, 0) + " and " + fmt(kBankLoudest, 0) + " dBFS",
         .inputs = {{"shape", "as authored"}}},
        render(s, 1.5));
    if (m.loudest_db < kBankQuietest || m.loudest_db > kBankLoudest) {
      lab.fail(id + " peaks at " + fmt(m.loudest_db, 1) + " dBFS, outside the bank's band",
               "Retune its voices' volumes in engine/water/sound.cpp's bank. Skill: audio.");
    }
  }
}

void splashes(gl::Lab& lab) {
  struct Case {
    const char* id;
    const char* what;
    gw::Splash splash;
  };
  const Case cases[] = {
      {"pebble", "a pebble dropped in at 4 m/s (2 cm)", {.speed = 4, .size = 0.02}},
      {"stone", "a stone thrown in at 7 m/s (6 cm)", {.speed = 7, .size = 0.06}},
      {"ball", "the bank's reference: a ball at 5 m/s (15 cm)", {.speed = 5, .size = 0.15}},
      {"crate", "a crate dropped in at 6 m/s (30 cm)", {.speed = 6, .size = 0.3}},
      {"skier", "a skier going in at 9 m/s (50 cm)", {.speed = 9, .size = 0.5}},
      {"boulder", "a boulder off a cliff at 14 m/s (1 m)", {.speed = 14, .size = 1}},
      {"out", "a body climbing out at 2 m/s (30 cm)", {.speed = 2, .size = 0.3, .entering = false}},
  };
  std::vector<a::Meter> m;
  for (const Case& c : cases) {
    a::Synth s;
    ge::core::Rng rng = ge::core::make_rng(7);
    gw::play_splash(s, rng, c.splash);
    m.push_back(gl::sound_shot(
        lab,
        {.id = std::string("splash-") + c.id,
         .title = std::string("SPLASH: ") + c.id,
         .caption = std::string(c.what) + ": the crack, the whump, the plop, the drops falling "
                                          "back and the bubbles ringing at their own pitches",
         .look_for = "a sharp onset; a bigger body darker (the bands' weight lower) and longer",
         .inputs = {{"speed", fmt(c.splash.speed, 1) + " m/s"},
                    {"size", fmt(c.splash.size, 2) + " m"}}},
        render(s, 2.5)));
  }
  if (!(m[0].centroid_hz > m[3].centroid_hz && m[3].centroid_hz > m[5].centroid_hz)) {
    lab.fail("the splashes do not darken with size (pebble " + fmt(m[0].centroid_hz, 0) +
                 " Hz, crate " + fmt(m[3].centroid_hz, 0) + ", boulder " +
                 fmt(m[5].centroid_hz, 0) + ")",
             "Check splash_shape's pitch in engine/water/sound.cpp. Skill: water.");
  }
  if (!(m[5].length_s > m[0].length_s)) {
    lab.fail("the boulder's splash rings no longer than the pebble's",
             "Check splash_shape's stretch in engine/water/sound.cpp. Skill: water.");
  }
}

// One place: the bed heard at an ear for `seconds`, metered after its first second.
struct Place {
  std::string id, what;
  const gw::World* world;
  double x, y, z, heading;
};

a::Meter listen(gl::Lab& lab, const Place& p, double seconds, gw::Hearing& heard) {
  a::Synth s;
  gw::WaterBed bed = gw::make_water_bed(0, 5);
  std::vector<float> out;
  const int frames = static_cast<int>(seconds * kFps);
  for (int i = 0; i < frames; ++i) {
    const double t = 10 + static_cast<double>(i) / kFps;
    heard = gw::water_bed_update(bed, s, *p.world, p.x, p.y, p.z, t, p.heading, 1.0f / kFps);
    std::vector<float> chunk = render(s, 1.0 / kFps);
    if (i >= kFps) out.insert(out.end(), chunk.begin(), chunk.end());
  }
  lab.metric(p.id + ".lap", heard.lap.level, "share");
  lab.metric(p.id + ".surf", heard.surf.level, "share");
  lab.metric(p.id + ".rush", heard.rush.level, "share");
  lab.metric(p.id + ".roar", heard.roar.level, "share");
  lab.metric(p.id + ".under", heard.under, "share");
  return gl::sound_shot(
      lab,
      {.id = "place-" + p.id,
       .title = "BY THE WATER: " + p.id,
       .caption = p.what + ". Heard: lap " + fmt(heard.lap.level, 2) + ", surf " +
                  fmt(heard.surf.level, 2) + ", rush " + fmt(heard.rush.level, 2) + ", roar " +
                  fmt(heard.roar.level, 2) + (heard.under > 0 ? ", under water" : ""),
       .look_for = "a steady bed (no clicks, no gaps); the lapping's level moving with the waves "
                   "past the shore; under water the weight in the lowest bands",
       .inputs = {{"ear", fmt(p.x, 0) + ", " + fmt(p.y, 1) + ", " + fmt(p.z, 0) + " m"},
                  {"seconds", fmt(seconds - 1, 1)}}},
      out);
}

void places(gl::Lab& lab, double seconds) {
  const gw::World calm = lake(1), fresh = lake(10), gale = sea(14);
  const gw::World slow = river(6, 10, 1.6), stream = river(6, 6, 0.5);
  const gw::World fall = falls();
  const gw::Fall* f = loudest_fall(fall);
  if (!f) {
    lab.fail("no sheet leaves the shelf after 30 s of 3 m3/s",
             "Check the falls world's bed and source in water_audition_lab.cpp. Skill: water.");
    return;
  }
  const double pool = f->land_y;
  const std::vector<Place> list = {
      {"lake-calm", "a calm lake (1 m/s of wind) from its south shore", &calm, 0, 1.7, -152, 0},
      {"lake-fresh", "the lake in a fresh breeze (10 m/s) from the shore it blows onto", &fresh, 0,
       1.7, -152, 0},
      {"lake-boat", "the fresh lake from a boat 30 m out", &fresh, 0, 1.0, -120, 0},
      {"lake-under", "the fresh lake with the ear 1.5 m under, 30 m out", &fresh, 0, -1.5, -120, 0},
      {"sea-gale", "a gale (14 m/s) onto a beach, from 3 m up the sand", &gale, 0, 1.7, 3, kPi},
      {"river-slow", "a slow river (6 m3/s, 10 m wide, 1.6 m deep) from 3 m off its bank", &slow, 8,
       1.7, 0, 0},
      {"stream", "a mountain stream (6 m3/s, 6 m wide, 0.5 m deep) from 3 m off its bank", &stream,
       6, 1.7, 0, 0},
      {"falls-15", "a 9 m waterfall of 3 m3/s from 15 m off its foot", &fall, f->land_x, pool + 1.7,
       f->land_z + 15, kPi},
      {"falls-60", "the waterfall from 60 m", &fall, f->land_x, pool + 1.7, f->land_z + 60, kPi},
      {"falls-under", "under the waterfall's pool, the ear 1.5 m down, 8 m from where it lands",
       &fall, f->land_x + 8, pool - 1.5, f->land_z, 0},
      {"nowhere", "500 m from the lake", &fresh, 500, 1.7, 500, 0},
  };
  std::vector<a::Meter> m;
  for (const Place& p : list) {
    gw::Hearing h;
    m.push_back(listen(lab, p, seconds, h));
  }
  const auto at = [&](const char* id) -> const a::Meter& {
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i].id == id) return m[i];
    return m.back();
  };
  for (size_t i = 0; i + 1 < list.size(); ++i) {
    if (m[i].loudest_db < kBedQuietest || m[i].loudest_db > kBedLoudest) {
      lab.fail(list[i].id + " is " + fmt(m[i].loudest_db, 1) + " dBFS, outside the bed's band",
               "Retune the layer's gain or its kFull* in engine/water/sound.*. Skill: audio.");
    }
  }
  const auto order = [&](const char* loud, const char* quiet, float by, const char* next) {
    if (at(loud).loudest_db < at(quiet).loudest_db + by) {
      lab.fail(std::string(loud) + " (" + fmt(at(loud).loudest_db, 1) + " dBFS) is not " +
                   fmt(by, 0) + " dB over " + quiet + " (" + fmt(at(quiet).loudest_db, 1) + ")",
               std::string(next) + " Skill: water.");
    }
  };
  order("lake-fresh", "lake-calm", 6, "Check hear's lapping intensity off Sample::hs.");
  order("sea-gale", "lake-fresh", 3, "Check the surf and the lapping's kFull*.");
  order("stream", "river-slow", 6, "Check the rush's c_f |v|^3.");
  order("falls-15", "falls-60", 6, "Check hear's 1 / r^2 for a sheet.");
  if (!(at("falls-60").centroid_hz < at("falls-15").centroid_hz)) {
    lab.fail("the far waterfall is no darker than the near one",
             "Check water_targets' air (the highs taken with distance). Skill: water.");
  }
  if (!(at("lake-under").centroid_hz < 0.7 * at("lake-boat").centroid_hz)) {
    lab.fail("under the lake is not darker than over it (" + fmt(at("lake-under").centroid_hz, 0) +
                 " Hz under, " + fmt(at("lake-boat").centroid_hz, 0) + " over)",
             "Check water_targets' muffle under water. Skill: water.");
  }
  if (at("nowhere").loudest_db > -90) {
    lab.fail("the bed is heard 500 m from any water", "Check hear's rings. Skill: water.");
  }
}

void distance(gl::Lab& lab) {
  const gw::World fall = falls();
  const gw::Fall* f = loudest_fall(fall);
  if (!f) return;
  // Against the distance to the landing (the ear's own, 1.7 m up), with 1 / r drawn through
  // the level heard at 100 m.
  gl::Trace heard{"the roar heard", {}, {}}, law{"1 / r through 100 m", {}, {}};
  double far_level = 0, far_r = 0;
  for (int i = 0; i <= 40; ++i) {
    const double d = 4 + 5.0 * i;
    const gw::Hearing h = gw::hear(fall, f->land_x, f->land_y + 1.7, f->land_z + d, 0, kPi);
    heard.x.push_back(h.roar.distance);
    heard.y.push_back(h.roar.level);
    if (i == 19) {
      far_level = h.roar.level;
      far_r = h.roar.distance;
    }
  }
  for (const double r : heard.x) {
    law.x.push_back(r);
    law.y.push_back(far_level * far_r / r);
  }
  gl::LineChart lc;
  lc.frame = {"THE WATERFALL'S ROAR BY DISTANCE", "a 9 m fall of 3 m3/s, the ear 1.7 m up",
              "distance to where it lands (m)", "level (share of full)"};
  lc.traces = {heard, law};
  lc.spikes = false;
  const gl::Chart c = gl::line_chart(lc);
  const std::string svg = gl::to_svg(c.plot);
  lab.attach("falls-distance.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({"falls-distance",
            c.title,
            "The roar's level as the ear walks away from the foot of the fall: an intensity "
            "falling as 1 / r^2 is a level falling as 1 / r.",
            "the two lines one: the level halves as the distance doubles",
            {}},
           gl::to_picture(c.plot));
  lab.metric("falls.roar-at-60-over-15", heard.y[11] / heard.y[2], "ratio");
}

// A swimmer's seat: his ears in the water, the bed a little under his own strokes and breath.
constexpr gw::WaterMix kSwimmersSeat{.level = 0.7f, .tone = 1};

void swim(gl::Lab& lab) {
  const gw::World world = lake(10);
  a::Synth s;
  gw::WaterBed bed = gw::make_water_bed(0, 11);
  gw::SwimState st;
  st.level = 2;
  st.v = {0, 0, 0.9};
  std::vector<float> out;
  const double seconds = 8;
  for (int i = 0; i < static_cast<int>(seconds * kFps); ++i) {
    const double t = static_cast<double>(i) / kFps;
    const bool down = t > 4.8 && t < 7.2;
    st.level = down ? 3 : 2;
    const double ear = down ? -0.8 : 0.1;
    gw::SwimEvents ev;
    if (i == 6) {
      ev.entered = true;
      ev.splash_speed = 3;
    }
    // On whole frames: a stroke every 66 (1.1 s) from frame 36, the gasp on frame 432 (7.2 s).
    if (i >= 36 && (i - 36) % 66 == 0) ev.stroke = true;
    if (i == 432) ev.gasp = true;
    const gw::Hearing h =
        gw::water_bed_update(bed, s, world, 0, ear, -120, 10 + t, 0, 1.0f / kFps, kSwimmersSeat);
    gw::play_swim(s, bed, ev, st, h.under);
    std::vector<float> chunk = render(s, 1.0 / kFps);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }
  gl::sound_shot(lab,
                 {.id = "swim",
                  .title = "A SWIM ON THE FRESH LAKE",
                  .caption = "in at 0.1 s; strokes every 1.1 s, a breath every other; under "
                             "from 4.8 to 7.2 s (the strokes let bubbles go, the bed muffled); "
                             "the gasp coming up",
                  .look_for = "the strokes' even pulses; the dip and the darker bands while "
                              "under; the gasp's spike at 7.2 s",
                  .inputs = {{"wind", "10 m/s"}}},
                 out);
}

void run(gl::Lab& lab) {
  const std::string only = lab.arg("scene");
  const double seconds = lab.arg_num("seconds");
  if (only == "all" || only == "bank") bank(lab);
  if (only == "all" || only == "splashes") splashes(lab);
  if (only == "all" || only == "places") places(lab, seconds);
  if (only == "all" || only == "distance") distance(lab);
  if (only == "all" || only == "swim") swim(lab);
}

const gl::LabSpec kSpec{
    "water-audition",
    gl::LabKind::sim,
    "engine",
    "engine/water/sound (the water's bed, splashes, bubbles, a swimmer)",
    "water",
    "What does the water sound like, by a shore, a river, a waterfall and under it, and does it "
    "follow the water's own numbers?",
    {
        {"scene", "all", "all, bank, splashes, places, distance or swim"},
        {"seconds", "4", "seconds each place is heard (the first settles the layers)"},
    },
    "--seconds=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
