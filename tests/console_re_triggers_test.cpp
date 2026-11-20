// engine/console/re_triggers: a trigger made, listed and shown; its alias run on a matching line
// of a kind its mask holds, the match settings filled; the newest tried first and `final` stopping
// the rest; `remove`, `interval`, `disable` and `noaction`; a pattern that does not read; the
// commands that take name patterns; re_trigger_match ignoring disable and interval; `on` false.
//
// ge:summary Tests the regular-expression triggers
// ge:keywords test re_trigger re_trigger_options re_trigger_match regex trigger mask
// ge:skill console testing
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "console/re_triggers.hpp"

namespace con = ge::console;

namespace {

std::string value(con::Console& c, const char* name) {
  const con::Cvar* v = c.cvars().find(name);
  return v != nullptr ? v->value : std::string("(none)");
}

}  // namespace

TEST(a_matching_line_runs_the_alias_and_fills_the_matches) {
  con::Console c;
  con::ReTriggers t;
  con::add_re_triggers(c, t);
  c.execute("set frags 0; alias fragged \"inc frags\"");
  c.execute("re_trigger fragged \"^([a-z]+) was gibbed by ([a-z]+)\"");
  CHECK(t.list.size() == 1 && t.list[0].pattern == "^([a-z]+) was gibbed by ([a-z]+)");
  con::ReLine r = con::re_trigger_line(c, t, "bob was gibbed by eve\n", con::kReMedium, 1);
  c.run();
  CHECK(!r.remove && value(c, "frags") == "1");
  CHECK(value(c, "re_trigger_match_1") == "bob" && value(c, "re_trigger_match_2") == "eve");
  CHECK(c.cvars().set("re_trigger_match_1", "x", con::Source::local) != con::SetResult::changed);
  // Not a kind its mask holds: the echo's.
  con::re_trigger_line(c, t, "bob was gibbed by eve\n", con::kReEcho, 2);
  c.run();
  CHECK(value(c, "frags") == "1");
  c.clear_output();
  c.execute("re_trigger fragged");
  CHECK(c.output().size() == 3 && c.output()[2] == "  matched 1 times");
}

TEST(options_order_and_the_pattern_commands) {
  con::Console c;
  con::ReTriggers t;
  con::add_re_triggers(c, t);
  c.execute("set a 0; set b 0; alias ta \"inc a\"; alias tb \"inc b\"");
  c.execute("re_trigger ta quad; re_trigger tb quad");  // tb is the newest: tried first
  c.execute("re_trigger_options tb final remove nolog");
  con::ReLine r = con::re_trigger_line(c, t, "you got the quad\n", con::kReLow, 1);
  c.run();
  CHECK(r.remove && r.nolog && value(c, "a") == "0" && value(c, "b") == "1");
  c.execute("re_trigger_options tb notfinal interval 5");
  con::re_trigger_line(c, t, "quad\n", con::kReLow, 2);  // tb too soon
  c.run();
  CHECK(value(c, "a") == "1" && value(c, "b") == "1");
  c.execute("re_trigger_disable \"^t\"");  // a pattern of names
  con::re_trigger_line(c, t, "quad\n", con::kReLow, 10);
  c.run();
  CHECK(value(c, "a") == "1");
  t.now = 10;
  c.execute("re_trigger_match ta \"the quad\"");  // even disabled
  CHECK(value(c, "a") == "2");
  c.execute("re_trigger_enable ta; re_trigger_options ta noaction");
  con::re_trigger_line(c, t, "quad\n", con::kReLow, 20);
  c.run();
  CHECK(value(c, "a") == "2" && t.list[1].matched == 3);
  t.on = false;
  CHECK(!con::re_trigger_line(c, t, "quad\n", con::kReLow, 30).remove);
  c.execute("re_trigger ta \"(\"");  // does not read: gone
  CHECK(t.list.size() == 1);
  c.execute("re_trigger_delete \"t.\"");
  CHECK(t.list.empty());
}
