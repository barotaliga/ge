// THE WALL TEXTURE FILE OF THE VERSION 38 FAMILY: a map of that family names its textures and keeps
// their pixels in files of their own beside it, one a texture (`textures/<name>.wal`). A file is a
// 100-byte head (the texture's own name, its size, the four levels' offsets, the name of the next
// frame of its animation, and the surface flags, contents and value a map compiler copies from it)
// and four levels of palette indices, each half the last across. `read_wal` reads one into the
// same `MapPicture` a map that carries its pictures gives (pictures.hpp), each level a VIEW into
// the file's bytes, so the scene's atlas takes either the same way. `wal_path` is where a texture
// of a map is looked for; `write_wal` writes one, for fixtures and tests.
//
// A file too short for its head, of a size 0 or past 4096 texels across, or with a level outside
// it is refused with the reason: a texture with no pixels is then drawn as a check.
//
// ge:summary Reads and writes the version 38 family's wall texture files (head and four mips)
// ge:keywords wal wall-texture texture-file mips palette-indices read_wal write_wal wal_path
// ge:keywords v38 external-textures map-pictures next-frame surface-flags contents
// ge:skill brush
// ge:test tests/brush_wal_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "brush/pictures.hpp"

namespace ge::brush {

inline constexpr size_t kWalHead = 100;

// What a wall texture's head says besides its pixels.
struct WalHead {
  std::string name;  // its own name, up to 32 bytes
  std::string next;  // the next frame of its animation, "" none
  uint32_t flags = 0, contents = 0;
  int32_t value = 0;
};

// The file `bytes` read into `out` (named `out->name` as the head names it; levels view `bytes`)
// and, when given, `head`. False with the reason, `where` naming the file in it.
bool read_wal(std::span<const uint8_t> bytes, std::string_view where, MapPicture* out,
              std::string* error, WalHead* head = nullptr);

// The path of a map's texture `name` in the game's files ("e1u1/floor1_3" ->
// "textures/e1u1/floor1_3.wal").
std::string wal_path(std::string_view name);

// THE PICTURES OF A MAP THAT KEEPS THEM OUTSIDE (38): one entry a texture of `map`, in its order,
// from `files[i]`, the bytes of texture i's file (`wal_path` of its name) or empty when the game's
// files hold none. An entry whose file is missing or does not read has its name and no pixels;
// each such texture is one line in `faults` (when given). The levels view `files`' bytes.
std::vector<MapPicture> wal_pictures(const Map& map,
                                     std::span<const std::span<const uint8_t>> files,
                                     std::vector<std::string>* faults = nullptr);

// A wall texture of `width` x `height` (each a multiple of 8) from its full-size `indices` (rows
// of `width`), its smaller levels each the top-left texel of every 2 x 2.
std::vector<uint8_t> write_wal(const WalHead& head, uint32_t width, uint32_t height,
                               std::span<const uint8_t> indices);

}  // namespace ge::brush
