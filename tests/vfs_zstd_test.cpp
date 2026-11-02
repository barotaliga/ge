// Zstandard frames (engine/vfs/zstd.hpp): bytes packed at the chosen setting and at others come
// back the same; a stream written in pieces with flushes unpacks after each flush to every byte
// given so far (a cut-off recording keeps them, `truncated` set) and after its end to the whole;
// frames one after another unpack to their bytes one after another; damage, a wrong magic and a
// bomb past the limit are refused with the reason.
//
// ge:summary Tests the zstd codec: round trips, the stream and its flushes, refusals
// ge:keywords test vfs zstd pack unpack stream flush truncated damaged limit
// ge:skill vfs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "vfs/zstd.hpp"

namespace vfs = ge::vfs;

namespace {

// Bytes that pack like a recording: records of a counter, a slowly moving value and noise.
std::vector<uint8_t> records(size_t n) {
  std::vector<uint8_t> out;
  uint32_t x = 12345;
  for (size_t i = 0; out.size() < n; i++) {
    x = x * 1664525u + 1013904223u;
    const uint8_t rec[8] = {static_cast<uint8_t>(i),
                            6,
                            static_cast<uint8_t>(i >> 8),
                            0x10,
                            static_cast<uint8_t>(i / 64),
                            static_cast<uint8_t>(x >> 24),
                            0,
                            9};
    out.insert(out.end(), rec, rec + 8);
  }
  out.resize(n);
  return out;
}

}  // namespace

TEST(zstd_round_trips_at_every_setting) {
  for (const size_t n : {size_t{0}, size_t{1}, size_t{777}, size_t{300000}}) {
    const auto plain = records(n);
    for (const vfs::ZstdLevel lv :
         {vfs::ZstdLevel{}, vfs::ZstdLevel{3, 0, false}, vfs::ZstdLevel{1, 0, false}}) {
      const auto packed = vfs::zstd_pack(plain, lv);
      CHECK(vfs::is_zstd(packed));
      std::vector<uint8_t> back;
      bool cut = true;
      std::string error;
      CHECK_MSG(vfs::zstd_unpack(packed, back, cut, error), error);
      CHECK(!cut);
      CHECK(back == plain);
      if (n == 300000) CHECK(packed.size() * 2 < plain.size());
    }
  }
}

TEST(zstd_stream_unpacks_after_each_flush) {
  const auto plain = records(200000);
  vfs::ZstdStream s;
  vfs::zstd_begin(s);
  size_t at = 0;
  for (const size_t piece : {size_t{13}, size_t{4096}, size_t{70000}, size_t{1}}) {
    vfs::zstd_write(s, std::span(plain).subspan(at, piece));
    at += piece;
    vfs::zstd_flush(s);
    std::vector<uint8_t> back;
    bool cut = false;
    std::string error;
    CHECK_MSG(vfs::zstd_unpack(s.out, back, cut, error), error);
    CHECK(cut);  // the frame has not ended
    CHECK(back == std::vector<uint8_t>(plain.begin(), plain.begin() + static_cast<long>(at)));
  }
  vfs::zstd_write(s, std::span(plain).subspan(at));
  vfs::zstd_end(s);
  CHECK(s.ended);
  CHECK(s.fed == static_cast<int64_t>(plain.size()));
  std::vector<uint8_t> back;
  bool cut = true;
  std::string error;
  CHECK_MSG(vfs::zstd_unpack(s.out, back, cut, error), error);
  CHECK(!cut);
  CHECK(back == plain);
}

TEST(zstd_reads_frames_one_after_another) {
  const auto a = records(5000), b = records(9000);
  auto both = vfs::zstd_pack(a);
  const auto pb = vfs::zstd_pack(b, {3, 0, false});
  both.insert(both.end(), pb.begin(), pb.end());
  std::vector<uint8_t> back;
  bool cut = true;
  std::string error;
  CHECK_MSG(vfs::zstd_unpack(both, back, cut, error), error);
  auto want = a;
  want.insert(want.end(), b.begin(), b.end());
  CHECK(back == want);
  CHECK(!cut);
}

TEST(zstd_refuses_damage_magic_and_bombs) {
  const auto plain = records(100000);
  auto packed = vfs::zstd_pack(plain);
  std::vector<uint8_t> back;
  bool cut = false;
  std::string error;
  auto bad = packed;
  bad[bad.size() / 2] ^= 0x5A;
  CHECK(!vfs::zstd_unpack(bad, back, cut, error));
  CHECK(error.find("damaged") != std::string::npos);
  CHECK(!vfs::zstd_unpack(plain, back, cut, error));
  CHECK(error.find("magic") != std::string::npos);
  CHECK(!vfs::zstd_unpack(packed, back, cut, error, 1000));
  CHECK(error.find("more than 1000") != std::string::npos);
  // Cut short: what the blocks before the cut hold, flagged.
  const std::vector<uint8_t> half(packed.begin(),
                                  packed.begin() + static_cast<long>(packed.size() / 2));
  CHECK_MSG(vfs::zstd_unpack(half, back, cut, error), error);
  CHECK(cut);
  CHECK(back.size() < plain.size());
  CHECK(std::equal(back.begin(), back.end(), plain.begin()));
}
