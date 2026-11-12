// engine/replay/radar.hpp: the radar's three chunks written and read back, a cut chunk refused,
// the bounds; a track added to a finished stream keeping every block's time, keyframe flag and
// chunks, each new chunk in the block its time falls in, the trailer and the header's meta.
//
// ge:summary Test: the radar track's chunks, and a track added to a finished stream
// ge:keywords test replay radar roster positions outline stream_add_track blocks keyframe
// ge:skill replay testing
#include "replay/radar.hpp"

#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::replay;

namespace {

std::vector<uint8_t> sample_stream() {
  StreamHeader h;
  h.game = "test_game";
  h.meta = {{"map", "tower"}};
  h.tracks = {{1, TrackKind::messages, TrackCodec::zstd, "moves"}};
  std::vector<uint8_t> out;
  StreamWriter w;
  stream_begin(w, h, out);
  for (int b = 0; b < 3; b++) {
    for (int i = 0; i < 4; i++) {
      const uint8_t c[] = {static_cast<uint8_t>(b), static_cast<uint8_t>(i)};
      stream_add(w, 1, b * 1000 + i * 250, c);
    }
    stream_end_block(w, b == 0);
  }
  stream_finish(w, {{"duration_ms", "3000"}});
  return out;
}

std::vector<Block> blocks_of(const std::vector<uint8_t>& file) {
  StreamReader r;
  std::string error;
  std::vector<Block> out;
  if (!read_stream_header(file, r, error)) return out;
  Block b;
  while (next_block(r, b, error)) out.push_back(b);
  return out;
}

}  // namespace

TEST(chunks_read_back) {
  const RadarPlayer roster[] = {{0, "ann", "red"}, {3, "bo", "blue"}};
  const RadarDot dots[] = {{0, 0, -120, 40, 64}, {3, kRadarDead, 900, -2000, 200}};
  const RadarLine lines[] = {{-500, -500, 500, -500}};
  Radar r;
  CHECK(read_radar(radar_roster(roster), 0, r));
  CHECK(read_radar(radar_positions(dots), 250, r));
  CHECK(read_radar(radar_outline(lines), 250, r));
  CHECK(r.roster.size() == 2 && r.roster[1].name == "bo" && r.roster[1].team == "blue");
  CHECK(r.frames.size() == 1 && r.frames[0].time_ms == 250 && r.frames[0].dots.size() == 2);
  CHECK(r.frames[0].dots[1].flags == kRadarDead && r.frames[0].dots[1].y == -2000);
  CHECK(r.frames[0].dots[0].heading == 64);
  CHECK(r.outline.size() == 1);
  CHECK(r.any && r.min_x == -500 && r.max_x == 900 && r.min_y == -2000 && r.max_y == 40);
  std::vector<uint8_t> cut = radar_positions(dots);
  cut.resize(cut.size() - 2);
  CHECK(!read_radar(cut, 0, r));
  const uint8_t other[] = {'X', 0};
  CHECK(!read_radar(other, 0, r));
}

TEST(a_track_is_added_to_a_finished_stream) {
  const std::vector<uint8_t> file = sample_stream();
  std::vector<Chunk> add;
  for (int t = -100; t <= 3400; t += 500) {  // one before the first block, one after the last
    const RadarDot d[] = {{0, 0, t, 0, 0}};
    add.push_back({9, t, radar_positions(d)});
  }
  std::vector<uint8_t> out;
  std::string error;
  CHECK_MSG(stream_add_track(file, {9, TrackKind::radar, TrackCodec::zstd, "radar"}, add, out,
                             error, {{"map", "other"}, {"player", "ann\tred"}, {"player", "bo\t"}}),
            error);
  const std::vector<Block> before = blocks_of(file), after = blocks_of(out);
  CHECK(before.size() == 3 && after.size() == 3);
  size_t radar = 0;
  for (size_t i = 0; i < after.size(); i++) {
    CHECK(after[i].time_ms == before[i].time_ms && after[i].keyframe == before[i].keyframe);
    std::vector<Chunk> own;
    for (const Chunk& c : after[i].chunks) {
      if (c.track == 1) own.push_back(c);
      if (c.track != 9) continue;
      radar++;
      CHECK(c.time_ms >= after[i].time_ms);
      if (i + 1 < after.size()) CHECK(c.time_ms < after[i + 1].time_ms);
    }
    CHECK(own.size() == before[i].chunks.size());
    for (size_t k = 0; k < own.size(); k++) {
      CHECK(own[k].time_ms == before[i].chunks[k].time_ms &&
            own[k].bytes == before[i].chunks[k].bytes);
    }
  }
  CHECK(radar == add.size());
  const std::optional<Trailer> t = read_trailer(out);
  CHECK(t && t->meta.size() == 1 && t->meta[0].second == "3000" && t->index.size() == 3);
  StreamReader sr;
  CHECK(read_stream_header(out, sr, error));
  CHECK(sr.header.meta.size() == 3);  // the map kept as it was; both players added
  CHECK(sr.header.meta[0].second == "tower" && sr.header.meta[2].second == "bo\t");
  Radar r;
  CHECK(read_radar_file(out, r, error) && r.frames.size() == add.size());
  Radar none;
  CHECK(read_radar_file(file, none, error) && !none.any);
  std::vector<uint8_t> again;
  CHECK(
      !stream_add_track(out, {9, TrackKind::radar, TrackCodec::zstd, "radar"}, add, again, error));
}
