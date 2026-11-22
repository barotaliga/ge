// A MAP'S PLACE NAMES: a `.loc` text read into points (eighths of a unit, both line ends, bad
// lines skipped), the macros expanded (longest key first, the separator a dash, an unknown key
// kept), and a point named by its nearest place.
//
// ge:summary Tests: the .loc format read, its macros expanded and the nearest place named
// ge:keywords test loc locations parse_locs expand_loc_name loc_name_at macro
// ge:skill classic testing
#include "check.hpp"
#include "classic/locs.hpp"

namespace sh = ge::shooter;

TEST(a_loc_text_is_read_into_points) {
  ge::classic::Locs l;
  const int n = ge::classic::parse_locs(
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
  CHECK(ge::classic::expand_loc_name("$loc_name_sng") == "sng");
  CHECK(ge::classic::expand_loc_name("$loc_name_ng") == "ng");
  CHECK(ge::classic::expand_loc_name("$loc_name_quad$loc_name_separator$loc_name_rl") == "quad-rl");
  CHECK(ge::classic::expand_loc_name("gl$loc_name_separatorrox") == "gl-rox");
  CHECK(ge::classic::expand_loc_name("$loc_name_flag2 room") == "flag2 room");
}

TEST(a_point_is_named_by_its_nearest_place) {
  ge::classic::Locs l;
  ge::classic::parse_locs("0 0 0 a\n8000 0 0 b\n", l);
  CHECK(ge::classic::loc_name_at(l, {100, 0, 0}) == "a");
  CHECK(ge::classic::loc_name_at(l, {900, 50, 0}) == "b");
  CHECK(ge::classic::loc_name_at(ge::classic::Locs{}, {0, 0, 0}).empty());
}

TEST(a_range_leaves_a_far_point_unnamed) {
  ge::classic::Locs l;
  ge::classic::parse_locs("0 0 0 a\n", l);
  CHECK(ge::classic::loc_name_at(l, {500, 0, 0}, 500) == "a");  // the edge is in reach
  CHECK(ge::classic::loc_name_at(l, {501, 0, 0}, 500).empty());
  CHECK(ge::classic::loc_name_at(l, {501, 0, 0}) == "a");  // 0: no limit
  CHECK(ge::classic::nearest_loc(l, {0, 900, 0}, 500) == nullptr);
  CHECK(!ge::classic::remove_nearest_loc(l, {0, 900, 0}, nullptr, 500));
}

TEST(each_place_is_named_once_at_its_points_middle) {
  ge::classic::Locs l;
  ge::classic::parse_locs("0 0 0 a\n8000 0 0 b\n1600 800 0 a\n", l);
  const std::vector<ge::classic::Loc> p = ge::classic::loc_places(l);
  CHECK(p.size() == 2);
  CHECK(p[0].name == "a" && p[1].name == "b");
  CHECK(p[0].at.x == 100 && p[0].at.y == 50 && p[0].at.z == 0);
  CHECK(p[1].at.x == 1000);
  CHECK(ge::classic::loc_places(ge::classic::Locs{}).empty());
}

TEST(places_are_added_removed_and_written_as_a_file_reads_them_back) {
  ge::classic::Locs locs;
  ge::classic::add_loc(locs, {100.06f, -50.0f, 24.0f}, "quad room");
  ge::classic::add_loc(locs, {-0.0625f, 512.0f, -3.1f}, "$loc_name_ra low");
  CHECK(locs.points.size() == 2);
  CHECK(locs.points[1].name == "ra low");
  // Eighths, halves away from zero, four places at least, the raw name kept, LF after each.
  CHECK_MSG(
      ge::classic::locs_text(locs) == " 800 -400  192 quad room\n  -1 4096  -25 $loc_name_ra low\n",
      ge::classic::locs_text(locs));
  ge::classic::Locs back;
  CHECK(ge::classic::parse_locs(ge::classic::locs_text(locs), back) == 2);
  CHECK(back.points[1].raw == "$loc_name_ra low" && back.points[1].name == "ra low");
  CHECK(ge::classic::locs_text(back) == ge::classic::locs_text(locs));
  // The nearest goes; then the other; then none.
  ge::classic::Loc gone;
  CHECK(ge::classic::remove_nearest_loc(locs, {90, -40, 20}, &gone) && gone.name == "quad room");
  CHECK(ge::classic::remove_nearest_loc(locs, {90, -40, 20}));
  CHECK(!ge::classic::remove_nearest_loc(locs, {0, 0, 0}));
  // A name over 63 characters is cut on reading.
  ge::classic::Locs cut;
  ge::classic::parse_locs("0 0 0 " + std::string(80, 'x') + "\n", cut);
  CHECK(cut.points[0].raw.size() == ge::classic::kLocNameMax);
}

TEST(a_recording_keeps_places_as_comment_lines_read_back_whole) {
  ge::classic::Locs l;
  ge::classic::add_loc(l, {100, 200, 24}, "quad");
  ge::classic::add_loc(l, {-50, 0, 24}, "$loc_name_ra room");
  const std::vector<std::string> lines = ge::classic::recorded_loc_lines(l);
  CHECK(lines.size() == 2);
  CHECK_MSG(lines[0] == "//loc  800 1600  192 quad\n", lines[0]);
  ge::classic::Locs back;
  for (const std::string& line : lines) CHECK(ge::classic::read_recorded_loc(line, back));
  CHECK(back.points.size() == 2 && back.points[1].name == "ra room");
  CHECK(!ge::classic::read_recorded_loc("fullserverinfo \"\\a\\b\"", back));
  CHECK(back.points.size() == 2);
}

TEST(a_point_out_of_sight_is_passed_over_for_the_next_nearest) {
  ge::classic::Locs l;
  ge::classic::parse_locs("80 0 0 behind the wall\n800 0 0 in the room\n",
                          l);  // 10 and 100 units off
  const ge::classic::LocSight wall = [](sh::Vec3f, sh::Vec3f to) { return to.x > 50; };
  int asked = 0;
  const ge::classic::LocSight counted = [&](sh::Vec3f, sh::Vec3f) {
    asked++;
    return true;
  };
  CHECK(ge::classic::loc_name_at(l, {0, 0, 0}) == "behind the wall");
  CHECK(ge::classic::loc_name_at(l, {0, 0, 0}, 0, wall) == "in the room");
  CHECK(ge::classic::nearest_loc(l, {0, 0, 0}, 50, wall) ==
        nullptr);  // the one in sight is too far
  CHECK(ge::classic::loc_name_at(l, {0, 0, 0}, 0, counted) == "behind the wall" && asked == 1);
}

TEST(pruning_keeps_only_the_points_a_spot_needs) {
  // A corridor 0..1000: "a" marked every 10 units up to 400, "b" from 600; a spot every 10 units.
  ge::classic::Locs locs;
  for (int x = 0; x <= 400; x += 10) ge::classic::add_loc(locs, {float(x), 0, 0}, "a");
  for (int x = 600; x <= 1000; x += 10) ge::classic::add_loc(locs, {float(x), 0, 0}, "b");
  std::vector<ge::shooter::Vec3f> spots;
  for (int x = 0; x <= 1000; x += 10) spots.push_back({float(x), 0, 0});
  std::vector<std::string> before;
  for (const auto& s : spots) before.push_back(ge::classic::loc_name_at(locs, s));
  const int gone = ge::shooter::prune_locs(locs, spots);
  CHECK_MSG(gone == 80 && locs.points.size() == 2, std::to_string(gone));
  CHECK(locs.points[0].name == "a" && locs.points[0].at.x == 400);  // the edge each side keeps
  CHECK(locs.points[1].name == "b" && locs.points[1].at.x == 600);
  for (size_t i = 0; i < spots.size(); ++i)
    CHECK(ge::classic::loc_name_at(locs, spots[i]) == before[i]);
  // With a reach of 150 the far ends need points of their own, or they would go unnamed.
  ge::classic::Locs reach;
  for (int x = 0; x <= 400; x += 10) ge::classic::add_loc(reach, {float(x), 0, 0}, "a");
  ge::shooter::prune_locs(reach, spots, 150);
  for (size_t i = 0; i < spots.size(); ++i)
    CHECK(ge::classic::loc_name_at(reach, spots[i], 150) == (spots[i].x <= 550 ? "a" : ""));
  CHECK_MSG(reach.points.size() >= 2 && reach.points.size() < 6,
            std::to_string(reach.points.size()));
}
