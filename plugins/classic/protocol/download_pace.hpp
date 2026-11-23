// A CLASSIC SERVER'S DOWNLOAD PACE: how big each piece of a file sent down the reliable stream
// is, and how long the server waits after a packet before the next, for every classic game's
// server. A piece goes when the client asks for the next (`nextdl`), one asked for at a time.
//
// The piece, by the game's own rule (each game keeps the size its players are used to):
// - `by_rate`: the client's rate over 25 (a piece a server frame at 25 a second), within 10 to
//   1024 bytes. QuakeWorld's server.
// - `fill_room`: the room the packet has, less the piece's own 4 bytes (its id, its size, its
//   percent): 1386 bytes on a 1390-byte packet. Quake II's server, for every protocol.
// Either way never more than the bytes left.
//
// The pause (`fill_room`): after a packet of `bytes`, nothing more for bytes / rate seconds in
// whole milliseconds (a 1398-byte packet: 93 ms at 15000, 23 ms at 60000); rate 0 none.
//
// ge:summary A classic server's download pieces: sized by rate or by the packet, and the pause
// ge:keywords download-pace download-piece nextdl piece-size fill-room rate-pause net_maxmsglen
// ge:skill classic net
// ge:test tests/classic_download_pace_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace ge::classic::protocol {

enum class DownloadPiece : uint8_t { by_rate, fill_room };

constexpr size_t kDownloadPieceHeader = 4;  // id, size (16 bits), percent

// The next piece's bytes: `rate` the client's bytes a second (0 or less: unlimited), `room` the
// packet's room for reliable data after what it already carries, `left` the file's bytes still
// to send.
size_t download_piece_bytes(DownloadPiece rule, int rate, size_t room, size_t left);

// The seconds to wait after a packet of `bytes` before the next at `rate` (0 or less: none).
double download_pause(size_t bytes, int rate);

}  // namespace ge::classic::protocol
