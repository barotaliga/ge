// cl_ignore_stufftext's filter (plugins/classic/protocol/stuff_filter.hpp).
//
// ge:summary Tests of which stuffed lines a classic client runs under cl_ignore_stufftext
// ge:keywords stufftext whitelist wildcard test
// ge:skill classic testing
#include "classic/protocol/stuff_filter.hpp"

#include "check.hpp"

namespace gc = ge::classic;

TEST(the_whitelist_patterns_match_the_whole_line) {
  CHECK(gc::stuff_pattern_match("bind *", "bind f1 +attack"));
  CHECK(!gc::stuff_pattern_match("bind", "bind f1 +attack"));  // the whole line
  CHECK(gc::stuff_pattern_match("set ?", "set x"));
  CHECK(!gc::stuff_pattern_match("set ?", "set xy"));
  CHECK(gc::stuff_pattern_match("a\\*b", "a*b") && !gc::stuff_pattern_match("a\\*b", "axb"));
  CHECK(!gc::stuff_pattern_match("Bind *", "bind f1"));  // case counts
  CHECK(gc::stuff_pattern_match("*", "") && gc::stuff_pattern_match("a*c*e", "abcde"));
  const auto lines = gc::read_stuff_whitelist("bind *\r\n\nalias *\n" + std::string(1024, 'x'));
  CHECK(lines.size() == 2 && lines[0] == "bind *" && lines[1] == "alias *");
}

TEST(cl_ignore_stufftext_lets_the_signon_and_the_whitelist_through) {
  gc::StuffFilter f;
  CHECK(gc::stuff_allowed(f, "bind f1 kill"));  // 0: everything
  f.level = 1;
  CHECK(!gc::stuff_allowed(f, "bind f1 kill"));
  CHECK(gc::stuff_allowed(f, "precache 1") && gc::stuff_allowed(f, "reconnect"));
  CHECK(gc::stuff_allowed(f, "changing") && gc::stuff_allowed(f, "cmd new"));
  CHECK(gc::stuff_ignored_line(f, "bind f1 kill").empty());  // 1: silently
  f.patterns = {"bind *"};
  CHECK(gc::stuff_allowed(f, "bind f1 kill") && !gc::stuff_allowed(f, "cmd new"));
  f.level = 2;
  CHECK(gc::stuff_ignored_line(f, "alias x y") == "Ignored stufftext: alias x y");
}
