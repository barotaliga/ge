// Tests: a program's words by key (plugins/progs/localized.hpp): the text file's lines (comments,
// quotes, escapes, the byte-order mark, CRLF, the first of two keys), UTF-8 into the classic set,
// a whole-string key resolved and anything else left, and the print rule's placeholders and its
// join when there are none.
//
// ge:summary Tests: the localized-text file and the `$key` print rule
// ge:keywords test localization localized read_localized localize_args placeholder dollar-key
// ge:skill progs testing
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "progs/localized.hpp"

namespace pg = ge::progs;

namespace {

pg::LocalizedText table() {
  pg::LocalizedText t;
  pg::read_localized(
      "\xEF\xBB\xBF// the words\r\n"
      "qc_got_item = \"You got the {}\\n\"\r\n"
      "qc_death_rl1 = \"{0} rides {1}'s rocket\\n\" trailing words dropped\n"
      "  qc_nailgun=\"Nailgun\"\n"
      "map_skill_normal = \"This hall selects NORMAL skill\"\n"
      "qc_item_armor = \"You got armor\\n\"\n"
      "qc_item_armor = \"the second one loses\"\n"
      "qc_bare = bare words   \n"
      "qc_escapes = \"a\\tb \\\"q\\\" \\\\ \\x\"\n"
      "qc_braces = \"{x} {2} {}\"\n"
      "qc_utf8 = \"caf\xC3\xA9 \xE2\x80\x9Cquoted\xE2\x80\x9D \xE2\x80\x94 \xE2\x80\xA6 "
      "\xE2\x98\x83\"\n"
      "no equals sign here\n"
      "\n",
      t);
  return t;
}

std::string print(const pg::LocalizedText* t, std::vector<std::string_view> args) {
  char buf[256];
  pg::localize_args(t, args, buf, sizeof buf);
  return buf;
}

}  // namespace

TEST(the_file_reads_comments_quotes_escapes_and_the_first_of_two_keys) {
  const pg::LocalizedText t = table();
  CHECK(t.words.size() == 9);
  CHECK(t.words.at("qc_got_item") == "You got the {}\n");
  CHECK(t.words.at("qc_death_rl1") == "{0} rides {1}'s rocket\n");
  CHECK(t.words.at("qc_nailgun") == "Nailgun");
  CHECK(t.words.at("qc_item_armor") == "You got armor\n");
  CHECK(t.words.at("qc_bare") == "bare words");
  CHECK(t.words.at("qc_escapes") == "a\tb \"q\" \\ x");
  CHECK(t.words.at("qc_utf8") == "cafe \"quoted\" - ... \x0B");
}

TEST(only_a_whole_string_key_the_table_has_is_resolved) {
  const pg::LocalizedText t = table();
  CHECK(pg::resolve_key(&t, "$map_skill_normal") == "This hall selects NORMAL skill");
  CHECK(pg::resolve_key(&t, "$qc_missing") == "$qc_missing");
  CHECK(pg::resolve_key(&t, "a $qc_nailgun") == "a $qc_nailgun");
  CHECK(pg::resolve_key(&t, "$qc_nailgun ") == "$qc_nailgun ");
  CHECK(pg::resolve_key(&t, "qc_nailgun") == "qc_nailgun");
  CHECK(pg::resolve_key(&t, "") == "");
  CHECK(pg::resolve_key(nullptr, "$qc_nailgun") == "$qc_nailgun");
}

TEST(a_key_with_arguments_fills_its_placeholders_each_argument_resolved) {
  const pg::LocalizedText t = table();
  CHECK(print(&t, {"$qc_got_item", "$qc_nailgun"}) == "You got the Nailgun\n");
  CHECK(print(&t, {"$qc_death_rl1", "Ranger", "Bot"}) == "Ranger rides Bot's rocket\n");
  CHECK(print(&t, {"$qc_item_armor"}) == "You got armor\n");
  // `{}` is argument 0; a missing index puts in nothing; other braces stay.
  CHECK(print(&t, {"$qc_braces", "zero", "one"}) == "{x}  zero");
}

TEST(with_no_placeholder_or_no_table_the_arguments_are_joined) {
  const pg::LocalizedText t = table();
  CHECK(print(&t, {"You got the ", "$qc_nailgun", "\n"}) == "You got the Nailgun\n");
  CHECK(print(&t, {"$qc_missing", " x"}) == "$qc_missing x");
  CHECK(print(nullptr, {"$qc_got_item", "$qc_nailgun"}) == "$qc_got_item$qc_nailgun");
  char small[8];
  const std::vector<std::string_view> args = {"$qc_got_item", "$qc_nailgun"};
  pg::localize_args(&t, args, small, sizeof small);
  CHECK(std::string(small) == "You got");
}
