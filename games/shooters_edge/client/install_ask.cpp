// The window asking for -classic <id>'s install (install_ask.hpp).
//
// ge:summary The window asking for -classic <id>'s install folder and checking it is that game's
// ge:keywords game-number install-ask folder-chooser check_game_folder
// ge:skill platform shooter
#include "shooters_edge/client/install_ask.hpp"

#include "platform/window.hpp"
#include "shooters_edge/client/game_folder.hpp"

namespace shooters_edge::client {

namespace {

void open_chooser(ge::platform::Window& window, const InstallAsk& ask, bool again,
                  const std::string& start) {
  const std::string what(ask.game->what);
  ge::platform::pick_folder(
      window,
      again ? "That folder is not " + what + ". Pick the folder it is in"
            : "Game " + std::to_string(ask.number) + ": the folder " + what + " is installed in",
      start);
}

}  // namespace

InstallAsk ask_install(ge::platform::Window& window, const Args& args, bool measured) {
  InstallAsk ask;
  if (!args.ask_install || measured || args.game_number < 1) return ask;
  ask.game = &ge::classicenhance::kProfiles[static_cast<size_t>(args.game_number) - 1];
  ask.number = args.game_number;
  open_chooser(window, ask, false, args.basedir);
  return ask;
}

bool answer_install(ge::platform::Window& window, InstallAsk& ask, std::string* folder,
                    std::string* why) {
  if (ask.game == nullptr) return true;
  if (folder->empty() || !why->empty()) {  // cancelled, or the chooser could not open
    ask.game = nullptr;
    return true;
  }
  std::string basedir;
  if (!check_game_folder(*folder, &basedir, why, *ask.game)) {
    open_chooser(window, ask, true, *folder);
    folder->clear();
    return false;
  }
  *folder = basedir;
  ask.game = nullptr;
  return true;
}

}  // namespace shooters_edge::client
