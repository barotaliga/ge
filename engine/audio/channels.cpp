// Sounds in a world: the hearing law and the channel table over the synth's sampled voices.
//
// ge:summary Sounds in a world: the distance gain, the pan, the channel table's replace and steal
// ge:keywords spatial hear listener attenuation pan channels steal replace update
// ge:skill audio
// ge:test tests/audio_channels_test.cpp
#include "audio/channels.hpp"

#include "core/math.hpp"

namespace ge::audio {

namespace {

// Nearer than this a source is on the listener: centred.
constexpr double kOnListener = 1e-3;
// A heard gain under this is silence (-100 dB).
constexpr float kInaudible = 1e-5f;

}  // namespace

Heard hear(const Listener& listener, core::Vec3 at, float volume, float attenuation, float width) {
  const double dx = at.x - listener.at.x, dy = at.y - listener.at.y, dz = at.z - listener.at.z;
  const double d = core::hypot3(dx, dy, dz);
  Heard h;
  const double fall = 1 - d * static_cast<double>(attenuation);
  h.gain = fall > 0 ? volume * static_cast<float>(fall) : 0;
  if (d > kOnListener) {
    const double side = (dx * listener.right.x + dy * listener.right.y + dz * listener.right.z) / d;
    h.pan = static_cast<float>(core::clamp(side * width, -1, 1));
  }
  return h;
}

Channels::Channels(Synth& synth, int limit, int bus, float width)
    : synth_(synth), bus_(bus), width_(width) {
  slots_.resize(static_cast<size_t>(limit < 1 ? 1 : limit > kMaxSamples ? kMaxSamples : limit));
}

Heard Channels::heard_of(int entity, core::Vec3 at, float volume, float attenuation) const {
  if (entity >= 0 && entity == listener_.entity) return Heard{volume, 0};
  return hear(listener_, at, volume, attenuation, width_);
}

SampleId Channels::play(const Pcm& pcm, const Emit& e) {
  const bool loops = e.loop == Loop::whole || (e.loop == Loop::file && pcm.loops());
  const Heard h = heard_of(e.entity, e.at, e.volume, e.attenuation);
  // A sound that ends before anyone could walk into earshot is not worth a slot.
  if (!loops && h.gain < kInaudible) {
    skipped_++;
    return SampleId{};
  }
  Slot* pick = nullptr;
  if (e.channel != 0 && e.entity >= 0) {
    for (Slot& s : slots_) {
      if (s.voice.valid() && s.entity == e.entity && s.channel == e.channel) pick = &s;
    }
  }
  if (!pick) {
    for (Slot& s : slots_) {
      if (!s.voice.valid()) {
        pick = &s;
        break;
      }
    }
  }
  if (!pick) {
    Slot* quiet = &slots_[0];
    for (Slot& s : slots_) {
      if (s.heard.gain < quiet->heard.gain ||
          (s.heard.gain == quiet->heard.gain && s.order < quiet->order)) {
        quiet = &s;
      }
    }
    if (quiet->heard.gain > h.gain) {
      skipped_++;
      return SampleId{};
    }
    stolen_++;
    pick = quiet;
  }
  if (pick->voice.valid()) synth_.sample_stop(pick->voice);
  SamplePlay play;
  play.gain = h.gain;
  play.pan = h.pan;
  play.pitch = e.pitch;
  play.echo = e.echo;
  play.loop = e.loop;
  Slot s;
  s.voice = synth_.sample(pcm, play, bus_);
  s.entity = e.entity;
  s.channel = e.channel;
  s.at = e.at;
  s.volume = e.volume;
  s.attenuation = e.attenuation;
  s.loops = loops;
  s.order = ++order_;
  s.heard = h;
  *pick = s;
  return s.voice;
}

void Channels::move(int entity, core::Vec3 at) {
  for (Slot& s : slots_) {
    if (s.voice.valid() && s.entity == entity) s.at = at;
  }
}

void Channels::stop(int entity, int channel, float fade_s) {
  for (Slot& s : slots_) {
    if (!s.voice.valid() || s.entity != entity || (channel != 0 && s.channel != channel)) continue;
    synth_.sample_stop(s.voice, fade_s);
    s = Slot{};
  }
}

void Channels::update(const Listener& listener, float glide_s) {
  listener_ = listener;
  for (Slot& s : slots_) {
    if (!s.voice.valid()) continue;
    if (!synth_.sample_live(s.voice)) {
      s = Slot{};
      continue;
    }
    s.heard = heard_of(s.entity, s.at, s.volume, s.attenuation);
    synth_.sample_set(s.voice, s.heard.gain, s.heard.pan, glide_s);
  }
}

int Channels::playing() const {
  int n = 0;
  for (const Slot& s : slots_) n += s.voice.valid();
  return n;
}

}  // namespace ge::audio
