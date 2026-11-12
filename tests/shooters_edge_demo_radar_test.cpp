// games/shooters_edge/client/demo_radar.hpp: a scripted multiview match's radar, four times a
// second, every player's place, heading and death; the roster with the teams; the track added to a
// packed file, the file still unpacking to the same recording; a client recording refused.
//
// ge:summary Test: a multiview recording's radar track, sampled and added to its packed file
// ge:keywords test demo radar track multiview positions heading roster add_radar
// ge:skill replay testing
#include "shooters_edge/client/demo_radar.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "replay/radar.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/demo_write.hpp"

namespace cl = shooters_edge::client;
namespace gr = ge::replay;

namespace {

cl::MatchScript match() {
  cl::MatchScript m;
  m.map = "tower";
  m.frames = 200;  // 10 s at 20 frames a second
  for (int p = 0; p < 2; p++) {
    cl::MatchPlayer pl;
    pl.name = p == 0 ? "ann" : "bo";
    pl.yaw = p == 0 ? 0.0f : 90.0f;  // east; north
    for (int i = 0; i < m.frames; i++) {
      pl.origins.push_back({static_cast<float>(p * 300 + i * 2), static_cast<float>(-p * 100), 24});
      pl.health.push_back(p == 1 && i >= 100 && i < 120 ? 0 : 100);
      pl.frags.push_back(0);
    }
    pl.weapon = {1};
    pl.armour = {0};
    m.players.push_back(pl);
  }
  return m;
}

}  // namespace

TEST(a_match_reads_into_radar_chunks) {
  const std::vector<uint8_t> rec = cl::write_multiview_match(match());
  std::vector<gr::Chunk> chunks;
  gr::Meta meta;
  std::string error;
  CHECK_MSG(cl::demo_radar(rec, chunks, meta, error), error);
  gr::Radar r;
  for (const gr::Chunk& c : chunks) CHECK(gr::read_radar(c.bytes, c.time_ms, r));
  CHECK(r.roster.size() == 2 && r.roster[0].name == "ann" && r.roster[1].name == "bo");
  CHECK(r.frames.size() >= 38 && r.frames.size() <= 42);  // 10 s at 4 a second
  const gr::RadarFrame& f = r.frames[20];
  CHECK(f.dots.size() == 2);
  CHECK(f.dots[0].heading == 64);  // east: a quarter turn clockwise from north
  CHECK(f.dots[1].heading == 0);   // north
  CHECK(f.dots[1].y == -100 && f.dots[1].x > 300);
  bool dead = false;
  for (const gr::RadarFrame& g : r.frames) {
    for (const gr::RadarDot& d : g.dots) dead |= d.slot == 1 && (d.flags & gr::kRadarDead);
  }
  CHECK(dead);
  CHECK(!meta.empty() && meta[0].first == "map" && meta[0].second == "tower");
}

TEST(the_track_rides_the_packed_file) {
  const std::vector<uint8_t> rec = cl::write_multiview_match(match());
  std::vector<uint8_t> ged, with;
  std::string error;
  CHECK(cl::pack_demo(rec, ged, error));
  CHECK_MSG(cl::add_radar(rec, ged, with, error), error);
  std::vector<uint8_t> back;
  CHECK_MSG(cl::unpack_demo(with, back, error), error);
  CHECK(back == rec);  // the recording's own track untouched
  gr::Radar r;
  CHECK(gr::read_radar_file(with, r, error) && r.any);
  CHECK(r.frames.size() >= 38);
  CHECK(r.max_x >= 300 + 2 * 190 && r.min_y == -100);
  const std::optional<gr::Trailer> t = gr::read_trailer(with);
  CHECK(t.has_value() && !t->meta.empty());
  gr::StreamReader sr;
  CHECK(gr::read_stream_header(with, sr, error));
  int players = 0;
  for (const auto& [k, v] : sr.header.meta) players += k == "player" ? 1 : 0;
  CHECK(players == 2);
  // A second add is refused: the id is taken.
  std::vector<uint8_t> twice;
  CHECK(!cl::add_radar(rec, with, twice, error) && !error.empty());
}

TEST(only_a_multiview_recording) {
  cl::DemoScript s;
  std::vector<uint8_t> client = cl::write_client_demo(s);
  std::vector<gr::Chunk> chunks;
  gr::Meta meta;
  std::string error;
  CHECK(!cl::demo_radar(client, chunks, meta, error));
  CHECK(!error.empty());
}
