// Tests: a classic server's download pace (classic/protocol/download_pace.hpp): QuakeWorld's
// piece by the rate kept as it was, Quake II's piece filling the packet, and the pause after one.
//
// ge:summary Test: download pieces by rate and by the packet's room, and the pause after each
// ge:keywords test download-pace download-piece fill-room rate-pause
// ge:skill classic net testing
#include "check.hpp"
#include "classic/protocol/download_pace.hpp"

namespace cp = ge::classic::protocol;

TEST(a_piece_by_rate_is_the_rate_over_25_within_10_and_1024) {
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::by_rate, 2500, 0, 5000) == 100);
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::by_rate, 30000, 0, 5000) == 1024);
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::by_rate, 100, 0, 5000) == 10);
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::by_rate, 0, 0, 5000) == 1000);
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::by_rate, 30000, 0, 300) == 300);
}

TEST(a_piece_filling_the_room_leaves_its_own_four_bytes) {
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::fill_room, 15000, 1390, 9000) == 1386);
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::fill_room, 15000, 4086, 9000) == 4082);
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::fill_room, 15000, 1390, 700) == 700);
  CHECK(cp::download_piece_bytes(cp::DownloadPiece::fill_room, 15000, 3, 700) == 0);
}

TEST(the_pause_after_a_packet_is_its_bytes_over_the_rate_in_whole_ms) {
  CHECK(cp::download_pause(1398, 15000) == 0.093);
  CHECK(cp::download_pause(1398, 60000) == 0.023);
  CHECK(cp::download_pause(1398, 5000) == 0.279);
  CHECK(cp::download_pause(1398, 0) == 0);
}
