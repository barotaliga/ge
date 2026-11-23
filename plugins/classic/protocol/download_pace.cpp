// A classic server's download pace (download_pace.hpp).
//
// ge:summary A classic server's download pieces: sized by rate or by the packet, and the pause
// ge:keywords download-pace download-piece nextdl
// ge:skill classic net
// ge:test tests/classic_download_pace_test.cpp
#include "classic/protocol/download_pace.hpp"

#include <algorithm>

namespace ge::classic::protocol {

size_t download_piece_bytes(DownloadPiece rule, int rate, size_t room, size_t left) {
  size_t n = 0;
  if (rule == DownloadPiece::by_rate) {
    const int r = rate > 0 ? rate : 25000;
    n = static_cast<size_t>(std::clamp(r / 25, 10, 1024));
  } else {
    n = room > kDownloadPieceHeader ? room - kDownloadPieceHeader : 0;
  }
  return std::min(n, left);
}

double download_pause(size_t bytes, int rate) {
  if (rate <= 0) return 0;
  const int64_t ms = static_cast<int64_t>(bytes) * 1000 / rate;  // whole milliseconds
  return static_cast<double>(ms) / 1000.0;
}

}  // namespace ge::classic::protocol
