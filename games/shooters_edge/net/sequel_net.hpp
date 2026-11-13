// THE SEQUEL'S LIVE PROTOCOL (34), WHAT BOTH ENDS SHARE (porting/shooters-edge/sequel-net.md holds
// the facts): the client's messages, the move and its checksum, and the out-of-band words.
//
// THE CONNECTION is the classic channel (classic_channel.hpp): the same header (a client's with its
// qport), one reliable payload in flight and its even/odd bit, stale packets dropped. A server's
// payload is read as the sequel's recording reads a block (client/demo.hpp, `next_sequel`).
//
// OUT OF BAND: `getchallenge` gets `challenge <n> p=34`; `connect 34 <qport> <challenge>
// "<userinfo>"` gets `client_connect`; a refusal is `print` and a line. `info 34`, `status` and
// `ping` are answered too.
//
// THE SIGNON, by the client's string commands: `new` gets the server data and a stuffed
// `cmd configstrings <count> 0`; each `configstrings <count> <i>` gets the configstrings from i and
// the next prompt, then `cmd baselines <count> 0`; the baselines likewise, then `precache <count>`;
// `begin <count>` puts the client in the game. The count is the server's for this level.
//
// THE MOVE (clc 2): the checksum byte, the newest frame the client read (-1 or 0: send it whole),
// then three commands, oldest first, each a delta from the one before (the first from zero): a bits
// byte (pitch 0x01, yaw 0x02, roll 0x04 as shorts in 65536ths of a turn; forward 0x08, side 0x10,
// up 0x20 as shorts; buttons 0x40, impulse 0x80 as bytes), then msec and the light level always.
// The checksum covers the bytes after it to the third command's end (at most 60), with four bytes
// of the key table (sequel_checksum_table.cpp, measured) at `sequence mod 1020` appended: their
// CRC-16 (polynomial 0x1021, initial 0xFFFF, no reflection, no final XOR) XORed with the plain sum
// of the same bytes, the low byte kept. A server ignores a move whose checksum is wrong, and the
// rest of its packet, but takes the frame it acknowledges.
//
// ge:summary The sequel's live protocol shared by both ends: client messages, moves, checksums
// ge:keywords sequel protocol-34 live clc move usercmd delta checksum challenge connect
// ge:keywords client_connect signon configstrings baselines precache begin out-of-band
// ge:keywords sequel_move_checksum write_sequel_move read_sequel_move sequel_words
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "net/wire.hpp"
#include "shooter/pmove.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;
namespace sh = ge::shooter;

constexpr int kSequelNetProtocol = 34;
constexpr uint16_t kPortSequel = 27910;
constexpr size_t kSequelPacket = 1400;    // the largest datagram, its header in
constexpr size_t kSequelReliable = 1384;  // the largest reliable payload
constexpr double kSequelFrame = 0.1;      // s between server frames

// The client's message ids.
constexpr uint8_t kClcNop = 1, kClcMove = 2, kClcUserInfo = 3, kClcStringCmd = 4;

// A command's bits.
constexpr uint8_t kSqCmPitch = 0x01, kSqCmYaw = 0x02, kSqCmRoll = 0x04, kSqCmForward = 0x08,
                  kSqCmSide = 0x10, kSqCmUp = 0x20, kSqCmButtons = 0x40, kSqCmImpulse = 0x80;

// A command as the wire holds it.
struct SequelCmd {
  int16_t angles[3] = {};  // 65536ths of a turn
  int16_t forward = 0, side = 0, up = 0;
  uint8_t buttons = 0, impulse = 0, msec = 0, light = 0;
  bool operator==(const SequelCmd&) const = default;
};

SequelCmd sequel_cmd(const sh::MoveCmd& c);
sh::MoveCmd move_cmd(const SequelCmd& c);

void write_sequel_cmd(gn::Writer& w, const SequelCmd& from, const SequelCmd& to);
SequelCmd read_sequel_cmd(gn::Reader& r, const SequelCmd& from);

// The key table (sequel_checksum_table.cpp): a wire constant measured for interop.
constexpr int kSequelChecksumTableSize = 1024;
extern const uint8_t kSequelChecksumTable[kSequelChecksumTableSize];
// The checksum byte over `body` (the bytes after the checksum byte) for packet `sequence`.
uint8_t sequel_move_checksum(const uint8_t* body, size_t size, uint32_t sequence);

// A move message whole (its id, checksum, frame and commands) for packet `sequence`.
void write_sequel_move(gn::Writer& w, int32_t frame, const SequelCmd cmds[3], uint32_t sequence);
// A move message after its id: false when it runs past the end. `checksum_ok` says whether its
// byte is the one the key table gives for `sequence`.
struct SequelMove {
  int32_t frame = -1;
  SequelCmd cmds[3];
  bool checksum_ok = false;
};
bool read_sequel_move(gn::Reader& r, uint32_t sequence, SequelMove& out);

// An out-of-band line's words: split on spaces, a double-quoted word kept whole (its quotes off).
std::vector<std::string> sequel_words(const std::string& line);
// A datagram's out-of-band text (after the four 0xFF bytes, to a zero byte or the end).
std::string sequel_oob_text(const uint8_t* data, size_t size);
// An out-of-band datagram of `text`.
std::vector<uint8_t> sequel_oob(const std::string& text);

}  // namespace shooters_edge::net
