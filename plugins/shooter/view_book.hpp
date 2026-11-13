// A MAP'S VIEW BOOK: where people look at each spot of a map, by the way they move there, read off
// their recordings, so a bot's calm view can look where a person would (into the openings people
// watch, round the corner they are about to take) and not only along its route.
//
// THE KEY: a cell `cell_xy` units across in the plan and `cell_z` units high (the player's origin
// floored to it), and the octant of the run's heading (0 is +x, counter-clockwise, 45 degrees
// each, centred on its direction). THE ENTRY: the circular mean of the view's yaw there, in tenths
// of a degree, how many frames it was read off, and how consistent they were (the mean resultant
// length in hundredths: 100 all alike, 0 any way at all).
//
// THE FILE FORMAT (text, a line each, `#` lines and blank lines skipped, LF or CR LF):
//   cell <xy> <z>
//   <cx> <cy> <cz> <octant> <yaw tenths> <frames> <consistency hundredths>
// every number a whole one, so a book reads to the same bits everywhere.
//
// Parsing allocates; `view_book_yaw` does not (a binary search over the sorted keys).
//
// ge:summary A map's view book: people's view yaw by spot and heading octant, read and looked up
// ge:keywords view-book viewbook facing calm-view where-people-look view-atlas parse_view_book
// ge:keywords view_book_yaw people-recordings
// ge:skill shooter bots
// ge:test tests/shooter_view_book_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "shooter/vec.hpp"

namespace ge::shooter {

struct ViewBookEntry {
  int64_t key = 0;
  int32_t yaw_tenths = 0;
  int32_t frames = 0;
  int32_t consistency = 0;  // hundredths
};

struct ViewBook {
  int32_t cell_xy = 96, cell_z = 64;
  std::vector<ViewBookEntry> entries;  // sorted by key
  std::string from;                    // the file it was read from
};

// Reads `text` into `out` (its entries replaced); the number of entries read. A line that does not
// parse is skipped; with no `cell` line the default cells stand.
int parse_view_book(std::string_view text, ViewBook& out);

// The octant of a heading in degrees (0 is +x, counter-clockwise).
int view_book_octant(float heading_deg);

// The book's yaw (degrees) at `origin` moving toward `heading_deg`, when the cell holds an entry
// read off at least `min_frames` frames at least `min_consistency` hundredths consistent (the
// cell's own height first, then the one under and over it). False when none does.
bool view_book_yaw(const ViewBook& book, Vec3f origin, float heading_deg, int32_t min_frames,
                   int32_t min_consistency, float& yaw_deg);

}  // namespace ge::shooter
