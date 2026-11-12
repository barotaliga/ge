// THE CLASSIC PROTOCOL'S SERVER ENDPOINT: a game's rules, held as a runner
// (games/shooters_edge/server/rules_runner.hpp), served to clients that speak the classic protocol,
// over engine/net's transport (a UDP socket or the simulated network). It owns no game rules: a
// client's moves become the loop's packets, its string commands the mod's, and what the mod writes
// (messages.hpp) becomes svc messages.
//
// ONE FRAME (`classic_frame`, called once a server frame with the network's time): every datagram
// waiting is read (out of band: ping, status, getchallenge, connect, rcon (classic_admin.hpp), the
// rest answered or ignored as docs/shooters-edge.md § The network says; in band: the channel by
// address and qport, then its clc messages in order, a client that sends an unknown one dropped);
// the spawned clients' moves run as one packet each in the runner's `frame`; what the mod wrote is
// routed (prints, sounds, stuffed text, light styles, effects as temporary entities, static
// entities and sounds, multicasts, the Write buffers, pauses); then every client the server heard
// from since its last reply gets one packet when its rate allows (else its choke count grows): the
// reliable stream's next message, and, once spawned, the choke count, the players it can see, the
// entities as a delta against the frame it acknowledged (or from the baselines), and the frame's
// unreliable messages.
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
// LOCAL SEATS (`classic_add_local`, a bot): shown to every client as a player, sent nothing, kept
// over a map change, its moves the game's (`local_moves`). `game_command` sees a client's words
// before the rules (a lobby's).
//
// VOICE (`ServerConfig::voice`): the x2 family's voice bit offered, and each voice message a
// client sends (clc 83, at most `kMostVoiceBytes`) relayed as svc 84 with the talker's slot to the
// clients that agreed the bit and may hear it: by `voice_route`, everyone, or only the players
// whose `team` is the talker's (in a free-for-all, nobody's: everyone); never his own back. A
// listen server's own player hears through `local_voice` and talks by `classic_local_voice`. A
// multiview recording running (`ClassicServer::mvd`) gets every relayed message, whoever heard it.
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
// ge:keywords classic_change map-change changing reconnect classic_add_local local-seat bots
// ge:keywords verify_move_checksum antilag status voice voice-relay team-voice voice_route mvd
// ge:keywords sv_timeout timed-out flood-protection sv_floodprot chat-flood most-players
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "net/status.hpp"
#include "net/transport.hpp"
#include "server/guards.hpp"
#include "shooters_edge/net/classic_antilag.hpp"
#include "shooters_edge/net/classic_channel.hpp"
#include "shooters_edge/net/classic_svc.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

namespace sv = shooters_edge::server;
namespace gs = ge::server;

struct MultiviewRecorder;

inline constexpr size_t kMostVoiceBytes = 1024;  // a client's voice message's frames, bytes

enum class VoiceRoute : uint8_t { all, team };

struct ServedFile {
  std::string path;  // lower case, slashes: "skins/blue.pcx"
  std::vector<uint8_t> bytes;
};

struct ServerConfig {
  std::string hostname = "Shooter's Edge";
  std::string gamedir = "edge";  // the game directory serverdata names
  std::string password, spectator_password;
  std::string rcon_password;  // the remote console's (classic_admin.hpp; empty: none offered)
  bool verify_move_checksum = true;  // false: moves are accepted unchecked (and counted)
  int maxrate = 0;                   // sv_maxrate: a client's rate cap, bytes/s (0: none)
  int maxfps = 77;                   // maxfps: the moves a second clients are told they may send
  // THE GUARDS (never on a local seat): a client unheard for `timeout_s` is dropped and everyone
  // told "<name> timed out"; a `say` or `say_team` past the flood rule (engine/server/guards.hpp)
  // is dropped and only its sender told why.
  double timeout_s = 65;  // sv_timeout
  gs::FloodRule flood;    // sv_floodprot
  std::string flood_msg;  // sv_floodprotmsg: what a flooder is told ("": the silence)
  bool allow_download = true, allow_skins = true, allow_models = true, allow_sounds = true;
  bool allow_maps = true, allow_other = false;
  std::vector<ServedFile> files;
  Ext offered = first_tier();
  int chunks_per_frame = 30;
  bool antilag = false;
  bool voice = false;  // offer the voice bit and relay voice
  VoiceRoute voice_route = VoiceRoute::team;
  uint32_t seed = 1;  // the challenges' stream
  // Players at most, the game's local seats counted (0: the loop's slots): a coop game's eight.
  int most_players = 0;
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
  bool rules_active = false;  // the mod placed the player (after `begin`)
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
  bool local = false;    // a seat the game plays in its own process (a bot): no address, no packets
  int frags_told = 0;    // the frags every client was last sent
  gs::FloodGuard flood;  // its chat lines' times and its silence
};

