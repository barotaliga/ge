// The voices in a recording, heard (demo_voice.hpp).
//
// ge:summary The demo's voice messages decoded onto a stream a talker, the gaps concealed
// ge:keywords demo-voice decode stream conceal sequence talker
// ge:skill audio shooter net
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_voice.hpp"

#include <span>
#include <vector>

namespace shooters_edge::client {

namespace gn = ge::net;

DemoVoices::DemoVoices(ge::audio::Synth& synth, float volume, int bus)
    : synth_(synth), volume_(volume), bus_(bus) {}

DemoVoices::~DemoVoices() {
  for (Talker& t : talkers_)
    if (t.stream.valid()) synth_.stop(t.stream);
}

void DemoVoices::play(const DemoVoice& v) {
  stats_.messages++;
  std::vector<std::span<const uint8_t>> frames;
  if (v.sender < 0 || v.sender >= kVoiceTalkers || (v.gen >> 4) != kVoiceOpus ||
      !voice_frames(v, frames)) {
    stats_.bad++;
    return;
  }
  Talker& t = talkers_[v.sender];
  std::string error;
  if (!t.decoder.is_open() && !t.decoder.open(error)) {
    stats_.bad++;
    return;
  }
  if (!t.stream.valid()) {
    t.stream = synth_.stream(bus_);
    if (!t.stream.valid()) return;
    synth_.steer(t.stream, volume_, 0, 0);
  }
  stats_.talkers |= 1u << v.sender;
  float pcm[gn::kVoiceFrame];
  if (t.gen == v.gen && t.next_seq >= 0) {
    const int gap = (v.seq - t.next_seq) & 0xFF;
    if (gap > 0 && gap <= kVoiceConcealMost) {
      for (int k = 0; k < gap; k++) {
        t.decoder.conceal(pcm);
        synth_.feed(t.stream, pcm, gn::kVoiceFrame);
        stats_.concealed++;
      }
    }
  }
  t.gen = v.gen;
  for (const std::span<const uint8_t> f : frames) {
    if (!t.decoder.decode(f.data(), f.size(), pcm)) stats_.bad++;
    synth_.feed(t.stream, pcm, gn::kVoiceFrame);
    stats_.frames++;
  }
  t.next_seq = (v.seq + static_cast<int>(frames.size())) & 0xFF;
}

}  // namespace shooters_edge::client
