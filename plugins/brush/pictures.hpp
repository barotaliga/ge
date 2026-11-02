// THE PICTURES A MAP FILE CARRIES ITSELF: a version 29 or BSP2 map holds every texture's pixels in
// its own file (lump 2: a miptex record a texture, its name, its size and four levels of palette
// indices, each half the last across); a version 38 or 46 map holds only the names, its pictures
// living in other files of the game. `read_map_pictures` is the first kind's reader: one entry a
// texture of the map, in the map's texture order, each level a VIEW into the file (valid while
// the file's bytes are), so the scene's atlas (app/draw_data.hpp) and a game's own reader of its
// textures take the same pixels without a copy. A game's own reader may also hand a full-colour
// picture with alpha in a texture's place (`MapPicture::rgba`).
//
// A record that does not fit the lump (a bad offset, a level past the end) is an entry with no
// pixels (`width` 0, every level null): the scene draws a check for it, as before. A file of
// another family, or one too short for a lump directory, gives no entries.
//
// ge:summary The pictures a 29 or BSP2 map carries itself: each miptex's size and four levels
// ge:keywords pictures miptex mips texture-pixels lump palette-indices map-pictures embedded
// ge:keywords read_map_pictures v29 bsp2 textures full-colour rgba alpha replacement
// ge:skill brush
// ge:test tests/brush_pictures_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ge::brush {

struct MapPicture {
  std::string name;                // as stored, up to 16 bytes
  uint32_t width = 0, height = 0;  // level 0's; 0 when the record does not fit
  const uint8_t* level[4] = {nullptr, nullptr, nullptr, nullptr};  // (width >> k) x (height >> k)
  // A FULL-COLOUR PICTURE in its place (a game's own file of the texture: a PNG or a TGA, read by
  // the game's adapter): RGBA8 from the top row, straight alpha, `rgba_width` x `rgba_height`,
  // any size. Its alpha is see-through (0) to solid (255): the scene cuts its faces where it is
  // under a half. Empty: the indexed levels above are the picture.
  std::vector<uint8_t> rgba;
  uint32_t rgba_width = 0, rgba_height = 0;
};

// Every texture's pictures in a 29 or BSP2 map file; none for another family.
std::vector<MapPicture> read_map_pictures(std::span<const uint8_t> file);

// True when the file is a family that carries its pictures (29 or BSP2), by its first bytes.
bool carries_pictures(std::span<const uint8_t> file);

}  // namespace ge::brush
