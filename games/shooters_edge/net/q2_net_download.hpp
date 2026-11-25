// QUAKE II'S SERVER DOWNLOADS (q2_net_server.hpp § DOWNLOADS): a file asked for with `download
// <path> [offset]` checked and read, then sent a piece for each `nextdl`, down the reliable stream.
//
// THE PACE (classic/protocol/download_pace.hpp, `fill_room`): one piece owed at a time: the
// `download` owes the first, each `nextdl` one more (a second `nextdl` before it goes owes no
// second). It goes once nothing reliable is waiting or in flight and the pause after the last
// one is over: it fills the packet's room less its 4 bytes (1386 on a 1390-byte packet, more on a
// longer one a protocol 35 or 36 client asked for), the last one the file's rest; after it the
// client is sent nothing more for the packet's bytes over its rate (in whole ms; none on this
// machine). Every protocol alike. A resume at the file's very end gets one empty piece at 100.
//
// ge:summary Quake II server downloads: the file checked and read, pieces filling the packet
// ge:keywords q2-download download nextdl svc-download svc-zdownload piece-size download-pace
// ge:keywords sv_max_download_size allow_download_maps file-size-differs plain-download-path
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_download_pace_test.cpp
#pragma once

#include <string>

#include "shooters_edge/net/q2_net_server.hpp"

namespace shooters_edge::net {

// A download's name made plain: `\` as `/`, lower case, leading slashes and `.` parts dropped,
// `..` taking the part before it away; "" when it climbs out of the game's folders.
std::string q2_plain_download_path(std::string path);

// Whether the download switches let a client have the plain path `path` (the game's channel and its
// web files alike): sv_download on, a file in a folder whose ends are a letter, digit, `_` or `-`,
// its kind (its first folder) switched on.
bool q2_download_allowed_path(const Q2NetConfig& cfg, const std::string& path);

// `download <asked> [offset]`: the file read and its first piece owed, or the refusal queued.
// Answers the server log's line for a refusal ("" when it is sent).
std::string q2_start_download(Q2NetServer& s, Q2NetClient& c, const std::string& asked, int offset);

// The owed piece queued when it may go now (THE PACE); true when it was.
bool q2_download_piece(Q2NetServer& s, Q2NetClient& c);

}  // namespace shooters_edge::net
