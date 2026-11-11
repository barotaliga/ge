// The replay file version 2 (engine/replay/stream.hpp): a header and its blocks come back as
// written, track by track and in time order; the keyframe flags and the trailer's index agree with
// the blocks; a file cut short reads to its last whole block; a damaged block fails its sum; a
// newer version and a file that is not one are refused with the reason.
//
// ge:summary Tests: replay file v2 round trip, keyframes, trailer index, cut file, damage, version
// ge:keywords test replay-v2 stream block keyframe trailer index truncated damaged version
// ge:skill replay testing
// ge:test tests/replay_stream_test.cpp
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/stream.hpp"

namespace gr = ge::replay;

namespace {

gr::StreamHeader sample_header() {
  gr::StreamHeader h;
  h.game = "fall_line";
  h.game_version = 3;
  h.meta = {{"map", "seed-38"}, {"mode", "race"}, {"players", "4"}};
  h.blob = {1, 2, 3, 250};
  h.tracks = {{1, gr::TrackKind::inputs, gr::TrackCodec::zstd, "inputs"},
              {2, gr::TrackKind::events, gr::TrackCodec::stored, "events"},
              {5, gr::TrackKind::messages, gr::TrackCodec::field, "multiview"}};
  return h;
}

std::vector<uint8_t> payload(int block, int track, int i) {
  std::vector<uint8_t> b;
  const int n = (block * 7 + track * 3 + i) % 40;
  b.reserve(static_cast<size_t>(n));
  for (int k = 0; k < n; k++) b.push_back(static_cast<uint8_t>(block * 31 + track * 5 + i + k));
  return b;
}

// Eight blocks of a second each, a keyframe every fourth; every track writes a chunk every 100 ms,
// the events track only in even blocks.
std::vector<uint8_t> sample_file(bool finish, std::vector<gr::Block>* want = nullptr) {
  std::vector<uint8_t> out;
  gr::StreamWriter w;
  gr::stream_begin(w, sample_header(), out);
  for (int block = 0; block < 8; block++) {
    gr::Block b;
    b.index = block;
    b.time_ms = int64_t{block} * 1000;
    b.keyframe = block % 4 == 0;
    for (int i = 0; i < 10; i++) {
      for (const uint32_t track : {1u, 2u, 5u}) {
        if (track == 2 && block % 2 == 1) continue;
        const int64_t t = block * 1000 + i * 100 + static_cast<int64_t>(track);
        const auto bytes = payload(block, static_cast<int>(track), i);
        gr::stream_add(w, track, t, bytes);
        b.chunks.push_back({track, t, bytes});
      }
    }
    // The block's start is its first chunk's time; the reader returns chunks track by track.
    b.time_ms = b.chunks.front().time_ms;
    std::vector<gr::Chunk> by_track;
    for (const uint32_t track : {1u, 2u, 5u})
      for (const gr::Chunk& c : b.chunks)
        if (c.track == track) by_track.push_back(c);
    b.chunks = by_track;
    gr::stream_end_block(w, block % 4 == 0);
    if (want != nullptr) want->push_back(b);
  }
  if (finish) gr::stream_finish(w, {{"duration", "8000"}, {"winner", "2"}});
  return out;
}

bool same_block(const gr::Block& a, const gr::Block& b) {
  if (a.index != b.index || a.time_ms != b.time_ms || a.keyframe != b.keyframe) return false;
  if (a.chunks.size() != b.chunks.size()) return false;
  for (size_t i = 0; i < a.chunks.size(); i++) {
    const gr::Chunk& x = a.chunks[i];
    const gr::Chunk& y = b.chunks[i];
    if (x.track != y.track || x.time_ms != y.time_ms || x.bytes != y.bytes) return false;
  }
  return true;
}

std::vector<gr::Block> read_all(const std::vector<uint8_t>& bytes, std::string& error) {
  gr::StreamReader r;
  std::vector<gr::Block> got;
  if (!gr::read_stream_header(bytes, r, error)) return got;
  gr::Block b;
  while (gr::next_block(r, b, error)) got.push_back(b);
  return got;
}

}  // namespace

