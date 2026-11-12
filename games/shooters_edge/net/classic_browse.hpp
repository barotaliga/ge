// THE CLASSIC STATUS QUERY AND THE MASTERS, as the engine's server list reads them
// (net/server_list.hpp): `classic_query()` is the four functions a server browser hands it, so
// the same browser lists this game's own servers and the classic servers already running on the
// internet, whichever program serves them.
//
// - THE STATUS QUERY is a connectionless packet, `\xff\xff\xff\xff` then `status 23\n` (23 asks
//   for the server info, the players, the spectators and each player's team). The reply is
//   `\xff\xff\xff\xff` `n`, the server info as `\key\value` pairs on one line, then one line a
//   player: `userid frags minutes ping "name" "skin" top bottom "team"`, a spectator's name
//   starting `\s\` (its frags `-9999` or `S`, its ping negative on some servers).
// - THE INFO READ into the engine's words: `hostname` the name, `map`, `maxclients` and
//   `maxspectators` the slots, `*gamedir` the game ("" when absent: the base game), `needpass` bit
//   1 a password, `skill`, a match mod's `status` the match's state. The MODE is the server's
//   `mode` key when it has one; else `coop` when `coop` is set, `team` when `deathmatch` and
//   `teamplay` are, `ffa` when `deathmatch` alone is, `single` when neither.
// - THE MASTER QUERY is `c\n` (no header); the reply is `\xff\xff\xff\xff` `d\n` then six bytes a
//   server, its IPv4 address and port in network order. A master with many servers sends several
//   replies; each is read as it comes.
// - THE REMOTE CONSOLE is a connectionless `rcon <password> <command>` (the password quoted when
//   it holds a space); a server that takes it answers with prints (`\xff\xff\xff\xff` `n` then
//   the console's text), as many as the output needs.
//
// ge:summary The classic status query and master list for the engine's server list
// ge:keywords classic status-query master-server server-browser serverinfo hostname maxclients
// ge:keywords needpass skill coop deathmatch teamplay mode players spectators team masters
// ge:keywords rcon remote-console classic_rcon classic_read_print
// ge:keywords pingstatus forwarder route-finder classic_route_query
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_browse_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "forward/routes.hpp"
#include "net/server_list.hpp"

namespace shooters_edge::net {

// The masters a game folder's own browser lists: `<basedir>/<folder>/sb/sources.txt`, one source
// a line, `master "<name>" <host>:<port>` for a master (other kinds and `//` lines skipped). The
// tree names no master of its own: the player's `sb_masters` and the install's lists are asked.
std::vector<std::string> classic_masters_in(const std::string& text);
// Every such list in the install at `basedir` (each folder's `sb/sources.txt`), masters once each.
std::vector<std::string> classic_install_masters(const std::string& basedir);
// The ports a LAN's servers are looked for on (the broadcast goes to each).
inline constexpr uint16_t kLanFirstPort = 27500, kLanLastPort = 27505;

size_t classic_status_query(uint8_t* out, size_t cap);
bool classic_read_status(const uint8_t* data, size_t size, ge::net::ServerInfo& out);
size_t classic_master_query(uint8_t* out, size_t cap);
bool classic_read_master(const uint8_t* data, size_t size, std::vector<ge::net::Address>& out);

// The remote console's request (0 when it does not fit `cap`), and a print's text (false: not a
// print).
size_t classic_rcon(std::string_view password, std::string_view command, uint8_t* out, size_t cap);
bool classic_read_print(const uint8_t* data, size_t size, std::string& text);

// The four together.
ge::net::ServerQuery classic_query();

// THE FORWARDERS' LISTS, for the route finder (engine/forward/routes.hpp): a classic forwarder is
// a server whose `*version` starts `qwfwd`; asked `pingstatus` (the header and the word, no
// newline) it answers the header, `n`, then eight bytes a server: its IPv4 address in order, its
// port and its ping in milliseconds each low byte first (65535: no answer yet).
bool classic_is_forwarder(const ge::net::ServerInfo& info);
size_t classic_pingstatus_query(uint8_t* out, size_t cap);
bool classic_read_pingstatus(const uint8_t* data, size_t size,
                             std::vector<std::pair<ge::net::Address, int>>& out);
// The three together.
ge::forward::RouteQuery classic_route_query();

// The mode a server's info says (the rule above).
std::string classic_mode(const std::vector<std::pair<std::string, std::string>>& keys);

}  // namespace shooters_edge::net
