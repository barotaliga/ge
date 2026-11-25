// THE ADMINISTRATOR'S WORDS ON QUAKE II'S SERVER (net/q2_net_server.hpp), in a modern server's
// words (porting/shooters-edge/q2-operator.md):
//
// - `status [d|l|p|s|t|v]`: `Current map: <map>`, then a table a slot (`No UDP clients.` when
// none):
//   by default its number, score, ping (or ZMBI, ASGN, DNLD, HTTP, CNCT), name, the milliseconds
//   since its last packet, address, rate, protocol and moves a second; `d` the downloads, `l` the
//   lag (loss both ways, round trip least, average and most, duplicates, time scale), `p` the
//   protocols, `s` the client settings, `t` idle and connected time, `v` the versions. Another
//   letter: `Usage: status [d|l|p|s|t|v]` and the default table.
// - A player is named by a slot number or an exact name; with `sv_enhanced_setplayer` also in any
//   case or by a unique part (engine/server/operator.hpp's `pick_player` and its answers).
// - `kick <id>` (`<name> was kicked` to him and everyone in the game), `kickban <id>` (his address
//   banned too, until the server stops), `dumpuser <id>` (his user info, then the misc block:
//   version, protocol, ping, round trips, loss, time scale, connected time), `stuff <id> <text>`,
//   `stuffall <text>`, `printall <text>` (level 2 to every client), `stuffcvar <id> <var>...`
//   (each answer on the console), the stuff lists (`addstuffcmd`, `delstuffcmd`, `liststuffcmds`),
//   `killserver` (`Server was killed.` to every client, nothing answered until the next map) and
//   `pickclient <address:port>` (`passive_connect` to a client waiting for a server).
// - The engine's address list (engine/server/bans.hpp): `addip`, `removeip`, `listip`, `writeip`
//   (to `ban_file`, in the game's folder, read back when the server starts); `sv_banmode` (the
//   Quake II's spelling: `filterban`) 1 refuses the list's addresses with "Banned.", 0 lets in only
//   them. A client already on the server whose address an `addip` comes to refuse is dropped.
//
// THE REMOTE CONSOLE (`q2_rcon`): `rcon <password> <line>` out of band. No `rcon_password`, or
// a wrong one: "Bad rcon_password." (either way), and an address the ban list refuses is not
// answered. The line is the words after the password joined by single spaces; it runs as the
// server console's would (these words first, then the rules' console: `fraglimit 30`, `map q2dm2`)
// and what it printed comes back as one `print`. The log keeps the sender and the line, never the
// password.
//
// ge:summary Quake II server's administrator words: status, kick, dumpuser, bans, remote console
// ge:keywords q2 admin rcon rcon_password remote-console status kick dumpuser addip removeip
// ge:keywords listip writeip filterban sv_banmode banned q2_admin q2_rcon
// ge:keywords q2_status_table q2_find_slot kickban stuff stuffall printall stuffcvar
// ge:keywords killserver pickclient passive_connect sv_enhanced_setplayer status-letters
// ge:keywords sv-addip svcmd filter-list q2_status_keys server-info-keys gamename gamedir
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_admin_test.cpp
#pragma once

#include <string>

#include "net/address.hpp"
#include "shooters_edge/net/q2_net_server.hpp"

namespace shooters_edge::net {

// `status`'s table (above).
std::string q2_status_table(const Q2NetServer& s, char letter = ' ');
// The slot a word names (a digit first: a slot number; else an exact name), or -1 with `error`
// the stock words for why.
int q2_find_slot(const Q2NetServer& s, const std::string& word, std::string& error);
// An administrator's console line: true when it was one of the words above, its answer in `out`.
// The server info's keys from the rules' live settings (`deathmatch`, `coop`, `dmflags`,
// `fraglimit`, `timelimit`, `cheats`), the build's (`version`, `gamename`, `gamedir`, `game` for a
// mod), the server's `port`, and the keys an administrator added (`sets`), added to `info`.
void q2_status_keys(const Q2NetServer& s, std::string& info);
bool q2_admin(Q2NetServer& s, const std::string& line, std::string& out);
// A remote console packet's text (`rcon <password> <line>`) from `from`, answered (above).
void q2_rcon(Q2NetServer& s, const gn::Address& from, const std::string& text);

}  // namespace shooters_edge::net
