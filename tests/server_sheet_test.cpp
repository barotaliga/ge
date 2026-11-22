// engine/server/sheet: a server's settings sheet round-trips its rows through their wire text,
// drops the help and then the values to fit a message, takes the begin, row and end parts,
// replaces a row by name, offers a menu the grammar's values and a ladder of round numbers, and
// writes the line that changes a row (a vote's by default, or the row's own).
//
// ge:summary Test: the settings sheet's rows, wire, choices and change lines
// ge:keywords test settings-sheet sheet_row_text sheet_take sheet_choices sheet_change_line
// ge:skill net testing
#include "check.hpp"
#include "server/sheet.hpp"

using namespace ge::server;

namespace {

SheetRow frags() {
  SheetRow r;
  r.name = "fraglimit";
  r.words = "Frag limit";
  r.value = "20";
  r.values = "0: no limit; 1..N: that many frags";
  r.help = "The frags a player needs to end the match.";
  r.how = SheetHow::vote;
  return r;
}

}  // namespace

TEST(a_row_round_trips_through_its_wire_text) {
  SheetRow r = frags();
  r.group = "Rules";
  r.line = "vote fraglimit %v";
  SheetRow back;
  CHECK(sheet_row_read(sheet_row_text(r), back));
  CHECK(back.name == r.name && back.words == r.words && back.group == r.group);
  CHECK(back.value == r.value && back.values == r.values && back.help == r.help);
  CHECK(back.line == r.line && back.how == SheetHow::vote);
  r.words = "a\\b";  // a backslash would end the field: written as a slash
  CHECK(sheet_row_read(sheet_row_text(r), back) && back.words == "a/b");
  CHECK(!sheet_row_read("\\w\\no name", back));
  // A key this reader does not know is passed over.
  CHECK(sheet_row_read("\\n\\x\\zz\\later\\v\\1", back) && back.name == "x" && back.value == "1");
}

TEST(a_long_row_drops_its_help_then_its_values) {
  SheetRow r = frags();
  r.help = std::string(400, 'h');
  r.values = std::string(300, 'v');
  const std::string whole = sheet_row_text(r);
  CHECK(whole.size() > 700);
  SheetRow back;
  CHECK(sheet_row_read(sheet_row_text(r, 400), back));
  CHECK(back.help.empty() && back.values.size() == 300 && back.how == SheetHow::vote);
  CHECK(sheet_row_read(sheet_row_text(r, 100), back));
  CHECK(back.help.empty() && back.values.empty() && back.value == "20");
}

TEST(the_parts_build_a_sheet_and_a_row_replaces_its_name) {
  SettingsSheet s;
  s.asked = true;
  sheet_take(s, 0, "");
  sheet_take(s, 1, sheet_row_text(frags()));
  SheetRow aid;
  aid.name = "speedjump:3";
  aid.words = "Easy jump for Ann";
  aid.group = "Player help";
  aid.value = "off";
  sheet_take(s, 1, sheet_row_text(aid));
  CHECK(s.rows.size() == 2 && !s.whole && s.asked);
  sheet_take(s, 2, "");
  CHECK(s.whole && !s.asked);
  const uint32_t rev = s.revision;
  SheetRow later = frags();
  later.value = "30";
  sheet_take(s, 1, sheet_row_text(later));
  CHECK(s.rows.size() == 2 && s.rows[0].value == "30" && s.revision != rev);
  CHECK(sheet_find(s, "speedjump:3") != nullptr && sheet_find(s, "nope") == nullptr);
  CHECK(sheet_groups(s).size() == 1 && sheet_groups(s)[0] == "Player help");
  sheet_take(s, 0, "");
  CHECK(s.rows.empty() && !s.whole);
}

TEST(choices_are_the_named_values_then_round_numbers) {
  SheetRow r = frags();
  const std::vector<SheetChoice> c = sheet_choices(r);
  CHECK(c.size() > 10);
  CHECK(c[0].value == "0" && c[0].meaning == "no limit");
  CHECK(c[1].value == "1" && c[1].meaning.empty());
  bool twenty = false;
  for (const SheetChoice& x : c) twenty = twenty || x.value == "20";
  CHECK(twenty);
  // The value now, when it is no round number, stands where it falls.
  r.value = "17";
  const std::vector<SheetChoice> d = sheet_choices(r);
  for (size_t i = 0; i < d.size(); i++)
    if (d[i].value == "17") CHECK(d[i - 1].value == "15" && d[i + 1].value == "20");
  // A short range is every whole number; the default's mark is not shown.
  SheetRow m;
  m.name = "k_midair_minheight";
  m.values = "0: 64 units; 1: 128 units (the default); 2: 256 units";
  m.value = "1";
  const std::vector<SheetChoice> e = sheet_choices(m);
  CHECK(e.size() == 3 && e[1].meaning == "128 units");
  CHECK(sheet_meaning(m, "2") == "256 units");
  SheetRow t;
  t.name = "hostname";
  t.values = "TEXT: the name";
  t.value = "Arena";
  CHECK(sheet_choices(t).size() == 1 && sheet_choices(t)[0].value == "Arena");
}

TEST(the_change_line_is_a_vote_by_default_or_the_rows_own) {
  SheetRow r = frags();
  CHECK(sheet_change_line(r, "30") == "vote fraglimit 30");
  r.how = SheetHow::direct;
  CHECK(sheet_change_line(r, "30") == "fraglimit 30");
  r.line = "cmd timelimit %v";
  CHECK(sheet_change_line(r, "a b") == "cmd timelimit \"a b\"");
  r.line = "antilag";  // a word that turns it by itself
  CHECK(sheet_change_line(r, "5") == "antilag");
  CHECK(std::string(sheet_mark(SheetHow::vote)) == "*" && std::string(sheet_mark(SheetHow::fixed)).empty());
}

TEST(words_come_from_the_name_when_none_are_given) {
  CHECK(sheet_words_of("timelimit") == "Time limit");
  CHECK(sheet_words_of("sv_gravity") == "Gravity");
  CHECK(sheet_words_of("k_midair_minheight") == "Midair minheight");
  CHECK(sheet_words_of("pm_speedjump") == "Speedjump");
}

TEST(a_stepping_row_sends_its_steps) {
  SheetRow r;
  r.name = "fraglimit";
  r.values = "0: no limit; 10: 10 frags; 20: 20 frags; 30: 30 frags";
  r.value = "10";
  r.line = "fragsup|fragsdown";
  r.how = SheetHow::direct;
  CHECK(sheet_change_lines(r, "30") == std::vector<std::string>({"fragsup", "fragsup"}));
  CHECK(sheet_change_lines(r, "0") == std::vector<std::string>({"fragsdown"}));
  CHECK(sheet_change_lines(r, "10").empty() && sheet_change_lines(r, "15").empty());
  r.name = "teamplay";
  r.values = "1: one; 2: two; 3: three; 4: four";
  r.value = "3";
  r.line = "tp|";  // cycles forward only
  CHECK(sheet_change_lines(r, "4") == std::vector<std::string>({"tp"}));
  CHECK(sheet_change_lines(r, "1") == std::vector<std::string>({"tp", "tp"}));
  r.line = "";
  r.how = SheetHow::vote;
  CHECK(sheet_change_lines(r, "1") == std::vector<std::string>({"vote teamplay 1"}));
}
