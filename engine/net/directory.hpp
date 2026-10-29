// THE SERVER DIRECTORY: what a game's server browser reads. One refresh gathers the games of one
// protocol from two places and shows each game once:
//
// - the LAN: a probe broadcast on the discovery port (`lan_probes` times, for a LAN's own loss),
//   answered by every host there (discovery.hpp);
// - the internet: the rendezvous's list (rendezvous.hpp) of the sessions their hosts advertised
//   as listed (hosting.hpp): its first page says how many there are, the rest are asked at once,
//   and the missing ones again every `ask_us`.
//
// A game found both ways (a host on the player's own LAN that is also listed) is one entry: the
// advert carries the session's id at the rendezvous, and the entry keeps both addresses. Then
// every entry is PINGED: the discovery probe sent straight to its session address (its LAN
// address when it has one), which a hosting node answers on its own socket, `pings` times a
// refresh, round the list one entry every `ping_gap_us` so a refresh of a long list does not
// burst and a host that never answers holds nobody up; the lowest round trip is the entry's ping. A
// host behind a NAT that lets no stranger in (and forwards no port) cannot be pinged: its ping
// stays unknown (-1), and joining it goes through the rendezvous (reach.hpp), which
// `directory_route` says.
//
// The game's browser asks for a VIEW: the rows that pass a filter (full, empty, playing, locked,
// a ping above a bound, a part of the name, where it was found), sorted by ping, by players or by
// name, ties broken by name and then the session id, so the same entries always give the same
// rows.
//
//   directory_init(d, config, transport)      a socket of the browser's own (a client's)
//   directory_refresh(d, now)                 when the browser opens, or the player asks again
//   every frame: directory_update(d, now)     answers read, pages asked, pings sent
//   directory_view(d, filter, sort, rows)     the rows to draw
//
// The clock is the caller's and the nonces come off `seed`: a refresh over the simulated network
// replays to the bit (the `net-host` lab).
//
// ge:summary The server directory: the LAN's and the rendezvous's games, merged, pinged, filtered
// ge:keywords net directory server-browser server-list browse find-game lan internet master-server
// ge:keywords listing ping filter sort full empty locked password refresh join-route
// ge:skill net
// ge:test tests/net_directory_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/prng.hpp"
#include "net/address.hpp"
#include "net/discovery.hpp"
#include "net/transport.hpp"

namespace ge::net {

struct DirectoryConfig {
  uint32_t protocol = 0;
  Address lan;                    // the broadcast on the discovery port (none: no LAN)
  Address rendezvous;             // the list's server (none: no internet list)
  int lan_probes = 3;             // LAN probes a refresh (a LAN loses packets too)
  int64_t lan_every_us = 150000;  // this far apart
  int64_t lan_wait_us = 500000;   // LAN answers taken this long after the first
  // Pages not answered are asked again this often, this many rounds before the list is given up:
  // asks a few hundred ms apart, so a burst of loss (counted in packets) does not outlast them.
  int64_t ask_us = 250000;
  int asks = 16;
  int pings = 3;  // pings an entry gets a refresh (at most kMaxPings); the lowest is its ping
  int64_t ping_gap_us = 20000;        // pings sent this far apart, across the whole list
  int64_t ping_timeout_us = 1000000;  // a ping not answered by then is lost
  uint64_t seed = 0;                  // the nonces
};

constexpr int kMaxPings = 4;

// DirectoryEntry::from.
constexpr uint8_t kFoundLan = 1, kFoundListed = 2;

struct DirectoryEntry {
  uint8_t from = 0;  // kFoundLan | kFoundListed
  Advert advert;     // the freshest (the LAN's over the list's)
  Address lan;       // its session's LAN address (from & kFoundLan)
  Address internet;  // its public address as listed (from & kFoundListed)
  int ping_ms = -1;  // the lowest round trip this refresh; -1: none answered
  int pings_sent = 0, pings_answered = 0;
  uint64_t ping_nonce[kMaxPings] = {};  // each ping's
  int64_t ping_at_us[kMaxPings] = {};   // when each left; 0: answered or lost
  int64_t seen_us = 0;
};

struct DirectoryStats {
  uint64_t probes = 0, adverts = 0;  // LAN probes sent, LAN adverts taken
  uint64_t asks = 0, pages = 0;      // list pages asked (again included) and answered
  uint64_t pings = 0, pongs = 0;     // pings sent and answered
  uint64_t stale = 0;                // answers to a ping already timed out or a past refresh
  uint64_t foreign = 0;              // anything else on the socket
};

struct Directory {
  DirectoryConfig config;
  Transport* transport = nullptr;
  core::Rng rng{0};
  uint64_t nonce = 0;  // this refresh's LAN probe's
  int64_t refreshed_us = -1;
  int lan_sent = 0;                // LAN probes sent this refresh
  int pages = 0;                   // the list's pages, once page 0 has said (0: not yet)
  std::vector<uint8_t> page_have;  // per page: answered
  int page_tries = 0;              // rounds of asks since the last new page count
  int64_t page_asked_us = 0;
  bool list_done = false;  // every page in, or given up
  int64_t next_ping_us = 0;
  std::vector<DirectoryEntry> entries;
  DirectoryStats stats;
  uint8_t buffer[kMaxDatagram];
};

void directory_init(Directory& d, const DirectoryConfig& config, Transport& transport);
// Forget the last refresh's games and start again: the LAN probe and the list's first page.
void directory_refresh(Directory& d, int64_t now_us);
void directory_update(Directory& d, int64_t now_us);
// Every answer this refresh will get is in: the LAN's window passed, the list read (or given
// up), every entry pinged and every ping answered or lost.
bool directory_done(const Directory& d, int64_t now_us);

struct DirectoryFilter {
  bool hide_full = false, hide_empty = false, hide_playing = false, hide_locked = false;
  int max_ping_ms = 0;                      // 0: any; else an unpinged game is hidden too
  const char* name_has = nullptr;           // a part of the name, as typed (ASCII case folded)
  uint8_t from = kFoundLan | kFoundListed;  // shown when found any of these ways
};
enum class DirectorySort : uint8_t { ping, players, name };
const char* directory_sort_name(DirectorySort s);

// The entries (their indices) the filter passes, in the sort's order: ping lowest first and the
// unpinged last; players most first; name A to Z.
void directory_view(const Directory& d, const DirectoryFilter& filter, DirectorySort sort,
                    std::vector<int>& rows);

// How to join an entry: straight to its LAN address, or through the rendezvous by the session's
// id (reach.hpp), which crosses the NATs on the way.
struct JoinRoute {
  bool lan = false;
  Address address;       // lan: the session's address
  uint64_t session = 0;  // !lan: the id to reach
};
JoinRoute directory_route(const DirectoryEntry& e);

}  // namespace ge::net
