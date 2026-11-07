// THE GAME'S OWN SOUNDS UNDER THE NAMES A MOD PLAYS: with no classic game's folder open (the
// built-in arena), the free recordings in `content/sounds/` stand in for the classic names the
// baseline mod precaches ("weapons/rocket1i.wav"), by the `sound <classic path> <our name>` lines
// of `content/classic.assetmap`. They are packed in memory as `sound/<classic path>` and mounted
// on the search path, so the window's hearing (listen_sound.hpp) finds them as it finds a pack's.
// Never mounted over a classic game's own folder: a classic game hears only its own recordings.
// A row whose recording is missing is counted, not refused (the registry's open rows).
//
// ge:summary The game's own WAVs mounted under the classic sound names, by classic.assetmap
// ge:keywords own-sounds assetmap classic-assetmap sound-map free-sounds arena silent mount
// ge:keywords content-sounds stand-in replacement
// ge:skill audio shooter
// ge:test tests/shooters_edge_own_sounds_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "shooters_edge/client/files.hpp"

namespace shooters_edge::client {

// The `sound` lines of an asset map: (classic path, our name), in order. Other lines (`model`,
// `set`) and comments are skipped. False with the line and the fix on a `sound` line without two
// names.
bool read_sound_map(std::string_view text, std::vector<std::pair<std::string, std::string>>& out,
                    std::string& error);

struct OwnSounds {
  int mapped = 0;   // `sound` rows read
  int mounted = 0;  // rows whose recording was found and mounted
};

// `<content>/classic.assetmap` read and every row's `<content>/sounds/<our name>.wav` mounted on
// `files` as `sound/<classic path>`. False with the reason and the next step when the map does not
// read; a missing recording only leaves `mounted` short.
bool mount_own_sounds(SearchPath& files, const std::string& content, OwnSounds& out,
                      std::string& error);

// The tree's content folder from the working folder or from `built_in` (where the tree was when
// the game was built; "" none): the first holding `classic.assetmap`, else "".
std::string find_content_dir(const std::string& built_in);

}  // namespace shooters_edge::client
