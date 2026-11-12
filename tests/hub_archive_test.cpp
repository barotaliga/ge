// engine/hub/archive.hpp: the hub's demo archive. A replay file's row comes from its meta words
// and trailer; files are kept by SHA-256 once, survive a restart with their tags, come in through
// the inbox (a bad one set aside), are searched by player, mode, map, team, tag and age 15 a page,
// and pruned oldest untagged first; the archive's paths answer JSON and the file; the Demos page
// hides the scores until asked.
//
// ge:summary Test: the hub's demo archive (rows, files by hash, inbox, search, prune, paths, page)
// ge:keywords test hub archive demo sha256 search inbox prune download demos-page
// ge:skill net testing
#include "hub/archive.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/json_read.hpp"
#include "hub/pages.hpp"
#include "replay/stream.hpp"

using namespace ge::hub;
namespace gr = ge::replay;
namespace fs = std::filesystem;

namespace {

struct DemoSpec {
  std::string map = "tower", mode = "4on4", server = "Night Arena";
  int64_t start = 1794450000;
  std::vector<std::pair<std::string, std::string>> players = {{"Vale", "red"}, {"sable", "blue"}};
  std::vector<std::pair<std::string, int>> scores = {{"Vale", 41}, {"sable", 39}};
  std::vector<std::string> tags;
  bool finished = true;
  int salt = 0;  // makes the bytes differ
};

std::string tabbed(const std::string& a, const std::string& b) {
  std::string s = a;
  s += '\t';
  s += b;
  return s;
}

std::vector<uint8_t> demo(const DemoSpec& d) {
  gr::StreamHeader h;
  h.game = "test_game";
  h.meta = {{"map", d.map},       {"mode", d.mode},
            {"server", d.server}, {"start", std::to_string(d.start)},
            {"team", "red"},      {"team", "blue"}};
  for (const auto& [name, team] : d.players) h.meta.emplace_back("player", tabbed(name, team));
  for (const std::string& t : d.tags) h.meta.emplace_back("tag", t);
  h.tracks = {{1, gr::TrackKind::meta, gr::TrackCodec::stored, "meta"}};
  std::vector<uint8_t> out;
  gr::StreamWriter w;
  gr::stream_begin(w, h, out);
  const uint8_t bytes[] = {1, 2, 3, static_cast<uint8_t>(d.salt)};
  gr::stream_add(w, 1, 0, bytes);
  gr::stream_end_block(w, true);
  if (d.finished) {
    gr::Meta fin = {{"duration_ms", "1200000"}, {"team_score", "red\t122"}};
    for (const auto& [name, s] : d.scores)
      fin.emplace_back("score", tabbed(name, std::to_string(s)));
    gr::stream_finish(w, fin);
  }
  return out;
}

fs::path fresh_dir(const std::string& name) {
  const fs::path p = fs::temp_directory_path() / ("ge-hub-archive-" + name);
  fs::remove_all(p);
  return p;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(row_from_meta_words_and_trailer) {
  DemoRow r;
  std::string err;
  CHECK(demo_row(demo(DemoSpec{}), r, err));
  CHECK(r.game == "test_game" && r.map == "tower" && r.mode == "4on4");
  CHECK(r.server == "Night Arena" && r.start_s == 1794450000 && r.duration_ms == 1200000);
  CHECK(r.players.size() == 2 && r.players[0].name == "Vale" && r.players[0].team == "red");
  CHECK(r.players[0].scored && r.players[0].score == 41);
  CHECK(r.teams.size() == 2 && r.teams[0].scored && r.teams[0].score == 122 && !r.teams[1].scored);
  DemoSpec cut;
  cut.finished = false;
  CHECK(demo_row(demo(cut), r, err) && r.duration_ms == -1 && !r.players[0].scored);
  const std::vector<uint8_t> junk = {'n', 'o', 't', ' ', 'a', ' ', 'd', 'e', 'm', 'o'};
  CHECK(!demo_row(junk, r, err) && !err.empty());
}

TEST(kept_by_hash_once_and_reopened_with_tags) {
  const fs::path dir = fresh_dir("keep");
  Archive a;
  std::string err;
  CHECK(archive_open(a, dir.string(), err));
  const std::vector<uint8_t> bytes = demo(DemoSpec{});
  DemoRow row;
  CHECK(archive_add(a, bytes, 100, {}, err, &row) == AddResult::added);
  CHECK(row.sha.size() == 64 && fs::exists(dir / archive_path(row.sha)));
  CHECK(archive_path(row.sha) == "d/" + row.sha.substr(0, 3) + "/" + row.sha + ".ger");
  CHECK(archive_add(a, bytes, 200, {"league"}, err) == AddResult::duplicate);
  CHECK(a.rows.size() == 1 && a.rows[0].tags == std::vector<std::string>{"league"});
  CHECK(a.rows[0].added_s == 100);
  const std::vector<uint8_t> junk(40, 7);
  CHECK(archive_add(a, junk, 200, {}, err) == AddResult::refused);
  Archive again;
  CHECK(archive_open(again, dir.string(), err));
  CHECK(again.rows.size() == 1 && again.rows[0].sha == row.sha);
  CHECK(again.rows[0].tags == std::vector<std::string>{"league"} && again.rows[0].added_s == 100);
  CHECK(again.bytes == static_cast<int64_t>(bytes.size()));
  fs::remove_all(dir);
}

TEST(inbox_takes_demos_and_sets_aside_the_rest) {
  const fs::path dir = fresh_dir("inbox");
  Archive a;
  std::string err;
  CHECK(archive_open(a, dir.string(), err));
  {
    const std::vector<uint8_t> b = demo(DemoSpec{});
    std::ofstream(dir / "inbox" / "match.ger", std::ios::binary)
        .write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    std::ofstream(dir / "inbox" / "broken.ger") << "this is not a replay";
    std::ofstream(dir / "inbox" / "notes.txt") << "left alone";
  }
  std::vector<std::string> refused;
  CHECK(archive_ingest(a, 50, refused) == 1);
  CHECK(a.rows.size() == 1 && refused.size() == 1 && has(refused[0], "broken.ger"));
  CHECK(!fs::exists(dir / "inbox" / "match.ger") &&
        fs::exists(dir / "inbox" / "refused" / "broken.ger"));
  CHECK(fs::exists(dir / "inbox" / "notes.txt"));
  fs::remove_all(dir);
}

TEST(search_filters_and_pages) {
  Archive a;  // in memory: no directory
  std::string err;
  for (int i = 0; i < 40; i++) {
    DemoSpec d;
    d.salt = i;
    d.start = 1794450000 - int64_t{i} * 3600;
    d.mode = i % 2 ? "ffa" : "4on4";
    d.map = i % 3 ? "tower" : "crypt";
    if (i == 7) d.players = {{"Quill", "red"}, {"brand", "blue"}};
    if (i == 9) d.tags = {"final"};
    CHECK(archive_add(a, demo(d), 1794450000, {}, err) == AddResult::added);
  }
  const int64_t now = 1794450000;
  DemoPage p = archive_search(a, DemoQuery{}, now);
  CHECK(p.total == 40 && p.pages == 3 && p.rows.size() == 15);
  CHECK(p.rows[0]->start_s == now && p.rows[1]->start_s == now - 3600);  // newest first
  DemoQuery q;
  q.page = 3;
  CHECK(archive_search(a, q, now).rows.size() == 10);
  q = DemoQuery{};
  q.player = "QUI";
  p = archive_search(a, q, now);
  CHECK(p.total == 1 && p.rows[0]->start_s == now - int64_t{7} * 3600);
  q = DemoQuery{};
  q.mode = "ffa";
  q.map = "crypt";
  CHECK(archive_search(a, q, now).total == 7);  // odd multiples of 3 under 40
  q = DemoQuery{};
  q.tag = "final";
  CHECK(archive_search(a, q, now).total == 1);
  q = DemoQuery{};
  q.days = 1;
  CHECK(archive_search(a, q, now).total == 25);  // 0..24 hours old
  q = DemoQuery{};
  q.team = "green";
  CHECK(archive_search(a, q, now).total == 0);
  const DemoQuery parsed = demo_query("/v1/demos?player=qu%20ill&mode=ffa&page=2&days=x");
  CHECK(parsed.player == "qu ill" && parsed.mode == "ffa" && parsed.page == 2 && parsed.days == 0);
}

TEST(prune_drops_the_oldest_untagged) {
  Archive a;
  std::string err;
  for (int i = 0; i < 6; i++) {
    DemoSpec d;
    d.salt = i;
    d.start = 1000 + int64_t{i} * 100;
    if (i == 0) d.tags = {"league"};
    CHECK(archive_add(a, demo(d), 2000, {}, err) == AddResult::added);
  }
  const int64_t one = a.rows[0].size;
  const int64_t cap = a.bytes - 2 * one;       // room for all but two untagged demos
  CHECK(archive_prune(a, cap, 0, 2000) == 2);  // the two oldest untagged
  CHECK(a.rows.size() == 4 && a.bytes == cap);
  CHECK(a.rows.back().start_s == 1000);  // the tagged one stays
  CHECK(a.rows[a.rows.size() - 2].start_s == 1300);
  CHECK(archive_prune(a, 0, 550, 2000) == 2);  // older than 550 s: 1300 and 1400, not 1500
  CHECK(a.rows.size() == 2 && a.rows[0].start_s == 1500 && a.rows[1].start_s == 1000);
}

TEST(paths_answer_json_and_the_file) {
  const fs::path dir = fresh_dir("paths");
  Archive a;
  std::string err;
  CHECK(archive_open(a, dir.string(), err));
  const std::vector<uint8_t> bytes = demo(DemoSpec{});
  DemoRow row;
  CHECK(archive_add(a, bytes, 1794450000, {}, err, &row) == AddResult::added);
  Answer out;
  CHECK(archive_answer(&a, "/v1/demos?player=vale", 1794450000, out));
  ge::core::FlatJson j;
  CHECK(out.code == 200 && ge::core::read_flat(out.body, j, err, true));
  CHECK(j.at("total").text == "1" && j.at("demos.0.sha256").text == row.sha);
  CHECK(j.count("demos.0.players.0.score") == 0);  // no result in the list
  CHECK(archive_answer(&a, "/v1/demos/" + row.sha, 0, out) && out.code == 200);
  CHECK(has(out.body, "\"score\":41"));
  CHECK(archive_answer(&a, "/" + archive_path(row.sha), 0, out) && out.code == 200);
  CHECK(out.type == "application/octet-stream" && out.max_age_s == 31536000);
  CHECK(out.body == std::string(bytes.begin(), bytes.end()));
  CHECK(archive_answer(&a, "/d/" + row.sha.substr(0, 3) + "/" + row.sha + ".json", 0, out));
  CHECK(out.code == 200 && has(out.body, "\"map\":\"tower\""));
  CHECK(archive_answer(&a, "/d/abc/" + row.sha + ".ger", 0, out) && out.code == 404);
  CHECK(archive_answer(&a, "/v1/demos/nothex", 0, out) && out.code == 404);
  CHECK(!archive_answer(&a, "/v1/servers", 0, out));
  CHECK(archive_answer(nullptr, "/v1/demos", 0, out) && out.code == 404 &&
        has(out.body, "--archive"));
  fs::remove_all(dir);
}

TEST(demos_page_lists_and_folds_the_scores) {
  Archive a;
  std::string err;
  DemoSpec d;
  d.players = {{"<b>Vale</b>", "red"}, {"sable", "blue"}};
  d.scores = {{"<b>Vale</b>", 41}};
  CHECK(archive_add(a, demo(d), 1794450000, {}, err) == AddResult::added);
  HubSpec spec;
  spec.game = "Test Game";
  spec.id = "test_game";
  spec.modes = {{"4on4", 9}};
  Poller p;
  const Answer page = hub_answer(p, spec, "/demos?player=vale", 0, &a, 1794450000);
  CHECK(page.code == 200 && page.type == "text/html");
  CHECK(has(page.body, "&lt;b&gt;Vale&lt;/b&gt;") && !has(page.body, "<b>Vale"));
  CHECK(has(page.body, "<details><summary>scores</summary>"));
  CHECK(has(page.body, "test_game://demo/" + a.rows[0].sha));
  CHECK(has(page.body, "2026-11-12 02:20") && has(page.body, "20:00"));
  CHECK(has(hub_answer(p, spec, "/demos?player=nobody", 0, &a, 0).body, "No demo matches."));
  CHECK(has(hub_answer(p, spec, "/demos", 0, nullptr, 0).body, "This hub keeps no demos."));
  CHECK(utc_text(0) == "1970-01-01 00:00" && utc_text(951782400) == "2000-02-29 00:00");
}
