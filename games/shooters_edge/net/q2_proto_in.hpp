// QUAKE II'S LATER PROTOCOLS READ AS PROTOCOL 34 (the client's side of protocols 35 and 36;
// porting/shooters-edge/demo-formats-q2.md § A holds the facts). A joined client that speaks 35
// or 36 hands each payload the server sent to `q2_proto_in`, which writes the same messages in
// protocol 34's form, cut into blocks of a recording: so the recording's reader
// (client/demo_q2.cpp) stays the one reader of every Quake II stream, and a client's recording of a
// later protocol's server is a protocol 34 `.dm2` any client plays, as the later clients themselves
// write theirs.
//
// What it rewrites:
// - The server data: protocol 34, the tail (35's minor version and strafe-jump hack; 36's minor
//   version, server state and movement flags) taken off and kept here.
// - The frame: the long whose low 27 bits are the frame number and top 5 bits how many frames
//   back its delta is (31: none); the suppress byte (35's count of held packets, 36's flags, its
//   high nibble four of the extra player flags with the command byte's top three bits the other
//   three); the player state whose origin and velocity z, roll, gun offset and angles and stats
//   come apart under the extra flags, and 36's client number; the entity list with no id of its
//   own. The player state is kept for each of the last 32 frames, so the 34 form's grouped fields
//   (the whole origin, the gun's frame with its offset and angles, every stat's mask) are written
//   whole.
// - The entities and baselines: the solid a long (35 from minor 1905, every 36), packed back to
//   the 16-bit box; 36's 16-bit angles (header bit 13, minor 1018 on) as bytes; skin, effects and
//   render effects read whole and sized again by value.
// - The compressed packet (21): raw deflate, inflated and read again (never one inside another).
// - The compressed download (22): 35's pieces each inflated alone and handed on as plain pieces;
//   36's pieces one stream, gathered while each piece is handed on empty with its percent (so the
//   client asks for the next), inflated whole at 100 % into `download`.
// - 36's gamestate (23) and its configstring and baseline streams (25, 26): plain configstrings
//   and baselines. The setting (24): the server's frame rate kept, nothing handed on.
// - Every other message is protocol 34's already and is copied as it is (its end found by the
//   recording's own reader); the game's own messages (ids 117 to 127) too.
//
// ge:summary Quake II's protocols 35 and 36 rewritten as protocol 34 for the one reader
// ge:keywords protocol-35 protocol-36 zpacket zdownload gamestate configstring-stream
// ge:keywords baseline-stream svc-setting long-solid angle16 extra-player-flags client-number
// ge:keywords frame-flags strafe-jump-hack qw-movement waterjump-hack q2_proto_in downgrade
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_proto_in_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace shooters_edge::net {

struct Q2ProtoIn {
  int protocol = 34;             // the server data's (set by it)
  bool has_server_data = false;  // a server data read: `protocol` is the server's
  int minor = 0;
  // The server data's tail: the movement hacks (prediction's), the server's state (36: 0 dead, 1
  // loading, 2 a game, 3 a picture, 4 a relay, 5 a cinematic).
  bool strafe_hack = false, qw_move = false, waterjump_hack = false;
  int server_state = 2;
  int client_number = -1;  // 36's: the player whose view the frames are (-1: never said)
  int server_fps = 10;     // the setting message's (24, index 1)
  // A compressed download's bytes once whole (36's stream), for the piece that said 100 %.
  std::vector<uint8_t> download;
  bool download_ready = false;
  // Lines for the player the reading raised (a server's version taken as another), taken by the
  // caller.
  std::vector<std::string> warnings;

  // Inside.
  std::vector<uint8_t> zdl;  // 36's compressed download so far
  server::Q2PlayerState views[32];
  int32_t view_frame[32] = {};
  client::Demo scratch;  // the recording's reader, to find each copied message's end
  bool started = false;
};

// One payload (`in`) into protocol 34 blocks appended to `blocks` (each at most 1400 bytes unless
// one message alone is longer). False with the reason in `error` (the connection is then
// dropped: the server sent what neither protocol has).
bool q2_proto_in(Q2ProtoIn& p, std::span<const uint8_t> in,
                 std::vector<std::vector<uint8_t>>& blocks, std::string& error);

// The 16-bit box a long solid (35 from 1905, 36) packs: 31 a brush model, 0 none.
int q2_short_solid(uint32_t long_solid);

}  // namespace shooters_edge::net
