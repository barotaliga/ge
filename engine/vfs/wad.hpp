// A WAD ARCHIVE and the classic PICTURE format: the 2D art a classic game's status bar, menus and
// console are drawn from. A wad (magic `WAD2`) is a 12-byte header (the magic, the number of
// lumps, the directory's offset), the lumps' bytes and a directory of 32-byte entries (the
// lump's offset, its size on disk, its size in memory, its type byte, a compression byte, two
// pad bytes and a name of up to 16 bytes, zero padded). Names are matched without regard to case.
//
// A PICTURE (a wad lump of type `kPictureLump`, or a `.lmp` file of its own in a pack) is its
// width and its height as little-endian 32-bit integers and then width x height bytes, each an
// index into the game's palette, row by row from the top. The palette is the game's, so a picture
// here is indices only: turning it into colours (and which index is see-through) is the reader's
// call. A wad's 128x128 console font sheet is a raw lump of indices with no header
// (`kRawLump`); `read_raw_picture` reads one at a size the caller knows.
//
// `read_wad` checks the whole directory before it hands anything back, and `read_picture` checks
// that the pixels the header promises are there, so a damaged or hostile file is refused with the
// lump and the sizes rather than read out of range. `write_wad` and `write_picture` make the
// formats for tests and fixtures.
//
// ge:summary Reads and writes the wad archive and classic palette pictures (indices, no colours)
// ge:keywords vfs wad wad2 lump picture lmp palette indexed status-bar conchars console-font
// ge:keywords read write refuse fixture
// ge:skill vfs
// ge:test tests/vfs_wad_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::vfs {

inline constexpr uint8_t kPictureLump = 0x42;  // a picture: width, height, indices
inline constexpr uint8_t kRawLump = 0x44;      // bytes with no header (the console font sheet)

struct WadEntry {
  std::string name;  // as stored
  uint32_t offset = 0, size = 0;
  uint8_t type = 0;
  bool compressed = false;
};

struct Wad {
  std::vector<WadEntry> entries;  // in the directory's order
};

// A palette picture: width x height indices, row by row from the top.
struct IndexedPicture {
  int width = 0, height = 0;
  std::vector<uint8_t> indices;
};

// The directory of the wad in `bytes`; false with the reason in `*error`.
bool read_wad(std::span<const uint8_t> bytes, Wad* out, std::string* error);
// The entry named `name` (any case), or nullptr. The first wins when a name is stored twice.
const WadEntry* wad_find(const Wad& wad, std::string_view name);
// A lump's bytes as a view into the wad `bytes` the directory was read from.
std::span<const uint8_t> wad_lump(std::span<const uint8_t> bytes, const WadEntry& entry);
// The picture in `bytes` (a picture lump or a `.lmp` file); false with the reason.
bool read_picture(std::span<const uint8_t> bytes, IndexedPicture* out, std::string* error);
// A raw lump of `width` x `height` indices; false when it holds fewer bytes.
bool read_raw_picture(std::span<const uint8_t> bytes, int width, int height, IndexedPicture* out,
                      std::string* error);

// A picture's bytes, for fixtures.
std::vector<uint8_t> write_picture(const IndexedPicture& picture);
struct WadLump {
  std::string name;
  uint8_t type = kPictureLump;
  std::vector<uint8_t> bytes;
};
// A wad of `lumps` in the given order; false when a name is empty or longer than 15 bytes.
bool write_wad(const std::vector<WadLump>& lumps, std::vector<uint8_t>* out, std::string* error);

}  // namespace ge::vfs
