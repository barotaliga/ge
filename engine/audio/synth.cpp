// The synth's graph: the call ring, the voice pool, the layers, the sampled voices, the buses,
// the echo, the room and the limiter, rendered a block at a time. Everything is allocated in the
// constructor; `render` only reads and writes what is already there.
//
// ge:summary The synth's render: the ring, one-shot envelopes, layers, buses, echo and limiter
// ge:keywords synth render envelope attack hold decay voice-pool steal layer glide bus echo
// ge:keywords limiter soft-knee gain-reduction ring drain sampled-voice resample catmull-rom
// ge:keywords cubic-interpolation fixed-point loop steal-quietest
// ge:skill audio
// ge:test tests/audio_synth_test.cpp
#include "audio/synth.hpp"

#include <array>
#include <atomic>
#include <vector>

#include "audio/dsp.hpp"
#include "audio/reverb.hpp"
#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"

namespace ge::audio {

namespace {

namespace libm = core::libm;

// The limiter (see synth.hpp).
constexpr float kLimitThresholdDb = -10;
constexpr float kLimitKneeDb = 12;
constexpr float kLimitRatio = 12;
constexpr float kLimitAttackS = 0.005f;
constexpr float kLimitReleaseS = 0.25f;

// An exponential envelope may not reach zero, so it starts from and ends on this (-80 dB).
constexpr float kEnvFloor = 0.0001f;
// A layer under this level with a target of silence renders nothing.
constexpr float kSilent = 1e-6f;
// How long a stopped layer fades before its slot is free, s.
constexpr float kStopFadeS = 0.02f;
constexpr float kStopFreeS = 0.15f;
constexpr float kSetEpsilon = 1e-4f;

int64_t frames_of(float ms) {
  return static_cast<int64_t>(core::num::round(static_cast<double>(ms) * kRate / 1000));
}

void pan_gains(float pan, float& gl, float& gr) {
  pan = pan < -1 ? -1 : pan > 1 ? 1 : pan;
  // A balance law: the centre is full on both sides, and a hard pan silences the far side.
  gl = pan > 0 ? 1 - pan : 1;
  gr = pan < 0 ? 1 + pan : 1;
}

float db_of(float x) {  // 20 log10 x, for x > 0
  return static_cast<float>(8.68588963806503655 * libm::log(static_cast<double>(x)));
}
float gain_of_db(float db) { return static_cast<float>(libm::exp(0.11512925464970229 * db)); }

enum class Cmd : uint8_t {
  tone,
  noise,
  layer_new,
  layer_set,
  layer_stop,
  bus,
  room,
  sample_new,
  sample_set,
  sample_stop
};

struct Command {
  Cmd kind = Cmd::tone;
  uint8_t bus = 0;
  int16_t slot = -1;
  Tone tone;
  Noise noise;
  LayerSpec spec;
  LayerTarget target;
  RoomSound room;
  float glide = 0;
  float gain = 0;
  float pan = 0;
  uint32_t id = 0;  // a sampled voice's
  const Pcm* pcm = nullptr;
  SamplePlay play;
};

// ONE ONE-SHOT, from its first frame to its last.
struct Voice {
  bool live = false;
  bool tone = true;
  int64_t start = 0, end = 0;  // absolute frames, [start, end)
  int64_t attack_end = 0, hold_end = 0;
  uint64_t order = 0;  // for stealing the oldest
  float env = 0, peak = 0, floor = 0;
  bool exp_attack = true, exp_decay = true;
  float attack_step = 0, decay_step = 0;  // a multiplier (exp) or an increment (lin)
  // the tone
  Wave wave = Wave::sine;
  int oscs = 1;
  float phase[2] = {0, 0};
  float detune[2] = {1, 1};
  float hz = 440, hz_mul = 1;
  float lfo_phase = 0, lfo_dt = 0, vib_depth = 0;
  int64_t vib_from = 0, vib_full = 0;
  const float* curve = nullptr;
  float push = 1;
  // the noise
  int color = 0;
  uint32_t pos = 0;
  // the filter
  Svf svf;
  float cut = 1000, cut_mul = 1;
  int ctl = 0;
  // the output
  float gl = 1, gr = 1, echo = 0;
  int bus = 0;
};

// ONE LAYER, from `layer` to the end of its fade after `stop`.
struct LayerVoice {
  bool live = false;
  bool fresh = true;  // not steered yet: the first target is taken at once, but for the level
  bool stopping = false;
  int64_t free_at = 0;
  int bus = 0;
  LayerSpec spec;
  float level = 0, t_level = 0, c_level = 1;
  float hz = 100, t_hz = 100, c_hz = 1;
  float cut = 1000, t_cut = 1000, c_cut = 1;
  float push = kGritFloor, t_push = kGritFloor, c_push = 1;
  float pan = 0, t_pan = 0, c_pan = 1;
  int oscs = 1;
  float phase[2] = {0, 0};
  float detune[2] = {1, 1};
  float lfo_phase = 0, lfo_dt = 0;
  const float* curve = nullptr;
  uint32_t pos = 0;
  Svf svf;
  int ctl = 0;
  float gl = 1, gr = 1;
};

// ONE SAMPLED VOICE, from `sample` to its recording's end or the end of its fade. Its place in
// the recording is fixed point, 32.32 frames, so the step a 22 050 Hz file takes at 48 kHz is
// exact to 2^-32 and the voice lands on the same sample on every platform.
struct SampleVoice {
  bool live = false;
  bool stopping = false;
  uint32_t id = 0;
  uint64_t order = 0;
  const Pcm* pcm = nullptr;
  int64_t start = 0;  // the absolute frame its first sample plays on
  uint64_t pos = 0, step = 0;
  int64_t loop_start = -1, loop_end = 0;
  int64_t free_at = 0;
  int bus = 0;
  int ctl = 0;
  float gain = 0, t_gain = 0, c_gain = 1;
  float pan = 0, t_pan = 0, c_pan = 1;
  float gl = 1, gr = 1, echo = 0;
};

// The recording's frame `k` as the voice hears it: inside a loop it wraps, before the start and
// past the end of a recording that does not loop it is silence.
float sample_at(const SampleVoice& s, int64_t k) {
  if (s.loop_start >= 0 && k >= s.loop_end) {
    k = s.loop_start + (k - s.loop_start) % (s.loop_end - s.loop_start);
  }
  if (k < 0 || k >= s.pcm->frames()) return 0;
  return s.pcm->samples[static_cast<size_t>(k)];
}

struct Bus {
  float gain = 1, target = 1, coeff = 1;
};

}  // namespace

struct Synth::Impl {
  Tables tables;
  core::Rng rng;
  // the ring
  std::vector<Command> ring;
  std::atomic<uint32_t> head{0}, tail{0};
  // the producer's side
  std::array<bool, kMaxLayers> used{};
  std::array<LayerTarget, kMaxLayers> sent{};
  std::array<bool, kMaxLayers> has_sent{};
  std::atomic<uint32_t> dropped{0};
  // the render side
  std::array<Voice, kMaxVoices> voices{};
  std::array<LayerVoice, kMaxLayers> layers{};
  std::array<SampleVoice, kMaxSamples> samples{};
  std::array<Bus, kBuses> buses{};
  uint64_t order = 0;
  int64_t frame = 0;  // the first frame of the next block
  std::array<float, kBlock> mix_l{}, mix_r{}, mix_e{};
  std::array<std::array<float, kBlock>, kBuses> bus_gain{};
  // the echo
  std::vector<float> line;
  int line_write = 0, line_delay = 1;
  float echo_feedback = 0, echo_coeff = 1, echo_damp = 0;
  // the room
  Reverb reverb;
  std::array<float, kBlock> room_l{}, room_r{};
  // the limiter
  float limit_g = 0, limit_attack = 1, limit_release = 1, limit_below = 0;
  // the sampled voices' ids, for `sample_live`: the producer's next, the render side's
  // highest applied and each slot's sounding id (0: free)
  uint32_t next_sample = 1;
  std::atomic<uint32_t> applied_sample{0};
  std::array<std::atomic<uint32_t>, kMaxSamples> sample_ids{};
  // the stats, read from any thread
  std::atomic<uint64_t> frames{0};
  std::atomic<uint32_t> voices_live{0}, layers_live{0}, samples_live{0}, stolen{0};
  std::atomic<float> reduction{0};

