// QUAKE II'S WIRE, SERVER SIDE: Quake II's protocol 34 server messages written from what a
// runner shows (rules_runner.hpp's `RulesCommon`, rules_world.hpp's rows), for one viewpoint
// slot. One encoder for every place Quake II's messages go: a recording (`.dm2`, and through
// client/q2_demo.hpp the replay file `.ged`), a hosted match's broadcast through TV, and the
// Quake II's own server for real clients (M18v9) sending the same bytes.
//
// - **The signon** (`q2_signon`): the server data (protocol 34, attract loop 1 as a client's
//   recording writes it), every configstring in index order, a baseline for every entity shown,
//   and `precache`, cut into blocks the way the game's own recorder cuts them (a configstring
//   starts a new block past 1400 bytes with a 32-byte margin, a baseline with a 64-byte one).
// - **A frame** (`q2_frame`): the reliable messages first (changed configstrings, prints,
//   centred prints, stuffed lines, the viewer's layout program and inventory when they changed),
//   then the frame (its number, the one it deltas from, area bits that show every area), the player
//   state as a delta from the last frame's, the entities as a delta list against the last frame's
//   (an entity new to the list from its baseline, a player always with its old origin, a gone one
//   removed), then the frame's sounds.
//
// The configstrings: 0 the map's title, 30 the most clients, 31 the map's checksum (the caller's:
// the MD4 of the whole map file, its four little-endian words XORed, as signed decimal, measured
// on the demo pack's map), 32 + 1 `maps/<map>.bsp`, the runner's models one place up (its `*1`
// at 32 + 2, as the game numbers them), its sounds at 288 + n, its light styles at 800 + n and
// each active client's `name\male/grunt` at 1312 + slot. A model index on the wire is the
// runner's plus one for the same reason. The layouts are in porting/shooters-edge/q2-demo.md.
//
// The status bar: the rules' own (rules_world.hpp's `status_bar`, with its pictures at image
// strings 544 + n and its item names at 1056 + n) and every one of the player's 32 stats; for
// rules with none, a bar of three numbers and stats 1 health, 3 the ammo in hand, 5 armour.
//
// ge:summary Quake II's protocol 34 server messages from a runner's rows: signon and frames
// ge:keywords q2 wire protocol-34 serverdata configstring spawnbaseline frame playerinfo
// ge:keywords packetentities entity-delta delta-frame dm2 recorder record viewpoint map-checksum
// ge:keywords q2_signon q2_signon_parts q2_frame q2_resync q2_record
// ge:keywords q2_entity_flags q2_write_entity q2_write_player q2_config_strings
// ge:keywords q2_entity_state q2_player_state q2_sight pvs-cut view-cut frame-cut
// ge:keywords q2_sight_link q2_sight_link_rules visible-entities
// ge:skill shooter replay
// ge:test tests/shooters_edge_q2_wire_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

inline constexpr int kQ2Protocol = 34;
inline constexpr int kQ2MaxBlock = 1400;
inline constexpr int kQ2Entities = 1024;
inline constexpr int kQ2ConfigStrings = 2080;

// One entity as the wire holds it: whole 1/8 units, angles in 256ths of a turn, the wire's
// model numbers.
struct Q2EntityState {
  bool present = false;
  int16_t origin[3] = {}, old_origin[3] = {};
  uint8_t angles[3] = {};
  int model[4] = {};
  int frame = 0;
  uint32_t skin = 0, effects = 0, renderfx = 0;
  int sound = 0, event = 0, solid = 0;
};

