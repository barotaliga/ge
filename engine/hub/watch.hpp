// WATCHING IN A BROWSER (docs/hub.md § Watching in a browser): the Watch and Demo pages that hold
// the game's web viewer, and the viewer's own files served beside them.
//
// - `/watch/{tv-node}/{stream}`: one live broadcast. The viewer's canvas fed by the TV node's
//   WebSocket (the node's status record names its web port, key `web`); the match's scoreboard
//   from the game server whose record lists the stream; the other live broadcasts by interest;
//   Open in the game (`<id>://tv/...`). A node that takes no browser viewers, or a hub without a
//   built viewer, gets the same page with the reason where the picture would be.
// - `/demo/{sha256}`: one archived demo in the viewer, read by hash from the hub's own `/d/...`
//   path; the demo's map, players and length, Play in the game, Download, and its Map (radar.hpp).
// - `/content/{sha256}.pak.zst`: a demo's content pack (content.hpp), which the Demo page hands the
//   viewer in place of every pack when the game names a demo's files.
// - `/play` and `/play/{map}` (a game with `HubSpec::map_files`, docs/hub.md § Playing in a
//   browser): the maps the packs beside the viewer hold, and a match on one played in the page
//   against `kPlayBots` bots by the player build (`<id>_play.js`: the viewer with the game's server
//   in it), handed the map's own content pack (content.hpp's `map_content`).
// - `/viewer/{file}`: the viewer's files (`<id>_viewer.js`, its `.wasm`, its data) from the hub's
//   `--viewer=<dir>`, the build's `ge_add_web_viewer` output, and the content packs (`.pak`,
//   `.pk3`) the hub's operator puts beside it for the viewer to draw from, handed to it by name
//   order (`--pack=/viewer/<name>`). A name with a slash, `..` or another extension is refused.
//
// The page's script is the few lines that start the viewer: the canvas and the arguments handed
// to it (`--tv=ws://...`, `--stream=`, `--demo=/d/...`, `--pack=`), the status line the viewer
// writes (`#viewer-status`: what is loading, or why there is no picture), then the viewer's
// loader. Every other part of the page is plain HTML, so the page reads the same with script off.
//
//   watch_path(node, stream)            the Watch page's path for a broadcast
//   viewer_built(site, spec)            the hub serves a viewer for this game
//   watch_answer(p, spec, target, now_us, site, out)   a Watch, Demo or viewer path answered
//
// ge:summary The hub's Watch, Demo and Play pages around the web viewer, and its files served
// ge:keywords hub watch demo page web-viewer canvas websocket tv viewer-files wasm loader
// ge:keywords watch_answer watch_path viewer_built player_built play-page play_page
// ge:skill net
// ge:test tests/hub_watch_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hub/api.hpp"
#include "hub/pages.hpp"
#include "hub/poller.hpp"
#include "hub/spec.hpp"

namespace ge::hub {

// The status key a TV node's record carries its WebSocket port in (tv/net_link.hpp writes it).
inline constexpr std::string_view kWebPortKey = "web";
// The bots a Play page's match seats beside the player.
inline constexpr int kPlayBots = 3;
// The viewer's files are cached this long.
inline constexpr int kViewerMaxAgeS = 3600;

std::string watch_path(const net::Address& node, uint32_t stream);
// True when `site.viewer_dir` holds `<id>_viewer.js`.
bool viewer_built(const Site& site, const HubSpec& spec);
// True when `site.viewer_dir` holds `<id>_play.js`, the player.
bool player_built(const Site& site, const HubSpec& spec);
// `/watch/...`, `/demo/...`, `/play...` or `/viewer/...` answered into `out`; false for any other
// path.
bool watch_answer(const Poller& p, const HubSpec& spec, std::string_view target, int64_t now_us,
                  const Site& site, Answer& out);

}  // namespace ge::hub
