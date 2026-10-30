// `ge digest` and `ge golden` (engine/cli/golden.hpp), the parts that need no build: a golden's
// two versions compared column by column under its header's names, a header of prose naming no
// column, rows added and gone, a double's bits shown as the double, and the golden files a test's
// source names.
//
// ge:summary Test: ge golden's column diff, its text and JSON, and the goldens a test names
// ge:keywords test cli golden diff columns moved header double-bits goldens_named_in
// ge:keywords parts-word
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

TEST(a_parts_word_names_each_part_a_moved_digest_moved_in) {
  const std::string before =
      "# digest parts stars\n"
      "aa11 skiers:00000001,army:00000002,wanted:00000003 2\n"
      "bb22 skiers:00000004,army:00000005,wanted:00000006 3\n";
  const std::string after =
      "# digest parts stars\n"
      "aa12 skiers:00000001,army:0000000f,wanted:00000003 2\n"
      "bb23 skiers:00000004,army:0000000e,wanted:0000000d 3\n";
  const GoldenDiff d = diff_golden("g.txt", before, after);
  CHECK(d.moved.size() == 3);
  CHECK(d.moved[0] == std::make_pair(std::string("digest"), 2));
  CHECK(d.moved[1] == std::make_pair(std::string("parts.army"), 2));
  CHECK(d.moved[2] == std::make_pair(std::string("parts.wanted"), 1));
  CHECK(d.rows[1].cells.size() == 3 && d.rows[1].cells[2].before == "00000006");
  const std::string text = format_golden_diff(d, 8);
  CHECK_MSG(has(text, "  parts.army moved in 2 rows\n") &&
                has(text, "parts.army 00000005 -> 0000000e; parts.wanted 00000006 -> 0000000d"),
            text);
  // Under a prose header the digest before a parts word is named for what it is.
  const GoldenDiff prose =
      diff_golden("g.txt", "# each minute: digest, parts, stars\n1 c325e3f04db56830 a:1,b:2 3\n",
                  "# each minute: digest, parts, stars\n1 c325e3f04db56831 a:1,b:3 3\n");
  CHECK(prose.moved.size() == 2 && prose.moved[0].first == "digest" &&
        prose.moved[1].first == "parts.b");
  CHECK_MSG(has(format_golden_diff(prose, 8), "digest c325e3f04db56830 -> c325e3f04db56831;"),
            format_golden_diff(prose, 8));
  // Words that only look like parts (one part, or the names changed) are whole words.
  const GoldenDiff one = diff_golden("g.txt", "a x:1\n", "a x:2\n");
  CHECK(one.moved.size() == 1 && one.moved[0].first == "column 2");
  const GoldenDiff renamed = diff_golden("g.txt", "a x:1,y:2\n", "a x:1,z:2\n");
  CHECK(renamed.moved.size() == 1 && renamed.moved[0].first == "column 2");
}

TEST(a_tests_goldens_are_the_files_its_source_names) {
  const auto named = goldens_named_in(
      "constexpr const char* kGolden = \"tests/golden/fall_line_sim.txt\";\n"
      "// rewrite tests/golden/fall_line_sim.txt and tests/golden/b_2.txt\n");
  CHECK(named.size() == 2 && named[0] == "tests/golden/fall_line_sim.txt" &&
        named[1] == "tests/golden/b_2.txt");
  CHECK(goldens_named_in("no goldens here").empty());
}

TEST(the_labs_a_change_reaches_are_its_objects_its_moved_rows_or_every_lab) {
  const std::map<std::string, std::string> sources{{"glide", "games/g/labs/glide_lab.cpp"},
                                                   {"grid", "engine/lab/labs/grid_lab.cpp"},
                                                   {"old", ""}};
  Reach r;
  r.changed = {"plugins/wing/acro.cpp"};
  r.objects = {"plugins/wing/acro.cpp", "games/g/labs/glide_lab.cpp"};
  CHECK((labs_reached(r, sources, {}) == std::vector<std::string>{"glide"}));
  CHECK((labs_reached(r, sources, {"old"}) == std::vector<std::string>{"glide", "old"}));
  r.changed.insert("plugins/wing/CMakeLists.txt");  // its own directory's: as before
  CHECK((labs_reached(r, sources, {}) == std::vector<std::string>{"glide"}));
  r.changed.insert("cmake/Modules.cmake");  // the tree's: every lab
  CHECK(labs_reached(r, sources, {}).size() == 3);
}

TEST(a_changed_baseline_file_names_the_lab_it_moved) {
  const std::set<std::string> changed = {"tests/digests/glide.txt", "tests/digests/ski-cross.txt",
                                         "tests/digests/old/x.txt", "tests/golden/glide.txt",
                                         "plugins/wing/acro.cpp"};
  CHECK((moved_digest_labs(changed) == std::set<std::string>{"glide", "ski-cross"}));
}

TEST(a_changed_asset_reaches_the_labs_that_read_it_by_path) {
  const auto a =
      asset_changes({"plugins/skier/assets/skier/skier.glb", "plugins/skier/assets/skier/build.py",
                     "plugins/skier/assets/skier/data.cpp", "engine/core/math.hpp"});
  CHECK(a.size() == 1 && a[0].root == "plugins/skier" && a[0].name == "skier");
  CHECK(reads_asset("plugins/skier/labs/anim_lab.cpp", "", a[0]));
  CHECK(reads_asset("games/g/labs/views_lab.cpp", "load(\"skier.glb\")", a[0]));
  CHECK(!reads_asset("games/g/labs/sled_lab.cpp", "#include \"sled.hpp\"", a[0]));
}
