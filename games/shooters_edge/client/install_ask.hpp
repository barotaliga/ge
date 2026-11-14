// ASKING FOR A GAME'S INSTALL: `se -game N` when no install is kept for game N on this computer
// (client/game_folder.hpp's `choose_folder` sets `Args::ask_install`). The desktop's folder chooser
// opens at the start, titled with the game's number and name; the folder picked is checked to be
// that game's install (`check_game_folder` against its content profile, so a folder of the other
// game is refused too), refused with the reason and asked again until it is or the player cancels.
// The folder that checks opens as LOAD GAME's would, which keeps it for the next `-game N`. Built
// into the `se` window alone: it asks the platform for the chooser.
//
// ge:summary The window asking for -game N's install folder and checking it is that game's
// ge:keywords game-number install-ask folder-chooser pick_folder check_game_folder ask_install
// ge:keywords answer_install select-game kept-install
// ge:skill platform shooter
#pragma once

#include <string>

#include "classicenhance/profile.hpp"
#include "shooters_edge/client/args.hpp"

namespace ge::platform {
class Window;
}

namespace shooters_edge::client {

struct InstallAsk {
  const ge::classicenhance::Profile* game = nullptr;  // asked for; null once answered or not asked
  int number = 0;                                     // its `-game` number
};

// The chooser opened for `args.game_number`'s install when `args.ask_install` (and not `measured`,
// a timed or recorded run, which asks nothing); what is asked, or nothing.
InstallAsk ask_install(ge::platform::Window& window, const Args& args, bool measured);

// The folder chooser answered `*folder` ("" when cancelled; `*why` the chooser's own failure)
// while `ask` waits: true with `*folder` to open, the install's own folder when it checks (or left
// as it is when nothing was asked, or cancelled); false when it is not that game's install, with
// `*why` the reason in the game's words, `*folder` cleared and the chooser opened again.
bool answer_install(ge::platform::Window& window, InstallAsk& ask, std::string* folder,
                    std::string* why);

}  // namespace shooters_edge::client
