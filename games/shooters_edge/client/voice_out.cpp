// A player's voice as the protocol sends it (voice_out.hpp).
//
// ge:summary The voice message cut: frames gathered, encoded while talking, the sequence kept
// ge:keywords voice voice-out microphone opus frame
// ge:skill net audio shooter
#include "shooters_edge/client/voice_out.hpp"

#include <string>

namespace shooters_edge::client {

bool voice_out_open(VoiceOut& v) {
  v.encoder = std::make_unique<ge::net::VoiceEncoder>();
  std::string error;
  if (!v.encoder->open(ge::net::VoiceCodecConfig{}, error)) v.encoder.reset();
  return v.encoder != nullptr;
}

int voice_out_cut(VoiceOut& v, std::span<const float> samples, bool talking,
                  std::vector<uint8_t>& data, int& first_seq) {
  data.clear();
  first_seq = v.seq;
  v.mic.insert(v.mic.end(), samples.begin(), samples.end());
  int frames = 0;
  size_t at = 0;
  for (; v.mic.size() - at >= static_cast<size_t>(ge::net::kVoiceFrame);
       at += ge::net::kVoiceFrame) {
    if (!talking || !v.encoder) continue;
    uint8_t frame[ge::net::kVoiceMaxBytes];
    const int n = v.encoder->encode(v.mic.data() + at, frame, sizeof frame);
    if (n <= 0) continue;
    data.push_back(static_cast<uint8_t>(n & 0xFF));
    data.push_back(static_cast<uint8_t>(n >> 8));
    data.insert(data.end(), frame, frame + n);
    frames++;
  }
  v.mic.erase(v.mic.begin(), v.mic.begin() + static_cast<std::ptrdiff_t>(at));
  v.seq = (v.seq + frames) & 0xFF;
  v.sent += frames;
  return frames;
}

}  // namespace shooters_edge::client
