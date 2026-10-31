// THE VOICES IN A RECORDING, HEARD: each voice message a demo frame carries (demo.hpp's
// `DemoVoice`: a talker's run of 20 ms Opus frames) decoded and played on its talker's own synth
// stream, so a client recording plays back with its team's talk and a multiview recording with
// everyone's, in step with the pictures and the game's sounds.
//
// A TALKER is a slot. Its frames play one after another as they arrive: a message's frames are fed
// to its stream at once and the stream plays them out at 48 kHz, so a recording that kept the
// voice as it was sent plays it back without gaps. The SEQUENCE byte tells a lost frame: a gap of
// up to `kVoiceConcealMost` frames in one generation is filled with the decoder's concealment, a
// longer one (or a new generation: the talker's stream started over) starts the talker afresh.
// Voice is heard centred at `volume`; who may hear whom was the server's decision when it was
// recorded (net/classic_record.hpp).
//
// A build without the codec (GE_ENCODERS=OFF) decodes silence and says so (`codec`), and the
// counts stay the same.
//
// ge:summary The voices in a recording decoded: a stream a talker, gaps concealed, in step
// ge:keywords demo-voice voice voice-chat opus decode talker stream conceal sequence generation
// ge:keywords team-voice multiview client-recording playback export
// ge:skill audio shooter net
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "audio/synth.hpp"
#include "net/voice_codec.hpp"
#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

inline constexpr int kVoiceTalkers = 32;
inline constexpr int kVoiceConcealMost = 3;  // frames concealed in a gap before a fresh start

struct DemoVoiceStats {
  int64_t messages = 0, frames = 0, concealed = 0, bad = 0;
  uint32_t talkers = 0;  // a bit a slot that spoke
};

class DemoVoices {
 public:
  DemoVoices(ge::audio::Synth& synth, float volume = 1, int bus = 0);
  ~DemoVoices();
  DemoVoices(const DemoVoices&) = delete;
  DemoVoices& operator=(const DemoVoices&) = delete;

  // One voice message played from now on.
  void play(const DemoVoice& v);
  const DemoVoiceStats& stats() const { return stats_; }
  bool codec() const { return ge::net::voice_codec_available(); }

 private:
  struct Talker {
    ge::net::VoiceDecoder decoder;
    ge::audio::StreamId stream;
    int gen = -1;
    int next_seq = -1;
  };
  ge::audio::Synth& synth_;
  float volume_;
  int bus_;
  Talker talkers_[kVoiceTalkers];
  DemoVoiceStats stats_;
};

}  // namespace shooters_edge::client
