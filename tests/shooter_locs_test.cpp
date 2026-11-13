// A MAP'S PLACE NAMES: a `.loc` text read into points (eighths of a unit, both line ends, bad
// lines skipped), the macros expanded (longest key first, the separator a dash, an unknown key
// kept), and a point named by its nearest place.
//
// ge:summary Tests: the .loc format read, its macros expanded and the nearest place named
// ge:keywords test loc locations parse_locs expand_loc_name loc_name_at macro
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/locs.hpp"

namespace sh = ge::shooter;

TEST(a_loc_text_is_read_into_points) {
  sh::Locs l;
  const int n = sh::parse_locs(
      "4096 -800 64 $loc_name_ra\r\n"
      "\r\n"
      "not a point\n"
      "-1312 2425 2656 $loc_name_mh$loc_name_separatorlow\n"
      "8 16 24   big room  \n"
      "1 2\n",
      l);
  CHECK(n == 3);
  CHECK(l.points.size() == 3);
  CHECK(l.points[0].name == "ra");
  CHECK(l.points[0].at.x == 512.f && l.points[0].at.y == -100.f && l.points[0].at.z == 8.f);
  CHECK(l.points[1].name == "mh-low");
  CHECK(l.points[1].at.x == -164.f);
  CHECK(l.points[2].name == "big room");
}

TEST(macros_expand_longest_key_first) {
  CHECK(sh::expand_loc_name("$loc_name_sng") == "sng");
  CHECK(sh::expand_loc_name("$loc_name_ng") == "ng");
  CHECK(sh::expand_loc_name("$loc_name_quad$loc_name_separator$loc_name_rl") == "quad-rl");
  CHECK(sh::expand_loc_name("gl$loc_name_separatorrox") == "gl-rox");
  CHECK(sh::expand_loc_name("$loc_name_flag2 room") == "flag2 room");
}

TEST(a_point_is_named_by_its_nearest_place) {
  sh::Locs l;
  sh::parse_locs("0 0 0 a\n8000 0 0 b\n", l);
  CHECK(sh::loc_name_at(l, {100, 0, 0}) == "a");
  CHECK(sh::loc_name_at(l, {900, 50, 0}) == "b");
  CHECK(sh::loc_name_at(sh::Locs{}, {0, 0, 0}).empty());
}

TEST(a_range_leaves_a_far_point_unnamed) {
  sh::Locs l;
  sh::parse_locs("0 0 0 a\n", l);
  CHECK(sh::loc_name_at(l, {500, 0, 0}, 500) == "a");  // the edge is in reach
  CHECK(sh::loc_name_at(l, {501, 0, 0}, 500).empty());
  CHECK(sh::loc_name_at(l, {501, 0, 0}) == "a");  // 0: no limit
  CHECK(sh::nearest_loc(l, {0, 900, 0}, 500) == nullptr);
  CHECK(!sh::remove_nearest_loc(l, {0, 900, 0}, nullptr, 500));
}

TEST(each_place_is_named_once_at_its_points_middle) {
  sh::Locs l;
  sh::parse_locs("0 0 0 a\n8000 0 0 b\n1600 800 0 a\n", l);
  const std::vector<sh::Loc> p = sh::loc_places(l);
  CHECK(p.size() == 2);
  CHECK(p[0].name == "a" && p[1].name == "b");
  CHECK(p[0].at.x == 100 && p[0].at.y == 50 && p[0].at.z == 0);
  CHECK(p[1].at.x == 1000);
  CHECK(sh::loc_places(sh::Locs{}).empty());
}

TEST(places_are_added_removed_and_written_as_a_file_reads_them_back) {
  ge::shooter::Locs locs;
  ge::shooter::add_loc(locs, {100.06f, -50.0f, 24.0f}, "quad room");
  ge::shooter::add_loc(locs, {-0.0625f, 512.0f, -3.1f}, "$loc_name_ra low");
  CHECK(locs.points.size() == 2);
  CHECK(locs.points[1].name == "ra low");
  // Eighths, halves away from zero, four places at least, the raw name kept, LF after each.
  CHECK_MSG(
      ge::shooter::locs_text(locs) == " 800 -400  192 quad room\n  -1 4096  -25 $loc_name_ra low\n",
      ge::shooter::locs_text(locs));
  ge::shooter::Locs back;
  CHECK(ge::shooter::parse_locs(ge::shooter::locs_text(locs), back) == 2);
  CHECK(back.points[1].raw == "$loc_name_ra low" && back.points[1].name == "ra low");
  CHECK(ge::shooter::locs_text(back) == ge::shooter::locs_text(locs));
  // The nearest goes; then the other; then none.
  ge::shooter::Loc gone;
  CHECK(ge::shooter::remove_nearest_loc(locs, {90, -40, 20}, &gone) && gone.name == "quad room");
  CHECK(ge::shooter::remove_nearest_loc(locs, {90, -40, 20}));
  CHECK(!ge::shooter::remove_nearest_loc(locs, {0, 0, 0}));
  // A name over 63 characters is cut on reading.
  ge::shooter::Locs cut;
  ge::shooter::parse_locs("0 0 0 " + std::string(80, 'x') + "\n", cut);
  CHECK(cut.points[0].raw.size() == ge::shooter::kLocNameMax);
}

TEST(a_recording_keeps_places_as_comment_lines_read_back_whole) {
  sh::Locs l;
  sh::add_loc(l, {100, 200, 24}, "quad");
  sh::add_loc(l, {-50, 0, 24}, "$loc_name_ra room");
  const std::vector<std::string> lines = sh::recorded_loc_lines(l);
  CHECK(lines.size() == 2);
  CHECK_MSG(lines[0] == "//loc  800 1600  192 quad\n", lines[0]);
  sh::Locs back;
  for (const std::string& line : lines) CHECK(sh::read_recorded_loc(line, back));
  CHECK(back.points.size() == 2 && back.points[1].name == "ra room");
  CHECK(!sh::read_recorded_loc("fullserverinfo \"\\a\\b\"", back));
  CHECK(back.points.size() == 2);
}
