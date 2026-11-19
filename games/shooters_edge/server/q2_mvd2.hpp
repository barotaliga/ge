// QUAKE II'S MULTIVIEW RECORDING (`.mvd2`), WRITTEN: a server's recording of every player of a
// match, from whole frames. The facts are in porting/shooters-edge/demo-formats-q2.md § 5;
// the reader and the one-player view are client/demo_mvd2.hpp.
//
// THE FILE: the magic `MVD2`, then blocks of a 16-bit length and that many bytes, a length of 0
// at the end. A level's first block is its game state: the server data (major 37, the minor
// version, the stream's flags, the server count, the game folder, the observer's client number),
// every settings string that is set (its index and text, ended by the index 2080), then a whole
// frame. After it, one block a 10 Hz frame: the settings strings that changed, the prints to
// everyone and the messages to one player (protocol 34's own bytes) first, then the frame (the
// area portals, the players as deltas ended by 255, the entities as protocol 34's delta list ended
// by two zero bytes), then the effects to everyone and the sounds (an entity and channel, no
// point).
//
// A PLAYER (`Mvd2Player`) is a client number, a short of flags and the fields they name: movement
// type, origin, view offset, view angles, roll, kick, the gun (index, frame, offset, angles), the
// blend, field of view, render flags, stats (a mask and the changed ones). A player who comes into
// the game is sent whole; one who leaves is his number and the flag 15 alone. Velocity, the move's
// timers and the delta angles are never sent.
//
// A PLAYER'S ENTITY whose player walks (movement type 0) goes without its origin and angles: a
// reader puts it where his state is (`mvd2_place_from_player`), and the writer deltas the next
// frame from that place.
//
// ge:summary Quake II's multiview recording (.mvd2) written from whole frames
// ge:keywords mvd2 multiview q2 protocol-37 gamestate mvd2_write is_mvd2 player-delta
// ge:keywords unicast multicast portal-bits mvd2_place_from_player
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_mvd2_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/server/q2_wire.hpp"

namespace shooters_edge::server {

inline constexpr uint8_t kMvd2Magic[4] = {'M', 'V', 'D', '2'};
inline constexpr int kMvd2Major = 37;
inline constexpr int kMvd2Minor = 2010;  // the ordinary limits' minor version, as written
inline constexpr int kMvd2MostClients = 256;
inline constexpr size_t kMvd2MostCast = 2047;  // a unicast's or a multicast's payload, bytes

// The stream's message ids (the low 5 bits of the command byte; the top 3 carry a length's
// high bits or the stream's flags).
enum : uint8_t {
  kMv2Nop = 1,
  kMv2ServerData = 4,
  kMv2ConfigString = 5,
  kMv2Frame = 6,
  kMv2Unicast = 8,
  kMv2UnicastReliable = 9,
  kMv2MulticastAll = 10,
  kMv2MulticastHear = 11,
  kMv2MulticastSee = 12,
  kMv2MulticastAllReliable = 13,
  kMv2MulticastHearReliable = 14,
  kMv2MulticastSeeReliable = 15,
  kMv2Sound = 16,
  kMv2Print = 17,
};

// A player entry's flag bits.
enum : uint16_t {
  kMv2Type = 1u << 0,
  kMv2OriginXY = 1u << 1,
  kMv2OriginZ = 1u << 2,
  kMv2ViewOffset = 1u << 3,
  kMv2ViewAngles = 1u << 4,
  kMv2ViewRoll = 1u << 5,
  kMv2Kick = 1u << 6,
  kMv2Blend = 1u << 7,
  kMv2Fov = 1u << 8,
  kMv2GunIndex = 1u << 9,
  kMv2GunFrame = 1u << 10,
  kMv2GunOffset = 1u << 11,
  kMv2GunAngles = 1u << 12,
  kMv2RdFlags = 1u << 13,
  kMv2Stats = 1u << 14,
  kMv2Removed = 1u << 15,  // on the ordinary limits: the player left the game
  kMv2All = (1u << 15) - 1,
};

// Whether `bytes` open as a multiview recording of Quake II (its magic).
bool is_mvd2(std::span<const uint8_t> bytes);

// A sound started on an entity (the recording keeps no point: it is the entity's).
struct Mvd2Sound {
  int index = 0;         // the sound's number in the sounds' settings strings
  int volume = 255;      // 0 to 255
  int attenuation = 64;  // 64ths
  int offset = 0;        // its delay in milliseconds
  int entity = 0, channel = 0;
};

struct Mvd2Player {
  bool in = false;  // in the game this frame
  Q2PlayerState s;
};

// One frame whole, as the writer takes it and the reader keeps it.
struct Mvd2Frame {
  std::vector<std::pair<int, std::string>> config_strings;  // set this frame (index, text)
  std::vector<std::pair<int, std::string>> prints;          // to everyone (level, text)
  // Protocol 34 messages to one player (his client number, the bytes) and to everyone.
  std::vector<std::pair<int, std::vector<uint8_t>>> unicasts;
  std::vector<std::vector<uint8_t>> multicasts;
  std::vector<Mvd2Sound> sounds;
  std::vector<Mvd2Player> players;      // by client number, `max clients` long
  std::vector<Q2EntityState> entities;  // by number, kQ2Entities long
};

// A level's start: the game state.
struct Mvd2Level {
  int32_t server_count = 1;
  std::string game;                         // the game folder ("" the base game)
  int observer = -1;                        // the recording's own observer's client number
  std::vector<std::string> config_strings;  // kQ2ConfigStrings long
  Mvd2Frame base;                           // the frame the game state holds
};

// The writer, a block at a time: the magic (once, at a file's start), a level's game state, a
// frame after the one before it (`was`: the game state's or the last frame's), the end.
void mvd2_write_magic(std::vector<uint8_t>& out);
void mvd2_write_level(std::vector<uint8_t>& out, const Mvd2Level& level);
void mvd2_write_frame(std::vector<uint8_t>& out, const Mvd2Frame& was, const Mvd2Frame& now);
void mvd2_write_end(std::vector<uint8_t>& out);
// A whole recording of `level` and `frames` (the frames after the game state, in order).
std::vector<uint8_t> mvd2_write(const Mvd2Level& level, const std::vector<Mvd2Frame>& frames);

// A walking player's entity placed from his state: his origin, a third of his view's pitch (a
// pitch past half a turn wrapped below 0 first), his view's yaw, no roll.
void mvd2_place_from_player(Q2EntityState& e, const Q2PlayerState& p);
// Whether entity `number` is placed from its player's state in a frame of `players`.
bool mvd2_placed(const std::vector<Mvd2Player>& players, int number);

}  // namespace shooters_edge::server
