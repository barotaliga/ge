// THE CLASSIC PROTOCOL'S SERVER ENDPOINT: the mod server (games/shooters_edge/server/qc_server.hpp)
// served to clients that speak the classic protocol, over engine/net's transport (a UDP socket or
// the simulated network). It owns no game rules: a client's moves become the loop's packets, its
// string commands the mod's, and what the mod writes (messages.hpp) becomes svc messages.
//
// ONE FRAME (`classic_frame`, called once a server frame with the network's time): every datagram
// waiting is read (out of band: ping, status, getchallenge, connect, the rest answered or ignored
// as docs/shooters-edge.md § The network says; in band: the channel by address and qport, then
// its clc messages in order, a client that sends an unknown one dropped); the spawned clients'
// moves run as one packet each in `qc_frame`; what the mod wrote is routed (prints, sounds, stuffed
// text, light styles, effects as temporary entities, static entities and sounds, multicasts, the
// Write buffers, pauses); then every client the server heard from since its last reply gets one
// packet when its rate allows (else its choke count grows): the reliable stream's next message,
// and, once spawned, the choke count, the players it can see, the entities as a delta against the
// frame it acknowledged (or from the baselines), and the frame's unreliable messages.
//
// THE SIGNON (classic_signon.cpp): `new` (the `pext` step first when the connect named no
// extension families), `soundlist` and `modellist` in pages, `prespawn` (static entities,
// baselines, the signon buffers), `spawn` (every client's state, the light styles, the stats),
// the client's `skins` step and its downloads, then `begin`, when the mod's connect and spawn
// hooks place the player. A command carrying a spawn count other than the server's is ignored.
//
// THE MOVE CHECKSUM is verified on every move (classic_wire.hpp, over the key table that is the
// protocol's wire constant): a move whose byte is wrong is counted and dropped, the rest of its
// packet read. `verify_move_checksum = false` accepts every move unchecked and counts it.
//
// DOWNLOADS: files the server offers are handed in `ServerConfig::files` (the game's file system
// later); the classic transfer (a block a `nextdl`, on the reliable stream, its size the rate's
// 1/25 within 10..1024 bytes) or, when agreed, chunks of 1024 bytes asked for by number, the first
// a frame inside the packet and the rest out of band as `\chunk` packets.
//
// LAG COMPENSATION (`ServerConfig::antilag`, classic_antilag.hpp): each player's origins are kept
// with their server times; a hitscan trace a mod marks lagged sees the other players where the
// shooter saw them.
//
// Nothing reads a wall clock: the network's time is the caller's, the challenges are drawn from
// a stream seeded by `ServerConfig::seed`, so a session over the simulated network replays to
// the bit.
//
// ge:summary The classic protocol's server: connections, signon, frames and downloads over a mod
// ge:keywords classic-server endpoint connectionless challenge connect signon serverdata
// ge:keywords prespawn baselines spawn begin skins download chunked packet-entities playerinfo
// ge:keywords stats choke rate ping user-info server-info classic_start classic_frame
// ge:keywords verify_move_checksum antilag status
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "net/transport.hpp"
#include "shooters_edge/net/classic_antilag.hpp"
#include "shooters_edge/net/classic_channel.hpp"
#include "shooters_edge/net/classic_svc.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::net {

namespace sv = shooters_edge::server;

struct ServedFile {
  std::string path;  // lower case, slashes: "skins/blue.pcx"
  std::vector<uint8_t> bytes;
};

struct ServerConfig {
  std::string hostname = "Shooter's Edge";
  std::string gamedir = "edge";  // the game directory serverdata names
  std::string password, spectator_password;
  bool verify_move_checksum = true;  // false: moves are accepted unchecked (and counted)
  int maxrate = 0;                   // sv_maxrate: a client's rate cap, bytes/s (0: none)
  bool allow_download = true, allow_skins = true, allow_models = true, allow_sounds = true;
  bool allow_maps = true, allow_other = false;
  std::vector<ServedFile> files;
  Ext offered = first_tier();
  int chunks_per_frame = 30;
  bool antilag = false;
  uint32_t seed = 1;  // the challenges' stream
};

enum class ConnState : uint8_t { free, connected, spawned };

struct ServerFrame {
  uint32_t sequence = 0;   // the packet that carried it (0: never)
  double sent = 0;         // the network's time it was sent, s (negated once its ping counted)
  double server_time = 0;  // the server's time of the frame it showed
  bool entities = false;   // it carried an entity list
  std::vector<EntityState> ents;
};

struct ServerDownload {
  bool active = false, chunked = false;
  int file = -1;
  size_t offset = 0;
  int number = 0;
  std::vector<int> want;  // chunk numbers asked for, oldest first
  int64_t blocks = 0;
};

