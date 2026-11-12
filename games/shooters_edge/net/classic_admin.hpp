// THE ADMINISTRATOR'S WORDS ON THE CLASSIC ENDPOINT: who is on the server, by user id, and the
// player aids (engine/server/aids.hpp) granted to a chosen player (one who needs them, such as a
// player with a disability, or one on a phone's touch controls) while the server's own settings
// stay as they are for everyone else.
//
// THE AIDS (`classic_aids`, the game's table of them): `speedjump`, the easy jump (a share 0..1,
// `on` 0.8; the client strafes for him: `cl_speedjump 1`, plugins/shooter/speedjump_assist.hpp);
// `aimhelp`, the aim help (on or off; the server bends his hitscan shots by `sv_aimhelp_x` and
// `sv_aimhelp_y`, plugins/shooter/aim_help.hpp); `rjump`, the rocket jump helper (on or off;
// `+rjump` and `+rrjump`).
//
// THE CONSOLE LINES (`classic_admin`; the window's console when it hosts, the server's own
// console lines, and `rcon` from afar when `rcon_password` is set):
// - `users`: the engine's players list (aids.hpp `users_text`): one line a seat: the user id, the
//   frags, the name and the seat's aids, each with how it is allowed (granted, or on for all by a
//   server setting) and whether it is used (`cl_speedjump 1`, `3 shots helped`); then how many.
//   Any client may ask it too (a client command), so a player can tell the administrator his id.
// - `status`: the server's name, map and players, then a row a seat: slot, user id, name, frags,
//   ping, loss, minutes on, address (`bot` for a bot, `local` for the window's own player) and the
//   aids as `users` shows them.
// - `<aid> <id|name> on|off|<value>`: grants the aid to one player or takes it back; `<aid>`
//   alone lists the players it is granted to. A user id is tried first, then an exact name.
//
// A GRANT is the aid's server-only key (`*speedjump <share>`, `*aimhelp 1`, `*rjump 1`) in the
// player's user info (a client cannot set a `*` key; one sent in a connect is removed), so it is
// DISCLOSED: every client sees it in that player's info, everyone is told when it is granted or
// taken back, and a player who joins is told every grant on the server (`classic_tell_aids`). It
// stays with the seat across map changes (the user info is replayed to each map's rules) and goes
// when the player leaves. Players grant aids by vote too (`vote aimhelp <player> on`,
// classic_game.cpp).
//
// THE WINDOW'S CONSOLE (`add_admin_commands`): `users`, `status` and each aid's word answer for
// the match the window hosts; joined to a server elsewhere, `users` asks that server (any client
// may) and `rcon <password> <line>` runs an administrator's line there (`rcon aimhelp 3 on`).
//
// THE REMOTE CONSOLE (`classic_rcon`): `rcon <password> <line>` out of band; the line runs as the
// server console's would (these words first, then the rules' console: `set`, `map`, a cvar) and
// its answer returns as out-of-band prints. No password set: "Remote console is not offered by
// this server."; a wrong one: "Bad rcon_password." (and counted); more than `rcon_rate`
// (sv_rconrate) tries a second are not answered.
// THE HASHED PASSWORD: in place of the password a client may send 40 hex digits of the SHA-1 of
// "rcon " + the password + the time's hex digits + " " + each word of the line and a space, then
// the time's hex digits: at least 16, the 8 bytes of its Unix time least significant first. It is
// taken when the time is within `rcon_time_s` of the server's clock; with `rcon_crypt`
// (sv_rconcrypt) only it is taken. `rcon_digest_field` makes one.
//
// ge:summary The administrator's words on the classic endpoint: users, status, the easy jump
// ge:summary granted per player, and the remote console
// ge:keywords users status user-id player-list speedjump easy-jump grant accessibility
// ge:keywords disabled-player rcon rcon_password remote-console admin classic_admin
// ge:keywords classic_speedjump classic_users classic_status_table classic_rcon classic_grant
// ge:keywords classic_aids classic_tell_aids aimhelp aim-help rjump player-aids join-line
// ge:keywords add_admin_commands client_rcon window-console
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_admin_test.cpp
#pragma once

#include <span>
#include <string>

#include "server/aids.hpp"
#include "shooters_edge/net/classic_server.hpp"

namespace ge::console {
class Console;
}
namespace shooters_edge::client {
struct Listen;
}

namespace shooters_edge::net {

struct Remote;

inline constexpr float kGrantShare =
    0.8f;  // `speedjump <id> on`: the share granted (the stock share)

// The game's player aids: speedjump, aimhelp, rjump (above).
std::span<const ge::server::AidKind> classic_aids();

// `users`: the players list (aids.hpp `users_text`).
std::string classic_users(const ClassicServer& s);
// `status`: the server's line, then the table described above.
std::string classic_status_table(const ClassicServer& s);
// The seat a user id or else an exact name names; -1 when none.
int classic_find_seat(const ClassicServer& s, const std::string& id_or_name);
// The aid `kind` granted to the seat with user id `userid` at `value` (aids.hpp `aid_value`), or
// taken back (`value` empty); everyone told. False with `error` when no seat has that id.
bool classic_grant(ClassicServer& s, const ge::server::AidKind& kind, int userid,
                   const std::string& value, std::string& error);
// The value of `kind` granted to `slot`; empty: none.
std::string classic_aid_granted(const ClassicServer& s, int slot, const ge::server::AidKind& kind);
// The easy jump granted at `share` (0..1), or taken back (`share` < 0): `classic_grant`'s.
bool classic_speedjump(ClassicServer& s, int userid, float share, std::string& error);
// The easy jump's share granted to `slot` (-1: none).
float classic_speedjump_granted(const ClassicServer& s, int slot);
// The line a player who joins is told (aids.hpp `aids_join_line`); empty when nobody has an aid.
std::string classic_aids_join_line(const ClassicServer& s);
// That line told to `slot` (nothing when it is empty).
void classic_tell_aids(ClassicServer& s, int slot);
// An administrator's console line: true when it was one of the words above, its answer in `out`.
bool classic_admin(ClassicServer& s, const std::string& line, std::string& out);
// A remote console packet's text (`rcon <password> <line>`), from `from`: answered by out-of-band
// prints.
void classic_rcon(ClassicServer& s, const gn::Address& from, const std::string& text);
// The hashed password field for `line` sent with `password` at Unix time `now` (above).
std::string rcon_digest_field(const std::string& password, const std::string& line, int64_t now);

// The window's `users`, `status`, `speedjump` and `rcon` (above), over the match `l` hosts or the
// server `r` joined.
void add_admin_commands(ge::console::Console& con, client::Listen& l, Remote& r);

}  // namespace shooters_edge::net
