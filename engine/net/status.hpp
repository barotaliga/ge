// A NODE'S PUBLIC STATUS: what a game server, a broadcast (TV) node or a forwarder says about
// itself to anyone who asks, the same record for every game (docs/hub.md § The status record).
// A hub, a server browser or a player's CLI asks it of each address a master lists (master.hpp);
// the game fills its own words in (the mode, the score's meaning, the keys it wants seen).
//
// THE QUERY proves the asker's address before it is answered, so the status (up to a datagram)
// can never be aimed at a third party: the first ask carries no cookie and is answered with one,
// a keyed hash of the asker's address and a ten-second bucket; the ask again with that cookie is
// answered with the record. A cookie stays good for its bucket and the next. Every ask is padded
// to kStatusAskSize, and the cookie's answer is smaller than that.
//
//   ask     "geST" 1, protocol u32, nonce u64, cookie u64, padding to kStatusAskSize
//   cookie  "geST" 2, nonce u64, cookie u64
//   status  "geST" 3, nonce u64, then the record (status_write)
//
// THE RECORD is bounded by the datagram: what does not fit is left out from the end (the keys
// first, then the streams, the teams, and last the players at the end of their list, so a game
// lists its players before its spectators), and `truncated` says so. Strings are cut at their
// limits (kStatus*), on a byte; a game keeps its names short.
//
// ge:summary A node's public status record (game, map, mode, phase, players, teams, streams) and
// ge:summary its cookie-checked query
// ge:keywords net status server-status query cookie public-status record scoreboard players teams
// ge:keywords streams tv broadcast master hub server-browser amplification node-kind
// ge:skill net
// ge:test tests/net_status_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "net/address.hpp"

namespace ge::net {

constexpr size_t kStatusAskSize = 64;
constexpr size_t kStatusName = 63;    // a server's name, map and mode: bytes kept
constexpr size_t kStatusPlayer = 31;  // a player's name
constexpr size_t kStatusTeam = 15;    // a team's name
constexpr size_t kStatusKey = 31, kStatusValue = 63;
constexpr int kStatusMaxRows = 255;  // players, teams, streams or keys: each at most this many

// What kind of node answers: a game server, a broadcast node, a forwarder (docs/hub.md).
enum class NodeKind : uint8_t { game, tv, forward };
const char* node_kind_name(NodeKind k);
bool node_kind_of(std::string_view name, NodeKind& out);

// Where a match stands, in the server core's words (unknown: the game does not say).
enum class StatusPhase : uint8_t { unknown, lobby, countdown, playing, intermission };
const char* status_phase_name(StatusPhase p);

// StatusPlayer::flags.
constexpr uint8_t kPlayerBot = 1;
constexpr uint8_t kPlayerSpectator = 2;
constexpr uint8_t kPlayerViewer = 4;  // watching through a broadcast node, not on the server

struct StatusPlayer {
  std::string name, team;
  int32_t score = 0;     // in the game's own unit (frags, points, hundredths of a second)
  uint16_t ping_ms = 0;  // to the server, as the server measures it
  uint16_t minutes = 0;  // on the server
  uint8_t flags = 0;
  uint32_t colour = 0;  // 0xRRGGBB, the game's own choice of a player's colour
};

struct StatusTeam {
  std::string name;
  int32_t score = 0;
  uint32_t colour = 0;
};

// StatusStream::flags.
constexpr uint8_t kStreamLocked = 1;  // watching takes a password

// A broadcast of this server's match: the TV node that carries it and its stream there.
struct StatusStream {
  Address node;
  uint32_t stream = 0;
  uint16_t delay_s = 0;
  uint16_t viewers = 0;
  uint8_t flags = 0;
};

struct Status {
  NodeKind kind = NodeKind::game;
  uint32_t protocol = 0;  // the game's (packet.hpp's protocol_id)
  uint32_t version = 0;   // the game's build, as the game numbers it
  std::string name, map, mode;
  StatusPhase phase = StatusPhase::unknown;
  int32_t time_left_s = -1;  // -1: no limit, or the game does not say
  int max_players = 0, max_spectators = 0;
  char country[3] = {};  // ISO 3166-1 alpha-2 as the operator sets it ("": unsaid)
  bool locked = false;   // joining takes a password
  bool truncated = false;
  std::vector<StatusPlayer> players;  // players, spectators and viewers, flagged
  std::vector<StatusTeam> teams;
  std::vector<StatusStream> streams;
  std::vector<std::pair<std::string, std::string>> keys;  // anything else the game shows
};

// The record into `out` (at most `capacity` bytes, rows left out as the header says); its size.
size_t status_write(const Status& s, uint8_t* out, size_t capacity);
// The record back; false (and `s` partly filled) when the bytes are not a whole record.
bool status_read(const uint8_t* data, size_t size, Status& s);

// THE ASKER'S HALF. The ask's bytes (kStatusAskSize, cookie 0 the first time).
size_t status_ask(uint32_t protocol, uint64_t nonce, uint64_t cookie, uint8_t* out);
enum class StatusReply : uint8_t { none, cookie, status };
// A datagram read as an answer to the ask with `nonce`: a cookie to ask again with, or the record.
StatusReply status_read_reply(const uint8_t* data, size_t size, uint64_t nonce, uint64_t& cookie,
                              Status& s);

// THE ANSWERER'S HALF. Whether a datagram is a status ask (of any protocol).
bool is_status_ask(const uint8_t* data, size_t size);
// The answer to an ask from `from` (a cookie, or with a good cookie the record `s`) in `out`; its
// size, 0 when the datagram is not an ask of `protocol`. `secret` keys the cookies.
size_t status_answer(const uint8_t* data, size_t size, const Address& from, uint32_t protocol,
                     uint64_t secret, int64_t now_us, const Status& s, uint8_t* out,
                     size_t capacity);

}  // namespace ge::net
