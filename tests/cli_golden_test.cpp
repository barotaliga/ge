// `ge digest` and `ge golden` (engine/cli/golden.hpp), the parts that need no build: a golden's
// two versions compared column by column under its header's names, a header of prose naming no
// column, rows added and gone, a double's bits shown as the double, and the golden files a test's
// source names.
//
// ge:summary Test: ge golden's column diff, its text and JSON, and the goldens a test names
// ge:keywords test cli golden diff columns moved header double-bits goldens_named_in
// ge:skill testing agent-interface
#include <string>

#include "check.hpp"
#include "cli/golden.hpp"

using namespace ge::cli;

namespace {

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

const char* kBefore =
    "# seed finished time-bits flights digest (a_test, generator v1)\n"
    "38 1 406df5e817eab837 2 f881694eb8a19b3f\n"
    "1 1 406ccf156054cf31 4 115b94db49a6b3d7\n"
    "2 1 4070224e42ed721e 6 e71f3cd99ca55c5a\n";

}  // namespace

TEST(a_golden_diff_names_the_columns_that_moved) {
  const std::string after =
      "# seed finished time-bits flights digest (a_test, generator v1)\n"
      "38 1 406df5e817eab838 3 0000000000000001\n"
      "1 1 406ccf156054cf31 4 115b94db49a6b3d7\r\n"
      "2 1 4070224e42ed721e 6 0000000000000002\n";
  const GoldenDiff d = diff_golden("tests/golden/a.txt", kBefore, after);
  CHECK(d.rows_before == 3 && d.rows_after == 3 && !d.header_changed);
  CHECK(d.rows.size() == 2 && d.rows[0].row == 1 && d.rows[0].key == "38" && d.rows[1].row == 3);
  CHECK(d.moved.size() == 3);
  CHECK(d.moved[0] == std::make_pair(std::string("time-bits"), 1));
  CHECK(d.moved[1] == std::make_pair(std::string("flights"), 1));
  CHECK(d.moved[2] == std::make_pair(std::string("digest"), 2));
  const std::string text = format_golden_diff(d, 1);
  CHECK_MSG(has(text, "tests/golden/a.txt: 2 of 3 rows changed\n") &&
                has(text, "  digest moved in 2 rows\n") &&
                has(text,
                    "  row 1 (38): time-bits 406df5e817eab837 (239.68458171695394) -> "
                    "406df5e817eab838 (239.68458171695397); flights 2 -> 3;") &&
                has(text, "... and 1 more rows"),
            text);
  // A digest's bits are not a double.
  CHECK(!has(text, "f881694eb8a19b3f ("));
  const std::string json = golden_diff_json(d, 8);
  CHECK_MSG(has(json, "\"rows_changed\":2") && has(json, "{\"column\":\"digest\",\"rows\":2}") &&
                has(json, "\"before_value\":\"239.68458171695394\""),
            json);
}

TEST(a_prose_header_names_no_column_and_rows_can_come_and_go) {
  const std::string before = "# FNV-1a 64 of each render\nsine 4ce3121b87c5ebe9\nsquare f865\n";
  const std::string after = "# FNV-1a 64 of each render\nsine 4ce3121b87c5ebe9\nsaw 11 22\n";
  const GoldenDiff d = diff_golden("g.txt", before, after);
  CHECK(d.rows.size() == 1 && d.rows[0].key == "square");
  CHECK(d.moved.size() == 3 && d.moved[0].first == "column 1" && d.moved[2].first == "column 3");
  CHECK(d.rows[0].cells[2].before == "-" && d.rows[0].cells[2].after == "22");
  const GoldenDiff grown = diff_golden("g.txt", before, before + "tri 33\n");
  CHECK(grown.rows_after == 3 && grown.rows.size() == 1 && grown.rows[0].key == "tri");
  CHECK(has(format_golden_diff(grown, 8), "(2 rows before, 3 after)"));
  const GoldenDiff same = diff_golden("g.txt", before, before);
  CHECK(same.rows.empty() && same.moved.empty());
}

TEST(a_row_inserted_mid_table_is_one_row_added_not_every_row_after_it) {
  const std::string before =
      "# scenario top-speed sink\nlaunch 41 1.5\nglide 38 1.2\nturn 36 1.9\nland 20 0\n";
  const std::string after =
      "# scenario top-speed sink\nlaunch 41 1.5\ndive 60 9\nglide 38 1.2\nturn 36 2.0\n";
  const GoldenDiff d = diff_golden("g.txt", before, after);
  CHECK(!d.by_key);  // the same count: rows match by place
  const std::string grown_after =
      "# scenario top-speed sink\nlaunch 41 1.5\ndive 60 9\nglide 38 1.2\nturn 36 2.0\n"
      "land 20 0\n";
  const GoldenDiff g = diff_golden("g.txt", before, grown_after);
  CHECK(g.by_key);
  CHECK(g.rows.size() == 2);
  CHECK(g.rows[0].key == "dive" && g.rows[0].change == "added" && g.rows[0].row == 2);
  CHECK(g.rows[1].key == "turn" && g.rows[1].change.empty() && g.rows[1].cells.size() == 1);
  CHECK(g.moved.size() == 1 && g.moved[0] == std::make_pair(std::string("sink"), 1));
  const std::string text = format_golden_diff(g, 8);
  CHECK_MSG(has(text, "rows matched by their first word") && has(text, "  row 2 (dive): added\n") &&
                has(text, "  row 4 (turn): sink 1.9 -> 2.0\n"),
            text);
  const GoldenDiff shrunk = diff_golden("g.txt", grown_after, before);
  CHECK(shrunk.by_key && shrunk.rows.size() == 2);
  CHECK(shrunk.rows[1].key == "dive" && shrunk.rows[1].change == "gone");
}

TEST(a_tests_goldens_are_the_files_its_source_names) {
  const auto named = goldens_named_in(
      "constexpr const char* kGolden = \"tests/golden/fall_line_sim.txt\";\n"
      "// rewrite tests/golden/fall_line_sim.txt and tests/golden/b_2.txt\n");
  CHECK(named.size() == 2 && named[0] == "tests/golden/fall_line_sim.txt" &&
        named[1] == "tests/golden/b_2.txt");
  CHECK(goldens_named_in("no goldens here").empty());
}
