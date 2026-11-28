// A demo series (engine/replay/series.hpp): replay files added one by one read back in order,
// each entry's bytes the very file added and its meta the file's header and final meta (less the
// packed report); a series cut short or damaged keeps every whole entry before and says where;
// merging replay files and series flattens them in order and splitting gives the same bytes; a
// newer version and a file that is not a series are refused with the reason. The bytes of a
// fixed series (its tracks stored, so no codec moves them) are held by their FNV-1a, so the
// format cannot drift unseen.
//
// ge:summary Tests: demo series round trip, entry meta, cut and damaged series, merge, split
// ge:keywords test demo-series geds series merge split entry-sum version
// ge:skill replay testing
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/series.hpp"
#include "replay/stream.hpp"

namespace gr = ge::replay;

namespace {

// A small replay file of `blocks` blocks on `map`, finished with a score in its final meta.
std::vector<uint8_t> ged(const std::string& map, int blocks) {
  gr::StreamHeader h;
  h.game = "shooters_edge";
  h.game_version = 2;
  h.meta = {{"map", map}, {"mode", "team"}};
  h.tracks = {{1, gr::TrackKind::messages, gr::TrackCodec::stored, "multiview"}};
  std::vector<uint8_t> out;
  gr::StreamWriter w;
  gr::stream_begin(w, h, out);
  for (int b = 0; b < blocks; b++) {
    for (int i = 0; i < 5; i++) {
      std::vector<uint8_t> chunk(static_cast<size_t>(8 + i), static_cast<uint8_t>(b * 9 + i));
      gr::stream_add(w, 1, int64_t{b} * 1000 + int64_t{i} * 200, chunk);
    }
    gr::stream_end_block(w, b % 3 == 0);
  }
  gr::stream_finish(w, {{"duration_ms", std::to_string(blocks * 1000)},
                        {"team_score", "red\t" + std::to_string(blocks)},
                        {"report", "00ff"}});
  return out;
}

uint32_t fnv(const std::vector<uint8_t>& b) {
  uint32_t h = 2166136261u;
  for (const uint8_t c : b) h = (h ^ c) * 16777619u;
  return h;
}

std::vector<uint8_t> three(std::vector<std::vector<uint8_t>>& files) {
  files = {ged("dm2", 4), ged("dm3", 6), ged("e1m2", 3)};
  const std::vector<std::string> names = {"dm2-20261128-200000", "dm3-20261128-202500",
                                          "e1m2-20261128-205000"};
  std::vector<uint8_t> s;
  gr::series_begin(s, {{std::string(gr::kSeriesKind), "matches"},
                       {std::string(gr::kSeriesTitle), "red vs blue"}});
  std::string error;
  for (size_t i = 0; i < files.size() && i < names.size(); i++)
    CHECK_MSG(gr::series_add(s, names[i], files[i], error), error);
  return s;
}

}  // namespace

TEST(a_series_reads_back_its_replay_files_in_order_byte_for_byte) {
  std::vector<std::vector<uint8_t>> files;
  const std::vector<uint8_t> s = three(files);
  CHECK(gr::is_series(s));
  CHECK(!gr::is_stream(s));
  CHECK(!gr::is_series(files[0]));
  gr::SeriesView v;
  std::string error;
  CHECK_MSG(gr::read_series(s, v, error), error);
  CHECK(v.cut.empty());
  CHECK(v.version == gr::kSeriesVersion);
  CHECK(gr::meta_get(v.meta, gr::kSeriesTitle) == "red vs blue");
  CHECK(v.entries.size() == 3);
  for (size_t i = 0; i < v.entries.size() && i < files.size(); i++) {
    const gr::SeriesEntry& e = v.entries[i];
    CHECK(std::vector<uint8_t>(e.bytes.begin(), e.bytes.end()) == files[i]);
  }
  CHECK(v.entries[1].name == "dm3-20261128-202500");
  // The entry's meta: the game, the header's, then the final meta less the packed report.
  const gr::Meta& m = v.entries[1].meta;
  CHECK(gr::meta_get(m, "game") == "shooters_edge");
  CHECK(gr::meta_get(m, "map") == "dm3");
  CHECK(gr::meta_get(m, "duration_ms") == "6000");
  CHECK(gr::meta_get(m, "team_score") == "red\t6");
  CHECK(gr::meta_get(m, "report").empty());
  // The format held: these bytes, this sum.
  CHECK_MSG(fnv(s) == 0xbcad33bcu && s.size() == 1832,
            "series sum " + std::to_string(fnv(s)) + " size " + std::to_string(s.size()));
}

