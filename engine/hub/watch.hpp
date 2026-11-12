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
// - `/viewer/{file}`: the viewer's files (`<id>_viewer.js`, its `.wasm`, its data) from the hub's
//   `--viewer=<dir>`, the build's `ge_add_web_viewer` output. A name with a slash, `..` or another
//   extension is refused.
//
// The page's script is the few lines that start the viewer: the canvas and the arguments handed
// to it (`--tv=ws://...`, `--stream=`, `--demo=/d/...`), then the viewer's loader. Every other
// part of the page is plain HTML, so the page reads the same with script off.
//
//   watch_path(node, stream)            the Watch page's path for a broadcast
//   viewer_built(site, spec)            the hub serves a viewer for this game
//   watch_answer(p, spec, target, now_us, site, out)   a Watch, Demo or viewer path answered
//
// ge:summary The hub's Watch and Demo pages around the web viewer, and the viewer's files served
// ge:keywords hub watch demo page web-viewer canvas websocket tv viewer-files wasm loader
// ge:keywords watch_answer watch_path viewer_built
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
// The viewer's files are cached this long.
inline constexpr int kViewerMaxAgeS = 3600;

std::string watch_path(const net::Address& node, uint32_t stream);
// True when `site.viewer_dir` holds `<id>_viewer.js`.
bool viewer_built(const Site& site, const HubSpec& spec);
// `/watch/...`, `/demo/...` or `/viewer/...` answered into `out`; false for any other path.
bool watch_answer(const Poller& p, const HubSpec& spec, std::string_view target, int64_t now_us,
                  const Site& site, Answer& out);

}  // namespace ge::hub
