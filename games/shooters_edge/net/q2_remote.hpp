// THE WINDOW JOINED TO A SERVER OF QUAKE II: the `shooters_edge` window, with Quake II loaded,
// playing on a server of Quake II's protocol (`connect <host>[:port]`, port 27910 when none):
// a stock server of Quake II running its own game or a mod, or ours (`sesv -classic q2`).
//
// HOW IT IS MADE: the protocol is the scripted Quake II client's (q2_client.hpp) driven by the
// window's hands, reading straight into the window's drawn recording (`Remote::demo`), so the
// window draws a game it plays as it draws a Quake II recording it watches. Everything a frame of
// the window does not ask for is the QuakeWorld join's (qw_remote.hpp): the tick clock, the
// console's `connect`, `disconnect`, `reconnect` and `netstatus`, the stages said on the console.
//
// THE MOVE RATE: Quake II's own `cl_maxfps` (`Remote::q2_maxfps`, 62 by default; 0 the top, 125;
// at least 10), as whole milliseconds between moves (`q2_move_ms`: 62 a second is 16 ms apart).
// THE HANDS: a move a tick with the window's view angles (`+use` as Quake II's use button, the
// buttons byte's second bit). Quake II's game draws the view as those angles plus the turn the
// server holds for the player (its delta angles: a spawn's facing, a teleport's), so the drawn view
// is the hands' plus that turn, never a tick behind. A weapon key
// (`impulse 1` .. `impulse 11`: the weapon groups and the quick weapon ask the same way) goes as
// Quake II's `use <weapon>` command, which a stock server takes and an impulse it does not.
//
// BETWEEN FRAMES: the server sends 10 frames a second; the other entities, the other players and
// the player himself are drawn between the last two frames read, by the window's clock against
// when each frame came (one frame behind the newest), so nothing steps at 10 Hz.
// THE EASY JUMP: with `cl_speedjump 1` the user info says so and the hands strafe for the player
// as on Quake's servers (shooter/speedjump_assist.hpp), off the newest prediction, by the
// Quake II's air law at the air acceleration the server states; at the share our server tells
// (`*ge_ext` bit 16, svc 126; its `pm_speedjump`, 0 the aid off), else `kStockShare`.
//
// ge:summary The window joined to a server of Quake II: its client driven by the hands, drawn
// ge:summary as a Quake II recording, its frames drawn in between
// ge:keywords remote-client connect join-server q2 protocol-34 q2-join delta-angles
// ge:keywords use-weapon impulse lerp q2_remote_start q2_remote_step
// ge:keywords cl_speedjump easy-jump speedjump-share cl_maxfps move-rate q2_move_ms
// ge:keywords download add_q2_download_command skins add_q2_skins_command userinfo user-info
// ge:keywords q2_user_info_moved setinfo rename name-change
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

// Milliseconds between Quake II's moves at `cl_maxfps` `rate` moves a second: 1000 over the rate
// held to 10..125 (0: 125), rounded down.
int q2_move_ms(int rate);
// Quake II's client started over `r.transport` to `r.server` with the player's settings.
void q2_remote_start(Remote& r, const ClientConfig& cfg);
// THE USER INFO KEPT UP TO DATE: the player's settings (`setting`, the console's value by name;
// "" unset) as Quake II's user info: name, skin (as at the join: `skin` with a slash, else
// `model`/`skin`, else male/grunt), hand, fov (cl_fov rounded; 0 its 90), msg, gender (picked from
// the figure under gender_auto 1), rate and password. True, `q` changed, when any moved: the
// window sends the user info again so a joined server sees a new name or skin at once.
bool q2_user_info_moved(Q2ClientConfig& q,
                        const std::function<std::string(const std::string&)>& setting);
// One window frame of `wall_s` s (qw_remote.hpp's `remote_step` for a server of Quake II).
RemoteStep q2_remote_step(Remote& r, double wall_s, std::vector<std::string>& said);
// `download <file>`: a file asked of the server joined (q2_client.hpp's `q2_download`), saying
// "Must be connected to a server." when none is, "Downloads are permanently disabled." with
// `allow_download -1`, "Usage: download <filename>" without exactly one file, and "Couldn't
// download <file>: <reason>" when it is refused.
void add_q2_download_command(ge::console::Console& con, Remote& r);
// `skins`, every game's: every player's figure and skin read again ("Must be in a level to load
// skins." when none is joined), a line each, "client <n>: <name>\<look> --> <look>" (Quake II's
// model/skin, QuakeWorld's skin).
void add_q2_skins_command(ge::console::Console& con, Remote& r);

}  // namespace shooters_edge::net
