// THE SERVER LIST: what a game's server browser reads when its servers answer a STATUS QUERY of
// the game's own protocol (a game whose servers are not the engine's sessions; those are
// directory.hpp's). The game hands it its protocol as a table of four functions (`ServerQuery`):
// the status query's bytes, the reply read into a `ServerInfo`, and, when the game has masters,
// the master's query and its reply read into addresses. The list does the rest, the same for
// every game:
//
// - ONE REFRESH gathers servers from three places: the LAN (the status query broadcast to each of
//   `lan`, a broadcast address on a port the game's servers listen on; whoever answers inside
//   `lan_wait_us` is found), the MASTERS (each asked for its list, again every `master_timeout_us`
//   up to `master_tries` times; every address any master gives is queried), and the servers the
//   player named (`servers`, or `server_list_add` for one typed in).
// - EVERY SERVER IS QUERIED: the status query sent straight to it, `tries` times at most, again
//   when no reply came in `timeout_us`. A reply is matched to its server by the address it came
//   from; the time from the query's send to the update that read its reply is the server's PING
//   (so it includes the wait for the caller's next frame: a browser updates every frame). The
//   sends are paced, one every `gap_us` round the whole list, so a refresh of a master's thousand
//   servers does not burst.
// - A VIEW is the rows that pass a filter (full, empty, unanswered, a ping bound, a part of the
//   name, the map or the mode), sorted by ping, players, name, map or mode, ties broken by name
//   and then the address, so the same entries always give the same rows.
//
//   server_list_init(l, config, query, transport)   a socket of the browser's own
//   server_list_refresh(l, now)                      when the browser opens, or the player asks
//   every frame: server_list_update(l, now)          replies read, queries sent
//   server_list_view(l, filter, sort, rows)          the rows to draw
//
// The clock is the caller's: a refresh over the simulated network replays to the bit.
//
// ge:summary The server list: status queries to the LAN, the masters and named servers, pinged
// ge:keywords net server-list server-browser status-query master-server masters lan broadcast
// ge:keywords ping refresh filter sort full empty mode map players slots skill join favourites
// ge:skill net
// ge:test tests/net_server_list_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "net/address.hpp"
#include "net/transport.hpp"

namespace ge::net {

// One player a server's reply lists.
struct ServerPlayer {
  std::string name, team;
  int frags = 0;
  int ping_ms = 0;
  bool spectator = false;
};

// What a server says about itself, in the engine's words; the game's reader fills what its
// protocol carries and leaves the rest.
struct ServerInfo {
  std::string name;   // the host's name for it
  std::string map;    // the map it is on
  std::string mode;   // the kind of game: "ffa", "team", "coop", "duel", a mod's own word
  std::string game;   // the game or mod folder it runs
  std::string state;  // what the match is doing, when it says ("Countdown", "Standby")
  int players = 0, max_players = 0;
  int spectators = 0, max_spectators = 0;
  int skill = -1;       // the bots' or the monsters' skill; -1: not said
  bool locked = false;  // a password is needed
  std::vector<ServerPlayer> roster;
  std::vector<std::pair<std::string, std::string>> keys;  // every key it sent, in its order
};

// THE GAME'S PROTOCOL. Each writer returns the bytes written (at most `cap`); each reader returns
// false for a datagram that is not its reply.
struct ServerQuery {
  size_t (*status)(uint8_t* out, size_t cap) = nullptr;
  bool (*read_status)(const uint8_t* data, size_t size, ServerInfo& out) = nullptr;
  size_t (*master)(uint8_t* out, size_t cap) = nullptr;  // null: the game has no masters
  bool (*read_master)(const uint8_t* data, size_t size, std::vector<Address>& out) = nullptr;
};

struct ServerListConfig {
  std::vector<Address> lan;      // broadcast addresses to query (one per port servers use)
  std::vector<Address> masters;  // the masters to ask for their lists
  std::vector<Address> servers;  // servers the player named (favourites)
  int64_t lan_wait_us = 600000;  // LAN replies taken this long after the broadcast
  int master_tries = 3;
  int64_t master_timeout_us = 1000000;
  int tries = 3;                // queries a server gets before it is given up
  int64_t timeout_us = 800000;  // a query not answered by then is asked again
  int64_t gap_us = 4000;        // queries sent this far apart, round the whole list
  size_t most = 2048;           // servers kept (a master's list beyond is dropped)
};

// ServerEntry::from.
constexpr uint8_t kServerFromLan = 1, kServerFromMaster = 2, kServerFromNamed = 4;

struct ServerEntry {
  Address address;
  uint8_t from = 0;
  bool answered = false;
  ServerInfo info;
  int ping_ms = -1;  // -1: no reply yet
  int tries = 0;
  int64_t asked_us = -1;  // the last query's send; -1 never
};

struct ServerListStats {
  uint64_t queries = 0, replies = 0;
  uint64_t master_asks = 0, master_replies = 0, master_addresses = 0;
  uint64_t dropped = 0;  // addresses past `most`
  uint64_t foreign = 0;  // anything else on the socket
};

struct ServerList {
  ServerListConfig config;
  ServerQuery query;
  Transport* transport = nullptr;
  int64_t refreshed_us = -1;
  int64_t lan_sent_us = -1;
  struct Master {
    int tries = 0;
    int64_t asked_us = -1;
    bool answered = false;
  };
  std::vector<Master> masters;  // per config.masters
  int64_t next_send_us = 0;
  std::vector<ServerEntry> entries;
  ServerListStats stats;
  std::vector<uint8_t> buffer;
};

void server_list_init(ServerList& l, const ServerListConfig& config, const ServerQuery& query,
                      Transport& transport);
// Forget the last refresh's servers and start again: the broadcast, the masters, the named.
void server_list_refresh(ServerList& l, int64_t now_us);
void server_list_update(ServerList& l, int64_t now_us);
// One server more, queried from the next update (an address typed in); its index.
int server_list_add(ServerList& l, const Address& address, uint8_t from, int64_t now_us);
// Every reply this refresh will get is in: the LAN's window passed, every master answered or
// given up, every server answered or given up.
bool server_list_done(const ServerList& l, int64_t now_us);
// Servers answered so far, and servers known (answered or not).
int server_list_answered(const ServerList& l);

struct ServerFilter {
  bool hide_full = false, hide_empty = false;
  bool hide_silent = true;  // servers that have not answered
  int max_ping_ms = 0;      // 0: any
  std::string name_has;     // a part of the name, the map or the mode (ASCII case folded)
  std::string mode;         // "": every mode
  uint8_t from = kServerFromLan | kServerFromMaster | kServerFromNamed;
};
enum class ServerSort : uint8_t { ping, players, name, map, mode };
const char* server_sort_name(ServerSort s);

// The entries (their indices) the filter passes, in the sort's order: ping lowest first and the
// silent last; players most first; name, map and mode A to Z.
void server_list_view(const ServerList& l, const ServerFilter& filter, ServerSort sort,
                      std::vector<int>& rows);

}  // namespace ge::net
