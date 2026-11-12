// Tests: the main menu's clicks (client/menu_sound.hpp): each cue names its recording, the game's
// own recordings (mounted under the classic names, as the built-in arena does) play for every cue,
// a click cuts the last one, a folder without them is silent and counted once, and recordings let
// go by `forget` are read again on the next click.
//
// ge:summary Tests: the main menu's clicks played from the game's recordings
// ge:keywords test menu-sound menu-cue click menu1 menu2 menu3 own-sounds
// ge:skill audio testing
#include <string>
#include <vector>

#include "audio/synth.hpp"
#include "check.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/menu_sound.hpp"
#include "shooters_edge/client/own_sounds.hpp"

namespace {

namespace cl = shooters_edge::client;
using ge::ui::MenuCue;

const std::string kContent = std::string(GE_SOURCE_DIR) + "/games/shooters_edge/content";

TEST(each_cue_names_the_classic_menu_recording) {
  CHECK(cl::menu_sound_name(MenuCue::none).empty());
  CHECK(cl::menu_sound_name(MenuCue::move) == "misc/menu1.wav");
  CHECK(cl::menu_sound_name(MenuCue::enter) == "misc/menu2.wav");
  CHECK(cl::menu_sound_name(MenuCue::change) == "misc/menu3.wav");
  CHECK(cl::menu_sound_name(MenuCue::back) == "misc/menu3.wav");
}

TEST(every_cue_plays_from_the_game_s_recordings) {
  cl::SearchPath files;
  cl::OwnSounds own;
  std::string error;
  CHECK_MSG(cl::mount_own_sounds(files, kContent, own, error), error);
  ge::audio::Synth synth;
  cl::MenuSounds sounds(synth);
  const cl::GameAdapter& game = cl::classic_adapter();
  CHECK(!sounds.play(MenuCue::none, game, files));
  for (MenuCue c : {MenuCue::enter, MenuCue::move, MenuCue::move, MenuCue::change, MenuCue::back})
    CHECK_MSG(sounds.play(c, game, files, 0.7f), std::string(cl::menu_sound_name(c)));
  CHECK(sounds.stats().played == 5 && sounds.stats().missing == 0);
  CHECK(sounds.sounding());
  std::vector<float> out(2 * 256);
  float peak = 0;
  for (int block = 0; block < 8; block++) {
    synth.render(out.data(), 256);
    for (float v : out) peak = v > peak ? v : (-v > peak ? -v : peak);
  }
  CHECK_MSG(peak > 0.001f, "the click is heard: peak " + std::to_string(peak));
  // Another folder: read again on the next click; the old ones kept while the synth reads them.
  sounds.forget();
  CHECK(sounds.play(MenuCue::back, game, files));
  CHECK(sounds.stats().played == 6);
}

TEST(a_folder_without_the_recordings_is_silent_and_counted_once) {
  cl::SearchPath none;
  ge::audio::Synth synth;
  cl::MenuSounds sounds(synth);
  const cl::GameAdapter& game = cl::classic_adapter();
  CHECK(!sounds.play(MenuCue::move, game, none));
  CHECK(!sounds.play(MenuCue::move, game, none));
  CHECK(sounds.stats().missing == 1 && sounds.stats().played == 0);
  CHECK(!sounds.sounding());
}

}  // namespace
