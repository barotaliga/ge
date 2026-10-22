// LAB `audition`: everything Fall Line sounds like, rendered offline and metered, for a session
// with no speakers. Three scenes:
//
//   bank  every sound in the bank alone, its picture, its WAV and its numbers; a sound outside
//         the bank's level band, or one with no sentence, fails
//   beds  the ride bed held at a moment (stood still, a cruise, a schuss in a tuck, a carve, a
//         skid, powder, the air) until its layers settle, then a second of it; the checks a
//         listener would make fail here: the tuck is louder than the cruise, powder is darker
//         than the groomer, a skid rasps brighter than a straight run, and nothing reaches full
//         scale
//   run   the bot down seed 38 with the route and the bed live, step by step and frame by frame,
//         as the app plays it: the whole run as one WAV
//
// ge:summary Lab: Fall Line's bank, its ride bed's moments and a whole run, metered and as WAV
// ge:keywords lab audition bank bed run sound meter wav listen route ride-bed fall-line
// ge:skill audio labs
// ge:test tests/fall_line_sound_test.cpp
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "audio/meter.hpp"
#include "audio/sound.hpp"
#include "audio/synth.hpp"
#include "core/libm/libm.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/game.hpp"
#include "fall_line/sim/simulate.hpp"
#include "fall_line/sound/bank.hpp"
#include "fall_line/sound/mix.hpp"
#include "fall_line/sound/route.hpp"
#include "lab/lab.hpp"
#include "lab/sound.hpp"
#include "skier/app/ride_bed.hpp"