struct ServerCounts {
  int64_t oob_in = 0, oob_out = 0, connects = 0, rejects = 0, drops = 0, chunks_oob = 0;
  int64_t datagrams_in = 0, datagrams_out = 0, bytes_in = 0, bytes_out = 0, choked = 0;
  int64_t effects_dropped = 0, events = 0;
  int64_t mod_bytes_dropped = 0;  // a mod's raw bytes that did not read as whole messages
  int64_t voice_in = 0, voice_out = 0, voice_refused = 0, voice_dropped = 0;
  int64_t chat_flooded = 0;                // say and say_team lines dropped by the flood rule
  int64_t rcon_run = 0, rcon_refused = 0;  // remote console lines run; refused or not answered
};

struct Challenge {
  gn::Address address;  // the port ignored
  int32_t value = 0;
  int64_t made = 0;  // the order it was made, for replacing the oldest
};

struct ClassicServer {
  ServerConfig cfg;
  const sv::RulesRunner* rules = nullptr;  // the rules served (their map started)
  sv::RulesCommon* game = nullptr;         // what every runner keeps: `*rules`'s
  gn::Transport* transport = nullptr;
  ClassicClient clients[sh::kMaxClients];
  Challenge challenges[64];
  int64_t challenges_made = 0;
  ge::core::Rng rng{1};
  int spawncount = 1;
  int next_userid = 1;
  int64_t rcon_second = -1;  // the second (network time) the rcon tries are counted in
  int rcon_tries = 0;
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
  MultiviewRecorder* mvd = nullptr;  // a multiview recording running (classic_record.hpp)
  // A client's string command the game sees before the rules (a lobby's words): true takes it.
  bool (*game_command)(void* ctx, int slot, const std::string& line) = nullptr;
  void* game_ctx = nullptr;
  std::vector<sh::Packet> local_moves;  // the local seats' moves for the next frame (the game's)
  // A listen server's own player, a local seat: the rules' frame handed to the game before the
  // endpoint routes its events (what the player sees and hears), and the lines printed to a local
  // seat (the lobby's answers), which no packet carries.
  void (*local_tap)(void* ctx, const sv::RulesCommon& game) = nullptr;
  void (*local_print)(void* ctx, int slot, const std::string& text) = nullptr;
  // A voice relayed to a listen server's own player (`slot`, a local seat that is no bot), which
  // no packet carries: `v` the voice message as a client would read it (its talker in ints[0]).
  void (*local_voice)(void* ctx, int slot, const SvcMsg& v) = nullptr;
  void* local_ctx = nullptr;
  // A game that seats its players itself (a campaign, which carries them from map to map): a
  // client's `begin` and its drop go to it in place of the rules' connect and drop.
  void (*seat)(void* ctx, int slot, const std::string& name, bool spectator) = nullptr;
  void (*unseat)(void* ctx, int slot) = nullptr;
  void* seat_ctx = nullptr;
};

// Serves `rules` (its map already started) over `t`. False with `error` when it cannot.
bool classic_start(ClassicServer& s, const sv::RulesRunner& rules, gn::Transport& t,
                   const ServerConfig& cfg, std::string& error);
// The endpoint moves to `next` (its map loaded; the server before it is no longer read and may
// go): every client is told the map is changing and to sign on again (`reconnect`), and the
// next signon is the new map's. False with `error` when `next` has no map.
bool classic_change(ClassicServer& s, const sv::RulesRunner& next, std::string& error);
// The endpoint lets go of the rules it serves before the game destroys them in place (a campaign's
// next map is made where the last one was): nothing reads them until `classic_change`.
void classic_unbind(ClassicServer& s);
// A seat the game plays in its own process at `slot` (a bot it connected to the rules itself):
// shown to every client as a player (`*bot` in its user info), sent nothing, never timed out, kept
// over a map change (the game connects it to the next map's rules), its moves `local_moves`.
// False with `error` when the slot is a client's. `bot` false: a listen server's own player.
bool classic_add_local(ClassicServer& s, int slot, const std::string& name, std::string& error,
                       bool bot = true);
// A listen server's own player talking (`slot` a local seat that is no bot): his voice message
// relayed as a client's is, by `voice_route`, when the server has voice (`ServerConfig::voice`).
void classic_local_voice(ClassicServer& s, int slot, const ClcMsg& m);
// One key of a seat's user info set, told to every client and to the rules (a lobby's team).
void classic_set_info(ClassicServer& s, int slot, const std::string& key, const std::string& value);
// A line printed to every spawned client.
void classic_print_all(ClassicServer& s, const std::string& text);
// One server frame at the network's time `now_us`.
void classic_frame(ClassicServer& s, int64_t now_us);
// Drops a client with a reason (it is told with a disconnect).
void classic_drop(ClassicServer& s, int slot, const std::string& why);
int classic_count(const ClassicServer& s, ConnState at_least);

// The status text (`status <opt>`): the server info, then a line a client.
std::string status_text(const ClassicServer& s, int opt);
// Every connected client as a hub's status record's players (net/status.hpp): name, team, frags,
// ping, minutes; a local seat as a bot, a spectator as one.
void status_players(const ClassicServer& s, std::vector<ge::net::StatusPlayer>& out);

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
// A player's stats as the server sends them (`view_height`: the z set's view height in 16).
void player_stats(const ClassicServer& s, int slot, int32_t out[kStats], bool view_height);

}  // namespace shooters_edge::net
