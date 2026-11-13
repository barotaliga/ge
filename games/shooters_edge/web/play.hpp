// A GAME PLAYED ON THE HUB'S PAGE (docs/hub.md § Playing in a browser, M56i): the web viewer's
// `--play=<map>` mode. The game's own listen server (client/listen.hpp: the baseline mod on the
// match server, the lobby with its bots) runs inside the page's WebAssembly, on the map the hub's
// content pack holds, and the player plays it at the canvas: W A S D and the arrows move, SPACE
// jumps, the mouse looks once a click has locked the pointer to the canvas (Escape lets it go),
// the left button fires, 1..8 and the wheel pick a weapon, TAB holds the scores. The bots
// (`--bots`) are seated and ready, so the countdown starts at once and the match is on a few
// seconds after the page opens. A map change the server asks for that the page's files lack
// plays the same map again. Nothing leaves the page: the match is the player's alone.
//
// ge:summary The web viewer's play mode: the listen server and its bots in the page, played at
// ge:summary the canvas
// ge:keywords web play play-in-browser listen-server bots canvas pointer-lock hub-play
// ge:skill net shooter
#pragma once

#include "shooters_edge/client/files.hpp"
#include "web/viewer_args.hpp"

namespace shooters_edge::web {

// The game played on `args.play` from `files` on the canvas `canvas` until the page closes; 1
// with the page's line said when it cannot start.
int play_in_page(const ge::web::ViewerArgs& args, const client::SearchPath& files,
                 const char* canvas);

}  // namespace shooters_edge::web