struct ClassicClient {
  ConnState state = ConnState::free;
  gn::Address address;
  Channel ch;
  std::string userinfo, name;
  int userid = 0;
  bool spectator = false;
  bool qc_active = false;  // the mod placed the player (after `begin`)
  Ext ext;
  Coords coords = Coords::shorts;
  bool need_pext = false;
  int msg_level = 1;    // the `msg` key: prints below it are not sent
  bool heard = false;   // a packet arrived since the last reply
  int delta_byte = -1;  // the last clc_delta (-1: none)
  ServerFrame frames[64];
  int chokecount = 0;
  int32_t stats[kStats] = {};
  bool stats_known[kStats] = {};
  double connect_time = 0;  // the server's time
  double ping_s = 0;
  int ping_samples = 0;
  int loss_pct = 0;
  uint64_t last_accepted = 0, last_dropped = 0;  // the loss window's start
  ServerDownload dl;
  std::vector<uint8_t> datagram;  // this frame's unreliable messages
  uint64_t moves = 0, moves_unchecked = 0, moves_bad_checksum = 0;
  int statics_sent = 0;  // static entities the client has (signon and later)
};

struct ServerCounts {
  int64_t oob_in = 0, oob_out = 0, connects = 0, rejects = 0, drops = 0, chunks_oob = 0;
  int64_t datagrams_in = 0, datagrams_out = 0, bytes_in = 0, bytes_out = 0, choked = 0;
  int64_t effects_dropped = 0, events = 0;
  int64_t mod_bytes_dropped = 0;  // a mod's raw bytes that did not read as whole messages
};

struct Challenge {
  gn::Address address;  // the port ignored
  int32_t value = 0;
  int64_t made = 0;  // the order it was made, for replacing the oldest
};

struct ClassicServer {
  ServerConfig cfg;
  sv::QcServer* qc = nullptr;
  gn::Transport* transport = nullptr;
  ClassicClient clients[sh::kMaxClients];
  Challenge challenges[64];
  int64_t challenges_made = 0;
  ge::core::Rng rng{1};
  int spawncount = 1;
  int next_userid = 1;
  std::string serverinfo;
  std::vector<EntityState> baselines;  // by entity number
  struct StaticSound {
    sh::Vec3f origin;
    int sound = 0, volume = 255, attenuation = 64;
  };
  std::vector<StaticSound> static_sounds;
  std::vector<std::vector<uint8_t>> signon;  // the signon buffers prespawn sends after baselines
  int player_model = 0;                      // the model list's index of the player model
  int statics_known = 0;                     // static entities routed so far
  double now = 0;                            // the network's time, s
  double last_ping_update = 0;
  int64_t frames = 0;
  std::vector<sh::Packet> packets;
  std::vector<std::string> log;  // connects, rejects and drops, in order (the lab, the tests)
  ServerCounts counts;
  AntilagHistory antilag;
  sh::Vec3f lag_saved[sh::kMaxClients];  // a lagged trace's moved players' own origins
  bool lag_moved[sh::kMaxClients] = {};
  int64_t lag_rewinds = 0;
  // Field offsets of the mod's own fields a stat or the player state reads (-1: absent).
  int f_currentammo = -1, f_armorvalue = -1, f_ammo[4] = {-1, -1, -1, -1}, f_weapon = -1;
  int f_weaponframe = -1;
};

// Serves `qc` (its map already loaded) over `t`. False with `error` when it cannot.
bool classic_start(ClassicServer& s, sv::QcServer& qc, gn::Transport& t, const ServerConfig& cfg,
                   std::string& error);
// One server frame at the network's time `now_us`.
void classic_frame(ClassicServer& s, int64_t now_us);
// Drops a client with a reason (it is told with a disconnect).
void classic_drop(ClassicServer& s, int slot, const std::string& why);
int classic_count(const ClassicServer& s, ConnState at_least);

// The status text (`status <opt>`): the server info, then a line a client.
std::string status_text(const ClassicServer& s, int opt);

// ---- Shared by the endpoint's files (classic_server.cpp, classic_signon.cpp,
//      classic_frame.cpp); not an API for a caller. ----
void send_oob(ClassicServer& s, const gn::Address& to, const uint8_t* data, size_t size);
void send_oob_print(ClassicServer& s, const gn::Address& to, const std::string& text);
void reliable_bytes(ClassicServer& s, int slot, const uint8_t* data, size_t size);
void reliable_svc(ClassicServer& s, int slot, const SvcMsg& m);
void stuff(ClassicServer& s, int slot, const std::string& text);
void client_print(ClassicServer& s, int slot, int level, const std::string& text);
void full_client_update(ClassicServer& s, int about, int to);  // `to` -1: every connected client
void string_command(ClassicServer& s, int slot, const std::string& line);
void route_events(ClassicServer& s);
void send_frames(ClassicServer& s);
void send_download_chunks(ClassicServer& s, int slot, gn::Writer& dgram);
EntityState entity_state(const ClassicServer& s, int e);

}  // namespace shooters_edge::net
