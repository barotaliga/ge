// THE OPEN CALLS EVERY SHOOTER GAME SHARES: a classic game's files read by what they are, never by
// which game they came from, through the search path (content/files.hpp). A map of the 38 family
// copied into the first game's folder draws with its wall textures, a model is read by its kind,
// so the games' files mix where a remix asks, and a later classic game reads them with no code of
// its own. A game's adapter (Shooter's Edge's client/game_adapter.hpp) is a table of these and the
// few calls that are truly its own (its pictures, its menu, its demos, its held weapons).
//
// - **map_info**: one map's name, title (the world's message), family and texture count, the map
//   found by the content row's maps (`maps/<name>.bsp`).
// - **map_textures**: a map carrying its pictures (29, BSP2) gives them; any other family's come
//   from their wall texture files (`textures/<name>.wal`, brush/wal.hpp); then a PNG or TGA of a
//   texture's name stands in (`textures/<map>/<name>`, `textures/<name>`, a `*` written `#`).
// - **open_model** by the file's kind: `.mdl` and `.md2` (morph targets; a `.mdl` skin replaced
//   by `<model>_<n>.png`, a `.md2`'s skins read from their pictures), `.spr` and `.sp2`, `.bsp`,
//   and a skeletal `.iqm` (or a file whose first bytes say it is one) posed into frames.
// - **open_sound**: a sound under the content row's sound folder (`sound/`), read from its WAV.
// - **find_config**: a config file `exec` names, from the files (never a path climbing out).
// - **open_locations**: a map's place names, `locs/<map>.loc`, into `Locs` (locs.hpp).
// - **stand_in**: the first PNG or TGA of a list of stems the files hold, read; one that will not
//   read is named on the status line and the next is tried.
//
// These are load-time calls, never a frame's.
//
// ge:summary The open calls every shooter game shares: models by kind, sounds, map textures, maps
// ge:keywords content open_model open_sound map_textures map_info find_config open_locations
// ge:keywords stand_in replacement-texture replacement-skin model-form opened-model wal iqm
// ge:keywords mixed-files remix load-time
// ge:skill classic shooter
// ge:test tests/classic_content_open_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/image.hpp"
#include "audio/pcm.hpp"
#include "brush/map.hpp"
#include "brush/pictures.hpp"
#include "classic/content/files.hpp"
#include "classic/locs.hpp"
#include "classicenhance/profile.hpp"
#include "morph/model.hpp"

namespace ge::classic::content {

// `s` in lower case (ASCII), for matching a file's name or extension.
std::string folded(std::string_view s);

// What a map is, read off its file.
struct MapInfo {
  std::string name;   // as the game names it ("dm6")
  std::string title;  // the world's message, "" when it has none
  brush::Family family = brush::Family::v29;
  uint32_t textures = 0;
};

// The shared form a model file is read into.
enum class ModelForm : uint8_t { none, morph, sprite, brush };

// A model the files hold, read.
struct OpenedModel {
  ModelForm form = ModelForm::none;
  morph::Model morph;    // form morph
  morph::Sprite sprite;  // form sprite
  brush::Map brush;      // form brush
  // A full-colour picture with alpha standing in for each of `morph`'s skins (width 0: none).
  std::vector<assets::Image> skins;
};

// The map `name`: false with the reason and the next step (a player's words) when the files do
// not hold it or it does not read.
bool map_info(const SearchPath& files, const classicenhance::Profile& content,
              std::string_view name, MapInfo& out, std::string& error);
// The map's textures' pixels, one entry a texture in the map's order (see above); `store` holds
// the bytes of wall texture files read loose.
std::vector<brush::MapPicture> map_textures(const SearchPath& files, std::string_view map,
                                            std::span<const uint8_t> map_file,
                                            std::vector<uint8_t>& store);
// The model `name` ("progs/armor.mdl"): false with `error` empty when the files do not hold it,
// else with why it did not read.
bool open_model(const SearchPath& files, const std::string& name, OpenedModel& out,
                std::string& error);
// The sound `name` ("weapons/r_exp3.wav") under the content row's sound folder: false with `error`
// empty when the files do not hold it.
bool open_sound(const SearchPath& files, const classicenhance::Profile& content,
                const std::string& name, audio::Pcm& out, std::string& error);
bool find_config(const SearchPath& files, std::string_view name, std::string& text);
bool open_locations(const SearchPath& files, std::string_view map, ge::classic::Locs& out);
// The first picture file of `stems` the files hold, each tried as .png then .tga, read; one that
// will not read is named (`own` says what is drawn instead) and the next is tried.
std::optional<assets::Image> stand_in(const SearchPath& files, std::span<const std::string> stems,
                                      const std::string& own);

}  // namespace ge::classic::content
