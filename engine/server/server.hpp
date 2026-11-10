// THE SERVER CORE: one multiplayer game's server, the same for every game on the engine and for
// both ways to run it (docs/servers.md). A game plugs its rules in through `ServerGame`, a table of
// functions; `Server` runs them on a fixed frame and, when it has a socket, owns the network round
// them: the node on the game's port, the session host (seats, chat, kick), and the hosting
// (net/hosting.hpp: the LAN's answers and pings, by the server's name).
//
// - A DEDICATED server is a `Server` stepped by `server_main` (main.hpp), the game's own
//   `<game>_server` binary (`ge_add_server`, cmake/Server.cmake).
// - A game with its own protocol (`ServerGame::serve`) gets the port itself and answers its own
//   clients on it; the frame clock and the settings are still the core's.
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
// `sv_lobby` (the engine's lobby, match.hpp, before every match) and its timers `sv_countdown`
// and `sv_intermission`, `sv_seed`, `sv_password`,
// `sv_visibility`; and those kept for what reads them as it lands:
// `sv_motd`, `sv_admin`, `sv_maprotation`, `sv_maxspectators`, `sv_spectatorpassword`,
// `sv_rendezvous`, `sv_mod` (read-only: the mods the server was started with), `rcon_password`
// (the remote console's password; empty, a game offers none). The guards a game's
// endpoint holds its clients to: `sv_timeout` (a client nothing was heard from for that long is
// dropped: a dead link, not an idle player, who still sends moves) and `sv_floodprot`,
// `sv_floodprotmsg` (the chat flood rule, guards.hpp). Players' votes on the settings for a map
// (vote.hpp): `sv_vote`, `sv_vote_time`, `sv_vote_wait`, `sv_vote_allow`, `sv_vote_deny`, read by
// the game that runs them (`vote_settings`). A dedicated server
// reads them from the game directories' `server.cfg` in layers and its command line (config.hpp,
// main.hpp).
//
// ge:summary The server core: ServerGame's table, the frame, the node, session and hosting round it
// ge:keywords server dedicated-server listen-server server-core servergame server_frame
// ge:keywords server_open hostname description port listen-address maxclients bots seed
// ge:keywords sv_hostname sv_description net_ip net_port sv_maxclients sv_map sv_bots sv_seed
// ge:keywords sv_lobby sv_timeout sv_floodprot sv_floodprotmsg client-timeout sv_countdown
// ge:keywords sv_intermission intermission sv_vote sv_vote_time sv_vote_wait sv_vote_allow
// ge:keywords sv_vote_deny vote_settings
// ge:keywords sv_password sv_visibility sv_motd sv_admin sv_maprotation sv_maxspectators
// ge:keywords sv_spectatorpassword sv_rendezvous sv_mod lan-advert multiplayer hosting
// ge:keywords rcon_password remote-console
// ge:skill net
// ge:test tests/server_core_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "console/cvar.hpp"
#include "net/hosting.hpp"
#include "net/node.hpp"
#include "net/session.hpp"
#include "net/transport.hpp"
#include "server/vote.hpp"

