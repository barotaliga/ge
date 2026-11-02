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
