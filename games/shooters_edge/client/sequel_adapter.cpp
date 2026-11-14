// The sequel's adapter (game_adapter.hpp): the shared calls (adapter_parts.hpp) over its rows, so
// its mods, programs (native game libraries, named and passed over), maps, their wall textures
// (`textures/*.wal`), sounds, config files and demo listing read as any classic game's; and its
// own calls, each answering what this build reads of the game so far: the console's letters and
// background and its menu's pictures, its status bar drawn from its rules' own program
// (sequel_bar.hpp), the shell's menu, its held weapons (the frame its rules' hand is on); its
// client recordings. Each grows here as the sequel's rows land (docs/goals/M18v.md).
//
// ge:summary The sequel's adapter: the shared calls over its rows, and its own calls as they land
// ge:keywords sequel-adapter sequel adapter wal wall-texture native-library gamex86 md2 pcx
// ge:keywords second-game adapter_of
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/pcx.hpp"
#include "classicenhance/texture.hpp"
#include "classicenhance/weapons.hpp"
#include "shooters_edge/client/adapter_parts.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/server/sequel_combat.hpp"

namespace shooters_edge::client {

namespace {

namespace pf = shooters_edge::profiles;

// A picture of the game's files (`pics/<name>.pcx`) in its own palette, `clear` see-through; empty
// when the files hold none that reads. `print`, when given, folds in the picture's palette indices
// (FNV-1a, 64-bit).
ge::ui::Image pcx_picture(const SearchPath& files, std::string_view name, int clear,
                          uint64_t* print = nullptr) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes =
      find_file(files, "pics/" + std::string(name) + ".pcx", store);
  ge::assets::IndexedImage img;
  std::string why;
  if (bytes.empty() || !ge::assets::decode_pcx(bytes, name, &img, &why) || img.palette.empty())
    return {};
  if (print != nullptr)
    for (const uint8_t v : img.indices) *print = (*print ^ v) * 0x100000001b3ULL;
  ge::ui::Image out;
  out.width = img.width;
  out.height = img.height;
  out.rgba = ge::assets::pcx_rgba(img);
  for (size_t i = 0; i < img.indices.size(); i++)
    if (img.indices[i] == clear) out.rgba[i * 4 + 3] = 0;
  return out;
}

// The menu's pictures by name: the main menu's plates (each with its lit copy, `_sel`), the plaque
// and the logo down the left, the spinning cursor's fifteen frames and each page's banner.
constexpr std::string_view kMenuPictures[] = {
    "m_main_game",           "m_main_multiplayer",   "m_main_options",         "m_main_quit",
    "m_main_video",          "m_main_game_sel",      "m_main_multiplayer_sel", "m_main_options_sel",
    "m_main_quit_sel",       "m_main_video_sel",     "m_main_plaque",          "m_main_logo",
    "m_banner_game",         "m_banner_multiplayer", "m_banner_options",       "m_banner_video",
    "m_banner_start_server", "m_banner_join_server", "m_banner_plauer_setup",  "m_banner_load_game",
    "m_banner_save_game",    "m_banner_customize"};
constexpr int kCursorFrames = 15;
// The plates the menu's missing words are cut from (classic_view.cpp), in the print's order.
constexpr int kPrinted = 5;

// The console's letters (`conchars`, its last colour see-through) and background (`conback`), and
// the menu's pictures (above, the same colour see-through), the first five printed so words cut
// from their letters are cut only from the plates they were measured on. The status bar is the
// sequel's own, drawn from the program its rules send (sequel_bar.hpp).
ClassicPictures pictures(const GameAdapter&, const SearchPath& files, bool enhance,
                         const std::filesystem::path&) {
  ClassicPictures out;
  out.enhanced = enhance;
  out.hud_why = "the sequel draws its own bar, from the program its rules send";
  out.menu.glyphs = pcx_picture(files, "conchars", 255);
  if (out.menu.glyphs.width != 128 || out.menu.glyphs.height != 128) out.menu.glyphs = {};
  if (ge::ui::Image back = pcx_picture(files, "conback", -1); !back.empty())
    out.menu.pictures.emplace("conback", std::move(back));
  uint64_t print = 0xcbf29ce484222325ULL;
  int printed = 0;
  for (size_t i = 0; i < std::size(kMenuPictures); i++) {
    const bool printing = i < kPrinted;
    ge::ui::Image pic = pcx_picture(files, kMenuPictures[i], 255, printing ? &print : nullptr);
    if (pic.empty()) continue;
    if (printing) printed++;
    out.menu.pictures.emplace(std::string(kMenuPictures[i]), std::move(pic));
  }
  for (int f = 0; f < kCursorFrames; f++) {
    const std::string name = "m_cursor" + std::to_string(f);
    if (ge::ui::Image pic = pcx_picture(files, name, 255); !pic.empty())
      out.menu.pictures.emplace(name, std::move(pic));
  }
  if (printed == kPrinted) out.menu.strip_print = print;
  return out;
}

const shooters_edge::menu::MenuTable& menu(const GameAdapter&) {
  return shooters_edge::menu::classic_menu();
}

// A palette of the files: the classic game's (`gfx/palette.lmp`) or the sequel's (the one its
// colour map picture carries); false when the files hold none.
bool classic_palette(const SearchPath& files, ge::classicenhance::Palette& out) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> lmp = find_file(files, "gfx/palette.lmp", store);
  if (lmp.size() < out.rgb.size()) return false;
  std::copy_n(lmp.begin(), out.rgb.size(), out.rgb.begin());
  return true;
}

