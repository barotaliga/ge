// The first game's adapter (game_adapter.hpp): the shared calls (adapter_parts.hpp: its mods,
// QC programs, maps and their pictures, models, sounds under `sound/`, config files and place
// names) and its own: its pictures from the picture wad and `gfx/*.lmp`, its classic menu pages,
// its held weapons (`progs/v_*.mdl` with their firing frames), and its demos, the recordings
// client/demo.hpp reads.
//
// ge:summary The first classic game's adapter: mod folders, QC programs, maps, pictures, demos
// ge:keywords classic-adapter list_mods find_program list_maps map_info map_textures list_demos
// ge:keywords find_demo open_demo mod-folder qwprogs replacement-texture png tga open_model
// ge:keywords open_sound replacement-skin mdl iqm skeletal-model spr wav pictures
// ge:keywords replacement-picture gfx-wad lmp
// ge:keywords find_config cfg open_locations loc-file held_weapons v-mdl classic_menu modes
// ge:skill shooter
// ge:test tests/shooters_edge_game_adapter_test.cpp
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <optional>
#include <set>
#include <span>
#include <system_error>
#include <utility>

#include "assets/image.hpp"
#include "assets/iqm.hpp"
#include "assets/pcx.hpp"
#include "assets/tga.hpp"
#include "brush/read.hpp"
#include "classicenhance/cached.hpp"
#include "classicenhance/weapons.hpp"
#include "core/output.hpp"
#include "morph/read.hpp"
#include "morph/skeletal.hpp"
#include "progs/layout.hpp"
#include "shooters_edge/client/adapter_parts.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/menu/classic_art.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::client {

namespace {

namespace fs = std::filesystem;
namespace pf = shooters_edge::profiles;
using parts::stand_in;

// A model as the classic game opens it. Another game's model of the second family (classic
// remix's `cr_items`: a weapon or an armour of the sequel's) keeps its skins' own colours: each
// picture through its own palette stands in for the skin, so the classic palette never paints it.
bool open_model_(const GameAdapter& self, const SearchPath& files, const std::string& name,
                 OpenedModel& out, std::string& error) {
  if (!parts::open_model(self, files, name, out, error)) return false;
  if (out.form != ModelForm::morph || !name.ends_with(".md2")) return true;
  out.skins.assign(out.morph.skins.size(), {});
  for (size_t s = 0; s < out.morph.skin_paths.size() && s < out.skins.size(); s++) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = find_file(files, out.morph.skin_paths[s], store);
    ge::assets::IndexedImage img;
    std::string why;
    if (bytes.empty() || !ge::assets::decode_pcx(bytes, out.morph.skin_paths[s], &img, &why))
      continue;
    out.skins[s] = {img.width, img.height, ge::assets::pcx_rgba(img)};
  }
  return true;
}

bool held_weapons(const GameAdapter& self, const SearchPath& files,
                  std::vector<ge::viewmodel::HeldModel>& out, std::string& why) {
  out.clear();
  why.clear();
  std::vector<uint8_t> store;
  const std::span<const uint8_t> lmp = find_file(files, "gfx/palette.lmp", store);
  ge::classicenhance::Palette palette;
  if (lmp.size() < palette.rgb.size()) {
    why = "no gfx/palette.lmp in the game's files to colour the held weapons' skins";
    return false;
  }
  std::copy_n(lmp.begin(), palette.rgb.size(), palette.rgb.begin());
  int found = 0;
  std::string missing;
  for (const std::string_view name : self.game->held_weapons) {
    OpenedModel m;
    std::string error;
    if (!open_model_(self, files, std::string(name), m, error) || m.form != ModelForm::morph) {
      missing += missing.empty() ? "" : ", ";
      missing.append(name);
      if (!error.empty()) missing += " (" + error + ")";
      continue;
    }
    found++;
    // A view model of another game's (classic remix): its rest and firing frames alone, in the
    // order the hand reads them (profiles/remix_arsenal.hpp).
    if (const pf::HeldFrames* f = pf::held_frames(name); f != nullptr) {
      const auto at = [&m](int i) {
        return m.morph.frames[size_t(std::clamp<int>(i, 0, int(m.morph.frames.size()) - 1))];
      };
      std::vector<ge::morph::Frame> kept{at(f->rest)};
      for (int i = f->fire_first; i <= f->fire_last; ++i) kept.push_back(at(i));
      m.morph.frames = std::move(kept);
    }
    ge::viewmodel::HeldModel h = ge::classicenhance::held_from_morph(m.morph, palette, 0);
    if (!m.skins.empty() && m.skins[0].width > 0) {  // a picture beside it, alpha and all
      h.skin.image = std::move(m.skins[0]);
      h.skin.glow.clear();
    }
    // The firing frames after the rest frame, at the classic rate. A weapon that fires every
    // frame (the nailguns, the lightning gun) steps them one a shot, round and round, each shot
    // from the barrel its frame shows firing; the others play them through on each shot, or, for
    // firing frames that are several takes (the axe's two swings), one take a shot: each its own
    // `fire` clip, which the hand picks between.
    const ge::classicenhance::ClassicFire* fire = ge::classicenhance::classic_fire_of(name);
    // The lightning gun's flash is an arc across its prongs: its ends, for the enhanced arc.
    if (fire != nullptr && fire->arc) ge::classicenhance::find_arc(h);
    const int firing = int(h.frames.size()) - 1;
    const int takes =
        fire != nullptr && fire->takes > 1 && firing % fire->takes == 0 ? fire->takes : 1;
    for (int k = 0; k < takes && firing > 0; ++k)
      h.clips.push_back(
          {.name = "fire",
           .first = 1 + k * (firing / takes),
           .count = firing / takes,
           .fps = 1.0 / ge::classicenhance::kFrameSeconds,
           .frame_a_shot = fire != nullptr && fire->refire_s <= ge::classicenhance::kFrameSeconds});
    out.push_back(std::move(h));
  }
  if (missing.empty()) return true;
  if (found > 0) why = "the game's files lack held weapons: " + missing;
  out.clear();
  return false;
}