  Impl(uint32_t seed, const EchoRoom& room) : tables(make_tables(seed)), rng(core::make_rng(seed)) {
    ring.resize(kRingSize);
    line.assign(static_cast<size_t>(kRate) + 1, 0);
    int d = static_cast<int>(core::num::round(static_cast<double>(room.delay_s) * kRate));
    line_delay = d < 1 ? 1 : d > kRate ? kRate : d;
    echo_feedback = room.feedback;
    echo_coeff = static_cast<float>(1 - libm::exp(-2 * kPi * room.damp_hz / kRate));
    limit_attack = glide_coeff(kLimitAttackS);
    limit_release = glide_coeff(kLimitReleaseS);
    limit_below = gain_of_db(kLimitThresholdDb - kLimitKneeDb / 2);
  }

  bool push(const Command& c) {
    const uint32_t h = head.load(std::memory_order_relaxed);
    if (h - tail.load(std::memory_order_acquire) >= kRingSize) {
      dropped.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    ring[h % kRingSize] = c;
    head.store(h + 1, std::memory_order_release);
    return true;
  }

  void drain() {
    uint32_t t = tail.load(std::memory_order_relaxed);
    const uint32_t h = head.load(std::memory_order_acquire);
    while (t != h) {
      apply(ring[t % kRingSize]);
      ++t;
    }
    tail.store(t, std::memory_order_release);
  }

  void apply(const Command& c) {
    switch (c.kind) {
      case Cmd::tone:
        start_tone(c.tone, c.bus);
        break;
      case Cmd::noise:
        start_noise(c.noise, c.bus);
        break;
      case Cmd::layer_new:
        start_layer(layers[c.slot], c.spec, c.bus);
        break;
      case Cmd::layer_set:
        steer(layers[c.slot], c.target, c.glide);
        break;
      case Cmd::layer_stop: {
        LayerVoice& l = layers[c.slot];
        l.stopping = true;
        l.t_level = 0;
        l.c_level = glide_coeff(kStopFadeS);
        l.free_at = frame + frames_of(kStopFreeS * 1000);
        break;
      }
      case Cmd::bus: {
        Bus& b = buses[c.bus];
        b.target = c.gain < 0 ? 0 : c.gain;
        b.coeff = glide_coeff(c.glide);
        if (c.glide <= 0) b.gain = b.target;  // a fader set, not moved: before anything plays
        break;
      }
      case Cmd::room:
        reverb.set(c.room, c.glide);
        break;
      case Cmd::sample_new:
        start_sample(c);
        break;
      case Cmd::sample_set:
        if (SampleVoice* s = sample_of(c.id)) {
          if (s->stopping) break;
          s->t_gain = c.gain < 0 ? 0 : c.gain;
          s->t_pan = c.pan < -1 ? -1 : c.pan > 1 ? 1 : c.pan;
          s->c_gain = glide_coeff(c.glide);
          s->c_pan = glide_coeff(c.glide);
        }
        break;
      case Cmd::sample_stop:
        if (SampleVoice* s = sample_of(c.id)) {
          if (s->stopping) break;
          s->stopping = true;
          s->t_gain = 0;
          s->c_gain = glide_coeff(c.glide);
          // Seven and a half time constants: the fade is 65 dB down when the slot is freed.
          s->free_at = frame + frames_of(c.glide * 7500);
        }
        break;
    }
  }

  SampleVoice* sample_of(uint32_t id) {
    for (SampleVoice& s : samples) {
      if (s.live && s.id == id) return &s;
    }
    return nullptr;
  }

  // A free slot, or the quietest voice's (the oldest among equals): a far sound gives way first.
  SampleVoice& claim_sample() {
    SampleVoice* pick = &samples[0];
    for (SampleVoice& s : samples) {
      if (!s.live) return s;
      if (s.t_gain < pick->t_gain || (s.t_gain == pick->t_gain && s.order < pick->order)) {
        pick = &s;
      }
    }
    stolen.fetch_add(1, std::memory_order_relaxed);
    return *pick;
  }

  void start_sample(const Command& c) {
    SampleVoice& s = claim_sample();
    s = SampleVoice{};
    s.live = true;
    s.id = c.id;
    s.order = ++order;
    s.pcm = c.pcm;
    s.bus = c.bus;
    s.start = frame + (c.play.delay_ms > 0 ? frames_of(c.play.delay_ms) : 0);
    const float pitch = c.play.pitch > 0.01f ? c.play.pitch : 0.01f;
    const double step = static_cast<double>(c.pcm->rate) * pitch / kRate * 4294967296.0;
    s.step = static_cast<uint64_t>(core::num::round(step));
    if (s.step < 1) s.step = 1;
    const bool whole = c.play.loop == Loop::whole;
    if (whole || (c.play.loop == Loop::file && c.pcm->loops())) {
      s.loop_start = whole ? 0 : c.pcm->loop_start;
      s.loop_end = whole ? c.pcm->frames() : c.pcm->loop_end;
    }
    s.gain = s.t_gain = c.play.gain < 0 ? 0 : c.play.gain;
    s.pan = s.t_pan = c.play.pan < -1 ? -1 : c.play.pan > 1 ? 1 : c.play.pan;
    pan_gains(s.pan, s.gl, s.gr);
    s.echo = clamp01(c.play.echo);
    // The slot's id before the applied mark, so `sample_live` never sees a gap between them.
    sample_ids[static_cast<size_t>(&s - samples.data())].store(c.id, std::memory_order_relaxed);
    applied_sample.store(c.id, std::memory_order_release);
  }

  Voice& claim() {
    Voice* oldest = &voices[0];
    for (Voice& v : voices) {
      if (!v.live) return v;
      if (v.order < oldest->order) oldest = &v;
    }
    stolen.fetch_add(1, std::memory_order_relaxed);
    return *oldest;
  }

  // The gain curve: from the floor to the peak over the attack (never under kMinAttackMs, never
  // over half the voice), the hold, then back to the floor at the end.
  static void envelope(Voice& v, float peak, float duration_ms, float attack_ms, float hold_ms,
                       bool exp_curve) {
    const int64_t n = v.end - v.start;
    float rise_ms = attack_ms > kMinAttackMs ? attack_ms : kMinAttackMs;
    if (rise_ms > duration_ms * 0.5f) rise_ms = duration_ms * 0.5f;
    int64_t rise = frames_of(rise_ms);
    rise = rise < 1 ? 1 : rise > n ? n : rise;
    int64_t hold = 0;
    if (hold_ms > 0) {
      int64_t until = rise + frames_of(hold_ms);
      const int64_t last = n - frames_of(5);
      if (until > last) until = last;
      if (until > rise) hold = until - rise;
    }
    v.exp_attack = v.exp_decay = exp_curve;
    v.floor = exp_curve ? kEnvFloor : 0;
    v.peak = exp_curve ? (peak > 1e-5f ? peak : 1e-5f) : peak;
    v.env = v.floor;
    v.attack_end = v.start + rise;
    v.hold_end = v.attack_end + hold;
    const int64_t decay = v.end - v.hold_end;
    if (exp_curve) {
      v.attack_step = ramp_ratio(v.floor, v.peak, rise);
      v.decay_step = ramp_ratio(v.peak, v.floor, decay);
    } else {
      v.attack_step = (v.peak - v.floor) / static_cast<float>(rise);
      v.decay_step = decay > 0 ? (v.floor - v.peak) / static_cast<float>(decay) : 0;
    }
  }

  void place(Voice& v, float delay_ms, float duration_ms) {
    v.live = true;
    v.order = ++order;
    v.start = frame + (delay_ms > 0 ? frames_of(delay_ms) : 0);
    int64_t n = frames_of(duration_ms);
    v.end = v.start + (n < 1 ? 1 : n);
  }

  void filter(Voice& v, const Filter& f) {
    svf_init(v.svf, f.type, f.q);
    v.cut = safe_cutoff(f.hz);
    v.cut_mul = f.to_hz > 0 ? ramp_ratio(v.cut, safe_cutoff(f.to_hz), v.end - v.start) : 1;
    v.ctl = 0;
    if (f.type != FilterType::none) svf_tune(v.svf, v.cut);
  }

  void start_tone(const Tone& t, int bus) {
    if (t.duration_ms <= 0 || t.volume <= 0) return;
    Voice& v = claim();
    v = Voice{};
    place(v, t.delay_ms, t.duration_ms);
    v.tone = true;
    v.bus = bus;
    v.wave = t.wave;
    v.oscs = t.detune_cents > 0 ? 2 : 1;
    if (v.oscs == 2) {
      v.detune[0] = static_cast<float>(libm::exp(t.detune_cents * 0.000577622650466621));
      v.detune[1] = 1 / v.detune[0];
    }
    v.hz = t.from_hz > 1 ? t.from_hz : 1;
    const float to = t.to_hz > 0 ? (t.to_hz > 1 ? t.to_hz : 1) : v.hz;
    v.hz_mul = ramp_ratio(v.hz, to, v.end - v.start);
    if (t.vibrato.rate_hz > 0 && t.vibrato.depth_cents != 0) {
      v.lfo_dt = t.vibrato.rate_hz / kRate;
      v.vib_depth = t.vibrato.depth_cents;
      v.vib_from = v.start + frames_of(t.vibrato.delay_ms);
      v.vib_full = v.vib_from + frames_of(80);
      if (v.vib_full > v.end) v.vib_full = v.end;
    }
    if (t.drive > 0) {
      v.curve = curve_for(tables, t.drive);
      v.push = shaper_push(t.drive);
    }
    filter(v, t.filter);
    envelope(v, v.oscs == 2 ? t.volume * kPairShare : t.volume, t.duration_ms, t.attack_ms,
             t.hold_ms, true);
    pan_gains(t.pan, v.gl, v.gr);
    v.echo = clamp01(t.echo);
  }

  void start_noise(const Noise& n, int bus) {
    if (n.duration_ms <= 0 || n.volume <= 0) return;
    Voice& v = claim();
    v = Voice{};
    place(v, n.delay_ms, n.duration_ms);
    v.tone = false;
    v.bus = bus;
    v.color = static_cast<int>(n.color);
    v.pos = static_cast<uint32_t>(core::next(rng) * kPoolFrames);
    filter(v, n.filter);
    envelope(v, n.volume, n.duration_ms, n.attack_ms, n.hold_ms, n.attack_ms > 0 || n.hold_ms > 0);
    pan_gains(n.pan, v.gl, v.gr);
    v.echo = clamp01(n.echo);
  }

  void start_layer(LayerVoice& l, const LayerSpec& spec, int bus) {
    l = LayerVoice{};
    l.live = true;
    l.bus = bus;
    l.spec = spec;
    l.oscs = spec.kind == LayerSpec::Kind::tone && spec.detune_cents > 0 ? 2 : 1;
    if (l.oscs == 2) {
      l.detune[0] = static_cast<float>(libm::exp(spec.detune_cents * 0.000577622650466621));
      l.detune[1] = 1 / l.detune[0];
    }
    if (spec.vibrato.rate_hz > 0) l.lfo_dt = spec.vibrato.rate_hz / kRate;
    if (spec.kind == LayerSpec::Kind::tone && spec.drive) l.curve = curve_for(tables, kLayerDrive);
    if (spec.kind == LayerSpec::Kind::noise) {
      l.pos = static_cast<uint32_t>(core::next(rng) * kPoolFrames);
    }
    svf_init(l.svf, spec.filter, spec.q);
    if (spec.filter != FilterType::none) svf_tune(l.svf, l.cut);
  }

  static void steer(LayerVoice& l, const LayerTarget& t, float glide) {
    if (!l.live || l.stopping) return;
    const float c = glide_coeff(glide);
    l.t_level = t.level > 0 ? t.level : 0;
    l.c_level = c;
    l.t_hz = t.hz > 1 ? t.hz : 1;
    l.c_hz = glide_coeff(glide * 0.5f);
    l.t_cut = safe_cutoff(t.cutoff);
    l.c_cut = c;
    l.t_push = kGritFloor + (kGritPush - kGritFloor) * clamp01(t.grit);
    l.c_push = c;
    l.t_pan = t.pan < -1 ? -1 : t.pan > 1 ? 1 : t.pan;
    l.c_pan = c;
    if (l.fresh) {
      l.fresh = false;
      l.hz = l.t_hz;
      l.cut = l.t_cut;
      l.push = l.t_push;
      l.pan = l.t_pan;
      if (l.spec.filter != FilterType::none) svf_tune(l.svf, l.cut);
      pan_gains(l.pan, l.gl, l.gr);
    }
  }

  void render_voice(Voice& v, int64_t b0, int count) {
    const int64_t b1 = b0 + count;
    const int64_t s = v.start > b0 ? v.start : b0;
    const int64_t e = v.end < b1 ? v.end : b1;
    const float* bus = bus_gain[v.bus].data();
    const float* pool = tables.noise[v.color].data();
    const bool filtered = v.svf.type != FilterType::none;
    for (int64_t f = s; f < e; f++) {
      // the envelope
      if (f < v.attack_end) {
        v.env = v.exp_attack ? v.env * v.attack_step : v.env + v.attack_step;
        if (f + 1 == v.attack_end) v.env = v.peak;
      } else if (f >= v.hold_end) {
        v.env = v.exp_decay ? v.env * v.decay_step : v.env + v.decay_step;
        if (v.env < 0) v.env = 0;
      }
      float x;
      if (v.tone) {
        float vib = 1;
        if (v.lfo_dt > 0 && f >= v.vib_from) {
          const float depth = f >= v.vib_full ? v.vib_depth
                                              : v.vib_depth * static_cast<float>(f - v.vib_from) /
                                                    static_cast<float>(v.vib_full - v.vib_from);
          vib = cents_ratio(depth * sine_at(tables, v.lfo_phase));
          v.lfo_phase = advance(v.lfo_phase, v.lfo_dt);
        }
        x = 0;
        for (int j = 0; j < v.oscs; j++) {
          const float dt = v.hz * v.detune[j] * vib / kRate;
          x += oscillate(tables, v.wave, v.phase[j], dt);
          v.phase[j] = advance(v.phase[j], dt);
        }
        v.hz *= v.hz_mul;
        if (v.curve) x = shape(v.curve, x * v.push);
      } else {
        x = pool[v.pos];
        if (++v.pos == kPoolFrames) v.pos = 0;
      }
      if (filtered) {
        v.cut *= v.cut_mul;
        if (++v.ctl == kControl) {
          v.ctl = 0;
          svf_tune(v.svf, v.cut);
        }
        x = svf_tick(v.svf, x);
      }
      const int i = static_cast<int>(f - b0);
      x *= v.env * bus[i];
      mix_l[i] += x * v.gl;
      mix_r[i] += x * v.gr;
      mix_e[i] += x * v.echo;
    }
    if (v.end <= b1) v.live = false;
  }

  void render_layer(LayerVoice& l, int64_t b0, int count) {
    if (l.stopping && b0 >= l.free_at) {
      l.live = false;
      return;
    }
    if (l.level < kSilent && l.t_level <= 0) {
      l.level = 0;
      return;
    }
    const float* bus = bus_gain[l.bus].data();
    const bool tone = l.spec.kind == LayerSpec::Kind::tone;
    const bool filtered = l.svf.type != FilterType::none;
    const float* pool = tables.noise[static_cast<int>(l.spec.color)].data();
    const float share = l.oscs == 2 ? kPairShare : 1;
    for (int i = 0; i < count; i++) {
      l.level += (l.t_level - l.level) * l.c_level;
      float x;
      if (tone) {
        l.hz += (l.t_hz - l.hz) * l.c_hz;
        float vib = 1;
        if (l.lfo_dt > 0) {
          vib = cents_ratio(l.spec.vibrato.depth_cents * sine_at(tables, l.lfo_phase));
          l.lfo_phase = advance(l.lfo_phase, l.lfo_dt);
        }
        x = 0;
        for (int j = 0; j < l.oscs; j++) {
          const float dt = l.hz * l.detune[j] * vib / kRate;
          x += oscillate(tables, l.spec.wave, l.phase[j], dt);
          l.phase[j] = advance(l.phase[j], dt);
        }
        if (l.curve) {
          l.push += (l.t_push - l.push) * l.c_push;
          x = shape(l.curve, x * l.push) * kGritTrim;
        }
      } else {
        x = pool[l.pos];
        if (++l.pos == kPoolFrames) l.pos = 0;
      }
      if (++l.ctl == kControl) {
        l.ctl = 0;
        // The cutoff and the pan move a control step at a time: their glides are tenths of a
        // second, a control step a third of a millisecond.
        for (int k = 0; k < kControl; k++) {
          l.cut += (l.t_cut - l.cut) * l.c_cut;
          l.pan += (l.t_pan - l.pan) * l.c_pan;
        }
        if (filtered) svf_tune(l.svf, l.cut);
        pan_gains(l.pan, l.gl, l.gr);
      }
      if (filtered) x = svf_tick(l.svf, x);
      x *= l.level * share * bus[i];
      mix_l[i] += x * l.gl;
      mix_r[i] += x * l.gr;
      mix_e[i] += x * l.spec.echo;
    }
  }

  void render_sample(SampleVoice& s, int64_t b0, int count) {
    if (s.stopping && b0 >= s.free_at) {
      s.live = false;
      return;
    }
    const float* bus = bus_gain[s.bus].data();
    const int64_t length = s.pcm->frames();
    for (int i = s.start > b0 ? static_cast<int>(s.start - b0) : 0; i < count; i++) {
      const int64_t k = static_cast<int64_t>(s.pos >> 32);
      if (s.loop_start < 0 && k >= length) {
        s.live = false;
        return;
      }
      // Catmull-Rom through the four frames round the point: smooth enough that a recording
      // played at twice its rate or more keeps no stair steps, and exact on the frames.
      const float t = static_cast<float>(static_cast<uint32_t>(s.pos)) * (1.0f / 4294967296.0f);
      const float a = sample_at(s, k - 1), b = sample_at(s, k), c = sample_at(s, k + 1),
                  d = sample_at(s, k + 2);
      float x =
          b + 0.5f * t * (c - a + t * (2 * a - 5 * b + 4 * c - d + t * (3 * (b - c) + d - a)));
      s.pos += s.step;
      if (s.loop_start >= 0 && static_cast<int64_t>(s.pos >> 32) >= s.loop_end) {
        s.pos -= static_cast<uint64_t>(s.loop_end - s.loop_start) << 32;
      }
      s.gain += (s.t_gain - s.gain) * s.c_gain;
      if (++s.ctl == kControl) {
        s.ctl = 0;
        for (int j = 0; j < kControl; j++) s.pan += (s.t_pan - s.pan) * s.c_pan;
        pan_gains(s.pan, s.gl, s.gr);
      }
      x *= s.gain * bus[i];
      mix_l[i] += x * s.gl;
      mix_r[i] += x * s.gr;
      mix_e[i] += x * s.echo;
    }
  }

  void block(float* out, int count) {
    drain();
    for (int i = 0; i < count; i++) mix_l[i] = mix_r[i] = mix_e[i] = 0;
    for (int b = 0; b < kBuses; b++) {
      Bus& bus = buses[b];
      for (int i = 0; i < count; i++) {
        bus.gain += (bus.target - bus.gain) * bus.coeff;
        bus_gain[b][i] = bus.gain;
      }
    }
    uint32_t live = 0;
    for (Voice& v : voices) {
      if (!v.live || v.start >= frame + count) continue;
      render_voice(v, frame, count);
      live += v.live;
    }
    uint32_t built = 0;
    for (LayerVoice& l : layers) {
      if (!l.live) continue;
      render_layer(l, frame, count);
      built += l.live;
    }
    uint32_t sounding = 0;
    for (int k = 0; k < kMaxSamples; k++) {
      SampleVoice& s = samples[k];
      if (s.live && s.start < frame + count) render_sample(s, frame, count);
      sounding += s.live;
      sample_ids[k].store(s.live ? s.id : 0, std::memory_order_relaxed);
    }
    samples_live.store(sounding, std::memory_order_relaxed);
    // The room (reverb.hpp): only while one is set, so a synth with none keeps its bits.
    const bool room = reverb.active();
    if (room) {
      for (int i = 0; i < count; i++) room_l[i] = room_r[i] = 0;
      reverb.process(mix_e.data(), room_l.data(), room_r.data(), count);
    }
    const float echo_share = room ? reverb.echo_share() : 1.0f;
    const int size = static_cast<int>(line.size());
    for (int i = 0; i < count; i++) {
      int read = line_write - line_delay;
      if (read < 0) read += size;
      echo_damp += (line[read] - echo_damp) * echo_coeff;
      line[line_write] = mix_e[i] + echo_feedback * echo_damp;
      if (++line_write == size) line_write = 0;
      float l = mix_l[i] + echo_damp * echo_share;
      float r = mix_r[i] + echo_damp * echo_share;
      if (room) {
        l += room_l[i];
        r += room_r[i];
      }
      // The limiter: a soft-knee gain computer on the louder channel, smoothed in dB.
      const float a = l < 0 ? -l : l, b = r < 0 ? -r : r;
      const float peak = a > b ? a : b;
      float want = 0;
      if (peak > limit_below) {
        const float x = db_of(peak);
        const float over = x - kLimitThresholdDb;
        float y;
        if (2 * over < -kLimitKneeDb) {
          y = x;
        } else if (2 * (over < 0 ? -over : over) <= kLimitKneeDb) {
          const float d = over + kLimitKneeDb / 2;
          y = x + (1 / kLimitRatio - 1) * d * d / (2 * kLimitKneeDb);
        } else {
          y = kLimitThresholdDb + over / kLimitRatio;
        }
        want = y - x;
      }
      limit_g += (want - limit_g) * (want < limit_g ? limit_attack : limit_release);
      if (limit_g < -1e-4f) {
        const float g = gain_of_db(limit_g);
        l *= g;
        r *= g;
      }
      const size_t at = 2 * static_cast<size_t>(i);
      out[at] = l < -1 ? -1 : l > 1 ? 1 : l;
      out[at + 1] = r < -1 ? -1 : r > 1 ? 1 : r;
    }
    frame += count;
    frames.store(static_cast<uint64_t>(frame), std::memory_order_release);
    voices_live.store(live, std::memory_order_relaxed);
    layers_live.store(built, std::memory_order_relaxed);
    reduction.store(limit_g, std::memory_order_relaxed);
  }
};

Synth::Synth(uint32_t seed, EchoRoom room) : impl_(std::make_unique<Impl>(seed, room)) {}
Synth::~Synth() = default;

static uint8_t bus_of(int bus) {
  return static_cast<uint8_t>(bus < 0 ? 0 : bus >= kBuses ? kBuses - 1 : bus);
}

void Synth::tone(const Tone& tone, int bus) {
  Command c;
  c.kind = Cmd::tone;
  c.bus = bus_of(bus);
  c.tone = tone;
  impl_->push(c);
}

void Synth::noise(const Noise& noise, int bus) {
  Command c;
  c.kind = Cmd::noise;
  c.bus = bus_of(bus);
  c.noise = noise;
  impl_->push(c);
}

LayerId Synth::layer(const LayerSpec& spec, int bus) {
  for (int i = 0; i < kMaxLayers; i++) {
    if (impl_->used[i]) continue;
    impl_->used[i] = true;
    impl_->has_sent[i] = false;
    Command c;
    c.kind = Cmd::layer_new;
    c.bus = bus_of(bus);
    c.slot = static_cast<int16_t>(i);
    c.spec = spec;
    impl_->push(c);
    return LayerId{static_cast<int16_t>(i)};
  }
  return LayerId{};
}

static bool near(float a, float b) {
  const float d = a > b ? a - b : b - a;
  const float m = a < 0 ? -a : a;
  return d <= kSetEpsilon * (m > 1 ? m : 1);
}

void Synth::set(LayerId id, const LayerTarget& t, float glide_s) {
  if (!id.valid() || !impl_->used[id.slot]) return;
  LayerTarget& last = impl_->sent[id.slot];
  if (impl_->has_sent[id.slot] && near(last.level, t.level) && near(last.hz, t.hz) &&
      near(last.cutoff, t.cutoff) && near(last.grit, t.grit) && near(last.pan, t.pan)) {
    return;
  }
  last = t;
  impl_->has_sent[id.slot] = true;
  Command c;
  c.kind = Cmd::layer_set;
  c.slot = id.slot;
  c.target = t;
  c.glide = glide_s;
  impl_->push(c);
}

void Synth::stop(LayerId id) {
  if (!id.valid() || !impl_->used[id.slot]) return;
  impl_->used[id.slot] = false;
  Command c;
  c.kind = Cmd::layer_stop;
  c.slot = id.slot;
  impl_->push(c);
}

void Synth::bus_gain(int bus, float gain, float glide_s) {
  Command c;
  c.kind = Cmd::bus;
  c.bus = bus_of(bus);
  c.gain = gain;
  c.glide = glide_s;
  impl_->push(c);
}

void Synth::room(const RoomSound& room, float glide_s) {
  Command c;
  c.kind = Cmd::room;
  c.room = room;
  c.glide = glide_s;
  impl_->push(c);
}

SampleId Synth::sample(const Pcm& pcm, const SamplePlay& play, int bus) {
  if (pcm.samples.empty() || pcm.rate < 1) return SampleId{};
  Command c;
  c.kind = Cmd::sample_new;
  c.bus = bus_of(bus);
  c.id = impl_->next_sample;
  c.pcm = &pcm;
  c.play = play;
  if (!impl_->push(c)) return SampleId{};
  if (++impl_->next_sample == 0) impl_->next_sample = 1;
  return SampleId{c.id};
}

void Synth::sample_set(SampleId id, float gain, float pan, float glide_s) {
  if (!id.valid()) return;
  Command c;
  c.kind = Cmd::sample_set;
  c.id = id.id;
  c.gain = gain;
  c.pan = pan;
  c.glide = glide_s;
  impl_->push(c);
}

void Synth::sample_stop(SampleId id, float fade_s) {
  if (!id.valid()) return;
  Command c;
  c.kind = Cmd::sample_stop;
  c.id = id.id;
  c.glide = fade_s > 0.001f ? fade_s : 0.001f;
  impl_->push(c);
}

bool Synth::sample_live(SampleId id) const {
  if (!id.valid()) return false;
  // Sent and not applied yet: it will sound at the next block.
  if (id.id > impl_->applied_sample.load(std::memory_order_acquire)) return true;
  for (const std::atomic<uint32_t>& s : impl_->sample_ids) {
    if (s.load(std::memory_order_relaxed) == id.id) return true;
  }
  return false;
}

void Synth::render(float* out, int frames) {
  while (frames > 0) {
    const int n = frames < kBlock ? frames : kBlock;
    impl_->block(out, n);
    out += 2 * static_cast<size_t>(n);
    frames -= n;
  }
}

double Synth::now_s() const {
  return static_cast<double>(impl_->frames.load(std::memory_order_acquire)) / kRate;
}

Stats Synth::stats() const {
  Stats s;
  s.frames = impl_->frames.load(std::memory_order_acquire);
  s.voices = impl_->voices_live.load(std::memory_order_relaxed);
  s.layers = impl_->layers_live.load(std::memory_order_relaxed);
  s.samples = impl_->samples_live.load(std::memory_order_relaxed);
  s.dropped = impl_->dropped.load(std::memory_order_relaxed);
  s.stolen = impl_->stolen.load(std::memory_order_relaxed);
  s.reduction_db = impl_->reduction.load(std::memory_order_relaxed);
  return s;
}

}  // namespace ge::audio
