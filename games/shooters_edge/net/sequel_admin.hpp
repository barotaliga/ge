// THE ADMINISTRATOR'S WORDS ON THE SEQUEL'S SERVER (net/sequel_net_server.hpp), as an operator of
// the sequel's stock server types them (porting/shooters-edge/sequel-server-inventory.md § 5):
//
// - `status`: the map, then a row a slot: its number, frags, ping (`CNCT` while it signs on),
//   name, the milliseconds since its last packet, its address and qport.
// - `kick <slot|name>`: a word starting with a digit is a slot, else the exact name. The client is
//   told "You were kicked from the game", everyone "<name> was kicked", and it is dropped; it may
//   come back. "Bad client slot: <n>", "Client <n> is not active", "Userid <name> is not on the
//   server" otherwise.
// - `dumpuser <slot|name>`: that client's user info, a key and value a line.
// - The engine's address list (engine/server/bans.hpp): `addip`, `removeip`, `listip`, `writeip`
//   (to `ban_file`, in the game's folder, read back when the server starts); `sv_banmode` (the
//   sequel's spelling: `filterban`) 1 refuses the list's addresses with "Banned.", 0 lets in only
//   them. A client already on the server whose address an `addip` comes to refuse is dropped.
//
// THE REMOTE CONSOLE (`sequel_rcon`): `rcon <password> <line>` out of band. No `rcon_password`, or
// a wrong one: "Bad rcon_password." (either way), and an address the ban list refuses is not
// answered. The line is the words after the password joined by single spaces; it runs as the
// server console's would (these words first, then the rules' console: `fraglimit 30`, `map q2dm2`)
// and what it printed comes back as one `print`. The log keeps the sender and the line, never the
// password.
//
// `status` reads every client's ping as 0: the endpoint does not measure one yet.
//
// ge:summary The sequel server's administrator words: status, kick, dumpuser, bans, remote console
// ge:keywords sequel admin rcon rcon_password remote-console status kick dumpuser addip removeip
// ge:keywords listip writeip filterban sv_banmode banned sequel_admin sequel_rcon
// ge:keywords sequel_status_table sequel_find_slot
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_admin_test.cpp
#pragma once

#include <string>

#include "net/address.hpp"
#include "shooters_edge/net/sequel_net_server.hpp"

namespace shooters_edge::net {

// `status`'s table (above).
std::string sequel_status_table(const SequelNetServer& s);
// The slot a word names (a digit first: a slot number; else an exact name), or -1 with `error`
// the stock words for why.
int sequel_find_slot(const SequelNetServer& s, const std::string& word, std::string& error);
// An administrator's console line: true when it was one of the words above, its answer in `out`.
bool sequel_admin(SequelNetServer& s, const std::string& line, std::string& out);
// A remote console packet's text (`rcon <password> <line>`) from `from`, answered (above).
void sequel_rcon(SequelNetServer& s, const gn::Address& from, const std::string& text);

}  // namespace shooters_edge::net
