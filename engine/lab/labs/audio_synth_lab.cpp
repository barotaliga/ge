// LAB `synth`: what the instrument (engine/audio/synth.hpp) does with each thing it can be told,
// rendered offline and metered. Every scene is a buffer with its picture (the waveform, its level
// envelope, its octave bands) and its WAV, and its numbers as metrics; together they are the
// before-and-after any change to the DSP owes. The checks a reader would make by ear are made
// here as failures: the noise colours fall in order of brightness, a filter sweep brightens, a
// harder drive adds harmonics, the echo answers, a layer follows its targets, and the limiter
// keeps a stack under full scale. The bench renders a busy race's load (layers and one-shots at
// once) and reports the share of real time a block costs, measured.
//
// ge:summary Lab: the synth's every voice, colour, filter, drive, echo, layer and the limiter,
// metered ge:keywords lab audio synth audition render meter wav waveform spectrum limiter bench
// ge:keywords realtime cost layers voices
// ge:skill audio labs
// ge:test tests/audio_synth_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "audio/meter.hpp"
#include "audio/synth.hpp"
#include "lab/lab.hpp"
#include "lab/sound.hpp"

namespace {

using namespace ge::audio;
using ge::lab::Det;

std::vector<float> render(Synth& s, double seconds) {
  std::vector<float> out(2 * static_cast<size_t>(seconds * kRate));
  s.render(out.data(), static_cast<int>(out.size() / 2));
  return out;
}

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

Meter scene(ge::lab::Lab& lab, Synth& s, double seconds, const char* id, const char* title,
            std::string caption, std::string look_for) {
  return ge::lab::sound_shot(lab,
                             {.id = id,
                              .title = title,
                              .caption = std::move(caption),
                              .look_for = std::move(look_for),
                              .inputs = {{"seconds", fmt(seconds, 2)}}},
                             render(s, seconds));
}

void waves(ge::lab::Lab& lab) {
  const Wave kinds[] = {Wave::sine, Wave::triangle, Wave::square, Wave::saw};
  const char* names[] = {"sine", "triangle", "square", "saw"};
  float last = 0;
  for (int i = 0; i < 4; i++) {
    Synth s;
    s.tone({.wave = kinds[i], .from_hz = 220, .duration_ms = 500, .hold_ms = 300});
    const std::string id = std::string("wave-") + names[i];
    const Meter m = scene(lab, s, 0.6, id.c_str(), ("A " + id + " AT 220 HZ").c_str(),
                          "Half a second of a " + std::string(names[i]) +
                              " at 220 Hz, held 300 ms and falling away exponentially.",
                          "the waveform's shape; the bands' energy rising up the octaves from "
                          "sine to saw (centroid order: sine < triangle < square < saw)");
    if (m.centroid_hz < last) {
      lab.fail(id + "'s centroid " + fmt(m.centroid_hz, 0) + " Hz is under the duller wave's",
               "A wave's harmonics moved: run tests/audio_dsp_test.cpp. Skill: audio.");
    }
    last = m.centroid_hz;
  }
}

void colours(ge::lab::Lab& lab) {
  const Color kinds[] = {Color::white, Color::pink, Color::brown};
  const char* names[] = {"white", "pink", "brown"};
  float last = 1e9f;
  for (int i = 0; i < 3; i++) {
    Synth s;
    s.noise({.duration_ms = 600, .color = kinds[i], .attack_ms = 60, .hold_ms = 250});
    const std::string id = std::string("noise-") + names[i];
    const Meter m = scene(lab, s, 0.7, id.c_str(), ("A " + id + " SWELL").c_str(),
                          "600 ms of " + std::string(names[i]) +
                              " noise swelling in over 60 ms, held 250 ms and thinning out.",
                          "white flat across the bands, pink falling about 3 dB an octave, brown "
                          "about 6 (centroid order: white > pink > brown)");
    if (m.centroid_hz > last) {
      lab.fail(id + "'s centroid " + fmt(m.centroid_hz, 0) + " Hz is over the brighter colour's",
               "The noise pools' colouring moved (make_tables). Skill: audio.");
    }
    last = m.centroid_hz;
  }
}

void shapes(ge::lab::Lab& lab) {
  {
    Synth s;
    s.tone({.wave = Wave::saw,
            .from_hz = 110,
            .duration_ms = 1200,
            .hold_ms = 900,
            .filter = {.type = FilterType::lowpass, .hz = 150, .to_hz = 8000, .q = 4}});
    const Meter m =
        scene(lab, s, 1.3, "sweep", "A RESONANT LOWPASS SWEEP",
              "A 110 Hz saw through a lowpass at q 4 swept from 150 Hz to 8 kHz across 1.2 s.",
              "the waveform brightening left to right; energy reaching the top bands");
    if (m.band_db[8] < m.band_db[0] - 30) {
      lab.fail("the sweep never opened: its 8 kHz band is " + fmt(m.band_db[8], 1) + " dB",
               "The filter's sweep (Voice::cut_mul) or svf_tune moved. Skill: audio.");
    }
  }
  {
    float last = 0;
    for (const float drive : {0.0f, 0.5f, 1.0f}) {
      Synth s;
      s.tone({.wave = Wave::triangle,
              .from_hz = 110,
              .duration_ms = 400,
              .hold_ms = 250,
              .drive = drive});
      const std::string id = "drive-" + fmt(drive, 1);
      const Meter m = scene(
          lab, s, 0.45, id.c_str(), ("A TRIANGLE DRIVEN " + fmt(drive, 1)).c_str(),
          "A 110 Hz triangle folded through the saturation curve at drive " + fmt(drive, 1) + ".",
          "the waveform squaring off as the drive rises; more energy up the bands");
      if (m.centroid_hz < last) {
        lab.fail(id + " is duller than the cleaner one",
                 "The shaper (shape, curve_for). Skill: audio.");
      }
      last = m.centroid_hz;
    }
  }
  {
    Synth s;
    s.tone({.wave = Wave::saw,
            .from_hz = 196,
            .duration_ms = 1500,
            .attack_ms = 120,
            .hold_ms = 900,
            .detune_cents = 9,
            .vibrato = {.rate_hz = 5.5f, .depth_cents = 25, .delay_ms = 300},
            .pan = -0.5f,
            .filter = {.type = FilterType::lowpass, .hz = 2500}});
    scene(lab, s, 1.6, "pair-vibrato", "A DETUNED PAIR WITH VIBRATO",
          "A 196 Hz saw pair 9 cents apart, a 120 ms attack, vibrato 25 cents at 5.5 Hz fading in "
          "after 300 ms, panned half left.",
          "a beating envelope (the pair), the level ramping in, the left channel (up) louder");
  }
  {
    Synth s(1, EchoRoom{});
    s.noise({.duration_ms = 25, .volume = 0.08f, .color = Color::pink, .echo = 1});
    const Meter m = scene(lab, s, 1.2, "echo", "A KNOCK INTO THE ECHO",
                          "A 25 ms pink knock sent fully into the shared echo (0.21 s, feedback "
                          "0.24, damped at 1.8 kHz).",
                          "the knock, then repeats every 0.21 s, each quieter and duller");
    if (m.length_s < 0.4f) {
      lab.fail("the echo did not answer (the sound lasted " + fmt(m.length_s, 2) + " s)",
               "The echo bus (Impl::block). Skill: audio.");
    }
  }
}

void layers(ge::lab::Lab& lab) {
  Synth s;
  const LayerId bed = s.layer({.kind = LayerSpec::Kind::noise,
                               .color = Color::pink,
                               .filter = FilterType::lowpass,
                               .q = 0.6f});
  const LayerId hum = s.layer({.kind = LayerSpec::Kind::tone,
                               .wave = Wave::triangle,
                               .detune_cents = 6,
                               .drive = true,
                               .filter = FilterType::lowpass});
  std::vector<float> all;
  // A frame at a time for three seconds: the bed rises, the hum climbs and roughens, both fall.
  for (int frame = 0; frame < 180; frame++) {
    const float t = static_cast<float>(frame) / 60;
    const float up = t < 1.5f ? t / 1.5f : (3 - t) / 1.5f;
    s.set(bed, {.level = 0.05f * up, .cutoff = 400 + 3000 * up}, 0.15f);
    s.set(hum, {.level = 0.03f * up, .hz = 60 + 140 * up, .cutoff = 300 + 1500 * up, .grit = up},
          0.08f);
    const std::vector<float> chunk = render(s, 1.0 / 60);
    all.insert(all.end(), chunk.begin(), chunk.end());
  }
  const Meter m = ge::lab::sound_shot(
      lab,
      {.id = "layers",
       .title = "TWO LAYERS STEERED",
       .caption = "A pink noise bed and a driven triangle hum steered once a frame for 3 s: up "
                  "to full at 1.5 s and back down (level, cutoff, pitch 60 to 200 Hz, grit).",
       .look_for = "a smooth swell and fall with no step or click; the envelope peaking mid-way",
       .inputs = {{"frames", "180 at 60 Hz"}}},
      all);
  const size_t n = m.envelope_db.size();
  if (n < 10 || m.envelope_db[n / 2] < m.envelope_db[n / 10] + 6) {
    lab.fail("the layers did not swell toward the middle",
             "Layer steering (Impl::steer). Skill: audio.");
  }
}

void limiter(ge::lab::Lab& lab) {
  Synth s;
  for (int i = 0; i < 40; i++) {
    s.tone({.wave = Wave::square,
            .from_hz = 70.0f + 9.0f * static_cast<float>(i),
            .duration_ms = 500,
            .volume = 0.2f,
            .hold_ms = 300});
  }
  const Meter m = scene(lab, s, 0.6, "limiter", "FORTY LOUD SQUARES AT ONCE",
                        "Forty squares at volume 0.2 each (a sum far past full scale) through the "
                        "master limiter: -10 dBFS, a 12 dB knee, ratio 12.",
                        "the waveform held under full scale with no flat-topped clip; the level "
                        "a few dB under 0");
  lab.metric("limiter.reduction-db", static_cast<double>(s.stats().reduction_db), "dB");
  if (m.peak_db > 0) {
    lab.fail("the stack reached " + fmt(m.peak_db, 2) + " dBFS",
             "The limiter (Impl::block). Skill: audio.");
  }
}

void bench(ge::lab::Lab& lab) {
  const double seconds = lab.arg_num("bench-seconds");
  if (seconds <= 0) return;
  Synth s;
  std::vector<LayerId> ids;
  ids.reserve(24);
  for (int i = 0; i < 24; i++) {
    ids.push_back(s.layer({.kind = i % 3 ? LayerSpec::Kind::noise : LayerSpec::Kind::tone,
                           .color = static_cast<Color>(i % 3),
                           .detune_cents = 5,
                           .drive = i % 3 == 0,
                           .filter = FilterType::bandpass}));
  }
  std::vector<float> chunk(2 * kRate / 60);
  const int frames = static_cast<int>(seconds * 60);
  const auto t0 = std::chrono::steady_clock::now();
  for (int f = 0; f < frames; f++) {
    for (size_t i = 0; i < ids.size(); i++) {
      const float w = static_cast<float>((f + static_cast<int>(i) * 7) % 60) / 60;
      s.set(ids[i],
            {.level = 0.002f + 0.002f * w, .hz = 100 + 50 * w, .cutoff = 500 + 2000 * w, .grit = w},
            0.1f);
    }
    if (f % 2 == 0)
      s.noise({.duration_ms = 400,
               .volume = 0.01f,
               .filter = {.type = FilterType::lowpass, .hz = 3000, .to_hz = 300}});
    if (f % 3 == 0)
      s.tone(
          {.from_hz = 300, .to_hz = 600, .duration_ms = 300, .volume = 0.01f, .detune_cents = 10});
    s.render(chunk.data(), kRate / 60);
  }
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  lab.metric("bench.ms-per-audio-second", ms / seconds, "ms", Det::measured);
  lab.metric("bench.realtime-share", ms / (seconds * 1000) * 100, "%", Det::measured);
  lab.metric("bench.voices", s.stats().voices, "count", Det::measured);
  lab.note("bench: 24 layers and about " + std::to_string(s.stats().voices) +
           " one-shots sounding, " + fmt(ms / seconds, 2) + " ms of render per second of audio (" +
           fmt(ms / (seconds * 10), 2) + " % of real time; time it on PRESET=release).");
}

void run(ge::lab::Lab& lab) {
  const std::string only = lab.arg("scene");
  if (only == "all" || only == "waves") waves(lab);
  if (only == "all" || only == "colours") colours(lab);
  if (only == "all" || only == "shapes") shapes(lab);
  if (only == "all" || only == "layers") layers(lab);
  if (only == "all" || only == "limiter") limiter(lab);
  bench(lab);
}

const ge::lab::LabSpec kSpec{
    "synth",
    ge::lab::LabKind::sim,
    "engine",
    "engine/audio (the synth, the meter)",
    "audio",
    "What does each thing the synth can be told sound like, metered, and what does a busy block "
    "cost?",
    {
        {"scene", "all", "all, waves, colours, shapes, layers or limiter"},
        {"bench-seconds", "5", "seconds of a race's load to time (0: no bench)"},
    },
    "--bench-seconds=0.5",
    run,
};
GE_LAB(kSpec);

}  // namespace
