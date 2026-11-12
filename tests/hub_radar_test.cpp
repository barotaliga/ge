// engine/hub/radar.hpp: a radar drawn as SVG (the dots animated over the span, held still at a
// moment, trails broken at a jump, names escaped); the page's links a minute and a second either
// way; `/radar/{sha256}` from a real archive, 404 for a demo without radar or none at all; the
// Demos page's Map link only where there is a map.
//
// ge:summary Test: the hub's map of a match: SVG, the page's views, the route, the Map link
// ge:keywords test hub radar svg animate still page route archive map-link
// ge:skill net testing
#include "hub/radar.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "hub/pages.hpp"

using namespace ge::hub;
namespace gr = ge::replay;

namespace {

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

int count(const std::string& s, const std::string& part) {
  int n = 0;
  for (size_t at = s.find(part); at != std::string::npos; at = s.find(part, at + 1)) n++;
  return n;
}

gr::Radar sample() {
  gr::Radar r;
  const gr::RadarPlayer roster[] = {{0, "ann <EU>", "red"}, {1, "bo", "blue"}};
  gr::read_radar(gr::radar_roster(roster), 0, r);
  for (int i = 0; i <= 480; i++) {             // 2 minutes at 4 a second
    const int x = i < 240 ? i * 4 : 2000 + i;  // ann jumps at a minute (a respawn)
    const gr::RadarDot d[] = {
        {0, 0, x, 0, 64}, {1, static_cast<uint8_t>(i % 40 < 4 ? gr::kRadarDead : 0), 500, i, 0}};
    gr::read_radar(gr::radar_positions(d), int64_t{i} * 250, r);
  }
  return r;
}

HubSpec spec() {
  HubSpec s;
  s.game = "Test Game";
  s.id = "test_game";
  return s;
}

std::vector<uint8_t> file_with_radar(bool radar) {
  gr::StreamHeader h;
  h.game = "test_game";
  h.meta = {{"map", "tower"}, {"mode", "duel"}, {"title", "Final"}};
  h.tracks = {{1, gr::TrackKind::meta, gr::TrackCodec::stored, "meta"}};
  if (radar) h.tracks.push_back({9, gr::TrackKind::radar, gr::TrackCodec::zstd, "radar"});
  std::vector<uint8_t> out;
  gr::StreamWriter w;
  gr::stream_begin(w, h, out);
  const uint8_t c[] = {1};
  gr::stream_add(w, 1, 0, c);
  if (radar) {
    const gr::RadarPlayer roster[] = {{0, "ann", ""}};
    gr::stream_add(w, 9, 0, gr::radar_roster(roster));
    for (int i = 0; i < 8; i++) {
      const gr::RadarDot d[] = {{0, 0, i * 10, 0, 0}};
      gr::stream_add(w, 9, int64_t{i} * 250, gr::radar_positions(d));
    }
  }
  gr::stream_end_block(w, true);
  gr::stream_finish(w, {{"duration_ms", "2000"}});
  return out;
}

}  // namespace

TEST(the_svg_animates_the_span_and_stands_still_at_a_moment) {
  const gr::Radar r = sample();
  const std::string moving = radar_svg(r, 0, 60000, true);
  CHECK(has(moving, "<svg") && has(moving, "viewBox=\"0 0 1000"));
  CHECK(count(moving, "type=\"translate\"") == 2 && count(moving, "type=\"rotate\"") == 2);
  CHECK(has(moving, "dur=\"60s\""));
  CHECK(has(moving, "ann &lt;EU&gt;") && !has(moving, "ann <EU>"));
  CHECK(has(moving, "#e5534b") && has(moving, "#4a8fe7"));  // red and blue by name
  CHECK(count(moving, "<polyline") >= 3);  // ann's trail broken at the jump, bo's at each death
  const std::string still = radar_svg(r, 30000, 30000, false);
  CHECK(!has(still, "<animate"));
  CHECK(count(still, "rotate(90)") == 1);  // ann faces east
}

TEST(the_page_links_minutes_and_seconds) {
  const gr::Radar r = sample();
  DemoRow row;
  row.sha = std::string(64, 'a');
  row.map = "tower";
  row.title = "Final";
  const std::string first = radar_page(spec(), row, r, "/radar/x");
  CHECK(has(first, "0:00 to 1:00 of 2:00"));
  CHECK(has(first, "?from=60\">Next minute") && !has(first, "Earlier minute"));
  CHECK(!has(first, "<script"));
  const std::string at = radar_page(spec(), row, r, "/radar/x?at=30");
  CHECK(has(at, "at 0:30") && has(at, "?at=29\">") && has(at, "?at=35\">"));
  CHECK(!has(at, "<animate"));
  const std::string last = radar_page(spec(), row, r, "/radar/x?from=60");
  CHECK(has(last, "Earlier minute") && !has(last, "Next minute"));
}

TEST(the_route_reads_the_archive) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge-hub-radar";
  std::filesystem::remove_all(dir);
  Archive a;
  std::string error;
  CHECK(archive_open(a, dir.string(), error));
  DemoRow with, without;
  CHECK(archive_add(a, file_with_radar(true), 1, {}, error, &with) == AddResult::added);
  CHECK(archive_add(a, file_with_radar(false), 2, {}, error, &without) == AddResult::added);
  CHECK(with.radar && !without.radar);
  Answer out;
  CHECK(radar_answer(&a, spec(), "/radar/" + with.sha, out));
  CHECK(out.code == 200 && has(out.body, "<svg") && has(out.body, "Final"));
  CHECK(radar_answer(&a, spec(), "/radar/" + without.sha, out) && out.code == 404);
  CHECK(has(out.body, "without the positions"));
  CHECK(radar_answer(&a, spec(), "/radar/" + std::string(64, 'f'), out) && out.code == 404);
  CHECK(radar_answer(nullptr, spec(), "/radar/" + with.sha, out) && out.code == 404);
  CHECK(!radar_answer(&a, spec(), "/demos", out));
  Site site;
  site.archive = &a;
  const std::string demos = demos_page(site, spec(), "/demos");
  CHECK(count(demos, "/radar/") == 1 && has(demos, "/radar/" + with.sha));
  const Answer routed = hub_answer(Poller{}, spec(), "/radar/" + with.sha, 0, site);
  CHECK(routed.code == 200);
  std::filesystem::remove_all(dir);
}
