// QUAKE II'S CINEMATICS IN THE WINDOW (games/shooters_edge/client/cinematic.cpp): a movie from the
// packs plays at 14 frames a second on the clock and holds the next map back, waits while the
// menu is up, is skipped by a button held past a second, and gives way to the game's last
// picture, which stays until a new game is asked for; a movie the files lack is passed over and
// said.
//
// ge:summary Tests: the window's cinematic chain: the clock, the pause, the skip, the last picture
// ge:keywords test cinematic cin movie victory skip pause
// ge:skill classic testing
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "audio/synth.hpp"
#include "check.hpp"
#include "classic/content/cin.hpp"
#include "shooters_edge/client/cinematic.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace cc = ge::classic::content;

cl::SearchPath packs() {
  std::vector<std::vector<uint8_t>> frames;
  frames.reserve(28);
  for (int k = 0; k < 28; k++) frames.emplace_back(size_t(16 * 8), uint8_t(k));
  std::vector<uint8_t> palette(768);
  for (size_t i = 0; i < palette.size(); i++) palette[i] = uint8_t(i / 3);
  std::vector<int16_t> sound(size_t{22050} * 2, 100);
  ge::assets::IndexedImage still;
  still.width = 4;
  still.height = 2;
  still.indices.assign(8, 7);
  still.palette = palette;
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak(
      {{"video/eou1_.cin", cc::write_cin(16, 8, frames, palette, 22050, 1, sound)},
       {"pics/victory.pcx", ge::assets::encode_pcx(still)}},
      &pak, &error));
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  return files;
}

}  // namespace

TEST(a_movie_plays_on_the_clock_waits_for_the_menu_and_gives_way_to_the_last_picture) {
  const cl::SearchPath files = packs();
  ge::audio::Synth synth;
  cl::Cinematic c;
  cl::cinematic_start(c, "eou1_.cin+victory.pcx");
  CHECK(cl::cinematic_frame(c, files, synth, false, false, false, 10.0));
  CHECK(c.showing == "eou1_.cin" && c.shown == 0 && cl::cinematic_holds(c));
  CHECK(c.picture.width == 16 && c.picture.rgba[0] == 0);
  CHECK(cl::cinematic_frame(c, files, synth, false, false, false, 10.5));
  CHECK(c.shown == 7 && c.picture.rgba[0] == 7);                          // 14 frames a second
  CHECK(cl::cinematic_frame(c, files, synth, false, true, false, 30.0));  // the menu is up
  CHECK(c.shown == 7);
  CHECK(cl::cinematic_frame(c, files, synth, false, false, false, 30.5));
  CHECK(c.shown == 14);  // the clock picked up where it was
  CHECK(cl::cinematic_frame(c, files, synth, false, false, false, 32.0));  // past its 28 frames
  CHECK(c.showing == "victory.pcx" && c.still && !cl::cinematic_holds(c));
  CHECK(c.picture.width == 4 && c.picture.rgba[0] == 7);
  CHECK(cl::cinematic_frame(c, files, synth, true, false, false, 60.0));   // no skip, no clock
  CHECK(!cl::cinematic_frame(c, files, synth, false, false, true, 61.0));  // a new game
  CHECK(!cl::cinematic_on(c));
}

TEST(a_button_held_past_a_second_skips_and_a_missing_movie_is_passed_over) {
  const cl::SearchPath files = packs();
  ge::audio::Synth synth;
  cl::Cinematic c;
  cl::cinematic_start(c, "eou1_.cin");
  CHECK(cl::cinematic_frame(c, files, synth, true, false, false, 0.0));
  CHECK(cl::cinematic_frame(c, files, synth, true, false, false, 0.9));  // too soon
  CHECK(!cl::cinematic_frame(c, files, synth, true, false, false, 1.1));
  CHECK(!cl::cinematic_on(c));
  cl::cinematic_start(c, "eou2_.cin");
  CHECK(!cl::cinematic_frame(c, files, synth, false, false, false, 2.0));
  CHECK(c.said.size() == 1 && c.said[0].find("video/eou2_.cin") != std::string::npos);
}
