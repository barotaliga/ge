// THE CLASSIC PICTURES: the player's own copy of the classic content's status bar pictures read
// into the HUD's set (art.hpp) by what each shows, from either of two places that hold the same
// pixels:
//
//   - the PACKS themselves (`classic_art_from_packs`): every picture lump of the packs' graphics
//     archive (`gfx.wad`) and its glyph sheet, turned into colours through `gfx/palette.lmp`
//     (index 255 see-through in a picture, index 0 in the glyph sheet). This is what the window
//     reads at start, so the classic status bar shows with no tool run first;
//   - classic-enhance's local CACHE (`classic_art(dir)`): `pictures/<name>.png`, the same
//     pictures as its pictures stage left them, for a lab or a tool that has no packs open.
//
// They are drawn at one picture pixel per art pixel, sampled nearest, so the classic status bar
// looks as it did; the counts on the inventory bar are drawn from the pack's own glyph sheet,
// its gold figures included.
//
// ENHANCED (`ce_pictures`, on by default): given an `upgrade`, each picture is swapped for
// classic-enhance's upscaled copy of it (its pictures stage, `cached_picture`), and the set's
// density is that copy's scale, so the same status bar is drawn at four picture pixels an art
// pixel, filtered. A picture the cache lacks is the pack's own scaled up nearest to the same
// density, so one set never mixes sizes.
//
// THE WINDOW'S PICK (`hud_art`): `auto` draws the packs' pictures when the packs carry the whole
// set and the free art when they do not (the arena with no packs); `classic` asks for the packs'
// (the free art still drawn, and the reason said, when they lack one); `free` always draws the
// game's own. `pick_art` is that rule, for the window, the labs and the tests alike.
//
// Nothing here ships or is committed: the pictures are read at run time from the player's
// packs or cache. The crosshair is the glyph sheet's '+', as the classic game drew it; the pack
// has no health icon, so the looks that show one show the portrait.
//
// ge:summary Shooter's Edge's HUD drawn from the player's classic pictures: the packs or the cache
// ge:keywords hud classic-art classic-enhance cache pictures png status-bar portrait digits
// ge:keywords glyph-sheet conchars crosshair gfx-wad palette packs hud_art pick_art free-art
// ge:keywords enhanced upgrade ce_pictures density upscaled
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "shooters_edge/hud/art.hpp"

namespace shooters_edge::hud {

// A picture by the pack's name (`sbar`, `num_0`, `conchars`) into `*out`; false when there is
// none.
using PictureFind = std::function<bool(std::string_view name, ge::ui::Image* out)>;

// The classic set from whatever `find` reads; false with the first missing picture named in
// `*error` (`where` says where the pictures were looked for).
bool classic_art(const PictureFind& find, std::string_view where, HudArt* out, std::string* error);

// The classic set from the PNGs in `dir` (classic-enhance's `pictures/`, or `pictures/enhanced/`
// for its upscaled copies, the density read off the bar's width); false with the first missing or
// unreadable picture named in `*error`.
bool classic_art(const std::filesystem::path& dir, HudArt* out, std::string* error);

// A file by its path in the game ("gfx.wad"): a view into `store` or into a pack, empty when no
// place has it (the client's `find_file`).
using FileFind =
    std::function<std::span<const uint8_t>(std::string_view path, std::vector<uint8_t>& store)>;

// A picture's enhanced copy by the pack's name (`sbar`, `conchars`) for the pack's `w` x `h`
// into `*out`: the multiple of `w` x `h` it is at, or 0 to keep the pack's own (the window binds
// classic-enhance's `cached_picture` over the packs' cache).
using PictureUpgrade =
    std::function<uint32_t(std::string_view name, int w, int h, ge::ui::Image* out)>;

// The classic set straight from the packs' `gfx.wad` in `gfx/palette.lmp`'s colours, each picture
// upgraded through `upgrade` when it is set (above); false with the reason when the packs lack the
// palette, the archive or a picture of the set.
bool classic_art_from_packs(const FileFind& find, HudArt* out, std::string* error,
                            const PictureUpgrade& upgrade = {});

// The art the HUD draws by `hud_art` (`auto`, `classic`, `free`) given the packs' set (`classic`
// empty when the packs did not carry one): the packs' set or the free one.
const HudArt& pick_art(std::string_view hud_art, const HudArt& classic, const HudArt& free);

// Registers `hud_art` over `*value` (`value` must outlive `con`).
void add_art_cvar(ge::console::Console& con, std::string* value);

}  // namespace shooters_edge::hud
