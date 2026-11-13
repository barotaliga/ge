// THE MAIN MENU HEARD: the click each frame's presses make (menu/game_menu.hpp's `take_cue`,
// ui/menu.hpp's `MenuCue`), played centred on the synth from the loaded game's own recordings:
//
// | cue    | when                                              | recording          |
// | ------ | ------------------------------------------------- | ------------------ |
// | move   | the cursor moved to another row, a page flipped   | `misc/menu1.wav`   |
// | enter  | the menu opened, a page or an item entered        | `misc/menu2.wav`   |
// | change | a setting changed, a key bound                    | `misc/menu3.wav`   |
// | back   | out of a page to the one under it, the menu closed | `misc/menu3.wav`   |
//
// Both classic games name their menu's sounds so; the built-in arena hears the game's own
// (`content/classic.assetmap` mounts `ui/move`, `ui/select` and `ui/back` under those names,
// own_sounds.hpp). A recording is read through the game's adapter the first time its cue plays and
// kept until the folder changes (`forget`); a name the packs lack is silent and counted. A click
// cuts the one before it, as a held arrow's repeats do. Recordings let go are kept until the synth
// no longer reads them (a click fading out reads its recording in place).
//
// ge:summary The main menu's clicks: move, enter, change and back from the game's recordings
// ge:keywords menu-sound menu-cue menu1 menu2 menu3 click navigate enter back close main-menu
// ge:keywords menu-sounds local-sound centred
// ge:skill audio ui shooter
// ge:test tests/shooters_edge_menu_sound_test.cpp
#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "audio/pcm.hpp"
#include "audio/synth.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "ui/menu.hpp"

namespace shooters_edge::client {

// The recording a cue plays, as the game names it (the table above); "" for none.
std::string_view menu_sound_name(ge::ui::MenuCue cue);

struct MenuSoundStats {
  int played = 0;   // clicks started
  int missing = 0;  // recordings the packs lack (counted once each)
};

class MenuSounds {
 public:
  explicit MenuSounds(ge::audio::Synth& synth, int bus = 0);
  MenuSounds(const MenuSounds&) = delete;
  MenuSounds& operator=(const MenuSounds&) = delete;

  // `cue`'s recording played at `volume` (none: nothing), read from `files` by `game` the first
  // time. False when it did not play (none, or the packs lack it).
  bool play(ge::ui::MenuCue cue, const GameAdapter& game, const SearchPath& files,
            float volume = 1);
  // Another game folder opened: the recordings read again on the next click.
  void forget();
  // Whether the synth still reads one of its recordings.
  bool sounding() const;

  const MenuSoundStats& stats() const { return stats_; }

 private:
  void prune();

  ge::audio::Synth& synth_;
  int bus_;
  ge::audio::SampleId last_;
  std::map<std::string, std::unique_ptr<ge::audio::Pcm>, std::less<>> loaded_;  // null: missing
  std::vector<std::unique_ptr<ge::audio::Pcm>> retired_;  // let go, maybe still sounding
  MenuSoundStats stats_;
};

}  // namespace shooters_edge::client
