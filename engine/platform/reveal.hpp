// A FILE SHOWN IN THE SYSTEM'S FILE MANAGER, selected: the steps capture/reveal.hpp names for this
// desktop, run as processes in turn until one works (Finder on macOS, Explorer on Windows, the
// desktop's file manager or the folder on Linux). For any game with a window: the screenshots
// page's SHOW IN FINDER key runs the console's `reveal`, which a player may also bind or type.
//
// `reveal <path>` shows that file in its folder; a folder's path opens the folder. It runs only
// from this machine's console or its cfgs, never from a line a server sent. What did not work
// says why on the console.
//
// ge:summary A file shown selected in Finder, Explorer or the desktop's file manager; `reveal`
// ge:keywords reveal show-in-finder show-in-explorer file-manager open-folder add_reveal_command
// ge:keywords sdl-process create-process screenshots-folder
// ge:skill platform replay
#pragma once

#include <string>
#include <string_view>

#include "console/console.hpp"

namespace ge::platform {

// Shows `path` selected in its folder (`folder`: opens that folder). False, with `why`, when no
// step could start or every judged one failed.
bool reveal(std::string_view path, bool folder, std::string* why = nullptr);

// The console's `reveal <path>`.
void add_reveal_command(console::Console& console);

}  // namespace ge::platform
