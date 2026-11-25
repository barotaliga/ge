// QUAKE II'S LATER PROTOCOLS (35 and 36) on the live connection, what both ends share
// (porting/shooters-edge/q2-proto36-server.md holds the facts): the connect line's tail, the
// client's later messages (the move without its checksum and its compact form, the batched moves,
// the user info change, the client setting) and the frame's header form.
//
// THE CONNECT: `connect <protocol> <qport> <challenge> "<userinfo>"` and then, for 35, the largest
// message (absent 1390; 0 means 4086; below 0 or past 4086 refused "Invalid maximum message
// length.") and the minor version (absent 1903, held to 1903..1905); for 36 the largest message,
// the channel (absent 1, the newer one; only 0 or 1, else "Invalid netchan type."), compression
// (absent yes; 0 no) and the minor version (absent 1015, held to 1015..1026, 1016 read as 1015).
// The qport of 35 and 36 is a byte on the wire, none when it is 0.
//
// THE MOVE (clc 2) of 35 and 36 has no checksum byte. 35 from minor 1904 writes each command
// compactly: the bits byte, then the buttons byte (attack, use and any in bits 0, 1 and 7; bits 2,
// 3, 4 say forward, side and up are a signed byte times 5, bit 5 the pitch a signed byte times 64,
// bit 6 the yaw times 256), the angles and moves present (a short where no bit says a byte), the
// impulse, the msec and the light level.
//
// THE BATCHED MOVES (36: clc 10, the frame left out, asking for a whole one, and clc 11 with it):
// the id's top 3 bits the frames carried beyond the newest (at most 3, else "too many frames in
// packet"); clc 11's newest frame read (a long); the light level; then a bit stream, least
// significant bit first: per frame 5 bits of command count, each command 1 bit (0: the same as the
// one before) and then 8 bits of flags (the move's, bit 7 meaning the msec follows), pitch and yaw
// each a bit (1: 8 signed bits added to the one before; 0: 16 signed bits), roll 16, forward, side
// and up 10 signed bits each, buttons 3 bits (attack, use, any), msec 8; the impulse is not
// carried.
//
// THE USER INFO CHANGE (36: clc 12): a key and a value, again while the next byte is 12. THE
// CLIENT SETTING (35 and 36: clc 5): a short index and a short value.
//
// ge:summary Quake II's protocols 35 and 36: the connect's tail, the later moves and messages
// ge:keywords protocol-35 protocol-36 connect-tail maxmsglen netchan-type zlib minor-version
// ge:keywords clc-move-batched clc-move-nodelta clc-userinfo-delta clc-setting compact-move
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_ext_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/net/q2_net.hpp"

namespace shooters_edge::net {

constexpr int kQ2Protocol35 = 35, kQ2Protocol36 = 36;
constexpr int kQ2MinorFirst35 = 1903, kQ2MinorLast35 = 1905;
constexpr int kQ2MinorFirst36 = 1015, kQ2MinorLast36 = 1026;
constexpr int kQ2MsgLenDefault = 1390, kQ2MsgLenMost = 4086, kQ2MsgLenLeast = 512;

// The client's later message ids.
constexpr uint8_t kClcSetting = 5, kClcMoveNoDelta = 10, kClcMoveBatched = 11,
                  kClcUserInfoDelta = 12;
// The client settings' indexes (clc 5).
constexpr int kQ2SetNoGun = 0, kQ2SetNoBlend = 1, kQ2SetRecording = 2, kQ2SetPlayerUpdates = 3,
              kQ2SetFps = 4, kQ2SetNoGibs = 10, kQ2SetNoFootsteps = 11, kQ2SetNoPredict = 12,
              kQ2SetNoFlares = 13, kQ2Settings = 14;

// What a connect line asked past its user info.
struct Q2ConnectAsk {
  int protocol = 34;
  int msglen = kQ2MsgLenDefault;  // asked (0 already read as kQ2MsgLenMost)
  int chan = 0;                   // 1 the newer channel
  bool zlib = false;
  int minor = 0;
};
// The tail of a connect line's words (`w[0]` is `connect`); false with the refusal's text.
bool q2_connect_ask(const std::vector<std::string>& w, Q2ConnectAsk& out, std::string& refusal);
// AN EXTENDED SERVER (its game asked for the wider limits: `ext` 1, or 2 with the newer interface)
// takes protocol 36 alone, at minor 1024 or later (1025 for 2); any other client is refused with
// these words. "" when the client may join (always, on a server that is not extended).
constexpr int kQ2MinorExtended = 1024, kQ2MinorExtended2 = 1025;
std::string q2_extended_refusal(int ext, const Q2ConnectAsk& ask);
// The `protocol` key a status answer gives: 36 on an extended server, else 34.
int q2_status_protocol(int ext);
// The packet length a client gets: from 1390, the asked one for 35 and 36, held to
// `net_maxmsglen` when above 0 (not for a client on this machine), never under 512.
int q2_msglen(const Q2ConnectAsk& ask, int net_maxmsglen, bool local);

// A move of 35 or 36 (clc 2, after its id): no checksum; 35's compact form from minor 1904.
bool read_q2_move_ext(gn::Reader& r, int protocol, int minor, Q2Move& out);
void write_q2_move_ext(gn::Writer& w, int protocol, int minor, int32_t frame, const Q2Cmd cmds[3]);

// The batched moves (clc 10 and 11): the frames oldest first, each its commands in order.
struct Q2Batch {
  int32_t frame = -1;  // the newest frame read; -1 asks for a whole one (clc 10)
  uint8_t light = 0;
  std::vector<std::vector<Q2Cmd>> frames;
};
// After the id byte `id` (its top bits read here); false with the drop's reason.
bool read_q2_batch(gn::Reader& r, uint8_t id, Q2Batch& out, std::string& reason);
// The message whole, id first; `prev` the command before the first (zero at the start).
void write_q2_batch(gn::Writer& w, const Q2Batch& b);

}  // namespace shooters_edge::net