TEST(the_header_and_every_block_come_back) {
  std::vector<gr::Block> want;
  const auto bytes = sample_file(true, &want);
  CHECK(gr::is_stream(bytes));
  gr::StreamReader r;
  std::string error;
  CHECK_MSG(gr::read_stream_header(bytes, r, error), error);
  const gr::StreamHeader h = sample_header();
  CHECK(r.header.game == h.game);
  CHECK(r.header.game_version == h.game_version);
  CHECK(r.header.meta == h.meta);
  CHECK(r.header.blob == h.blob);
  CHECK(r.header.tracks.size() == 3);
  CHECK(r.header.tracks[2].id == 5 && r.header.tracks[2].kind == gr::TrackKind::messages &&
        r.header.tracks[2].codec == gr::TrackCodec::field &&
        r.header.tracks[2].name == "multiview");
  const auto got = read_all(bytes, error);
  CHECK_MSG(error.empty(), "a clean end gave an error: " + error);
  CHECK(got.size() == want.size());
  bool same = got.size() == want.size();
  for (size_t i = 0; same && i < got.size(); i++) same = same_block(got[i], want[i]);
  CHECK_MSG(same, "a block came back different from what was written");
}

TEST(the_trailer_indexes_every_block_and_its_keyframes) {
  const auto bytes = sample_file(true);
  const auto t = gr::read_trailer(bytes);
  CHECK(t.has_value());
  if (!t) return;
  CHECK(t->meta == gr::Meta({{"duration", "8000"}, {"winner", "2"}}));
  CHECK(t->index.size() == 8);
  std::string error;
  gr::StreamReader r;
  CHECK(gr::read_stream_header(bytes, r, error));
  bool agree = t->index.size() == 8;
  for (size_t i = 0; agree && i < t->index.size(); i++) {
    const gr::BlockEntry& e = t->index[i];
    agree = agree && e.keyframe == (i % 4 == 0);
    // A reader may start at any keyframe's offset.
    r.at = static_cast<size_t>(e.offset);
    gr::Block b;
    agree = agree && gr::next_block(r, b, error) && b.index == static_cast<int64_t>(i) &&
            b.time_ms == e.time_ms && b.keyframe == e.keyframe;
  }
  CHECK_MSG(agree, "the trailer's index does not match the blocks: " + error);
  CHECK(!gr::read_trailer(sample_file(false)).has_value());
}

TEST(a_file_cut_short_reads_to_its_last_whole_block) {
  std::vector<gr::Block> want;
  const auto whole = sample_file(false, &want);
  // A live stream with no trailer reads every block and ends clean.
  std::string error;
  CHECK(read_all(whole, error).size() == 8);
  CHECK(error.empty());
  // Cut in the middle of the sixth block: five come back, and the error names the cut.
  const auto t = gr::read_trailer(sample_file(true));
  CHECK(t.has_value());
  if (!t) return;
  const auto cut_at = static_cast<size_t>(t->index[5].offset) + 9;
  const std::vector<uint8_t> cut(whole.begin(), whole.begin() + static_cast<long>(cut_at));
  const auto got = read_all(cut, error);
  CHECK(got.size() == 5);
  CHECK(!error.empty());
  bool same = got.size() == 5;
  for (size_t i = 0; same && i < got.size(); i++) same = same_block(got[i], want[i]);
  CHECK(same);
}

TEST(a_damaged_block_fails_its_sum) {
  auto bytes = sample_file(true);
  const auto t = gr::read_trailer(bytes);
  CHECK(t.has_value());
  if (!t) return;
  bytes[static_cast<size_t>(t->index[3].offset) + 12] ^= 0x40;
  std::string error;
  const auto got = read_all(bytes, error);
  CHECK(got.size() == 3);
  CHECK_MSG(error.find("sum") != std::string::npos, "the error was: " + error);
}

TEST(a_newer_version_and_another_file_are_refused) {
  auto bytes = sample_file(true);
  bytes[8] = 3;
  gr::StreamReader r;
  std::string error;
  CHECK(!gr::read_stream_header(bytes, r, error));
  CHECK_MSG(error.find("version 3") != std::string::npos, "the error was: " + error);
  const std::vector<uint8_t> other = {'Q', 'W', 'D', 0, 1, 2, 3, 4, 5, 6};
  CHECK(!gr::is_stream(other));
  CHECK(!gr::read_stream_header(other, r, error));
  CHECK(!error.empty());
}

TEST(the_same_blocks_write_the_same_bytes) { CHECK(sample_file(true) == sample_file(true)); }
