// The open calls every shooter game shares (plugins/classic/content/open.hpp), with no game's
// adapter: a map's info read off a fixture map by the content row's folder, a missing one refused
// with the player's next step; a model read by its kind (a vertex model with a picture standing in
// for its skin, a sprite, a brush model), a missing one false with no reason and an unknown kind
// named; a sound under the row's sound folder; a config never climbing out of the files; a map's
// place names; and the same files opened under another game's content row read alike.
//
// ge:summary Tests the shared open calls: map info, models by kind, sounds, configs, place names
// ge:keywords test shooter content open_model open_sound map_info find_config open_locations
// ge:keywords stand_in replacement-skin mixed-files
// ge:skill shooter testing
#include <string>
#include <vector>

#include "assets/image.hpp"
#include "assets/tga.hpp"
#include "audio/wav.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/content/files.hpp"
#include "classic/content/open.hpp"
#include "classicenhance/profile.hpp"
#include "morph/write.hpp"
#include "vfs/pak.hpp"

namespace ct = ge::classic::content;
namespace ce = ge::classicenhance;

namespace {

std::vector<uint8_t> bytes_of(const std::string& s) { return {s.begin(), s.end()}; }

// One pack holding a map, a model with a skin's stand-in, a sprite, a sound, a config and a loc.
ct::SearchPath make_files(const ge::assets::Image& skin0) {
  std::string error;
  std::vector<uint8_t> bsp, pak;
  CHECK_MSG(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error),
            error);
  const std::vector<float> tone(96, 0.25f);
  CHECK(ge::vfs::write_pak(
      {{"maps/room.bsp", bsp},
       {"progs/armor.mdl", ge::morph::write_model(ge::morph::sample_model())},
       {"progs/armor.mdl_0.tga", ge::assets::encode_tga(skin0, true)},
       {"progs/s_bubble.spr", ge::morph::write_sprite(ge::morph::sample_sprite())},
       {"progs/odd.md9", {1, 2, 3}},
       {"sound/misc/tone.wav", ge::audio::wav_bytes(tone)},
       {"autoexec.cfg", bytes_of("echo hi\n")},
       {"locs/room.loc", bytes_of("0 0 0 hall\n")}},
      &pak, &error));
  ct::SearchPath files;
  CHECK_MSG(ct::add_archive(files, "pak0.pak", std::move(pak), error), error);
  return files;
}

ge::assets::Image skin() {
  ge::assets::Image img{8, 4, std::vector<uint8_t>(size_t{8} * 4 * 4, 200)};
  img.rgba[3] = 0;
  return img;
}

}  // namespace

TEST(a_maps_info_is_read_by_the_content_rows_folder) {
  const ct::SearchPath files = make_files(skin());
  ct::MapInfo info;
  std::string error;
  CHECK_MSG(ct::map_info(files, ce::q1_profile(), "room", info, error), error);
  CHECK(info.name == "room" && info.title == ge::brush::sample_fixture().message);
  CHECK(info.family == ge::brush::Family::v29 && info.textures > 0);
  CHECK(!ct::map_info(files, ce::q1_profile(), "nowhere", info, error));
  CHECK(error.find("maps/nowhere.bsp") != std::string::npos);
  CHECK(error.find("Type \"maps\"") != std::string::npos);  // a player's next step
}

TEST(a_model_is_read_by_its_kind) {
  const ge::assets::Image skin0 = skin();
  const ct::SearchPath files = make_files(skin0);
  ct::OpenedModel m;
  std::string error;
  CHECK_MSG(ct::open_model(files, "progs/armor.mdl", m, error), error);
  CHECK(m.form == ct::ModelForm::morph && !m.skins.empty());
  CHECK(m.skins[0].width == 8 && m.skins[0].rgba == skin0.rgba);  // the stand-in, alpha kept
  CHECK_MSG(ct::open_model(files, "progs/s_bubble.spr", m, error), error);
  CHECK(m.form == ct::ModelForm::sprite && !m.sprite.frames.empty());
  CHECK_MSG(ct::open_model(files, "maps/room.bsp", m, error), error);
  CHECK(m.form == ct::ModelForm::brush && !m.brush.surfaces.empty());
  CHECK(!ct::open_model(files, "progs/missing.mdl", m, error) && error.empty());
  CHECK(!ct::open_model(files, "progs/odd.md9", m, error) &&
        error.find(".md9") != std::string::npos);
}

TEST(sounds_configs_and_places_are_read_from_the_files) {
  const ct::SearchPath files = make_files(skin());
  ge::audio::Pcm pcm;
  std::string error;
  CHECK_MSG(ct::open_sound(files, ce::q1_profile(), "misc/tone.wav", pcm, error), error);
  CHECK(!ct::open_sound(files, ce::q1_profile(), "misc/none.wav", pcm, error) && error.empty());
  std::string text;
  CHECK(ct::find_config(files, "autoexec.cfg", text) && text == "echo hi\n");
  CHECK(!ct::find_config(files, "../autoexec.cfg", text));  // never out of the files
  CHECK(!ct::find_config(files, "", text));
  ge::classic::Locs locs;
  CHECK(ct::open_locations(files, "room", locs));
  CHECK(locs.points.size() == 1 && locs.points[0].name == "hall" && locs.from == "locs/room.loc");
  ge::classic::Locs none;
  CHECK(!ct::open_locations(files, "nowhere", none));
}

TEST(every_games_content_row_reads_the_same_files) {
  // A map and a sound in one game's folders open under any row whose folders name them alike.
  const ct::SearchPath files = make_files(skin());
  for (const ce::Profile& row : ce::kProfiles) {
    if (row.maps.dir != "maps/" || row.sounds.dir != "sound/") continue;
    ct::MapInfo info;
    std::string error;
    CHECK_MSG(ct::map_info(files, row, "room", info, error), std::string(row.base_dir) + error);
    ge::audio::Pcm pcm;
    CHECK_MSG(ct::open_sound(files, row, "misc/tone.wav", pcm, error), error);
  }
}
