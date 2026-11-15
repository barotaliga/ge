// FLAC (engine/audio/flac.hpp): what the writer writes reads back bit for bit (stereo that suits
// each channel layout, silence, a short last block, mono, 24 bits, the extremes of 16 bits), with
// its tags; the stream is smaller than the samples for music-like sound; a flipped byte is
// refused by its frame's CRC.
//
// ge:summary Tests FLAC: lossless round trips of every layout and depth, tags, size, damage
// ge:keywords test flac lossless round-trip stereo mid-side mono 24-bit crc refuse tags
// ge:skill audio testing
#include <cstdint>
#include <string>
#include <vector>

#include "audio/flac.hpp"
#include "check.hpp"

namespace audio = ge::audio;

namespace {

// Two voices that move together like music, with a little noise.
std::vector<int32_t> music(size_t frames, int channels, int amp, uint32_t seed) {
  std::vector<int32_t> s(frames * size_t(channels));
  int32_t a = 0, b = 0;
  for (size_t i = 0; i < frames; i++) {
    seed = seed * 1103515245u + 12345u;
    a += int32_t((i / 37) % 9) - 4;
    b += int32_t((i / 53) % 7) - 3;
    const int32_t noise = int32_t(seed >> 28) - 8;
    for (int c = 0; c < channels; c++) {
      int64_t v = int64_t(a) * 40 + (c ? b * 25 : 0) + noise;
      v = v % amp;
      s[i * size_t(channels) + size_t(c)] = int32_t(v);
    }
  }
  return s;
}

void round_trip(const std::vector<int32_t>& s, int channels, int bits) {
  const std::vector<uint8_t> f =
      audio::flac_encode(s, channels, 44100, bits, {{"TRACKNUMBER", "02"}, {"GAME", "classic"}});
  std::vector<int32_t> got;
  audio::FlacInfo info;
  std::string error;
  CHECK_MSG(audio::flac_decode(f, &got, &info, &error), error);
  CHECK(got == s);
  CHECK(info.channels == channels && info.bits == bits && info.rate == 44100);
  CHECK(info.frames == s.size() / size_t(channels));
  CHECK(info.tags.size() == 2 && info.tags[0].first == "TRACKNUMBER" &&
        info.tags[0].second == "02");
}

}  // namespace

TEST(flac_round_trips_stereo_mono_and_depths) {
  round_trip(music(4096 * 3 + 1234, 2, 30000, 1), 2, 16);
  round_trip(music(5000, 1, 30000, 2), 1, 16);
  round_trip(music(9000, 2, 8000000, 3), 2, 24);
  round_trip(std::vector<int32_t>(size_t{2} * 7000, 0), 2, 16);  // silence: constant subframes
  std::vector<int32_t> edge(size_t{2} * 4096);
  for (size_t i = 0; i < edge.size(); i++) edge[i] = (i / 3) % 2 ? 32767 : -32768;
  round_trip(edge, 2, 16);
  std::vector<int32_t> apart(size_t{2} * 6000);  // left and right unrelated: independent channels
  uint32_t s = 9;
  for (size_t i = 0; i < apart.size(); i++) {
    s = s * 1103515245u + 12345u;
    apart[i] = int32_t(s >> 16) - 32768;
  }
  round_trip(apart, 2, 16);
  round_trip({}, 2, 16);
}

TEST(flac_is_smaller_than_the_samples) {
  const std::vector<int32_t> s = music(size_t{44100} * 2, 2, 30000, 4);
  const std::vector<uint8_t> f = audio::flac_encode(s, 2, 44100, 16);
  CHECK(f.size() < s.size() * 2 / 2);  // under half the 16-bit samples' bytes
}

TEST(flac_refuses_a_flipped_byte) {
  const std::vector<int32_t> s = music(20000, 2, 30000, 5);
  std::vector<uint8_t> f = audio::flac_encode(s, 2, 44100, 16);
  f[f.size() / 2] ^= 0x10;
  std::vector<int32_t> got;
  audio::FlacInfo info;
  std::string error;
  CHECK(!audio::flac_decode(f, &got, &info, &error));
  CHECK(error.find("FLAC frame") != std::string::npos);
  CHECK(!audio::flac_decode(std::vector<uint8_t>{'R', 'I', 'F', 'F', 0, 0, 0, 0}, &got, &info,
                            &error));
}
