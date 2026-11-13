// The sequel's adapter (game_adapter.hpp): the shared calls (adapter_parts.hpp) over its rows, so
// its mods, programs (native game libraries, named and passed over), maps, their wall textures
// (`textures/*.wal`), sounds, config files and demo listing read as any classic game's; and its
// own calls, each answering what this build reads of the game so far: no pictures of its HUD and
// menus yet (the window draws its own), the shell's menu, no held weapons; its client recordings.
// Each grows here as the sequel's rows land (docs/goals/M18v.md).
//
// ge:summary The sequel's adapter: the shared calls over its rows, and its own calls as they land
// ge:keywords sequel-adapter sequel adapter wal wall-texture native-library gamex86 md2 pcx
// ge:keywords second-game adapter_of
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/client/adapter_parts.hpp"
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
                          .open_model = parts::open_model,
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
