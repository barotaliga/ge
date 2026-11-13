// The view book: its text read, the octants, the lookup by cell and height.
//
// ge:summary Tests the view book's parser, octants and lookup
// ge:keywords view-book viewbook test
// ge:skill testing bots
#include "shooter/view_book.hpp"

#include "check.hpp"

namespace sh = ge::shooter;

TEST(the_book_reads_its_cells_and_skips_what_does_not_parse) {
  sh::ViewBook b;
  const int n = sh::parse_view_book(
      "# people's e1m2\r\ncell 64 32\n1 2 3 0 -905 40 80\nbad line\n-1 -1 0 9 0 1 1\n", b);
  CHECK(n == 1);
  CHECK(b.cell_xy == 64);
  CHECK(b.cell_z == 32);
}

TEST(the_octants_are_centred_on_their_directions) {
  CHECK(sh::view_book_octant(0) == 0);
  CHECK(sh::view_book_octant(22) == 0);
  CHECK(sh::view_book_octant(23) == 1);
  CHECK(sh::view_book_octant(-22) == 0);
  CHECK(sh::view_book_octant(-23) == 7);
  CHECK(sh::view_book_octant(180) == 4);
  CHECK(sh::view_book_octant(-180) == 4);
}

TEST(a_lookup_finds_its_cell_the_height_beside_and_keeps_to_the_bars) {
  sh::ViewBook b;
  sh::parse_view_book("cell 96 64\n1 -1 2 2 -905 40 80\n", b);
  float yaw = 0;
  // x 100 -> cell 1, y -10 -> -1, z 130 -> 2; heading 90 -> octant 2.
  CHECK(sh::view_book_yaw(b, {100, -10, 130}, 90, 30, 50, yaw));
  CHECK(yaw == -90.5f);
  CHECK(sh::view_book_yaw(b, {100, -10, 200}, 90, 30, 50, yaw));  // the cell under
  CHECK(!sh::view_book_yaw(b, {100, -10, 130}, 0, 30, 50, yaw));  // another octant
  CHECK(!sh::view_book_yaw(b, {100, -10, 130}, 90, 41, 50, yaw));  // too few frames
  CHECK(!sh::view_book_yaw(b, {100, -10, 130}, 90, 30, 81, yaw));  // too loose
}
