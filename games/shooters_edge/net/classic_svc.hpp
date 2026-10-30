// THE CLASSIC PROTOCOL'S MESSAGES, server to client (svc) and client to server (clc), stated once:
// every id, its name, its body as a layout, and whether it is always sent reliably. An in-band
// payload is a run of messages, each a byte id and a body; a reader that meets an unknown id
// cannot resynchronise and the connection is dropped.
//
// THE LAYOUTS (`SvcDef::layout`): one character a field, read and written in order: `b` byte, `c`
// char, `h` short, `l` long, `f` float, `C` coord, `O` an origin coord (a float under the demo
// family's float coords), `A` angle, `s` string. A layout of `*` is a message with its own code:
// sound (6, and 55 under sound-double: flags, then the fields they name), setangle (10: a kind
// byte under the teleport bit, then three angles), serverdata (11: the agreed families' magic
// and bits, then the version, the spawn count, the game directory, the player number, the level's
// name and ten movement floats), spawnstatic2 and spawnbaseline2 (21, 66: an entity record from
// the null state, 66 after a short number), temp_entity (23: a kind byte, then the kind's own
// layout), download (41), playerinfo (42, classic_delta.hpp), nails (43: a count byte, then 6
// bytes a nail), the lists (45, 46, 56, 60: the start index, the names, an empty name, the next
// start's low byte), packetentities (47) and deltapacketentities (48: a from-sequence byte).
//
// DOWNLOADS (41): the classic transfer is a short block size (-1: refused, then a byte 0), a
// percent byte and the block. Under chunked downloads, a long first: -1 starts a transfer (a long
// total size, -1 not found, -2 refused, -3 cancelled, and the name), a chunk number 0 or more is
// a chunk of kChunkBytes (the last padded with zeros).
//
// `SvcMsg` is a message decoded field by field (the tests' round trips, the scripted client's
// reader, a capture's decode); the server writes its hot messages straight onto the wire with
// the writers beside it.
//
// ge:summary The classic messages: svc and clc ids, layouts, temp entities, decode and describe
// ge:keywords svc clc message-ids layout serverdata soundlist modellist download temp-entity
// ge:keywords sound setangle packetentities playerinfo stringcmd move delta tmove upload nails
// ge:keywords read_svc write_svc read_clc write_clc describe_svc describe_clc svc_def
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_messages_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/net/classic_delta.hpp"

namespace shooters_edge::net {

namespace svc {
constexpr uint8_t bad = 0, nop = 1, disconnect = 2, updatestat = 3, sound = 6, print = 8;
constexpr uint8_t stufftext = 9, setangle = 10, serverdata = 11, lightstyle = 12;
constexpr uint8_t updatefrags = 14, stopsound = 16, damage = 19, spawnstatic = 20;
constexpr uint8_t spawnstatic2 = 21, spawnbaseline = 22, temp_entity = 23, setpause = 24;
constexpr uint8_t centerprint = 26, killedmonster = 27, foundsecret = 28;
constexpr uint8_t spawnstaticsound = 29, intermission = 30, finale = 31, cdtrack = 32;
constexpr uint8_t sellscreen = 33, smallkick = 34, bigkick = 35, updateping = 36;
constexpr uint8_t updateentertime = 37, updatestatlong = 38, muzzleflash = 39;
constexpr uint8_t updateuserinfo = 40, download = 41, playerinfo = 42, nails = 43;
constexpr uint8_t chokecount = 44, modellist = 45, soundlist = 46, packetentities = 47;
constexpr uint8_t deltapacketentities = 48, maxspeed = 49, entgravity = 50, setinfo = 51;
constexpr uint8_t serverinfo = 52, updatepl = 53, soundextended = 55, soundlistshort = 56;
constexpr uint8_t modellistshort = 60, spawnbaseline2 = 66, updatestatstring = 78;
constexpr uint8_t updatestatfloat = 79, spawnstaticsound2 = 94;
}  // namespace svc

namespace clc {
constexpr uint8_t nop = 1, move = 3, stringcmd = 4, delta = 5, tmove = 6, upload = 7;
}  // namespace clc

// Temporary entity kinds (svc 23).
namespace te {
constexpr uint8_t spike = 0, superspike = 1, gunshot = 2, explosion = 3, tarexplosion = 4;
constexpr uint8_t lightning1 = 5, lightning2 = 6, wizspike = 7, knightspike = 8;
constexpr uint8_t lightning3 = 9, lavasplash = 10, teleport = 11, blood = 12;
constexpr uint8_t lightningblood = 13;
}  // namespace te

constexpr int kChunkBytes = 1024;
constexpr int kStats = 32;

struct SvcDef {
  uint8_t id;
  const char* name;
  const char* layout;
  bool reliable;  // always sent on the reliable stream
};
// The definition of an svc id; nullptr for an id the protocol has not.
const SvcDef* svc_def(uint8_t id);
const SvcDef* svc_defs(int& count);
// A temporary entity kind's layout after its kind byte; nullptr when unknown.
const char* temp_entity_layout(uint8_t kind);

// What the decoder needs to know about the connection.
struct WireCtx {
  Ext ext;
  bool chunked = false;  // downloads use the chunked form (the x family's chunked bit)
};

struct SvcMsg {
  uint8_t id = 0;
  std::vector<int32_t> ints;                       // the layout's integer fields, in order
  std::vector<float> floats;                       // its float, coord and angle fields, in order
  std::vector<std::string> strings;                // its strings, in order
  std::vector<uint8_t> data;                       // a download block, nails
  std::vector<std::pair<uint32_t, uint32_t>> ext;  // serverdata's (magic, bits) pairs
  std::vector<EntityRecord> ents;                  // packet entities (read against the null state)
  PlayerState player;
  uint32_t player_flags = 0;
};

// One message (the id and its body). False when the message cannot be written (a bad id).
bool write_svc(gn::Writer& w, const SvcMsg& m, const WireCtx& ctx);
// One message; false (and `error`) on an unknown id or a body past the end.
bool read_svc(gn::Reader& r, SvcMsg& m, const WireCtx& ctx, std::string& error);
// One line of text for a message: its name and fields.
std::string describe_svc(const SvcMsg& m);

struct ClcMsg {
  uint8_t id = 0;
  uint8_t checksum = 0, loss = 0;    // a move's
  sh::MoveCmd cmds[3];               // a move's, oldest first
  size_t body_at = 0, body_end = 0;  // a move's checksummed bytes in the reader's buffer
  std::string text;                  // a string command
  int delta = 0;                     // a delta's sequence byte
  sh::Vec3f origin;                  // a tmove's
  std::vector<uint8_t> data;         // an upload's block
  int size = 0, percent = 0;         // an upload's
};

bool write_clc(gn::Writer& w, const ClcMsg& m);
bool read_clc(gn::Reader& r, ClcMsg& m, std::string& error);
std::string describe_clc(const ClcMsg& m);
const char* clc_name(uint8_t id);

// The move's body (loss byte and three commands), written onto `w` after the checksum byte: the
// helper a client uses so the checksum covers exactly what it sent.
void write_move(gn::Writer& w, const sh::MoveCmd cmds[3], uint8_t loss, uint32_t sequence);

}  // namespace shooters_edge::net