namespace {

using namespace ge::audio;
namespace snd = fall_line::sound;
namespace sim = fall_line::sim;

// The bank's level band, dBFS of the loudest 10 ms: under it a sound is lost under the wind,
// over it a sound shouts over everything.
constexpr float kBankQuietest = -40;
constexpr float kBankLoudest = -16;
constexpr int kFps = 60;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

void bank(ge::lab::Lab& lab) {
  for (const SoundDef& def : snd::kBank) {
    const std::string id(def.id);
    if (def.description.empty()) {
      lab.fail(id + " has no sentence",
               "next: every bank sound says what it should feel like; write its "
               "description in games/fall_line/sound/bank.cpp.");
    }
    Synth s;
    snd::apply_mix(s, {}, 0);
    play_def(s, def, {}, snd::kBusEffects);
    std::vector<float> out(2 * static_cast<size_t>(kRate) * 3 / 2);
    s.render(out.data(), static_cast<int>(out.size() / 2));
    const Meter m = ge::lab::sound_shot(
        lab,
        {.id = "bank-" + id,
         .title = "BANK: " + id,
         .caption = std::string(def.description),
         .look_for = "the sentence in the picture: a snow sound opens over a few ms (no spike at "
                     "the left), a hard thing (wood, a gate, steel) has its spike; the loudest "
                     "10 ms between " +
                     fmt(kBankQuietest, 0) + " and " + fmt(kBankLoudest, 0) + " dBFS",
         .inputs = {{"mix", "default faders"}}},
        out);
    if (m.loudest_db < kBankQuietest || m.loudest_db > kBankLoudest) {
      lab.fail(id + " peaks at " + fmt(m.loudest_db, 1) + " dBFS, outside the bank's band",
               "Retune its voices' volumes in games/fall_line/sound/bank.cpp. Skill: audio.");
    }
  }
}

// A moment the bed is held at.
struct Moment {
  const char* id;
  const char* what;
  double speed;   // m/s, straight down +z
  double tuck;    // 0..1
  double edge;    // rad
  double across;  // rad between the heading and the way he goes (a skid)
  double packed;  // the snow's packed share
  bool on_snow;
};

constexpr Moment kMoments[] = {
    {"stand", "stood still on the groomer", 0, 0, 0, 0, 1, true},
    {"cruise", "a cruise at 45 km/h stood up, flat skis on the groomer", 12.5, 0, 0.05, 0, 1, true},
    {"tuck", "a schuss at 90 km/h folded into a tuck", 25, 1, 0.02, 0, 1, true},
    {"carve", "a carve at 65 km/h on a high edge", 18, 0.3, 0.8, 0.05, 1, true},
    {"skid", "a skid at 35 km/h, the skis pivoted 60 degrees across the way", 10, 0.2, 0.5, 1.05, 1,
     true},
    {"powder", "45 km/h through deep loose snow", 12.5, 0.2, 0.1, 0, 0, true},
    {"air", "in the air at 80 km/h", 22, 0.5, 0, 0, 1, false},
};

ge::skier::SkierState skier_at(const Moment& m) {
  ge::skier::SkierState s;
  s.heading = 0;
  s.vx = m.speed * ge::core::libm::sin(m.across);
  s.vz = m.speed * ge::core::libm::cos(m.across);
  s.speed = m.speed;
  s.tuck = m.tuck;
  s.edge = m.edge;
  s.packed = m.packed;
  s.on_snow = m.on_snow;
  return s;
}

Meter hold(ge::lab::Lab& lab, const Moment& mo) {
  Synth s;
  snd::apply_mix(s, {}, 0);
  ge::skier::RideBed bed = ge::skier::make_ride_bed(snd::kBusWind, snd::kBusEffects);
  const snd::Listener& ear = snd::listener_for(snd::Seat::chase);
  const ge::skier::RideHeard heard{{ear.wind, ear.tone}, {ear.snow}};
  const ge::skier::SkierState skier = skier_at(mo);
  std::vector<float> chunk(2 * static_cast<size_t>(kRate / kFps));
  std::vector<float> out;
  for (int f = 0; f < 2 * kFps; f++) {
    ge::skier::ride_bed_update(bed, s, skier, ge::skier::kReferenceSkis, 1.0f / kFps, heard);
    s.render(chunk.data(), kRate / kFps);
    // The first second settles every glide; the second is the moment.
    if (f >= kFps) out.insert(out.end(), chunk.begin(), chunk.end());
  }
  return ge::lab::sound_shot(
      lab,
      {.id = std::string("bed-") + mo.id,
       .title = std::string("THE RIDE BED: ") + mo.id,
       .caption = std::string("A second of the wind and the snow beds held at ") + mo.what +
                  ", heard from the chase seat, after a second to settle.",
       .look_for = "a steady sheet with no pumping (the envelope flat); the bands where the "
                   "moment puts them (powder low, a skid's rasp and a carve's tear high)",
       .inputs = {{"speed", fmt(mo.speed, 1) + " m/s"},
                  {"tuck", fmt(mo.tuck, 2)},
                  {"packed", fmt(mo.packed, 2)}}},
      out);
}

void beds(ge::lab::Lab& lab) {
  Meter m[std::size(kMoments)];
  for (size_t i = 0; i < std::size(kMoments); i++) {
    m[i] = hold(lab, kMoments[i]);
    if (m[i].peak_db > -0.5f) {
      lab.fail(
          std::string("bed-") + kMoments[i].id + " reaches " + fmt(m[i].peak_db, 1) + " dBFS peak",
          "A bed sits well under the limiter: take its levels down in plugins/skier/app/. "
          "Skill: audio.");
    }
  }
  const auto& cruise = m[1];
  const auto& tuck = m[2];
  const auto& skid = m[4];
  const auto& powder = m[5];
  if (tuck.loudest_db <= cruise.loudest_db) {
    lab.fail("the tuck at 90 km/h (" + fmt(tuck.loudest_db, 1) +
                 " dB) is not louder than the cruise (" + fmt(cruise.loudest_db, 1) + " dB)",
             "The wind climbs with the square of the air: read wind_targets. Skill: audio.");
  }
  if (powder.centroid_hz >= cruise.centroid_hz) {
    lab.fail("powder (" + fmt(powder.centroid_hz, 0) + " Hz) is not darker than the groomer (" +
                 fmt(cruise.centroid_hz, 0) + " Hz)",
             "The hush is brown, the hiss pink: read snow_targets. Skill: audio.");
  }
  if (skid.centroid_hz <= powder.centroid_hz) {
    lab.fail(
        "the skid's rasp (" + fmt(skid.centroid_hz, 0) + " Hz) is not brighter than powder's hush",
        "Read snow_targets' skid layer. Skill: audio.");
  }
  if (m[0].loudest_db > -60) {
    lab.fail(
        "stood still the bed is at " + fmt(m[0].loudest_db, 1) + " dBFS",
        "Nothing sounds at rest in still air: read the ramps in plugins/skier/app/. Skill: audio.");
  }
}

void run_scene(ge::lab::Lab& lab) {
  const double seed = lab.arg_num("seed");
  const double seconds = lab.arg_num("seconds");
  auto level = std::make_shared<const ge::mountain::mapgen::Level>(sim::race_map(seed));
  sim::Driver d(level, 0);
  Synth s;
  snd::apply_mix(s, {}, 0);
  snd::RunSound sound = snd::make_run_sound();
  constexpr int kStepsPerFrame = 2;  // 120 Hz steps, 60 Hz frames
  std::vector<float> chunk(2 * static_cast<size_t>(kRate / kFps));
  std::vector<float> out;
  const auto frames = static_cast<int64_t>(seconds * kFps);
  int64_t f = 0;
  for (; f < frames && !d.over(); f++) {
    for (int k = 0; k < kStepsPerFrame && !d.over(); k++) {
      d.step(sim::bot_controls(d.state(), 0, d.plan()));
      snd::run_sound_step(sound, s, d.state());
    }
    snd::run_sound_frame(sound, s, d.state(), 1.0f / kFps);
    s.render(chunk.data(), kRate / kFps);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }
  // The finish rings on after the line.
  snd::run_sound_silence(sound, s);
  for (int k = 0; k < kFps; k++) {
    s.render(chunk.data(), kRate / kFps);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }
  const auto rep = d.report(seed);
  const Meter m = ge::lab::sound_shot(
      lab,
      {.id = "run",
       .title = "THE BOT DOWN SEED " + fmt(seed, 0),
       .caption = "The bot's run as the app plays it: the bed steered every frame, every step's "
                  "events routed to the bank, heard from the chase seat; " +
                  std::to_string(rep.gates) + " gates, " + std::to_string(rep.flights) +
                  " flights" + (rep.finished ? ", home in " + fmt(rep.time, 2) + " s" : "") + ".",
       .look_for = "the wind swelling with the speed and through the tucks, the gates as small "
                   "spikes along it, the landings as swells, the finish's horn at the end; no "
                   "gap and no click",
       .inputs = {{"seed", fmt(seed, 0)}, {"seconds", fmt(seconds, 0)}}},
      out);
  lab.metric("run.frames", static_cast<double>(f), "count", ge::lab::Det::deterministic);
  if (m.peak_db > -0.1f) {
    lab.fail("the run reaches full scale (" + fmt(m.peak_db, 2) + " dBFS)",
             "The limiter should hold a run under 0 dBFS: read synth.cpp's limiter. Skill: audio.");
  }
  if (s.stats().dropped > 0) {
    lab.fail(std::to_string(s.stats().dropped) + " calls dropped on a full ring",
             "A frame sends more than the ring holds: read run_sound_frame. Skill: audio.");
  }
}

void run(ge::lab::Lab& lab) {
  const std::string only = lab.arg("scene");
  if (only == "all" || only == "bank") bank(lab);
  if (only == "all" || only == "beds") beds(lab);
  if (only == "all" || only == "run") run_scene(lab);
}

const ge::lab::LabSpec kSpec{
    "audition",
    ge::lab::LabKind::sim,
    "game:fall_line",
    "games/fall_line/sound (the bank, the route), plugins/skier/app (the beds)",
    "audio",
    "What does every sound in Fall Line's bank, every moment of its ride bed and a whole run "
    "sound like, metered, with no speakers?",
    {
        {"scene", "all", "all, bank, beds or run"},
        {"seed", "38", "the run scene's seed"},
        {"seconds", "300", "the run scene's limit, s (seed 38 is home in about 240)"},
    },
    "--seconds=8",
    run,
};
GE_LAB(kSpec);

}  // namespace
