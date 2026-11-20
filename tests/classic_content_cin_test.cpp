// QUAKE II'S CINEMATICS: a .cin written by the fixture writer read back frame for frame and sample
// for sample, the tree built from a row's counts as the format says (ties to the lower node), the
// sound read past the pictures, and damaged files refused with the byte named.
//
// ge:summary Tests: the .cin reader against the fixture writer, the trees, the sound, refusals
// ge:keywords test cin cinematic huffman fixture
// ge:skill classic testing
#include <cstring>

#include "check.hpp"
#include "classic/content/cin.hpp"

namespace cc = ge::classic::content;

namespace {

std::vector<std::vector<uint8_t>> frames_of(int count, int w, int h) {
  std::vector<std::vector<uint8_t>> out;
  for (int k = 0; k < count; k++) {
    std::vector<uint8_t> f(size_t(w) * size_t(h));
    for (size_t i = 0; i < f.size(); i++)
      f[i] = uint8_t((i * 7 + size_t(k) * 13) % 40 + (i % 5 == 0 ? 200 : 0));
    out.push_back(std::move(f));
  }
  return out;
}

std::vector<uint8_t> palette() {
  std::vector<uint8_t> p(768);
  for (size_t i = 0; i < p.size(); i++) p[i] = uint8_t(i * 3);
  return p;
}

}  // namespace

TEST(a_written_movie_reads_back_frame_for_frame) {
  const auto frames = frames_of(5, 32, 24);
  std::vector<int16_t> sound(size_t{11025} * 2);
  for (size_t i = 0; i < sound.size(); i++) sound[i] = int16_t(i * 31);
  const std::vector<uint8_t> bytes = cc::write_cin(32, 24, frames, palette(), 11025, 2, sound);
  cc::Cin cin;
  std::string error;
  CHECK(cc::open_cin(bytes, "t.cin", &cin, &error));
  CHECK(cin.width == 32 && cin.height == 24 && cin.rate == 11025 && cin.channels == 2);
  cc::CinFrame f;
  size_t sample = 0;
  int k = 0;
  cc::CinStep step;
  while ((step = cc::next_cin_frame(cin, &f, &error)) == cc::CinStep::frame) {
    CHECK(f.pixels == frames[size_t(k)]);
    CHECK(f.palette[5] == 15);
    // 11025 / 14 carries its remainder: frames of 787 and 788 samples
    const size_t want = size_t((k + 1) * 11025 / 14 - k * 11025 / 14);
    CHECK(f.left.size() == want);
    for (size_t i = 0; i < f.left.size(); i++) {
      CHECK(f.left[i] == float(sound[sample]) / 32768.0f);
      CHECK(f.right[i] == float(sound[sample + 1]) / 32768.0f);
      sample += 2;
    }
    k++;
  }
  CHECK(step == cc::CinStep::end);
  CHECK(k == 5);
  CHECK(sample / 2 == 5 * 11025 / 14);
}

TEST(the_sound_is_read_past_the_pictures) {
  const auto frames = frames_of(4, 16, 8);
  std::vector<int16_t> sound(22050);
  for (size_t i = 0; i < sound.size(); i++) sound[i] = int16_t(1000 - int(i));
  const std::vector<uint8_t> bytes = cc::write_cin(16, 8, frames, palette(), 22050, 1, sound);
  cc::Cin cin;
  std::string error;
  CHECK(cc::open_cin(bytes, "t.cin", &cin, &error));
  std::vector<float> left, right;
  cc::cin_sound(cin, &left, &right);
  CHECK(left.size() == size_t{4} * 1575);  // 22050 / 14 is whole: 1575 a frame
  CHECK(left == right);                    // mono on both
  CHECK(left[3] == float(997) / 32768.0f);
}

TEST(a_tree_takes_the_two_smallest_ties_to_the_lower_node) {
  // One context: A=1, B=1, C=2 (values 10, 11, 12). Node 256 = (A, B); then C and 256 tie at 2
  // and C, the lower number, is the 0 child: C = 0, A = 10, B = 11 in stream order.
  std::vector<uint8_t> bytes(20 + 65536, 0);
  const uint32_t header[5] = {3, 1, 0, 0, 0};
  std::memcpy(bytes.data(), header, sizeof(header));  // the tree's row for context 0, and C's
  auto row = [&](int p, int v, uint8_t n) { bytes[20 + size_t(p) * 256 + size_t(v)] = n; };
  for (int p : {0, 10, 11, 12}) {
    row(p, 10, 1);
    row(p, 11, 1);
    row(p, 12, 2);
  }
  // A frame of C, A, B: bits 0, 1 0, 1 1 = 0b11010 read low bit first: 0x1a.
  const uint32_t frame[3] = {1, 5, 3};  // a palette follows, the block's size, the pixel count
  std::vector<uint8_t> tail(4 + 768 + 4 + 4 + 1, 0);
  std::memcpy(tail.data(), &frame[0], 4);
  std::memcpy(tail.data() + 772, &frame[1], 4);
  std::memcpy(tail.data() + 776, &frame[2], 4);
  tail[780] = 0x1a;
  bytes.insert(bytes.end(), tail.begin(), tail.end());
  cc::Cin cin;
  std::string error;
  CHECK(cc::open_cin(bytes, "t.cin", &cin, &error));
  cc::CinFrame f;
  CHECK(cc::next_cin_frame(cin, &f, &error) == cc::CinStep::frame);
  CHECK(f.pixels == std::vector<uint8_t>({12, 10, 11}));
  CHECK(cc::next_cin_frame(cin, &f, &error) == cc::CinStep::end);  // the file's end
}

TEST(damaged_movies_are_refused_with_the_byte_named) {
  cc::Cin cin;
  std::string error;
  std::vector<uint8_t> shortfile(100, 0);
  CHECK(!cc::open_cin(shortfile, "s.cin", &cin, &error));
  CHECK(error.find("s.cin") != std::string::npos && error.find("next:") != std::string::npos);
  std::vector<uint8_t> bytes =
      cc::write_cin(32, 32, frames_of(2, 32, 32), palette(), 0, 1, std::span<const int16_t>{});
  bytes.resize(bytes.size() - 10);  // the end marker gone, the last frame cut short
  CHECK(cc::open_cin(bytes, "c.cin", &cin, &error));
  cc::CinFrame f;
  CHECK(cc::next_cin_frame(cin, &f, &error) == cc::CinStep::frame);
  CHECK(cc::next_cin_frame(cin, &f, &error) == cc::CinStep::fault);
  CHECK(error.find("frame 1") != std::string::npos);
  std::vector<uint8_t> huge = bytes;
  const uint32_t wide = 4000;
  std::memcpy(huge.data(), &wide, 4);
  CHECK(!cc::open_cin(huge, "w.cin", &cin, &error));
  CHECK(error.find("4000x32") != std::string::npos);
}
