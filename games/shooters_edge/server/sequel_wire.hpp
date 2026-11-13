// THE SEQUEL'S WIRE, SERVER SIDE: the sequel's protocol 34 server messages written from what a
// runner shows (rules_runner.hpp's `RulesCommon`, rules_world.hpp's rows), for one viewpoint
// slot. One encoder for every place the sequel's messages go: a recording (`.dm2`, and through
// client/sequel_demo.hpp the replay file `.ged`), a hosted match's broadcast through TV, and the
// sequel's own server for real clients (M18v9) sending the same bytes.
//
// - **The signon** (`sequel_signon`): the server data (protocol 34, attract loop 1 as a client's
//   recording writes it), every configstring in index order, a baseline for every entity shown,
//   and `precache`, cut into blocks the way the game's own recorder cuts them (a configstring
//   starts a new block past 1400 bytes with a 32-byte margin, a baseline with a 64-byte one).
// - **A frame** (`sequel_frame`): the reliable messages first (changed configstrings, prints,
//   centred prints, stuffed lines), then the frame (its number, the one it deltas from, area bits
//   that show every area), the player state as a delta from the last frame's, the entities as a
//   delta list against the last frame's (an entity new to the list from its baseline, a player
//   always with its old origin, a gone one removed), then the frame's sounds.
//
// The configstrings: 0 the map's title, 30 the most clients, 31 the map's checksum (the caller's:
// the MD4 of the whole map file, its four little-endian words XORed, as signed decimal, measured
// on the demo pack's map), 32 + 1 `maps/<map>.bsp`, the runner's models one place up (its `*1`
// at 32 + 2, as the game numbers them), its sounds at 288 + n, its light styles at 800 + n and
// each active client's `name\male/grunt` at 1312 + slot. A model index on the wire is the
// runner's plus one for the same reason. The layouts are in porting/shooters-edge/sequel-demo.md.
//
// The status bar: the rules' own (rules_world.hpp's `status_bar`, with its pictures at image
// strings 544 + n and its item names at 1056 + n) and every one of the player's 32 stats; for
// rules with none, a bar of three numbers and stats 1 health, 3 the ammo in hand, 5 armour.
//
// ge:summary The sequel's protocol 34 server messages from a runner's rows: signon and frames
// ge:keywords sequel wire protocol-34 serverdata configstring spawnbaseline frame playerinfo
// ge:keywords packetentities entity-delta delta-frame dm2 recorder record viewpoint map-checksum
// ge:keywords sequel_signon sequel_signon_parts sequel_frame sequel_resync sequel_record
// ge:skill shooter replay
// ge:test tests/shooters_edge_sequel_wire_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

inline constexpr int kSequelProtocol = 34;
inline constexpr int kSequelMaxBlock = 1400;
inline constexpr int kSequelEntities = 1024;
inline constexpr int kSequelConfigStrings = 2080;

// One entity as the wire holds it: whole 1/8 units, angles in 256ths of a turn, the wire's
// model numbers.
struct SequelEntityState {
  bool present = false;
  int16_t origin[3] = {}, old_origin[3] = {};
  uint8_t angles[3] = {};
  int model[4] = {};
  int frame = 0;
  uint32_t skin = 0, effects = 0, renderfx = 0;
  int sound = 0, event = 0, solid = 0;
};

// The player's state as the wire holds it.
struct SequelPlayerState {
  uint8_t type = 0;
  int16_t origin[3] = {}, velocity[3] = {};
  uint8_t time = 0, flags = 0;
  int16_t gravity = 0;
  int16_t delta_angles[3] = {};
  int8_t view_offset[3] = {};
  int16_t view_angles[3] = {};
  int8_t kick[3] = {};
  uint8_t gun_index = 0, gun_frame = 0;
  int8_t gun_offset[3] = {}, gun_angles[3] = {};
  uint8_t blend[4] = {};
  uint8_t fov = 0, rdflags = 0;
  int16_t stats[32] = {};
};

struct SequelWireOptions {
  int32_t server_count = 1;
  int32_t map_checksum = 0;  // configstring 31 (see above); a real client refuses another
  uint8_t attract_loop = 1;  // 1 a client's recording, 0 a live game
  // The world's sky (configstrings 2, 3 and 4: its name, axis and turn a second, as text), from
  // the map's world entity (`sequel_sky`); empty: none named.
  std::string sky, sky_axis, sky_rotate;
  // Lines stuffed at the end of the signon, before `precache` (a recording's place names).
  std::vector<std::string> stuffed;
};

// The sky the map's world entity names (its `sky`, `skyaxis` and `skyrotate` keys) into `opt`.
void sequel_sky(const ge::brush::Map& map, SequelWireOptions& opt);

// What the encoder keeps between frames for one viewpoint.
struct SequelWire {
  SequelWireOptions opt;
  int slot = 0;
  int32_t last_frame = -1;                         // the frame the next deltas from; -1 none yet
  std::vector<SequelEntityState> baselines, last;  // by entity number
  SequelPlayerState last_player;
  std::vector<std::string> sent;  // the configstrings the viewer holds
  int64_t events_done = 0;        // the sink's events already written (`events_total`)
};

// The map's checksum as configstring 31 carries it: the MD4 of the whole map file, its four
// little-endian words XORed, read as a signed number.
int32_t sequel_map_checksum(std::span<const uint8_t> bsp);

// The signon's messages one by one: the server data, each configstring (by index) and each
// baseline (by entity number), each message whole with its id. A live server sends them as a
// client asks for them (net/sequel_net_server.hpp); a recording cuts them into blocks.
struct SequelSignon {
  std::vector<uint8_t> server_data;
  std::vector<std::pair<int, std::vector<uint8_t>>> config_strings, baselines;
};
void sequel_signon_parts(SequelWire& w, const RulesCommon& c, int slot,
                         const SequelWireOptions& opt, SequelSignon& out);
// The signon for viewpoint `slot`, as blocks of messages (no lengths).
void sequel_signon(SequelWire& w, const RulesCommon& c, int slot, const SequelWireOptions& opt,
                   std::vector<std::vector<uint8_t>>& blocks);
// One frame's block: `frame_number` the rules' frame; `move` the viewpoint's move state when the
// runner keeps one (null: taken from the world's rows). With `reliable`, the changed
// configstrings, prints, centred prints and stuffed lines are appended there (a live server's
// reliable channel) and `block` holds the frame and its sounds alone.
void sequel_frame(SequelWire& w, const RulesCommon& c, int32_t frame_number,
                  const ge::shooter::SequelState* move, std::vector<uint8_t>& block,
                  std::vector<uint8_t>* reliable = nullptr);
// The viewer holds no frame (a live client lost the one the next would delta from): the next
// frame is whole, its entities from their baselines.
void sequel_resync(SequelWire& w);

// A RECORDING (`.dm2`): blocks of a 32-bit length and the block, ended by a length of -1.
struct SequelRecorder {
  SequelWire wire;
  std::vector<uint8_t> bytes;
  bool on = false;
  int64_t frames = 0;
  int32_t at = -1;  // the last frame written
};

void sequel_record_start(SequelRecorder& r, const RulesCommon& c, int slot,
                         const SequelWireOptions& opt);
// Writes a block when the rules have run a frame since the last one (call it every host tick).
void sequel_record_frame(SequelRecorder& r, const RulesCommon& c, int32_t frame_number,
                         const ge::shooter::SequelState* move);
// The -1 end; the recording is whole.
void sequel_record_stop(SequelRecorder& r);

}  // namespace shooters_edge::server
