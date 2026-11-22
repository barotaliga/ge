// The hub's match cards, the game's half: the game, its content, the board's look, the camera.
//
// ge:summary The hub cards' game half: a record's game, its content, charset, the still's camera
// ge:keywords hub match-card game-of content conchars still_view intermission spawn
// ge:skill net shooter
// ge:test tests/shooters_edge_hub_cards_test.cpp
#include "shooters_edge/hub/cards.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "classicenhance/profile.hpp"
#include "shooters_edge/client/classic_pictures.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "vfs/palette.hpp"

namespace shooters_edge::hub {

namespace pf = shooters_edge::profiles;
namespace cl = shooters_edge::client;

std::string hub_game_of(const ge::net::Status& s) {
  for (const auto& [k, v] : s.keys)
    if (k == "*game" && pf::find_game(v) != nullptr) return v;
  return "q1";
}

bool open_hub_content(std::string_view content, std::string_view game, SearchPath& out,
                      std::string& error) {
  const pf::GameProfile* g = pf::find_game(game);
  if (g == nullptr) {
    error = "no classic game is called '" + std::string(game) + "'";
    return false;
  }
  const ge::classicenhance::Profile& row = pf::content_of(*g);
  const std::string folder = (std::filesystem::path(content) / std::string(game)).string();
  return cl::open_game_dir(folder, std::string(row.base_dir), out, error, row);
}

bool hub_card_look(std::string_view content, std::string_view game, ge::hub::CardLook& out,
                   std::string& error) {
  SearchPath files;
  if (!open_hub_content(content, game, files, error)) return false;
  const pf::GameProfile* g = pf::find_game(game);
  const cl::ClassicPictures pics = cl::load_classic_pictures(files, false, {}, &cl::adapter_of(*g));
  const ge::ui::Image& sheet = pics.menu.glyphs;
  if (sheet.width < 16 || sheet.height < sheet.width) {
    error = "the packs hold no glyph sheet (conchars)";
    return false;
  }
  out.charset.width = sheet.width;
  out.charset.height = sheet.height;
  out.charset.rgba = sheet.rgba;
  // Quake's board: frags on the player's colours, the words plain; Quake II's: frags bare, the
  // words in the sheet's second (green) set.
  const bool q2 = game == "q2";
  out.score_box = !q2;
  out.words_alt = q2;
  // Quake's colour rows (0 to 13, sixteen shades each) in the shade its board fills a frag box
  // with: the row's ninth.
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = cl::find_file(files, "gfx/palette.lmp", store);
  ge::vfs::Palette pal;
  std::string ignored;
  if (!q2 && ge::vfs::read_palette(bytes, &pal, &ignored))
    for (int row = 0; row < 14; row++) {
      const size_t i = size_t(row * 16 + 8) * 3;
      out.rows.push_back(uint32_t(pal.rgb[i]) << 16 | uint32_t(pal.rgb[i + 1]) << 8 |
                         pal.rgb[i + 2]);
    }
  return true;
}

namespace {

bool three(const std::string& text, float out[3]) {
  return std::sscanf(text.c_str(), "%f %f %f", &out[0], &out[1], &out[2]) == 3;
}

}  // namespace

StillView still_view(const ge::brush::Map& map) {
  StillView v;
  // The intermission spot (its origin is the eye), else a spawn (its origin is the feet).
  constexpr const char* kSpots[] = {"info_intermission", "info_player_intermission",
                                    "info_player_deathmatch", "info_player_start"};
  for (const char* kind : kSpots) {
    for (const ge::brush::Entity& e : map.entities) {
      if (e.get("classname") != kind || !three(e.get("origin"), v.origin)) continue;
      float a[3] = {0, 0, 0};
      if (three(e.get("mangle"), a) || three(e.get("angles"), a)) {
        v.pitch = a[0];
        v.yaw = a[1];
      } else {
        v.yaw = float(std::atof(e.get("angle").c_str()));
      }
      v.from = kind;
      const bool spot = v.from == "info_intermission" || v.from == "info_player_intermission";
      if (!spot) v.origin[2] += 22;  // the eye over a standing player's origin
      return v;
    }
  }
  if (!map.models.empty()) {
    for (int k = 0; k < 3; k++) v.origin[k] = (map.models[0].mins[k] + map.models[0].maxs[k]) / 2;
  }
  return v;
}

}  // namespace shooters_edge::hub