// The player's state as the wire holds it.
struct Q2PlayerState {
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

struct Q2WireOptions {
  int32_t server_count = 1;
  int32_t map_checksum = 0;  // configstring 31 (see above); a real client refuses another
  uint8_t attract_loop = 1;  // 1 a client's recording, 0 a live game
  // The world's sky (configstrings 2, 3 and 4: its name, axis and turn a second, as text), from
  // the map's world entity (`q2_sky`); empty: none named.
  std::string sky, sky_axis, sky_rotate;
  // Lines stuffed at the end of the signon, before `precache` (a recording's place names).
  std::vector<std::string> stuffed;
  // A live server's cap on a frame's entities (sv_maxents; 0: none, the packet's room alone):
  // past it the lowest numbers are kept, or with `prioritize` (sv_entpriority) players, brush
  // models and anything alive first, pieces and model-less entities last, the nearest first in
  // each; the rest wait for a frame with room.
  int max_entities = 0;
  bool prioritize = false;
  // The viewer's protocol: 34, or a later one (35 or 36, q2_wire_ext.cpp) with its minor version.
  int protocol = kQ2Protocol;
  int minor = 0;
};

// The sky the map's world entity names (its `sky`, `skyaxis` and `skyrotate` keys) into `opt`.
void q2_sky(const ge::brush::Map& map, Q2WireOptions& opt);

// THE VIEWER'S CUT (a live server's): a client is sent only the entities it may see, as the
// Quake II's own server cuts them, so a client never learns what is behind the walls and a frame
// stays small. Each entity is linked once a frame by its box (`q2_sight_link`: the clusters it
// touches); a viewer's frame then keeps its own entity, a beam whose first cluster it can hear,
// and any other entity with a cluster in the union of the visible rows of every leaf within 8
// units of its eye (the player state's origin and view offset); an entity with no model (a sound
// or an effect alone) only within 400 units of the eye. An entity touching more than
// `kSightClusters` clusters is sent wherever the viewer is. Every area counts as connected (the
// net server keeps no area portals, porting/shooters-edge/q2-game-library.md § 17), so
// closed doors cut nothing. A recording keeps every entity (no cut set).
inline constexpr int kSightClusters = 16;
struct Q2SightLinks {
  int32_t clusters[kSightClusters] = {};
  uint8_t count = 0;        // clusters held
  bool everywhere = false;  // more than kSightClusters touched
};
struct Q2Sight {
  const ge::brush::Map* map = nullptr;  // null: no cut, every entity sent
  std::vector<Q2SightLinks> links;      // by entity number (kQ2Entities)
};
// Starts a frame's links over `map` (null: no cut); every entity unlinked.
void q2_sight_reset(Q2Sight& s, const ge::brush::Map* map);
// Entity `number` linked by its box in the world (`lo`..`hi`, map units).
void q2_sight_link(Q2Sight& s, int number, const float lo[3], const float hi[3]);
// Every entity of a runner's world linked by its box (its absolute box, else origin + its size).
void q2_sight_link_rules(Q2Sight& s, const RulesCommon& c);

struct Q2Wire;
// The cut's two halves, as each frame applies them (a lab measures them): the viewer's cluster
// bits worked out from its eye (map units; false when `w.sight` holds no map, so nothing is cut),
// then whether entity `n`, shown as `s`, is kept.
bool q2_sight_view(Q2Wire& w, const float eye[3]);
bool q2_sight_keeps(const Q2Wire& w, int n, const Q2EntityState& s, const float eye[3]);

// What the encoder keeps between frames for one viewpoint.
struct Q2Wire {
  Q2WireOptions opt;
  int slot = 0;
  int32_t last_frame = -1;                     // the frame the next deltas from; -1 none yet
  std::vector<Q2EntityState> baselines, last;  // by entity number
  Q2PlayerState last_player;
  std::vector<std::string> sent;   // the configstrings the viewer holds
  int64_t events_done = 0;         // the sink's events already written (`events_total`)
  std::string layout;              // the layout program the viewer holds
  std::vector<int16_t> inventory;  // the inventory counts the viewer holds
  // The cut the next frame applies (null or no map: none); the caller's, linked for this frame.
  const Q2Sight* sight = nullptr;
  std::vector<uint8_t> seen, heard;  // the viewer's cluster bits, this frame
  int64_t cut = 0;                   // entities the cut kept out, every frame summed
  int64_t capped = 0;                // entities the cap kept out (sv_maxents), every frame summed
  // The frames held back for the viewer's rate since the last one written: the next frame's
  // suppress count, set back to 0 as it is written (a live server's; a recording's stays 0).
  uint8_t suppress = 0;
  // A later protocol's viewer (q2_wire_ext.cpp): what its client asked to be left out (its client
  // settings: no gun 1 the whole gun, 2 all but its model; no blend; recording turns every leaving
  // out off; no gibs; no footsteps; no prediction) and the frame's flags for 36 (1 frames held
  // back, 2 the client's packets lost, 4 the client's moves ran short), cleared as it is written.
  struct Asked {
    int no_gun = 0;
    bool no_blend = false, recording = false, no_gibs = false, no_footsteps = false,
         no_predict = false;
  } asked;
  uint8_t frame_flags = 0;
};

// What the runner shows, in the wire's terms (the pieces every writer of Quake II's messages
// builds from): the settings strings, one entity's state (`present` false: none shown) and one
// slot's player state (`move` the slot's move state when the runner keeps one).
void q2_config_strings(const RulesCommon& c, const Q2WireOptions& opt,
                       std::vector<std::string>& out);
Q2EntityState q2_entity_state(const RulesCommon& c, int number);
Q2PlayerState q2_player_state(const RulesCommon& c, int slot, const ge::classic::Q2State* move);

// Protocol 34's deltas, for any writer of Quake II's messages (the recordings here, the
// multiview converter in client/demo_mvd2.cpp): an entity's flag bits from `from` to `to` (`old`
// forces the old origin; a beam always carries it), its entry (header, number, fields) appended to
// `out`, and the player state's message (its id, flags and fields; the gun's index always sent).
uint32_t q2_entity_flags(const Q2EntityState& from, const Q2EntityState& to, int number, bool old);
void q2_write_entity(std::vector<uint8_t>& out, uint32_t flags, int number,
                     const Q2EntityState& to);
void q2_write_player(std::vector<uint8_t>& out, const Q2PlayerState& from, const Q2PlayerState& to);
// A rules' effect event (a point or a trail) as its muzzle flash or temporary entity message;
// false when the protocol has none for it (nothing written).
bool q2_write_effect(std::vector<uint8_t>& out, const NetEvent& e);

// The map's checksum as configstring 31 carries it: the MD4 of the whole map file, its four
// little-endian words XORed, read as a signed number.
int32_t q2_map_checksum(std::span<const uint8_t> bsp);
// Our effect's name for the protocol's temporary entity `code` (a map's target_temp_entity style),
// or empty for one we do not send.
std::string q2_temp_entity_name(int code);

// The signon's messages one by one: the server data, each configstring (by index) and each
// baseline (by entity number), each message whole with its id. A live server sends them as a
// client asks for them (net/q2_net_server.hpp); a recording cuts them into blocks.
struct Q2Signon {
  std::vector<uint8_t> server_data;
  std::vector<std::pair<int, std::vector<uint8_t>>> config_strings, baselines;
};
void q2_signon_parts(Q2Wire& w, const RulesCommon& c, int slot, const Q2WireOptions& opt,
                     Q2Signon& out);
// The signon for viewpoint `slot`, as blocks of messages (no lengths).
void q2_signon(Q2Wire& w, const RulesCommon& c, int slot, const Q2WireOptions& opt,
               std::vector<std::vector<uint8_t>>& blocks);
// One frame's block: `frame_number` the rules' frame; `move` the viewpoint's move state when the
// runner keeps one (null: taken from the world's rows). With `reliable`, the changed
// configstrings, prints, centred prints and stuffed lines are appended there (a live server's
// reliable channel) and `block` holds the frame and its sounds alone.
void q2_frame(Q2Wire& w, const RulesCommon& c, int32_t frame_number,
              const ge::classic::Q2State* move, std::vector<uint8_t>& block,
              std::vector<uint8_t>* reliable = nullptr);
// The viewer holds no frame (a live client lost the one the next would delta from): the next
// frame is whole, its entities from their baselines.
void q2_resync(Q2Wire& w);

// A GAME THAT WRITES ITS OWN MESSAGES (a hosted game library, q2_native.hpp) shows the wire
// as it is: its configstrings in the protocol's own slots, its entities already numbered as the
// wire numbers them, each player's state whole, and the bytes it wrote for each slot (prints,
// sounds, temporary entities, layouts), reliable or not, waiting to be sent.
struct Q2Raw {
  std::vector<std::string> config;                  // kQ2ConfigStrings entries
  std::vector<Q2EntityState> entities;              // this frame's, by number (kQ2Entities)
  std::vector<Q2EntityState> baselines;             // the level's, taken once its entities settle
  std::vector<int16_t> owners;                      // by number: the owner's number (0 none)
  Q2Sight sight;                                    // this frame's links (the cut's)
  Q2PlayerState players[32];                        // by slot
  std::vector<uint8_t> reliable[32], datagram[32];  // by slot, taken by each frame written
};

// The signon from a raw game, as `q2_signon_parts`.
void q2_signon_raw(Q2Wire& w, const Q2Raw& raw, int slot, const Q2WireOptions& opt, Q2Signon& out);
// One frame from a raw game, as `q2_frame` with `reliable`: the slot's waiting bytes are
// moved into the reliable part and after the frame (dropped when they do not fit).
void q2_frame_raw(Q2Wire& w, Q2Raw& raw, int32_t frame_number, std::vector<uint8_t>& block,
                  std::vector<uint8_t>& reliable);

// QUAKE II'S LATER PROTOCOLS (35 and 36, q2_wire_ext.cpp; porting/shooters-edge/demo-formats-q2.md
// § A and q2-proto36-server.md): what a frame and the signon write differently to such a viewer.
// - The server data's tail: 35 a zero byte, the minor version (short), a zero byte and the
//   strafe-jump flag; 36 the minor version, the server's state (2, a game) and the movement flags
//   (a short from minor 1024, else three bytes), all clear: the move runs as 34's.
// - The frame: the id's top 3 bits and the suppress byte's top 4 the player state's extra flags
//   (gun offset, gun angles, velocity z, origin z, roll, stats, 36's client number); the frame
//   number in 27 bits and how far back its delta is in 5 (31: none); the suppress byte's low 4
//   bits the count held back (35) or the frame's flags (36); the player state with no id and
//   only what changed; the entities with no id.
// - Entities: the solid a long (36, and 35 from minor 1905). With `asked`: gibs left out, a
//   footstep's event cleared; to 36 not recording, the viewer's own entity alive keeps the origin,
//   angles and old origin its client has.
// - The player state, not recording: the view angles left out while the viewer is alive and
//   predicts, the delta angles while dead; the gun and blend as asked; 36 with no prediction
//   asked, the velocity, the move's time, flags and gravity. A field left out keeps its old value
//   in what the wire holds, so the next delta is from what the client has.
// The id-less frame top (frame, player state, client number) written after the reliable part,
// and the stored player state taken.
void q2_write_frame_top_ext(std::vector<uint8_t>& out, Q2Wire& w, int32_t number,
                            const Q2PlayerState& p);
// The server data's tail for `opt`'s protocol (nothing for 34).
void q2_server_data_tail(std::vector<uint8_t>& out, const Q2WireOptions& opt);
// An entity as a later protocol's viewer is sent it (`asked`, its own entity), from `base`.
void q2_entity_for_ext(const Q2Wire& w, int number, const Q2EntityState& base, Q2EntityState& s);
// The solid's long form from protocol 34's packed short (31 a brush model kept).
int32_t q2_long_solid(int solid);
// The protocol 36 signon's messages from its parts: one game state (minor below 1024), or the
// configstring then baseline streams, each message at most kQ2BigMessage bytes.
inline constexpr size_t kQ2BigMessage = 32768;
void q2_signon_streams(const Q2Signon& parts, int minor, std::vector<std::vector<uint8_t>>& out);
// A RECORDING (`.dm2`): blocks of a 32-bit length and the block, ended by a length of -1.
struct Q2Recorder {
  Q2Wire wire;
  std::vector<uint8_t> bytes;
  bool on = false;
  int64_t frames = 0;
  int32_t at = -1;  // the last frame written
};

void q2_record_start(Q2Recorder& r, const RulesCommon& c, int slot, const Q2WireOptions& opt);
// Writes a block when the rules have run a frame since the last one (call it every host tick).
void q2_record_frame(Q2Recorder& r, const RulesCommon& c, int32_t frame_number,
                     const ge::classic::Q2State* move);
// The -1 end; the recording is whole.
void q2_record_stop(Q2Recorder& r);

}  // namespace shooters_edge::server
