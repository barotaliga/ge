// A GAME SERVER PLAYED FROM THE HUB'S PAGE (docs/hub.md § Playing in a browser, M56i2): the web
// player's `--join=<url>` mode. The page cannot send UDP, so it opens a WebTransport session to
// the endpoint the URL names (`https://host:port/play?to=<server>`, its certificate trusted by the
// hash `--cert` gives), and the forwarder behind it sends each datagram on to the server as UDP,
// one for one. The game's own client (net/classic_remote.hpp) runs over that session as a pipe
// transport (engine/net/transport.hpp), so it joins and plays exactly as the window does over a
// socket; what each frame reads is drawn as a played demo's frame is, the map from the page's
// content packs. The canvas's keys and mouse are the player's hands, as in the play mode. When the
// server changes to a map the page does not hold, its files are asked of the hub by the map's name
// (web/file_list.hpp) and mounted over the rest, only those the page lacks fetched.
//
// ge:summary The web player's join mode: a server played over the page's WebTransport session
// ge:keywords web join play-in-browser webtransport pipe remote classic-client hub-play forward
// ge:keywords map-change
// ge:skill net shooter
#pragma once

#include "shooters_edge/client/files.hpp"
#include "web/fetch_files.hpp"
#include "web/viewer_args.hpp"

namespace shooters_edge::web {

// The server `args.join` names played from `files` on the canvas `canvas` until the session
// ends, a new map's files fetched into `files` past those `kept` holds; 1 with the page's line said
// when it cannot start or the session drops.
int join_in_page(const ge::web::ViewerArgs& args, client::SearchPath& files,
                 ge::web::FileStore& kept, const char* canvas);

}  // namespace shooters_edge::web