bool own_palette(const SearchPath& files, ge::classicenhance::Palette& out) {
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, "pics/colormap.pcx", store);
  ge::assets::IndexedImage img;
  std::string error;
  if (bytes.empty() || !ge::assets::decode_pcx(bytes, "pics/colormap.pcx", &img, &error) ||
      img.palette.size() < out.rgb.size())
    return false;
  std::copy_n(img.palette.begin(), out.rgb.size(), out.rgb.begin());
  return true;
}

// Another game's model (classic remix's `sv_wear`: the classic game's things on the sequel's
// rules) in its own colours. A model of the first family keeps its skins as pictures through the
// classic palette, which stand in for them; a brush model's textures are put into the sequel's
// palette, each index the nearest colour, since a brush model has no stand-in: a glowing index
// (from `kFullbrightFirst`) the nearest glowing one, any other the nearest that does not glow.
void own_colours(const SearchPath& files, const std::string& name, OpenedModel& out) {
  ge::classicenhance::Palette classic;
  if (!classic_palette(files, classic)) return;
  if (out.form == ModelForm::morph && name.ends_with(".mdl")) {
    out.skins.resize(out.morph.skins.size());
    for (size_t k = 0; k < out.morph.skins.size(); k++) {
      const ge::morph::Skin& skin = out.morph.skins[k];
      if (out.skins[k].width > 0 || skin.indices.empty()) continue;
      ge::assets::Image img;
      img.width = skin.width;
      img.height = skin.height;
      img.rgba.resize(skin.indices.size() * 4);
      for (size_t i = 0; i < skin.indices.size(); i++) {
        std::copy_n(classic.rgb.begin() + size_t(skin.indices[i]) * 3, 3, img.rgba.begin() + i * 4);
        img.rgba[i * 4 + 3] = 255;
      }
      out.skins[k] = std::move(img);
    }
  }
  ge::classicenhance::Palette own;
  if (out.form != ModelForm::brush || !own_palette(files, own)) return;
  std::array<uint8_t, 256> nearest{};
  constexpr int kGlow = ge::classicenhance::kFullbrightFirst;
  for (int c = 0; c < 256; c++) {
    int best = 0, best_d = 1 << 30;
    for (int o = c < kGlow ? 0 : kGlow; o < (c < kGlow ? kGlow : 256); o++) {
      int d = 0;
      for (int k = 0; k < 3; k++) {
        const int e =
            int(classic.rgb[size_t(c) * 3 + size_t(k)]) - int(own.rgb[size_t(o) * 3 + size_t(k)]);
        d += e * e;
      }
      if (d < best_d) {
        best_d = d;
        best = o;
      }
    }
    nearest[size_t(c)] = uint8_t(best);
  }
  for (ge::brush::Texture& t : out.brush.textures)
    for (uint8_t& v : t.indexed) v = nearest[v];
}

// A player's model names no skin (the player chooses it): until a player's own choice reaches the
// window, his model wears its folder's `grunt` skin, else the male model's (the game's own
// fallbacks for a missing skin).
bool open_model(const GameAdapter& self, const SearchPath& files, const std::string& name,
                OpenedModel& out, std::string& error) {
  // A carried weapon's name ("#w_shotgun.md2"): a file in the player's model folder.
  if (!name.empty() && name[0] == '#')
    return open_model(self, files, "players/male/" + name.substr(1), out, error);
  if (!parts::open_model(self, files, name, out, error)) return false;
  if (name.ends_with(".mdl") || name.ends_with(".bsp")) own_colours(files, name, out);
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

// Its own held weapons, in its rules' weapon order (server/sequel_combat.hpp's table): each
// weapon's view model, authored at the eye, every frame kept. The hand shows the frame its rules'
// hand is on (held.cpp), so no `fire` clip. Each skin is the picture beside the model in its own
// palette; the sequel's skins have no glowing texels. While the rules wear the classic game's
// things (classic remix), `held_weapons` names them in the same order: the classic game's view
// models in the classic palette, which the hand steps by the rules' frames (held.cpp).
bool held_weapons(const GameAdapter& self, const SearchPath& files,
                  std::vector<ge::viewmodel::HeldModel>& out, std::string& why) {
  out.clear();
  why.clear();
  ge::classicenhance::Palette palette, classic;
  if (!own_palette(files, palette)) {
    why = "no pics/colormap.pcx in the game's files to colour the held weapons' skins";
    return false;
  }
  const bool worn = !self.game->held_weapons.empty();
  if (worn && !classic_palette(files, classic)) {
    why = "no gfx/palette.lmp in the files to colour the classic game's held weapons";
    return false;
  }
  int found = 0;
  std::string missing;
  for (size_t w = 0; w < size_t(server::kSqWeaponCount); w++) {
    const std::string name = worn && w < self.game->held_weapons.size()
                                 ? std::string(self.game->held_weapons[w])
                                 : std::string(server::kSqWeapons[w].view_model);
    OpenedModel m;
    std::string error;
    if (!open_model(self, files, name, m, error) || m.form != ModelForm::morph) {
      missing += missing.empty() ? "" : ", ";
      missing += name;
      if (!error.empty()) missing += " (" + error + ")";
      continue;
    }
    found++;
    const bool first_family = name.ends_with(".mdl");
    ge::viewmodel::HeldModel h =
        ge::classicenhance::held_from_morph(m.morph, first_family ? classic : palette, 0);
    if (!m.skins.empty() && m.skins[0].width > 0) h.skin.image = std::move(m.skins[0]);
    h.skin.glow.clear();
    out.push_back(std::move(h));
  }
  if (missing.empty()) return true;
  if (found > 0) why = "the game's files lack held weapons: " + missing;
  out.clear();
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
