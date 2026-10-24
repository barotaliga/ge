// The replay file (engine/replay/file.hpp) and its index (index.hpp): a file written and read back
// is the same file (its log, marks, clip, version and date); a file of another version of the game
// is refused naming both versions; malformed lines are named by their line; the clip is held
// inside the log; a directory's replays are listed by path, a broken one as a problem.
//
// ge:summary Tests: the replay file round-trips, refuses another version; the index lists a folder
// ge:keywords test replay file round-trip version refuse clip mark parse index scan
// ge:skill replay testing
// ge:test tests/replay_file_test.cpp
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/file.hpp"
#include "replay/index.hpp"
#include "replay_walker.hpp"

namespace gp = ge::play;
namespace gr = ge::replay;

using namespace replay_test;

namespace {

gr::ReplayFile sample() {
  gr::ReplayFile f;
  f.log = walk_log();
  f.log.digest = straight(f.log, f.log.steps());
  f.game_version = 1;
  f.marks = {{12, "takeoff", 1}, {40, "wipeout", 3.5}, {40, "reset", 0.25}, {290, "finish", 2}};
  f.clip_in = 10;
  f.clip_out = 120;
  f.recorded = "2026-10-23T11:00:00Z";
  return f;
}

}  // namespace

TEST(a_file_reads_back_as_it_was_written) {
  const gr::ReplayFile f = sample();
  const std::string text = gr::to_text(f);
  std::string error;
  const auto back = gr::parse_replay(text, error);
  CHECK_MSG(back.has_value(), error);
  if (!back) return;
  CHECK(gr::to_text(*back) == text);
  CHECK(back->game_version == 1 && back->recorded == f.recorded);
  CHECK(back->clip_in == 10 && back->clip_out == 120);
  CHECK(back->marks.size() == 4 && back->marks[1].kind == "wipeout" &&
        back->marks[1].weight == 3.5 && back->marks[2].kind == "reset");
  CHECK(back->log.steps() == 300 && back->log.digest == f.log.digest);
  CHECK(gr::plays_on(*back, kWalker, error));
}

TEST(the_whole_run_needs_no_clip_line) {
  gr::ReplayFile f = sample();
  f.clip_in = 0;
  f.clip_out = -1;
  f.recorded.clear();
  const std::string text = gr::to_text(f);
  CHECK(text.find("clip") == std::string::npos && text.find("recorded") == std::string::npos);
  std::string error;
  const auto back = gr::parse_replay(text, error);
  CHECK(back && back->in() == 0 && back->out() == 300);
}

TEST(the_clip_is_held_inside_the_log) {
  gr::ReplayFile f = sample();
  f.clip_in = 250;
  f.clip_out = 900;
  CHECK(f.in() == 250 && f.out() == 300);
  f.clip_out = 100;  // out before in: an empty clip at in
  CHECK(f.out() == 250);
  f.clip_in = 400;
  CHECK(f.in() == 300 && f.out() == 300);
}

TEST(another_version_of_the_game_is_refused_naming_both) {
  gr::ReplayFile f = sample();
  f.game_version = 3;
  std::string error;
  CHECK(!gr::plays_on(f, kWalker, error));
  CHECK_MSG(error.find("version 3") != std::string::npos &&
                error.find("plays version 1") != std::string::npos,
            error);
  f = sample();
  f.log.game = "other";
  CHECK(!gr::plays_on(f, kWalker, error));
}

TEST(a_bad_line_is_named_by_its_line) {
  std::string error;
  CHECK(!gr::parse_replay("ge-inputs 1\n", error) && error.find("ge-replay") != std::string::npos);
  CHECK(!gr::parse_replay("ge-replay 2\n", error) && error.find("version 1") != std::string::npos);
  CHECK(!gr::parse_replay("ge-replay 1\ngame_version 1\nmark 4 Big-Air 1\ninputs\n", error));
  CHECK_MSG(error.find("line 3") != std::string::npos, error);
  CHECK(!gr::parse_replay("ge-replay 1\ninputs\n", error) &&
        error.find("game_version") != std::string::npos);
  CHECK(!gr::parse_replay("ge-replay 1\ngame_version 1\nfoo 1\n", error) &&
        error.find("unknown line `foo`") != std::string::npos);
  CHECK(!gr::parse_replay("ge-replay 1\ngame_version 1\n", error) &&
        error.find("before its `inputs`") != std::string::npos);
  CHECK(!gr::parse_replay("ge-replay 1\ngame_version 1\ninputs\nnot an input log\n", error) &&
        error.find("the inputs after line 3") != std::string::npos);
  CHECK(gr::mark_kind_ok("big_air2") && !gr::mark_kind_ok("") && !gr::mark_kind_ok("a b"));
}

TEST(the_index_lists_a_folder_of_replays) {
  namespace fsys = std::filesystem;
  const fsys::path dir = fsys::temp_directory_path() / "ge_replay_index_test";
  fsys::remove_all(dir);
  fsys::create_directories(dir);
  std::string error;
  gr::ReplayFile a = sample();
  CHECK(gr::save_replay((dir / "b.ge-replay").string(), a, error));
  a.marks.clear();
  a.clip_in = 0;
  a.clip_out = -1;
  CHECK(gr::save_replay((dir / "a.ge-replay").string(), a, error));
  std::ofstream((dir / "c.ge-replay").string()) << "not a replay\n";
  std::ofstream((dir / "notes.txt").string()) << "ignored\n";
  std::vector<std::string> problems;
  const std::vector<gr::IndexEntry> rows = gr::scan_replays(dir.string(), &problems);
  CHECK(rows.size() == 2 && problems.size() == 1);
  CHECK(rows[0].path.find("a.ge-replay") != std::string::npos && rows[0].marks == 0);
  const gr::IndexEntry& b = rows[1];
  CHECK(b.game == "walker" && b.seed == 7 && b.steps == 300 && b.seconds == 30);
  CHECK(b.marks == 4 &&
        b.kinds == std::vector<std::string>({"takeoff", "wipeout", "reset", "finish"}));
  CHECK(b.clip_in == 10 && b.clip_out == 120 && b.recorded == "2026-10-23T11:00:00Z");
  CHECK(problems[0].find("c.ge-replay") != std::string::npos);
  const auto loaded = gr::load_replay((dir / "b.ge-replay").string(), error);
  CHECK(loaded && gr::to_text(*loaded) == gr::to_text(sample()));
  fsys::remove_all(dir);
  problems.clear();
  CHECK(gr::scan_replays(dir.string(), &problems).empty() && problems.size() == 1);
}
