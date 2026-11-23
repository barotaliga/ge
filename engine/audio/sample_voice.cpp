// A sampled voice's start (the step, the loop, the gain and pan) and its render a block at a time.
//
// ge:summary A sampled voice's start and render: the 32.32 step, the loop, Catmull-Rom, the glides
// ge:keywords sampled-voice begin-sample render-sample resample catmull-rom loop glide
// ge:skill audio
// ge:test tests/audio_synth_test.cpp
#include "audio/sample_voice.hpp"

#include "audio/dsp.hpp"
#include "audio/voice.hpp"
#include "core/num.hpp"

namespace ge::audio {

namespace {

// The recording's frame `k` as the voice hears it: inside a loop it wraps, before the start and
// past the end of a recording that does not loop it is silence.
float sample_at(const SampleVoice& s, int64_t k) {
  if (s.loop_start >= 0 && k >= s.loop_end) {
    k = s.loop_start + (k - s.loop_start) % (s.loop_end - s.loop_start);
  }
  if (k < 0 || k >= s.pcm->frames()) return 0;
  return s.pcm->samples[static_cast<size_t>(k)];
}

}  // namespace

void begin_sample(SampleVoice& s, const Pcm& pcm, const SamplePlay& play, uint32_t id,
                  uint64_t order, int bus, int64_t start) {
  s = SampleVoice{};
  s.live = true;
  s.id = id;
  s.order = order;
  s.pcm = &pcm;
  s.bus = bus;
  s.start = start;
  const float pitch = play.pitch > 0.01f ? play.pitch : 0.01f;
  const double step = static_cast<double>(pcm.rate) * pitch / kRate * 4294967296.0;
  s.step = static_cast<uint64_t>(core::num::round(step));
  if (s.step < 1) s.step = 1;
  const bool whole = play.loop == Loop::whole;
  if (whole || (play.loop == Loop::file && pcm.loops())) {
    s.loop_start = whole ? 0 : pcm.loop_start;
    s.loop_end = whole ? pcm.frames() : pcm.loop_end;
  }
  s.gain = s.t_gain = play.gain < 0 ? 0 : play.gain;
  s.pan = s.t_pan = play.pan < -1 ? -1 : play.pan > 1 ? 1 : play.pan;
  pan_gains(s.pan, s.gl, s.gr);
  s.echo = clamp01(play.echo);
  const int64_t from = play.from < 0 ? 0 : play.from > pcm.frames() ? pcm.frames() : play.from;
  s.pos = static_cast<uint64_t>(from) << 32;
}

void far_ear(float pan, float& delay, float& gain, float& coeff) {
  const float a = pan < 0 ? -pan : pan;
  const float side = a > 1 ? 1 : a;
  delay = side * kItdFrames;
  gain = 1 - kFarShade * side;
  coeff = 1 - (1 - kFarCoeff) * side;
}

void render_sample(SampleVoice& s, int64_t b0, int count, const float* bus, float* l, float* r,
                   float* e, bool binaural) {
  if (s.stopping && b0 >= s.free_at) {
    s.live = false;
    return;
  }
  const int64_t length = s.pcm->frames();
  if (binaural) far_ear(s.pan, s.far_delay, s.far_gain, s.far_coeff);
  for (int i = s.start > b0 ? static_cast<int>(s.start - b0) : 0; i < count; i++) {
    const int64_t k = static_cast<int64_t>(s.pos >> 32);
    if (s.loop_start < 0 && k >= length) {
      s.live = false;
      return;
    }
    // Catmull-Rom through the four frames round the point: smooth enough that a recording played
    // at twice its rate or more keeps no stair steps, and exact on the frames.
    const float t = static_cast<float>(static_cast<uint32_t>(s.pos)) * (1.0f / 4294967296.0f);
    const float a = sample_at(s, k - 1), b = sample_at(s, k), c = sample_at(s, k + 1),
                d = sample_at(s, k + 2);
    float x = b + 0.5f * t * (c - a + t * (2 * a - 5 * b + 4 * c - d + t * (3 * (b - c) + d - a)));
    s.pos += s.step;
    if (s.loop_start >= 0 && static_cast<int64_t>(s.pos >> 32) >= s.loop_end) {
      s.pos -= static_cast<uint64_t>(s.loop_end - s.loop_start) << 32;
    }
    s.gain += (s.t_gain - s.gain) * s.c_gain;
    if (++s.ctl == kControl) {
      s.ctl = 0;
      for (int j = 0; j < kControl; j++) s.pan += (s.t_pan - s.pan) * s.c_pan;
      pan_gains(s.pan, s.gl, s.gr);
      if (binaural) far_ear(s.pan, s.far_delay, s.far_gain, s.far_coeff);
    }
    x *= s.gain * bus[i];
    e[i] += x * s.echo;
    if (!binaural) {
      l[i] += x * s.gl;
      r[i] += x * s.gr;
      continue;
    }
    // Two ears: the near one hears it now, the far one `far_delay` frames ago (between two
    // frames, linearly), through the head's shadow.
    s.ear[s.ear_at & (kEarFrames - 1)] = x;
    const float back = s.far_delay;
    const int whole = static_cast<int>(back);
    const float part = back - static_cast<float>(whole);
    const float x0 = s.ear[(s.ear_at - static_cast<uint32_t>(whole)) & (kEarFrames - 1)];
    const float x1 = s.ear[(s.ear_at - static_cast<uint32_t>(whole) - 1) & (kEarFrames - 1)];
    ++s.ear_at;
    s.far_low += (x0 + (x1 - x0) * part - s.far_low) * s.far_coeff;
    if (s.pan == 0) {  // centred: both ears hear what the level pan gives, to the bit
      l[i] += x * s.gl;
      r[i] += x * s.gr;
      continue;
    }
    // The near ear at the louder side's level, the far ear at that level through the shadow.
    const float level = s.gl > s.gr ? s.gl : s.gr;
    const float far = s.far_low * s.far_gain * level;
    if (s.pan > 0) {
      l[i] += far;
      r[i] += x * level;
    } else {
      l[i] += x * level;
      r[i] += far;
    }
  }
}

}  // namespace ge::audio
