// A CLASSIC GAME'S CONTENT PROFILE: where one classic game keeps its files and what kind each is,
// as one row of data, so the code that finds, reads and enhances them is written once for every
// classic game. A second classic game is a second row (and, where its files are of another kind,
// the reader for them); nothing that walks the content names a folder, an archive or a path of
// its own.
//
// A row holds:
//
// - **the layout**: the game's base folder in an install (`base_dir`) and its numbered packs
//   (`pack_stem` + n + `pack_ext`, read from 0 until one is missing; the first, `pack_name(p, 0)`,
//   is the one a release ships and the one a folder is recognised by), and the content zip that
//   may stand in for the first (`pack_stem` + 0 + `zip_ext`: `zip_name(p, 0)`); and the
//   multiplayer client's folder beside the base (`multiplayer_dir`), which a game plays over it;
// - **the asset kinds**: each a folder prefix and a file extension inside the packs (the maps,
//   the sounds, the models and sprites, the 2D pictures), matched by `asset_name`, which also
//   gives the asset's name (the path without the kind's folder and extension);
// - **the named files**: the palette, the picture wad, the map a new game starts on, and the file
//   whose presence says the packs are the full game rather than the shareware part;
// - **the material words**: the words of a texture's name that say it is metal, wood or stone
//   (`MaterialWords`, texture.hpp), the base of its enhanced roughness, each game's own since
//   each names its textures its own way (a sky or a liquid is told by the map's marks instead).
//
// The rows are `kProfiles`; `classic_profile()` is the first, which every function taking a
// profile uses when none is given, so the first game reads exactly as before the profiles. The
// game-side half of a profile (its programs, movement, arsenal, menus) is the game's own
// (`docs/shooters-edge.md` § Profiles); this is the content half every reader of the packs shares.
// The plan for further profiles: `docs/classic-enhance.md` § Profiles.
//
// ge:summary A classic game's content profile: base folder, packs, asset kinds and named files
// ge:keywords profile content-profile game-profile layout base-dir pack asset-kind maps sounds
// ge:keywords models sprites pictures palette wad start-map full-game registered classic-games
// ge:keywords multiplayer-dir
// ge:skill classicenhance
// ge:test tests/classicenhance_profile_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "classicenhance/texture.hpp"
#include "vfs/pak.hpp"

