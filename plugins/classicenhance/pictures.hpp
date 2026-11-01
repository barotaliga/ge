// THE PICTURES: the pack's 2D art (the status bar and its digits, the portraits, the icons, the
// console's font sheet and the menus' pictures) read out of the pack and turned from palette
// indices into colours, for a HUD or a menu to draw in place of its own art.
//
// The pictures come from two places: every picture lump and the font sheet in the pack's
// `gfx.wad`, named by their lump names in lower case (`sbar`, `num_0`, `face1`, `conchars`), and
// every picture file under `gfx/` in the pack (`gfx/ranking.lmp` is `gfx/ranking`). A file under
// `gfx/` that is not a picture (the palette, the colour map) is left out. Each picture's colours
// are the pack's palette's; index 255 is see-through in a picture, and index 0 in the font sheet,
// as the classic renderer drew them. `picture_rgba` scales and filters nothing: a HUD draws such a
// picture at a whole number of screen pixels a pixel, sampled nearest.
//
// ENHANCED (`enhance_picture`), a picture is upscaled the way a texture's albedo is (texture.hpp's
// `upscale_albedo`: the trained network, else the edge-aware kernel), with its see-through texels
// kept see-through:
//
// 1. the colours of the opaque texels are spread out into the see-through ones round them, so
//    the upscale never pulls the see-through index's colour (or black) into an edge;
// 2. the picture is padded by its own edge texels, so it is upscaled as it stands and not as a
//    tile that wraps; the font sheet is done a cell at a time, so no glyph bleeds into the next;
// 3. the alpha is the see-through mask sampled bilinearly at the new size and steepened about
//    one half, so a stroke one texel wide stays one texel wide, a staircase becomes a smooth
//    edge, and the edge is about a pixel and a quarter soft at the new size.
//
// A HUD draws the result at `scale` picture pixels an art pixel, filtered.
//
// ge:summary The pack's 2D pictures (gfx.wad and gfx/*.lmp) read, turned into RGBA and upscaled
// ge:keywords classicenhance pictures wad lmp status-bar hud menu conchars palette transparent
// ge:keywords enhance_picture upscale 2d-art glyph-sheet alpha bleed charset
// ge:skill classicenhance
// ge:test tests/classicenhance_pictures_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "assets/image.hpp"
#include "classicenhance/profile.hpp"
#include "classicenhance/texture.hpp"
#include "vfs/wad.hpp"

namespace ge::classicenhance {

struct PackPicture {
  std::string name;       // `sbar`, `conchars`, `gfx/ranking`: the file's path under pictures/
  std::string from;       // `gfx.wad` or the picture's path in the pack
  int transparent = 255;  // the index drawn see-through
  vfs::IndexedPicture picture;
};

// Every picture in `pack`, the wad's in its directory's order then `gfx/`'s by path, with the
// palette; false with the reason when the pack has no palette or no gfx.wad, or the wad is
// damaged. A wad lump that is neither a picture nor the font sheet is skipped.
bool pack_pictures(std::span<const uint8_t> pack, Palette* palette, std::vector<PackPicture>* out,
                   std::string* error, const Profile& profile = classic_profile());

// The picture in the palette's colours, `transparent` drawn with alpha 0.
assets::Image picture_rgba(const vfs::IndexedPicture& picture, const Palette& palette,
                           int transparent);

// The picture upscaled `options.scale` times (above): (width × scale) × (height × scale) RGBA,
// sRGB, the see-through texels at alpha 0 carrying their neighbours' colours.
assets::Image enhance_picture(const PackPicture& picture, const Palette& palette,
                              const EnhanceOptions& options = {});

}  // namespace ge::classicenhance
