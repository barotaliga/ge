// The dedicated server's macro list and $com_maplist (engine/server/macros.hpp).
//
// ge:summary Tests of the dedicated server's macrolist, wildcards and $com_maplist
// ge:keywords macrolist com_maplist wildcard
// ge:skill net
#include "server/macros.hpp"

#include "check.hpp"

namespace sv = ge::server;

TEST(macrolist_lists_each_macro_in_a_field_then_the_count) {
  ge::console::Console con(true);
  con.add_macro("net_uprate", [] { return std::string("1200"); });
  con.add_macro("com_maplist", [] { return std::string(70, 'm'); });
  con.add_macro("random", [] { return std::string("7"); });
  const std::string all = sv::server_macro_list(con, "");
  CHECK_MSG(all == "com_maplist      " + std::string(63, 'm') +
                       "\n"
                       "net_uprate       1200\n"
                       "random           7\n"
                       "3 of 3 macros",
            all);
  CHECK(sv::server_macro_list(con, "net_*") == "net_uprate       1200\n1 of 3 macros");
  CHECK(sv::server_macro_list(con, "NET_*") == "0 of 3 macros");  // case kept
}

TEST(a_wildcard_matches_the_whole_name) {
  CHECK(sv::wildcard_match("*", "anything") && sv::wildcard_match("com_?ate", "com_date"));
  CHECK(!sv::wildcard_match("com", "com_date") && sv::wildcard_match("c*e", "com_date"));
  CHECK(sv::wildcard_match("a\\*b", "a*b") && !sv::wildcard_match("a\\*b", "axb"));
}

TEST(com_maplist_sorts_ignoring_case_and_keeps_each_map_once) {
  CHECK(sv::server_map_list({"q2dm2", "Base1", "q2dm1", "base1", "Q2DM1"}) == "Base1 q2dm1 q2dm2");
  CHECK(sv::server_map_list({}).empty());
}

TEST(console_prefix_stamps_a_line_with_its_level_and_the_time) {
  std::tm now{};
  now.tm_year = 126;
  now.tm_mon = 10;
  now.tm_mday = 22;
  now.tm_hour = 7;
  now.tm_min = 5;
  CHECK(sv::terminal_prefix("", sv::LineLevel::normal, now).empty());
  CHECK(sv::terminal_prefix("[%H:%M] ", sv::LineLevel::normal, now) == "[07:05] ");
  CHECK(sv::terminal_prefix("@: ", sv::LineLevel::warning, now) == "W: ");
#if !defined(_WIN32)
  CHECK(sv::terminal_prefix("<?>", sv::LineLevel::error, now) == "<3>");
  CHECK(sv::terminal_prefix("<?>@ ", sv::LineLevel::normal, now) == "<6>A ");
#endif
}