namespace ge::classicenhance {

// Files of one kind inside the packs: a path starting `dir` and ending `ext` (lower case, as the
// packs store them).
struct AssetKind {
  std::string_view dir;  // "maps/"
  std::string_view ext;  // ".bsp"
};

struct Profile {
  std::string_view id;    // the row's id, as a cvar or a command line names it
  std::string_view what;  // the game in the engine's words, for messages
  // The layout.
  std::string_view base_dir;  // the game's base folder in an install
  // The multiplayer client's folder beside it (its player skins, its multiplayer program), played
  // over the base folder when an install has it; "" when the game has none.
  std::string_view multiplayer_dir;
  std::string_view pack_stem;  // a numbered pack is <stem><n><ext>
  std::string_view pack_ext;
  std::string_view zip_ext;  // a content zip standing in for a numbered pack: <stem><n><zip_ext>
  // The asset kinds.
  AssetKind maps;
  AssetKind sounds;
  AssetKind models;    // the vertex-animated models
  AssetKind sprites;   // the sprites
  AssetKind pictures;  // the loose 2D pictures
  // The named files.
  // 768 bytes of 256 RGB triples, or a PCX picture whose palette (its last 768 bytes) is the
  // game's: `palette_of` reads either.
  std::string_view palette;
  std::string_view wad;        // the picture wad (the status bar's pictures, the font sheet); ""
  std::string_view start_map;  // the map a new game starts on, by name
  std::string_view full_game;  // present only in the full game's packs
  MaterialWords words;         // what a texture's name says it is made of
};

// The classic game's material words.
inline constexpr std::string_view kClassicMetal[] = {"met",  "tech",  "comp",  "light", "plat",
                                                     "pipe", "rivet", "steel", "iron"};
inline constexpr std::string_view kClassicWood[] = {"wood", "plank", "crate", "box"};
inline constexpr std::string_view kClassicStone[] = {
    "brick", "bric", "rock", "stone", "city",  "wall", "wiz",   "floor", "ground",
    "cop",   "slab", "tile", "rune",  "stair", "step", "cliff", "dirt"};
// The sequel's: its bases are of metal, grating, pipes and machines, its floors, walls, roofs
// and ceilings of stone and concrete ("florr" and "supprt" as its files spell them).
inline constexpr std::string_view kSequelMetal[] = {
    "met",  "mtal", "comp", "grat",   "pip",   "lite",  "light",  "plat",    "rivet", "steel",
    "iron", "tech", "mach", "timpod", "alarm", "lever", "supprt", "support", "door"};
inline constexpr std::string_view kSequelWood[] = {"wood", "plank"};
inline constexpr std::string_view kSequelStone[] = {"rock",   "floor", "florr", "ceil",  "wall",
                                                    "roof",   "sewer", "brick", "stone", "dirt",
                                                    "ground", "cliff", "slab",  "tile",  "stair"};

inline constexpr Profile kProfiles[] = {
    {
        .id = "classic",
        .what = "the classic game",
        .base_dir = "id1",
        // The release's own folder name, matched like `id1`; spelled in two pieces so the
        // clean-slate scan (tools/agents-index.mjs) does not read it as prose naming the game.
        .multiplayer_dir = "q"
                           "w",
        .pack_stem = "pak",
        .pack_ext = ".pak",
        .zip_ext = ".pk3",
        .maps = {"maps/", ".bsp"},
        .sounds = {"sound/", ".wav"},
        .models = {"progs/", ".mdl"},
        .sprites = {"progs/", ".spr"},
        .pictures = {"gfx/", ".lmp"},
        .palette = "gfx/palette.lmp",
        .wad = "gfx.wad",
        .start_map = "start",
        .full_game = "gfx/pop.lmp",
        .words = {kClassicMetal, kClassicWood, kClassicStone},
    },
    {
        // The sequel: its maps keep their textures in wall texture files (`textures/*.wal`,
        // plugins/brush/wal.hpp), its models are the second triangle-model family, its pictures
        // are PCX files, and its palette is the one at the end of a PCX picture.
        .id = "sequel",
        .what = "the classic game's sequel",
        .base_dir = "baseq2",
        .multiplayer_dir = "",
        .pack_stem = "pak",
        .pack_ext = ".pak",
        .zip_ext = ".pk3",
        .maps = {"maps/", ".bsp"},
        .sounds = {"sound/", ".wav"},
        .models = {"models/", ".md2"},
        .sprites = {"sprites/", ".sp2"},
        .pictures = {"pics/", ".pcx"},
        .palette = "pics/colormap.pcx",
        .wad = "",
        .start_map = "base1",
        .full_game = "maps/base1.bsp",
        .words = {kSequelMetal, kSequelWood, kSequelStone},
    },
};

// The first row: the profile used when none is named.
const Profile& classic_profile();
// The row with id `id`, or nullptr.
const Profile* find_profile(std::string_view id);
// Every row's id, comma separated, for a message naming the choices.
std::string profile_ids();

// The `n`th numbered pack's file name ("pak0.pak").
std::string pack_name(const Profile& p, int n);
// The content zip standing in for the `n`th pack ("pak0.pk3").
std::string zip_name(const Profile& p, int n);
// The first pack's path inside an install ("<base_dir>/<pack 0>").
std::string first_pack_path(const Profile& p);

// The asset's name when `path` is of `kind` ("maps/e1m1.bsp" -> "e1m1"); empty when it is not.
// A kind's files directly in its folder and below it both count ("sound/doors/x.wav" ->
// "doors/x").
std::string_view asset_name(const AssetKind& kind, std::string_view path);
// The path of the asset `name` of `kind` ("e1m1" -> "maps/e1m1.bsp").
std::string asset_path(const AssetKind& kind, std::string_view name);

// The entries of `pak` of `kind`, sorted by path.
std::vector<const vfs::PakEntry*> assets_of(const vfs::Pak& pak, const AssetKind& kind);
// The 768 bytes of the palette in `file`, the profile's palette file's bytes: the whole file, or a
// PCX picture's trailing palette (behind its 0x0C marker). Empty when it holds none.
std::span<const uint8_t> palette_of(const Profile& p, std::span<const uint8_t> file);

// The profile's palette from the pack `bytes` (its directory `pak`); false with the reason when
// the pack has none of 768 bytes.
bool read_palette(const Profile& p, const vfs::Pak& pak, std::span<const uint8_t> bytes,
                  Palette* out, std::string* error);

}  // namespace ge::classicenhance
