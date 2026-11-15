// SPECTATORS ON THE SERVER ITSELF (docs/servers.md § Spectators on the server): the server's own
// TV node, so a match is watched from the server that plays it with no TV process beside it. The
// server's recording (record.hpp) feeds the node as it is written; the node holds every block back
// by `sv_tvdelay` seconds of game time and fans it out to up to `sv_tvmaxviewers` viewers on
// `sv_tvport` (engine/tv: a viewer, a relay, a browser's page, the hub's Watch). A viewer who comes
// late starts at the newest keyframe out, and watches any player's view through the game's watch
// hooks (playback.hpp), as a recording is watched.
//
// THE DELAY keeps a spectator from telling a player where his opponent is: what a spectator sees
// already happened `sv_tvdelay` seconds ago. 0 sends each block as soon as it is whole (a match
// nobody plays to win: a showcase, a lesson).
//
// LISTED BESIDE THE SERVER: on a server beating to masters (`sv_master`), the node is listed too,
// as a TV node of the game's protocol naming the server as its source, so a hub shows a Watch on
// the server's card.
//
//   server_tv_open(tv, server, transport)   after server_open and record_begin
//   every frame: server_frame(server); record_frame(rec, server);
//                server_tv_frame(tv, record_fresh(rec), server.now_us);
//   server_tv_end(tv) at record_finish; server_tv_close(tv, now_us) when the server stops
//
// ge:summary Spectators on the server itself: its own TV node fed by its recording, with a delay
// ge:keywords spectators spectate server-tv sv_tvport sv_tvdelay sv_tvmaxviewers tv-delay watch
// ge:keywords server_tv_open server_tv_frame broadcast late-joiner hub-watch
// ge:skill replay net
// ge:test tests/server_spectators_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "net/transport.hpp"
#include "server/server.hpp"
#include "tv/net_link.hpp"

namespace ge::server {

struct ServerTv {
  tv::TvHost host;
  bool on = false;
  std::string error;  // the broadcast stopped: why (a block the node refused)
};

// The node on `transport` (its own port), named for the server, with its delay and cap from the
// server's settings; listed on the server's masters when it beats to any.
void server_tv_open(ServerTv& t, const Server& s, net::Transport& transport);
// The recording's new bytes fed and the node run at the server's clock. False once the node
// refused the stream (`error` says why); the viewers keep what they have.
bool server_tv_frame(ServerTv& t, std::span<const uint8_t> fresh, int64_t now_us);
// The match is over: the viewers are sent the rest and told it ended.
void server_tv_end(ServerTv& t);
// Off the masters; the viewers' links are left to time out.
void server_tv_close(ServerTv& t, int64_t now_us);
// The viewers watching, every relay's under it counted.
int64_t server_tv_watching(const ServerTv& t);
// "tv=N" for the status line ("" when the server has no TV).
std::string server_tv_status(const ServerTv& t);

}  // namespace ge::server
