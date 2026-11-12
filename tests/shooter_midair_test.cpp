// MIDAIR'S RULES: the least height by level and the setting's text, the bands by the rise and
// their frags, a hit that counts or not, the push's scale and lift on the ground, low and high,
// and the lines a player reads (a hit, the tally, the level).
//
// ge:summary Tests: midair's levels, bands, scores, push and lines
// ge:keywords test midair midair_band midair_push midair_line midair-tally k_midair_minheight
// ge:skill shooter testing
#include <string>

#include "check.hpp"
#include "shooter/midair.hpp"

namespace sh = ge::shooter;

TEST(levels_name_the_least_height) {
  CHECK(sh::midair_min_height(0) == 64);
  CHECK(sh::midair_min_height(1) == 128);
  CHECK(sh::midair_min_height(4) == 1024);
  CHECK(sh::midair_min_height(9) == 64);
  CHECK(std::string(sh::midair_level_name(2)) == "silver");
  CHECK(sh::midair_level("3") == 3);
  CHECK(sh::midair_level("") == sh::kMidairDefaultLevel);
  CHECK(sh::midair_level("7") == sh::kMidairDefaultLevel);
}

TEST(bands_go_by_the_rise) {
  CHECK(sh::midair_band(-300) == sh::MidairBand::bronze);
  CHECK(sh::midair_band(256) == sh::MidairBand::bronze);
  CHECK(sh::midair_band(256.5f) == sh::MidairBand::silver);
  CHECK(sh::midair_band(600) == sh::MidairBand::gold);
  CHECK(sh::midair_band(1025) == sh::MidairBand::platinum);
  CHECK(sh::midair_points(sh::MidairBand::bronze) == 1);
  CHECK(sh::midair_points(sh::MidairBand::platinum) == 8);
}

TEST(a_hit_counts_from_the_least_height) {
  CHECK(sh::midair_score(127, 600, 1) == 0);
  CHECK(sh::midair_score(128, 600, 1) == 4);
  CHECK(sh::midair_score(70, -50, 0) == 1);
  CHECK(sh::midair_height(100, -24, 10) == 66);
}

TEST(the_push_lifts_the_low_and_grows_with_height) {
  // On the floor, a direct hit: the push as it is, and 4/8 of it straight up besides.
  sh::MidairPush p = sh::midair_push(0, 110, false);
  CHECK(p.scale == 1 && p.lift == 0.5f);
  // A weak splash on another player: 11/8, and 6 of the damage up (6/11 of the push).
  p = sh::midair_push(0, 40, false);
  CHECK(p.scale == 11.0f / 8.0f);
  CHECK(p.lift * p.scale * 8 == 6);
  // His own weak blast is not boosted.
  p = sh::midair_push(0, 40, true);
  CHECK(p.scale == 1 && p.lift == 0.5f);
  // In the air: double at 109, triple at 173, no lift.
  CHECK(sh::midair_push(109, 110, false).scale == 2);
  CHECK(sh::midair_push(173, 110, true).scale == 3);
  CHECK(sh::midair_push(173, 110, true).lift == 0);
}

TEST(the_lines_read_like_a_game) {
  CHECK(sh::midair_line(sh::MidairBand::gold, 612.34f, "Alice", "Bob") ==
        "Gold midair by Alice on Bob (612.3 up)\n");
  sh::MidairTally t;
  CHECK(sh::midair_tally_line("Alice", t).empty());
  sh::midair_note(t, 100);
  sh::midair_note(t, 300);
  sh::midair_note(t, -20);
  CHECK(t.total() == 3 && t.hits[0] == 2 && t.hits[1] == 1);
  CHECK(t.highest == 300);
  CHECK(sh::midair_tally_line("Alice", t) ==
        "Alice: 3 midairs (bronze 2, silver 1, gold 0, platinum 0), highest 300.0, average "
        "126.7\n");
  CHECK(sh::midair_level_line(2) == "Midair hits now count from 256 units up (silver)\n");
}
