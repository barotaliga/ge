// The sequel's adapter (game_adapter.hpp): the shared calls (adapter_parts.hpp) over its rows, so
// its mods, programs (native game libraries, named and passed over), maps, their wall textures
// (`textures/*.wal`), sounds, config files and demo listing read as any classic game's; and its
// own calls, each answering what this build reads of the game so far: the console's letters and
// background but no pictures of its HUD and menus yet (the window draws its own), the shell's menu,
// no held weapons; its client recordings. Each grows here as the sequel's rows land
// (docs/goals/M18v.md).
//
// ge:summary The sequel's adapter: the shared calls over its rows, and its own calls as they land
// ge:keywords sequel-adapter sequel adapter wal wall-texture native-library gamex86 md2 pcx
// ge:keywords second-game adapter_of
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/pcx.hpp"
#include "shooters_edge/client/adapter_parts.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"

namespace shooters_edge::client {

namespace {

namespace pf = shooters_edge::profiles;

// A picture of the game's files (`pics/<name>.pcx`) in its own palette, `clear` see-through; empty
// when the files hold none that reads.
ge::ui::Image pcx_picture(const SearchPath& files, std::string_view name, int clear) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes =
      find_file(files, "pics/" + std::string(name) + ".pcx", store);
  ge::assets::IndexedImage img;
  std::string why;
  if (bytes.empty() || !ge::assets::decode_pcx(bytes, name, &img, &why) || img.palette.empty())
    return {};
  ge::ui::Image out;
  out.width = img.width;
  out.height = img.height;
  out.rgba = ge::assets::pcx_rgba(img);
  for (size_t i = 0; i < img.indices.size(); i++)
    if (img.indices[i] == clear) out.rgba[i * 4 + 3] = 0;
  return out;
}

// The console's letters (`conchars`, its last colour see-through) and background (`conback`);
// the status bar's and the menus' pictures are not read yet, so the menu keeps its modern look.
ClassicPictures pictures(const GameAdapter&, const SearchPath& files, bool enhance,
                         const std::filesystem::path&) {
  ClassicPictures out;
  out.enhanced = enhance;
  out.hud_why = "the sequel's own status bar pictures are not read yet";
  out.menu.glyphs = pcx_picture(files, "conchars", 255);
  if (out.menu.glyphs.width != 128 || out.menu.glyphs.height != 128) out.menu.glyphs = {};
  if (ge::ui::Image back = pcx_picture(files, "conback", -1); !back.empty())
    out.menu.pictures.emplace("conback", std::move(back));
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

// Its own client recordings (demo.hpp § SEQUEL); another game's are refused.
bool open_demo_(const GameAdapter&, std::vector<uint8_t> bytes, Demo& out, std::string& error) {
  if (!open_demo(std::move(bytes), out, error)) return false;
  if (out.state.format == DemoFormat::sequel) return true;
  error = "This recording is of another game. Choose that game to watch it.";
  out = Demo{};
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
                          .open_demo = open_demo_};

}  // namespace

const GameAdapter& sequel_adapter() { return kSequel; }

}  // namespace shooters_edge::client