const shooters_edge::menu::MenuTable& menu(const GameAdapter&) {
  return shooters_edge::menu::classic_menu();
}

ClassicPictures pictures(const GameAdapter&, const SearchPath& files, bool enhance,
                         const fs::path& cache_root) {
  ClassicPictures out;
  out.enhanced = enhance;
  if (enhance && !content_packs(files).empty())
    out.cache = ge::classicenhance::find_cache(content_packs(files), cache_root);
  const auto find = [&files](std::string_view p, std::vector<uint8_t>& store) {
    return find_file(files, p, store);
  };
  const fs::path& dir = out.cache;
  // A picture's copy by its cache name ("sbar", "conchars", "gfx/qplaque"): a PNG or TGA of its
  // name under gfx/ when its size is a whole multiple of the pack's, else the cache's.
  const auto upgrade = [&files, &dir](std::string_view name, int w, int h, ge::ui::Image* img) {
    std::string base(name);
    if (base.rfind("gfx/", 0) == 0) base.erase(0, 4);
    const std::string stem[] = {"gfx/" + base};
    std::optional<ge::assets::Image> own = stand_in(files, stem, "the pack's own " + base);
    if (own && w > 0 && h > 0) {
      const int k = own->width / w;
      if (k >= 1 && own->width == k * w && own->height == k * h) {
        img->width = own->width;
        img->height = own->height;
        img->rgba = std::move(own->rgba);
        return static_cast<uint32_t>(k);
      }
      ge::core::output::status("shooters_edge: gfx/" + base + "'s picture is " +
                               std::to_string(own->width) + " x " + std::to_string(own->height) +
                               ", not a whole multiple of the pack's " + std::to_string(w) + " x " +
                               std::to_string(h) +
                               "; save it at that size or a "
                               "multiple of it");
    }
    if (dir.empty()) return uint32_t{0};
    ge::assets::Image big;
    const uint32_t k = ge::classicenhance::cached_picture(dir, name, static_cast<uint32_t>(w),
                                                          static_cast<uint32_t>(h), &big);
    if (k == 0) return k;
    img->width = big.width;
    img->height = big.height;
    img->rgba = std::move(big.rgba);
    return k;
  };
  std::string error;
  if (!menu::load_classic_art(find, &out.menu, &error, upgrade)) out.menu = {};
  if (!hud::classic_art_from_packs(find, &out.hud, &out.hud_why, upgrade)) out.hud = {};
  return out;
}

bool open_demo_(const GameAdapter&, std::vector<uint8_t> bytes, Demo& out, std::string& error) {
  if (!open_demo(std::move(bytes), out, error)) return false;
  if (out.state.format != DemoFormat::sequel) return true;
  error = "This recording is of another game. Choose that game to watch it.";  // its own assets
  out = Demo{};
  return false;
}

const GameAdapter kClassic{.game = &pf::kGameProfiles[0],
                           .list_mods = parts::list_mods,
                           .find_program = parts::find_program,
                           .list_maps = parts::list_maps,
                           .map_info = parts::map_info,
                           .map_textures = parts::map_textures,
                           .open_model = open_model_,
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

const GameAdapter& classic_adapter() { return kClassic; }

}  // namespace shooters_edge::client