TEST(a_series_cut_short_or_damaged_keeps_its_whole_entries) {
  std::vector<std::vector<uint8_t>> files;
  const std::vector<uint8_t> s = three(files);
  gr::SeriesView whole;
  std::string error;
  CHECK(gr::read_series(s, whole, error));
  // Cut inside the last entry: two whole entries, the cut named.
  std::vector<uint8_t> cut(s.begin(), s.begin() + static_cast<long>(whole.entries[2].offset + 20));
  gr::SeriesView v;
  CHECK_MSG(gr::read_series(cut, v, error), error);
  CHECK(v.entries.size() == 2);
  CHECK(v.cut.find("entry 3") != std::string::npos && v.cut.find("cut short") != std::string::npos);
  // A byte of the second entry's replay file changed: its sum fails, the first stays.
  std::vector<uint8_t> bad = s;
  bad[whole.entries[1].offset + 60] ^= 0x40;
  CHECK(gr::read_series(bad, v, error));
  CHECK(v.entries.size() == 1);
  CHECK_MSG(v.cut.find("sum does not hold") != std::string::npos, v.cut);
  // A merge refuses a damaged series rather than dropping its tail unsaid.
  std::vector<uint8_t> out;
  const gr::SeriesPart part{"bad.geds", bad};
  CHECK(!gr::series_merge({&part, 1}, {}, out, error));
  CHECK_MSG(error.find("bad.geds") != std::string::npos, error);
}

TEST(merging_flattens_series_and_files_in_order_and_splitting_gives_them_back) {
  std::vector<std::vector<uint8_t>> files;
  const std::vector<uint8_t> s = three(files);
  const std::vector<uint8_t> extra = ged("dm6", 2);
  std::vector<uint8_t> merged;
  std::string error;
  const gr::SeriesPart parts[] = {{"night.geds", s}, {"dm6-tiebreak", extra}, {"night.geds", s}};
  CHECK_MSG(gr::series_merge(parts, {}, merged, error), error);
  gr::SeriesView v;
  CHECK(gr::read_series(merged, v, error));
  CHECK(v.entries.size() == 7);
  CHECK(gr::meta_get(v.meta, gr::kSeriesTitle) == "red vs blue");  // the first series' meta
  CHECK(v.entries[3].name == "dm6-tiebreak");
  CHECK(gr::meta_get(v.entries[3].meta, "map") == "dm6");
  CHECK(std::vector<uint8_t>(v.entries[3].bytes.begin(), v.entries[3].bytes.end()) == extra);
  CHECK(std::vector<uint8_t>(v.entries[6].bytes.begin(), v.entries[6].bytes.end()) == files[2]);
  // The first three entries merged alone are the first series again, byte for byte.
  std::vector<gr::SeriesPart> back;
  for (size_t i = 0; i < 3; i++) back.push_back({v.entries[i].name, v.entries[i].bytes});
  std::vector<uint8_t> again;
  CHECK(gr::series_merge(back, v.meta, again, error));
  CHECK(again == s);
  // Something that is neither is refused by name.
  const std::vector<uint8_t> junk = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  const gr::SeriesPart j{"notes.txt", junk};
  CHECK(!gr::series_merge({&j, 1}, {}, again, error));
  CHECK_MSG(error.find("notes.txt is neither") != std::string::npos, error);
  CHECK(!gr::series_add(again, "notes", junk, error));
}

TEST(a_newer_series_and_other_files_are_refused_with_the_reason) {
  std::vector<uint8_t> s;
  gr::series_begin(s, {});
  s[8] = 2;
  gr::SeriesView v;
  std::string error;
  CHECK(!gr::read_series(s, v, error));
  CHECK_MSG(error.find("version 2") != std::string::npos, error);
  CHECK(!gr::read_series(ged("dm2", 1), v, error));
  CHECK_MSG(error.find("not a demo series") != std::string::npos, error);
  // An empty series is a series with no entries.
  gr::series_begin(s, {{"kind", "campaign"}});
  CHECK(gr::read_series(s, v, error) && v.entries.empty() && v.cut.empty());
}
