// The RIFF header and the samples, little-endian.
//
// ge:summary The WAV encoder: the RIFF header and the 16-bit samples
// ge:keywords wav riff header encode
// ge:skill audio labs
// ge:test tests/audio_meter_test.cpp
#include "audio/wav.hpp"

#include "audio/voice.hpp"
#include "core/num.hpp"

namespace ge::audio {

namespace {

void put32(std::vector<uint8_t>& b, uint32_t v) {
  for (int i = 0; i < 4; i++) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void put16(std::vector<uint8_t>& b, uint16_t v) {
  b.push_back(static_cast<uint8_t>(v));
  b.push_back(static_cast<uint8_t>(v >> 8));
}
void tag(std::vector<uint8_t>& b, const char* t) {
  for (int i = 0; i < 4; i++) b.push_back(static_cast<uint8_t>(t[i]));
}

}  // namespace

std::vector<uint8_t> wav_bytes(std::span<const float> stereo) {
  const uint32_t data = static_cast<uint32_t>(stereo.size() * 2);
  std::vector<uint8_t> b;
  b.reserve(44 + data);
  tag(b, "RIFF");
  put32(b, 36 + data);
  tag(b, "WAVE");
  tag(b, "fmt ");
  put32(b, 16);
  put16(b, 1);  // PCM
  put16(b, 2);  // channels
  put32(b, kRate);
  put32(b, kRate * 4);  // bytes a second
  put16(b, 4);          // bytes a frame
  put16(b, 16);         // bits a sample
  tag(b, "data");
  put32(b, data);
  for (const float x : stereo) {
    const double c = x < -1 ? -1 : x > 1 ? 1 : x;
    const int32_t s = static_cast<int32_t>(core::num::round(c * 32767));
    put16(b, static_cast<uint16_t>(static_cast<int16_t>(s)));
  }
  return b;
}

}  // namespace ge::audio
