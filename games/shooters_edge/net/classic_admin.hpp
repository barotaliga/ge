// THE ADMINISTRATOR'S WORDS ON THE CLASSIC ENDPOINT: who is on the server, by user id, and the
// easy jump granted to a chosen player (one who needs it, such as a player with a disability that
// makes strafing hard) while the server's own `pm_speedjump` stays as it is for everyone else.
//
// THE CONSOLE LINES (`classic_admin`; the window's console when it hosts, the server's own
// console lines, and `rcon` from afar when `rcon_password` is set):
// - `users`: one line a seat: the user id, the frags, the name; then how many. Any client may ask
//   it too (a client command), so a player can tell the administrator his id.
// - `status`: the server's name, map and players, then a row a seat: slot, user id, name, frags,
//   ping, loss, minutes on, address (`bot` for a bot, `local` for the window's own player) and the
//   easy jump (`-` not asked, `asks` asked with nothing allowed, the share when pm_speedjump allows
//   it, `<share> granted` when granted to him).
// - `speedjump <id|name> on|off|<share>`: grants the easy jump to one player (`on` is
//   `kGrantShare`, a number 0..1 the share itself) or takes it back; `speedjump` alone lists the
//   players it is granted to. A user id is tried first, then an exact name.
//
// THE GRANT is the server-only key `*speedjump <share>` in the player's user info (a client cannot
// set a `*` key; one sent in a connect is removed), so it is DISCLOSED: every client sees it in
// that player's info, and everyone is told when it is granted or taken back. It stays with the
// seat across map changes (the user info is replayed to each map's rules) and goes when the player
// leaves. The client's easy-jump aid reads its own `*speedjump` before the server's pm_speedjump,
// so the grant acts even where pm_speedjump is 0, and the player still turns it on himself with
// `cl_speedjump 1`.
//
// THE WINDOW'S CONSOLE (`add_admin_commands`): `users`, `status` and `speedjump` answer for the
// match the window hosts; joined to a server elsewhere, `users` asks that server (any client may)
// and `rcon <password> <line>` runs an administrator's line there (`rcon speedjump 3 on`).
//
// THE REMOTE CONSOLE (`classic_rcon`): `rcon <password> <line>` out of band; the line runs as the
// server console's would (these words first, then the rules' console: `set`, `map`, a cvar) and
// its answer returns as out-of-band prints. No password set: "Remote console is not offered by
// this server."; a wrong one: "Bad rcon_password." (and counted); more than `kRconTriesPerSecond`
// tries a second are not answered.
//
// ge:summary The administrator's words on the classic endpoint: users, status, the easy jump
// ge:summary granted per player, and the remote console
// ge:keywords users status user-id player-list speedjump easy-jump grant accessibility
// ge:keywords disabled-player rcon rcon_password remote-console admin classic_admin
// ge:keywords classic_speedjump classic_users classic_status_table classic_rcon kgrantshare
// ge:keywords add_admin_commands client_rcon window-console
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_admin_test.cpp
#pragma once

#include <string>

#include "shooters_edge/net/classic_server.hpp"

namespace ge::console {
class Console;
}
namespace shooters_edge::client {
struct Listen;
}

namespace shooters_edge::net {

struct Remote;

inline constexpr float kGrantShare = 0.5f;      // `speedjump <id> on`: the share granted
inline constexpr int kRconTriesPerSecond = 10;  // rcon tries answered a second, at most

// `users`: `userid frags name`, a seat a line, then `<n> total users`.
std::string classic_users(const ClassicServer& s);
// `status`: the server's line, then the table described above.
std::string classic_status_table(const ClassicServer& s);
// The seat a user id or else an exact name names; -1 when none.
int classic_find_seat(const ClassicServer& s, const std::string& id_or_name);
// The easy jump granted to the seat with user id `userid` at `share` (0..1), or taken back
// (`share` < 0); everyone told. False with `error` when no seat has that id.
bool classic_speedjump(ClassicServer& s, int userid, float share, std::string& error);
// The easy jump's share granted to `slot` (-1: none).
float classic_speedjump_granted(const ClassicServer& s, int slot);
// An administrator's console line: true when it was one of the words above, its answer in `out`.
bool classic_admin(ClassicServer& s, const std::string& line, std::string& out);
// A remote console packet's text (`rcon <password> <line>`), from `from`: answered by out-of-band
// prints.
void classic_rcon(ClassicServer& s, const gn::Address& from, const std::string& text);

// The window's `users`, `status`, `speedjump` and `rcon` (above), over the match `l` hosts or the
// server `r` joined.
void add_admin_commands(ge::console::Console& con, client::Listen& l, Remote& r);

}  // namespace shooters_edge::net
