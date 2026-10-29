// LAB `samples`: recorded sounds on the synth's sampled voices (engine/audio/synth.hpp's
// `sample`) and in a world (channels.hpp), rendered offline and metered. Every scene is a buffer
// with its picture and its WAV; the checks a listener would make are failures: a tone recorded at
// any rate plays at its own pitch with no stair steps above it, a pitch change moves the length
// the other way, a cue's loop keeps a sound going until it is stopped and the stop fades, a
// source flying past moves from the left ear to the right and is loudest abeam, and a full
// channel table keeps the nearest sounds. The bench times the pool full of voices, measured.
//
// ge:summary Lab: sampled voices resampled, pitched, looped, a fly-by heard, the channel limit
// ge:keywords lab audio sample sampled-voice resample pitch loop cue fly-by spatial pan
// ge:keywords channels steal limit bench realtime
// ge:skill audio labs
// ge:test tests/audio_channels_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/channels.hpp"
#include "audio/meter.hpp"
#include "audio/synth.hpp"
#include "core/libm/libm.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/sound.hpp"

namespace {

using namespace ge::audio;
namespace gl = ge::lab;
namespace libm = ge::core::libm;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<float> render(Synth& s, double seconds) {
  std::vector<float> out(2 * static_cast<size_t>(seconds * kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  return out;
}

// A tone recorded at `rate`: `hz` with a fifth of its second harmonic, `seconds` long, with
// 5 ms fades so the recording itself has no click.
Pcm record(int rate, double hz, double seconds, float a = 0.5f) {
  Pcm p;
  p.rate = rate;
  const size_t n = static_cast<size_t>(seconds * rate);
  const double fade = 0.005 * rate;
  p.samples.resize(n);
  for (size_t i = 0; i < n; i++) {
    const double t = static_cast<double>(i) / rate;
    double x = libm::sin(6.283185307179586 * hz * t) + 0.2 * libm::sin(12.566370614359172 * hz * t);
    const double edge = static_cast<double>(i < n - 1 - i ? i : n - 1 - i);
    if (edge < fade) x *= edge / fade;
    p.samples[i] = a * static_cast<float>(x / 1.2);
  }
  return p;
}

// An engine's drone for a loop: three harmonics of 90 Hz, a whole number of cycles at 22 050 Hz.
Pcm drone() {
  Pcm p;
  p.rate = 22050;
  p.samples.resize(22050);  // 1 s: 90 cycles
  for (size_t i = 0; i < p.samples.size(); i++) {
    const double w = 6.283185307179586 * 90 * static_cast<double>(i) / 22050;
    p.samples[i] = static_cast<float>(0.35 * (libm::sin(w) + 0.5 * libm::sin(2 * w) +
                                              0.3 * libm::sin(3 * w) + 0.15 * libm::sin(7 * w)));
  }
  p.loop_start = 0;
  p.loop_end = p.frames();
  return p;
}

void rates(gl::Lab& lab) {
  for (const int rate : {11025, 22050, 44100}) {
    const Pcm p = record(rate, 1000, 0.6);
    Synth s;
    s.sample(p, {.gain = 0.2f});
    const std::string id = "rate-" + std::to_string(rate);
    const Meter m = gl::sound_shot(
        lab,
        {id,
         "A 1 KHZ TONE RECORDED AT " + std::to_string(rate) + " HZ",
         "0.6 s of 1 kHz with a fifth of its octave, recorded at " + std::to_string(rate) +
             " Hz and played at 48 kHz on a sampled voice (cubic interpolation).",
         "the energy round the 1 and 2 kHz bands (the meter's bands are an octave wide, so their "
         "skirts show beside it); the same length at every rate; the steps, measured as the "
         "error against the exact tone, in rate-<hz>.error-db",
         {{"rate", std::to_string(rate)}}},
        render(s, 0.7));
    // The steps, measured: the played tone against the same tone computed at 48 kHz, over the
    // middle of the sound (the error's power under the tone's, dB).
    Synth again;
    again.sample(p, {.gain = 0.2f});
    const std::vector<float> out = render(again, 0.6);
    double err = 0, sig = 0;
    for (size_t i = 2400; i < 26400; i++) {
      const double t = static_cast<double>(i) / kRate;
      const double want = 0.2 * 0.5 *
                          (libm::sin(6.283185307179586 * 1000 * t) +
                           0.2 * libm::sin(12.566370614359172 * 1000 * t)) /
                          1.2;
      err += (out[2 * i] - want) * (out[2 * i] - want);
      sig += want * want;
    }
    const double error_db = 4.342944819032518 * libm::log(err / sig);
    lab.metric(id + ".error-db", error_db, "dB");
    if (m.length_s < 0.57f || m.length_s > 0.63f) {
      lab.fail(id + " lasts " + fmt(m.length_s, 3) + " s, not 0.6",
               "The step (Synth::Impl::start_sample) misreads the recording's rate. Skill: audio.");
    }
    if (error_db > -40) {
      lab.fail(id + " is off the exact tone by " + fmt(error_db, 1) + " dB (want under -40)",
               "The interpolation (render_sample) leaves steps. Skill: audio.");
    }
  }
}

void pitches(gl::Lab& lab) {
  const Pcm p = record(22050, 440, 0.5);
  float last = 1e9f;
  for (const float pitch : {0.5f, 1.0f, 2.0f}) {
    Synth s;
    s.sample(p, {.gain = 0.2f, .pitch = pitch});
    const std::string id = "pitch-" + fmt(pitch, 1);
    const Meter m =
        gl::sound_shot(lab,
                       {id,
                        "A 440 HZ RECORDING AT PITCH " + fmt(pitch, 1),
                        "Half a second of 440 Hz recorded at 22 050 Hz, played " + fmt(pitch, 1) +
                            " times as fast: an octave per doubling, and as much shorter.",
                        "the length " + fmt(0.5 / pitch, 2) + " s; the energy " +
                            (pitch < 1   ? "an octave lower"
                             : pitch > 1 ? "an octave higher"
                                         : "at 440 Hz"),
                        {{"pitch", fmt(pitch, 1)}}},
                       render(s, 1.1));
    if (m.length_s > last) {
      lab.fail(id + " is longer than the slower pitch's",
               "The step ignores the pitch. Skill: audio.");
    }
    last = m.length_s;
  }
}

void loops(gl::Lab& lab) {
  Pcm p = record(22050, 330, 0.8);
  p.loop_start = 8820;  // 0.4 s in
  p.loop_end = 13230;   // 0.6 s
  Synth s;
  const SampleId id = s.sample(p, {.gain = 0.2f});
  std::vector<float> out = render(s, 1.5);
  s.sample_stop(id, 0.08f);
  const std::vector<float> tail = render(s, 0.7);
  out.insert(out.end(), tail.begin(), tail.end());
  const Meter m = gl::sound_shot(
      lab,
      {"loop-cue",
       "A RECORDING LOOPED AT ITS CUE, THEN STOPPED",
       "0.8 s of 330 Hz whose cue loops 0.4 to 0.6 s, played for 1.5 s and stopped with an 80 ms "
       "fade. The loop's ends meet on the recording's own samples, so the seam is heard only as "
       "much as the recording differs across it.",
       "a steady level from 0.4 s to 1.5 s, then a smooth fall over about half a second",
       {{"loop", "0.4-0.6 s"}, {"stop", "1.5 s, 0.08 s fade"}}},
      out);
  if (m.length_s < 1.45f || m.length_s > 2.0f) {
    lab.fail("the looped sound lasts " + fmt(m.length_s, 2) + " s (want 1.5 to 2)",
             "The loop wrap or the stop's fade (render_sample, Cmd::sample_stop). Skill: audio.");
  }
}

void flyby(gl::Lab& lab) {
  const Pcm d = drone();
  Synth s;
  Channels ch(s, 8);
  const Listener l{.at = {0, 0, 0}, .right = {1, 0, 0}, .entity = 0};
  ch.update(l);
  // A source 60 units in front, crossing from 600 left to 600 right at 300 units a second.
  const double kSpeed = 300, kFrom = -600, kDepth = 60, kSeconds = 4;
  ch.play(d, {.entity = 1,
              .channel = 1,
              .at = {kFrom, 0, kDepth},
              .attenuation = 0.0012f,
              .loop = Loop::whole});
  gl::Trace gain{"gain", {}, {}}, pan{"pan", {}, {}};
  std::vector<float> out;
  const int per = kRate / 50;  // 50 frames a second
  std::vector<float> chunk(2 * static_cast<size_t>(per));
  for (int f = 0; f < static_cast<int>(kSeconds * 50); f++) {
    const double t = f / 50.0;
    ch.move(1, {kFrom + kSpeed * t, 0, kDepth});
    ch.update(l, 0.03f);
    gain.x.push_back(t);
    gain.y.push_back(ch.slots()[0].heard.gain);
    pan.x.push_back(t);
    pan.y.push_back(ch.slots()[0].heard.pan);
    s.render(chunk.data(), per);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }
  gl::LineChart line;
  line.frame = {"A SOURCE FLYING PAST: HOW IT IS HEARD",
                "60 units in front, 600 left to 600 right at 300 units/s, attenuation 1/833",
                "t (s)", "gain / pan"};
  line.traces = {gain, pan};
  line.marks = {{2.0, "abeam"}};
  line.rules = {0};
  line.spikes = false;
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"flyby-hearing",
            chart.title,
            "What Channels::update hands the synth each frame for a looping drone crossing in "
            "front of the listener: the gain rising as it nears and falling away, the pan "
            "sweeping from the left ear to the right through the centre when it is abeam.",
            "gain peaking at 2 s near 0.93; pan from -1 through 0 at 2 s to +1, steepest abeam",
            {{"frames", "50/s"}}},
           gl::to_picture(chart.plot));
  // The two halves' balance, metered: the first second on the left, the last on the right.
  const size_t sec = 2 * static_cast<size_t>(kRate);
  const Meter first = measure(std::span<const float>(out.data(), sec));
  const Meter last = measure(std::span<const float>(out.data() + out.size() - sec, sec));
  gl::sound_shot(lab,
                 {"flyby",
                  "A DRONE FLYING PAST, LEFT TO RIGHT",
                  "The same fly-by rendered: a 90 Hz drone looped whole, steered every 20 ms "
                  "with a 30 ms glide.",
                  "the level swelling to the middle and falling; the left channel (up) leading, "
                  "the right (down) trailing; no zipper as the pan moves",
                  {{"seconds", fmt(kSeconds, 0)}}},
                 out);
  lab.metric("flyby.first-balance", first.balance, "L-1..R+1");
  lab.metric("flyby.last-balance", last.balance, "L-1..R+1");
  if (!(first.balance < -0.5f && last.balance > 0.5f)) {
    lab.fail("the fly-by's balance runs " + fmt(first.balance, 2) + " to " + fmt(last.balance, 2) +
                 " (want under -0.5, then over 0.5)",
             "hear's pan or the sampled voice's pan steering (sample_set). Skill: audio.");
  }
}

void limit(gl::Lab& lab) {
  const Pcm p = record(22050, 520, 1.0, 0.3f);
  Synth s;
  Channels ch(s, 8);
  ch.update({});
  // Sixteen sources in a ring of rising distance, played nearest-last, into eight channels.
  gl::LabTable table{"limit", {"source", "distance", "gain", "kept"}, {}};
  for (int i = 15; i >= 0; i--) {
    const double r = 80 + 50.0 * i;
    const double a = 0.39269908169872414 * i;
    ch.play(
        p,
        {.entity = 10 + i, .at = {r * libm::sin(a), 0, r * libm::cos(a)}, .attenuation = 0.001f});
  }
  int kept_near = 0;
  for (int i = 0; i < 16; i++) {
    bool kept = false;
    for (const Channels::Slot& sl : ch.slots()) kept = kept || sl.entity == 10 + i;
    kept_near += kept && i < 8;
    table.rows.push_back({std::to_string(10 + i), fmt(80 + 50.0 * i, 0),
                          fmt(1 - (80 + 50.0 * i) * 0.001, 2), kept ? "yes" : "no"});
  }
  lab.table(table);
  lab.metric("limit.stolen", ch.stolen(), "plays");
  lab.metric("limit.kept-nearest", kept_near, "of 8");
  if (kept_near != 8) {
    lab.fail("the channel table kept " + std::to_string(kept_near) + " of the 8 nearest",
             "Channels::play's steal must take the quietest slot. Skill: audio.");
  }
  gl::sound_shot(lab,
                 {"limit",
                  "SIXTEEN SOURCES INTO EIGHT CHANNELS",
                  "Sixteen 520 Hz tones round the listener at 80 to 830 units, played farthest "
                  "first into a table of eight: each nearer one steals the quietest slot.",
                  "eight voices sounding; the far eight cut short at their start",
                  {{"limit", "8"}}},
                 render(s, 1.2));
}

void bench(gl::Lab& lab, double seconds) {
  if (seconds <= 0) return;
  const Pcm d = drone();
  Synth s;
  for (int i = 0; i < kMaxSamples; i++) {
    s.sample(d, {.gain = 0.01f,
                 .pan = static_cast<float>(i % 9 - 4) / 4.0f,
                 .pitch = 0.8f + 0.02f * static_cast<float>(i),
                 .loop = Loop::whole});
  }
  std::vector<float> buf(2 * static_cast<size_t>(kBlock));
  const int blocks = static_cast<int>(seconds * kRate / kBlock);
  const auto t0 = std::chrono::steady_clock::now();
  for (int b = 0; b < blocks; b++) s.render(buf.data(), kBlock);
  const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  lab.metric("bench.realtime-share", took / seconds, "of one core", gl::Det::measured);
}

void run(gl::Lab& lab) {
  const std::string& only = lab.arg("scene");
  if (only == "all" || only == "rates") rates(lab);
  if (only == "all" || only == "pitch") pitches(lab);
  if (only == "all" || only == "loop") loops(lab);
  if (only == "all" || only == "flyby") flyby(lab);
  if (only == "all" || only == "limit") limit(lab);
  bench(lab, lab.arg_num("bench-seconds"));
}

const gl::LabSpec kSpec{
    "samples",
    gl::LabKind::scenario,
    "engine",
    "engine/audio (sampled voices, pcm.hpp, channels.hpp)",
    "audio",
    "How do recordings sound on the synth's sampled voices: at their own rate and pitch, looped, "
    "heard from where they are, and kept or dropped by the channel limit?",
    {
        {"scene", "all", "all, rates, pitch, loop, flyby or limit"},
        {"bench-seconds", "5", "seconds of a full pool of sampled voices to time (0: no bench)"},
    },
    "--bench-seconds=0.5",
    run,
};
GE_LAB(kSpec);

}  // namespace
