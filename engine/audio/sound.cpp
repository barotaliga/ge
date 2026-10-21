// A play's shape applied to a voice, and the bank's lookup.
//
// ge:summary The play's shape on a voice, play_def's loop and the bank's lookup
// ge:keywords sound shaped play_def find_sound
// ge:skill audio
// ge:test tests/audio_sound_test.cpp
#include "audio/sound.hpp"

namespace ge::audio {

namespace {

Filter pitched(Filter f, float pitch) {
  f.hz *= pitch;
  if (f.to_hz > 0) f.to_hz *= pitch;
  return f;
}

}  // namespace

SoundVoice shaped(const SoundVoice& voice, const PlayShape& s) {
  SoundVoice out = voice;
  if (voice.kind == SoundVoice::Kind::tone) {
    Tone& t = out.tone;
    t.volume *= s.gain;
    t.duration_ms *= s.stretch;
    t.delay_ms *= s.stretch;
    t.from_hz *= s.pitch;
    if (t.to_hz > 0) t.to_hz *= s.pitch;
    t.filter = pitched(t.filter, s.pitch);
    if (s.place) t.pan = s.pan;
  } else {
    Noise& n = out.noise;
    n.volume *= s.gain;
    n.duration_ms *= s.stretch;
    n.delay_ms *= s.stretch;
    n.filter = pitched(n.filter, s.pitch);
    if (s.place) n.pan = s.pan;
  }
  return out;
}

void play_def(Synth& synth, const SoundDef& def, const PlayShape& shape, int bus) {
  for (const SoundVoice& authored : def.voices) {
    const SoundVoice v = shaped(authored, shape);
    if (v.kind == SoundVoice::Kind::tone) {
      synth.tone(v.tone, bus);
    } else {
      synth.noise(v.noise, bus);
    }
  }
}

const SoundDef* find_sound(std::span<const SoundDef> bank, std::string_view id) {
  for (const SoundDef& def : bank) {
    if (def.id == id) return &def;
  }
  return nullptr;
}

}  // namespace ge::audio