namespace ge::server {

constexpr int kMaxClients = 32;

// Everything a server is started with. The first block are cvars (`add_server_cvars`); the second
// only how a dedicated process runs (main.hpp's flags).
struct ServerConfig {
  std::string hostname = "server";   // sv_hostname: the name in the browser and the lobby
  std::string description;           // sv_description: a line under the name
  std::string ip;                    // net_ip: the address to listen on (empty: every interface)
  int32_t port = 0;                  // net_port: the game's port (0: the game's default)
  int32_t maxclients = 8;            // sv_maxclients: 1..32
  std::string map;                   // sv_map (empty: the game's default)
  int32_t bots = 0;                  // sv_bots: slots the game's bots fill at the start
  int32_t lobby = 0;                 // sv_lobby: 1 warms up every map until the lobby's match
  uint32_t seed = 1;                 // sv_seed: the run's seed
  std::string password;              // sv_password: joining takes it (the advert says locked)
  std::string visibility = "lan";    // sv_visibility: lan, invite or listed (net/hosting.hpp)
  std::string motd;                  // sv_motd: the message a player sees on joining
  std::string admin;                 // sv_admin: how to reach the server's owner
  std::string maprotation;           // sv_maprotation: the maps after the first, space-separated
  int32_t max_spectators = 0;        // sv_maxspectators: spectators beside the players
  std::string spectator_password;    // sv_spectatorpassword: watching takes it
  std::string rcon_password;         // rcon_password: the remote console's (empty: none offered)
  std::string rendezvous;            // sv_rendezvous: the rendezvous to list with (M17r)
  std::string mod;                   // sv_mod: the mods' directories, lowest first (read-only)
  double timeout_s = 65;             // sv_timeout: a client silent this long is dropped
  double countdown_s = 5;            // sv_countdown: everyone ready to the match, s (sv_lobby)
  double intermission_s = 0;         // sv_intermission: a match's end to the next map, s
  std::string floodprot = "10 1 1";  // sv_floodprot: "N S T" chat lines (guards.hpp)
  std::string floodprot_msg;         // sv_floodprotmsg: what a flooder is told ("": the silence)
  int32_t vote = 1;         // sv_vote: 1 lets players vote on settings for a map (vote.hpp)
  double vote_time_s = 30;  // sv_vote_time: how long a vote stays open, s
  double vote_wait_s = 30;  // sv_vote_wait: a failed vote's caller waits this long, s
  std::string vote_allow;   // sv_vote_allow: settings that may be voted on (empty: all)
  std::string vote_deny = kVoteDenyDefault;  // sv_vote_deny: settings that never may
  std::string root = ".";  // where the game directories are (`--root`): maps, cfgs
  // The game's own rules (ServerGame::rules: `fraglimit`, `timelimit`) by name, as set; empty is
  // unset, the game's own default.
  std::map<std::string, std::string> rules;

  double seconds = 60;         // server time to run (0: until stopped)
  double status_every = 5;     // a status line every this many seconds of server time (0: none)
  bool realtime = false;       // pace the frames to the wall clock
  bool offline = false;        // no socket: the game alone (a test, a lab, a run of bots)
  std::string log;             // write the run's input log here
  std::string replay;          // replay this input log instead of running
  bool explain = false;        // print every server value with its source instead of running
  std::string explain_prefix;  // only the cvars starting with this
  // A LISTEN server's content, prepared by the game's own client in its process (the program, the
  // map it already read, its player): the game's own type, read by its `create`; null for a
  // dedicated server, which makes everything from the settings above.
  void* host = nullptr;
};

// What a replay found.
struct ServerReplay {
  int frames = 0;
  int first_diverged = -1;  // -1: every frame agrees
  uint64_t digest = 0;      // the last frame's hash
};

// A setting the game's own rules read (`fraglimit`, `timelimit`): a server cvar beside the core's,
// set like them (every server.cfg layer, `+set`, the console; `--explain` shows where), and handed
// to `create` in `ServerConfig::rules`. Its fallback is empty: unset leaves the game's own default.
struct GameRule {
  const char* name;
  const char* help;  // what it does, with its unit
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
  const char* base_dir = "base";    // the base game directory, whose server.cfg is read first
  const GameRule* rules = nullptr;  // the game's own rules, `rule_count` of them
  int rule_count = 0;

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

  // A game that SPEAKS ITS OWN PROTOCOL on its port (a classic endpoint): `serve` is handed the
  // game's socket when the server opens with one, and the core opens no node, session or LAN
  // hosting on it; each frame is then `net_frame` at the frame's start time (the endpoint reads its
  // datagrams, runs the game's frame with the players' moves and answers), and `frame` only when
  // offline. Both null: the core's own node, session and hosting round `frame`.
  bool (*serve)(void* game, net::Transport& port, std::string& error) = nullptr;
  void (*net_frame)(void* game, int64_t now_us) = nullptr;
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
  bool own_protocol = false;       // the game answers its own clients on `port` (ServerGame::serve)
  net::Transport* port = nullptr;  // the game's socket (null offline)
  net::Node node;
  net::SessionHost session;
  net::Hosting hosting;
};

// The settings' cvars bound to `config` (which must outlive `cvars`), its values the fallbacks;
// the game's limits on them (the most clients, the default port and map) from `game`.
void add_server_cvars(console::Cvars& cvars, ServerConfig& config, const ServerGame& game);

// The vote's settings from the server's (`sv_vote`, `sv_vote_time`, `sv_vote_wait`,
// `sv_vote_allow`, `sv_vote_deny`).
VoteSettings vote_settings(const ServerConfig& config);

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
