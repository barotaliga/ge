// The demo watcher's table of every player and the player picked by name
// (games/shooters_edge/client/mvd_info.hpp), and a timedemo's record for timedemo.log
// (timedemo.hpp's bench_dump_record): the rows in slot order with their codes filled and padded,
// the header's words, unknown codes dropped, nothing for a one-player demo; names made plain and
// searched; the record's shape.
//
// ge:summary Tests: mvd_info's rows and header, demo_playlist_track_name's pick, the timedemo log
// ge:keywords test mvd_info mvd_info_setup header padding demo_playlist_track_name
// ge:keywords demo_benchmarkdumps timedemo-log
// ge:skill shooter replay testing
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/client/mvd_info.hpp"
#include "shooters_edge/client/timedemo.hpp"

namespace cl = shooters_edge::client;

namespace {

cl::Demo opened(const cl::MatchScript& m) {
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(cl::write_multiview_match(m), d, error), error.c_str());
  for (int i = 0; i < 3; i++) cl::next_frame(d, error);
  return d;
}

}  // namespace

TEST(mvd_info_lists_every_player_in_slot_order_with_the_codes_filled) {
  ge::console::Console con{false};
  const cl::Demo d = opened(cl::highlight_match());
  const std::vector<cl::MvdInfoLine> rows =
      cl::mvd_info_lines(d, "%-6n|%4h|%f|%Q|", false, con, nullptr);
  CHECK_MSG(rows.size() == 4, std::to_string(rows.size()));
  const cl::DemoState& s = d.state;
  for (size_t i = 0; i < rows.size(); i++) {
    std::string name = s.names[i];
    name.resize(std::max<size_t>(name.size(), 6), ' ');
    const std::string health = std::to_string(d.player_stats[i][cl::kStatHealth]);
    std::string want = name;
    want += '|';
    want.append(4 - std::min<size_t>(4, health.size()), ' ');
    want += health;
    want += '|';
    want += std::to_string(s.frags[i]);
    want += "||";  // %Q dropped
    CHECK_MSG(rows[i].text == want && !rows[i].header, rows[i].text + " / " + want);
  }
  const std::vector<cl::MvdInfoLine> head =
      cl::mvd_info_lines(d, "%-6n %h %W %", true, con, nullptr);
  CHECK(head.size() == 5 && head[0].header && head[0].text == "Nick   Health Best Weap. ");
  // The default reads the place (someplace with no names) and the weapon in hand.
  const std::vector<cl::MvdInfoLine> plain =
      cl::mvd_info_lines(d, cl::kMvdInfoSetup, false, con, nullptr);
  CHECK_MSG(plain[0].text.find(s.names[0] + " \x10") == 0, plain[0].text);
  // Not a multiview demo: nothing.
  cl::Demo single = d;
  single.state.format = cl::DemoFormat::single;
  CHECK(cl::mvd_info_lines(single, "%n", true, con, nullptr).empty());
}

TEST(demo_playlist_track_name_picks_the_earliest_match_and_never_a_spectator) {
  CHECK(cl::plain_track_name(" My/Name; ") == "my_name");
  CHECK(cl::plain_track_name("&cf00Red&rGuy") == "redguy");
  CHECK(cl::plain_track_name(std::string("\xc1\xc2", 2)) == "ab");  // the red half made plain
  cl::DemoState s;
  s.names[0] = "xxbob";
  s.names[2] = "Bobby";
  s.names[4] = "bob";
  s.spectator[4] = true;
  CHECK(cl::name_track_slot(s, "bob") == 2);   // at 0 in "bobby", before 2 in "xxbob"
  CHECK(cl::name_track_slot(s, "Bob") == -1);  // the wanted text is not lowered
  CHECK(cl::name_track_slot(s, "") == -1 && cl::name_track_slot(s, "zed") == -1);
  s.names[1] = "bob";
  CHECK(cl::name_track_slot(s, "bob") == 1);  // a tie: the lower slot
}

TEST(a_timedemo_s_result_is_kept_as_a_small_xml_record) {
  cl::BenchStats st;
  st.frames = 1234;
  st.seconds = 12.5;
  st.avg_fps = 98.72;
  const std::string r =
      cl::bench_dump_record("demo1 <x>", "640x480 \"lavapipe\"", st, "2026-11-21T06:00:00");
  CHECK_MSG(r.rfind("<timedemo date=\"2026-11-21T06:00:00\">\n", 0) == 0, r);
  CHECK(r.find("hardware=\"640x480 &quot;lavapipe&quot;\"") != std::string::npos);
  CHECK(r.find("<demo name=\"demo1 &lt;x&gt;\"/>") != std::string::npos);
  CHECK(r.find("<result frames=\"1234\" time=\"PT12.500000S\" fps=\"98.720000\"/>") !=
        std::string::npos);
  CHECK(r.size() > 20 && r.substr(r.size() - 12) == "</timedemo>\n");
  const std::filesystem::path log = std::filesystem::temp_directory_path() / "ge_se_timedemo.log";
  std::filesystem::remove(log);
  std::string error;
  CHECK(cl::bench_dump(log, r, error) && cl::bench_dump(log, r, error));
  CHECK(std::filesystem::file_size(log) == 2 * r.size());
  CHECK(!cl::bench_dump(log / "no" / "such", r, error) && !error.empty());
  CHECK(cl::local_date_now().size() == 19);
}
