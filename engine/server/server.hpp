// THE SERVER CORE: one multiplayer game's server, the same for every game on the engine and for
// both ways to run it (docs/servers.md). A game plugs its rules in through `ServerGame`, a table of
// functions; `Server` runs them on a fixed frame and, when it has a socket, owns the network round
// them: the node on the game's port, the session host (seats, chat, kick), and the hosting
// (net/hosting.hpp: the LAN's answers and pings, by the server's name).
//
// - A DEDICATED server is a `Server` stepped by `server_main` (main.hpp), the game's own
//   `<game>_server` binary (`ge_add_server`, cmake/Server.cmake).
// - A LISTEN server is the same `Server` stepped by the game's client loop between its frames.
// Nothing in a game branches on which one it is in.
//
// THE FRAME (`server_frame`): the game's frame, then the network at the frame's time: the node's
// datagrams in, the session host, the hosting, the node's datagrams out. The clock is the frame
// count times the game's frame length, never a wall clock, so a server over the simulated network
// replays to the bit (the `server-core` lab); a dedicated server only waits between frames.
//
// THE SETTINGS are `ServerConfig`, every field a cvar on the server's tier (`add_server_cvars`):
// `sv_hostname`, `sv_description`, `net_ip`, `net_port`, `sv_maxclients`, `sv_map`, `sv_bots`,
// `sv_seed`, `sv_password`, `sv_visibility`. A dedicated server reads them from `server.cfg`
// and its command line (main.hpp).
//
// ge:summary The server core: ServerGame's table, the frame, the node, session and hosting round it
// ge:keywords server dedicated-server listen-server server-core servergame server_frame
// ge:keywords server_open hostname description port listen-address maxclients bots seed
// ge:keywords sv_hostname sv_description net_ip net_port sv_maxclients sv_map sv_bots sv_seed
// ge:keywords sv_password sv_visibility lan-advert multiplayer hosting
// ge:skill net
// ge:test tests/server_core_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "console/cvar.hpp"
#include "net/hosting.hpp"
#include "net/node.hpp"
#include "net/session.hpp"
#include "net/transport.hpp"

namespace ge::server {

constexpr int kMaxClients = 32;

// Everything a server is started with. The first block are cvars (`add_server_cvars`); the second
// only how a dedicated process runs (main.hpp's flags).
struct ServerConfig {
  std::string hostname = "server";  // sv_hostname: the name in the browser and the lobby
  std::string description;          // sv_description: a line under the name
  std::string ip;                   // net_ip: the address to listen on (empty: every interface)
  int32_t port = 0;                 // net_port: the game's port (0: the game's default)
  int32_t maxclients = 8;           // sv_maxclients: 1..32
  std::string map;                  // sv_map (empty: the game's default)
  int32_t bots = 0;                 // sv_bots: slots the game's bots fill at the start
  uint32_t seed = 1;                // sv_seed: the run's seed
  std::string password;             // sv_password: joining takes it (the advert says locked)
  std::string visibility = "lan";   // sv_visibility: lan, invite or listed (net/hosting.hpp)

  double seconds = 60;      // server time to run (0: until stopped)
  double status_every = 5;  // a status line every this many seconds of server time (0: none)
  bool realtime = false;    // pace the frames to the wall clock
  bool offline = false;     // no socket: the game alone (a test, a lab, a run of bots)
  std::string log;          // write the run's input log here
  std::string replay;       // replay this input log instead of running
};

// What a replay found.
struct ServerReplay {
  int frames = 0;
  int first_diverged = -1;  // -1: every frame agrees
  uint64_t digest = 0;      // the last frame's hash
};

// A game's server, as functions over its own state (`void*`, made by `create`). Called once a
// frame, so the indirection costs nothing that matters; nothing is registered at run time.
struct ServerGame {
  const char* binary = "server";  // the executable's name, at the head of every line it prints
  const char* title = "a game";   // the game's name, for the banner and `--help`
  uint32_t protocol = 0;          // the LAN advert's protocol (net/packet.hpp's protocol_id)
  uint16_t default_port = 27500;
  uint16_t discovery_port = 0;    // the game's fixed LAN discovery port (0: no LAN answers)
  int max_clients = kMaxClients;  // the most `sv_maxclients` may be (at most kMaxClients)
  const char* default_map = "";

  // The game on `config`'s map with its bots; null with `error` (what and the next action).
  void* (*create)(const ServerConfig& config, std::string& error) = nullptr;
  void (*destroy)(void* game) = nullptr;
  // One frame's length, s (fixed for a run).
  double (*frame_seconds)(const void* game) = nullptr;
  void (*frame)(void* game) = nullptr;
  // Players and bots in the game's slots (what the LAN advert counts beside the session's seats).
  int (*seated)(const void* game) = nullptr;
  // The state's hash after the last frame (the digest).
  uint64_t (*hash)(const void* game) = nullptr;
  // The game's part of a status line (`players=8 commands=…`); may be null.
  std::string (*status)(const void* game) = nullptr;
  // The run's input log as bytes; false with `error` when there is none (may be null: no logs).
  bool (*save_log)(const void* game, std::vector<uint8_t>& bytes, std::string& error) = nullptr;
  // A log's frames run again; false with `error` when it cannot be read (may be null).
  bool (*replay)(const std::vector<uint8_t>& bytes, ServerReplay& out,
                 std::string& error) = nullptr;
};

struct Server {
  Server() = default;
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  ~Server();

  const ServerGame* game = nullptr;
  ServerConfig config;
  void* state = nullptr;  // the game's
  double frame_s = 0;     // the game's frame length
  int64_t frame = 0;      // frames run
  int64_t now_us = 0;     // the server's clock: `frame` frames of `frame_s`
  bool networked = false;
  net::Node node;
  net::SessionHost session;
  net::Hosting hosting;
};

// The settings' cvars bound to `config` (which must outlive `cvars`), its values the fallbacks;
// the game's limits on them (the most clients, the default port and map) from `game`.
void add_server_cvars(console::Cvars& cvars, ServerConfig& config, const ServerGame& game);

// The game created on `config`; with `transport` (the game's port) the node, the session host and
// the hosting round it, answering the LAN on `lan` (null: pings only). False with `error`.
bool server_open(Server& s, const ServerGame& game, const ServerConfig& config,
                 net::Transport* transport, net::Transport* lan, std::string& error);
// One frame: the game's, then the network at the frame's time.
void server_frame(Server& s);
// The game stopped and its listing taken down; the server may be opened again.
void server_close(Server& s);

// The game's hash after the last frame.
uint64_t server_digest(const Server& s);
// The server time run, s.
double server_time(const Server& s);
// The clock at the end of the next frame, us (a simulated network is advanced to it first).
int64_t server_next_us(const Server& s);
// `t=12.0 s frame=924 seated=8 clients=0 <the game's>`.
std::string server_status(const Server& s);
// What the server is: name, map, slots, address; the banner a dedicated server prints.
std::string server_banner(const Server& s);

}  // namespace ge::server
