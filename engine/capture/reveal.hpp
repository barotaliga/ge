// A FILE SHOWN IN THE SYSTEM'S FILE MANAGER, selected (the screenshots page's SHOW IN FINDER): the
// programs that do it on each desktop, in the order they are tried, with no process started here
// (engine/platform/reveal.hpp runs them). Pure, so every platform's commands are tested anywhere.
//
// - macOS: `open -R <file>` (Finder opens the folder with the file selected); a folder: `open`.
// - Windows: `explorer /select,"<file>"`, written as the raw command line Explorer reads (it
//   takes the quote after the comma, not around the whole word); a folder: `explorer "<folder>"`.
//   Explorer answers 1 even when it worked, so its answer is not judged.
// - Linux and the other desktops: the freedesktop file manager's `ShowItems` (a folder:
//   `ShowFolders`) over the session's bus with `dbus-send` (GNOME Files, Dolphin, Nemo, Caja and
//   Thunar answer it), judged by its answer; when no file manager answers, `xdg-open` on the folder
//   (the folder opens, nothing selected).
//
// ge:summary A file shown in the system's file manager: each desktop's commands, tried in order
// ge:keywords reveal show-in-finder show-in-explorer open-folder file-manager finder explorer
// ge:keywords dbus filemanager1 showitems xdg-open open-r select reveal_steps file_uri
// ge:skill replay platform
// ge:test tests/capture_reveal_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ge::capture {

enum class Desktop : unsigned char { mac, windows, freedesktop };

// The desktop this build runs on.
Desktop this_desktop();

// What the file manager is called on `d`, for a key's words: "FINDER", "EXPLORER", "FILES".
std::string_view file_manager_name(Desktop d);

// One program to try: its words (`argv`), or on Windows the raw command line (`line`) with
// `argv[0]` the program; `judged`: its exit code says whether it worked (0), so a failure tries
// the next step; unjudged, starting it is enough.
struct RevealStep {
  std::vector<std::string> argv;
  std::string line;  // Windows: the whole command line as written ("" elsewhere)
  bool judged = false;
};

// The steps that show `path` on `d`, selected in its folder (`folder`: open the folder itself).
// `path` absolute. Empty when `path` is empty.
std::vector<RevealStep> reveal_steps(Desktop d, std::string_view path, bool folder);

// `path` as a file URI: `file://` and the path with every byte outside the unreserved set and
// '/' percent-encoded ("/a b/ü.png" -> "file:///a%20b/%C3%BC.png").
std::string file_uri(std::string_view path);

}  // namespace ge::capture
