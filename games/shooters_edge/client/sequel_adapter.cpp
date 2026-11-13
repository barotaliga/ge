// The sequel's adapter (game_adapter.hpp): the shared calls (adapter_parts.hpp) over its rows, so
// its mods, programs (native game libraries, named and passed over), maps, their wall textures
// (`textures/*.wal`), sounds, config files and demo listing read as any classic game's; and its
// own calls, each answering what this build reads of the game so far: no pictures of its HUD and
// menus yet (the window draws its own), the shell's menu, no held weapons and no demo playback.
// Each grows here as the sequel's rows land (docs/goals/M18v.md).
//
// ge:summary The sequel's adapter: the shared calls over its rows, and its own calls as they land
// ge:keywords sequel-adapter sequel adapter wal wall-texture native-library gamex86 md2 pcx
// ge:keywords second-game adapter_of
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include <filesystem>
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "shooters_edge/client/adapter_parts.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client {

namespace {

namespace pf = shooters_edge::profiles;

ClassicPictures pictures(const GameAdapter&, const SearchPath&, bool enhance,
                         const std::filesystem::path&) {
  ClassicPictures out;
  out.enhanced = enhance;
  out.hud_why = "the sequel's own status bar pictures are not read yet";
  return out;
}

const shooters_edge::menu::MenuTable& menu(const GameAdapter&) {
  return shooters_edge::menu::classic_menu();
}

bool held_weapons(const GameAdapter&, const SearchPath&, std::vector<ge::viewmodel::HeldModel>& out,
                  std::string& why) {
  out.clear();
  why.clear();  // none yet: the sequel's held models are a later row
  return false;
}

// A player's model names no skin (the player chooses it): until a player's own choice reaches the
// window, his model wears its folder's `grunt` skin, else the male model's (the game's own
// fallbacks for a missing skin).
bool open_model(const GameAdapter& self, const SearchPath& files, const std::string& name,
                OpenedModel& out, std::string& error) {
  if (!parts::open_model(self, files, name, out, error)) return false;
  if (out.form != ModelForm::morph || !out.morph.skins.empty() || name.rfind("players/", 0) != 0)
    return true;
  const std::string folder = std::filesystem::path(name).parent_path().generic_string();
  for (const std::string& path : {folder + "/grunt.pcx", std::string("players/male/grunt.pcx")}) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = find_file(files, path, store);
    ge::assets::IndexedImage img;
    std::string why;
    if (bytes.empty() || !ge::assets::decode_pcx(bytes, path, &img, &why)) continue;
    out.morph.skins.push_back({img.width, img.height, std::move(img.indices), 0.0f, 0});
    break;
  }
  return true;
}

// The table fixes the bytes' type (a game that plays them takes them over).
bool open_demo(const GameAdapter&,
               std::vector<uint8_t>,  // NOLINT(performance-unnecessary-value-param)
               Demo&, std::string& error) {
  error = "This game's recordings can't be played yet.";
  return false;
}

const GameAdapter kSequel{.game = &pf::kGameProfiles[1],
                          .list_mods = parts::list_mods,
                          .find_program = parts::find_program,
                          .list_maps = parts::list_maps,
                          .map_info = parts::map_info,
                          .map_textures = parts::map_textures,
                          .open_model = open_model,
                          .open_sound = parts::open_sound,
                          .pictures = pictures,
                          .find_config = parts::find_config,
                          .open_locations = parts::open_locations,
                          .held_weapons = held_weapons,
                          .menu = menu,
                          .modes = parts::modes,
                          .list_demos = parts::list_demos,
                          .find_demo = parts::find_demo,
                          .open_demo = open_demo};

}  // namespace

const GameAdapter& sequel_adapter() { return kSequel; }

}  // namespace shooters_edge::client
