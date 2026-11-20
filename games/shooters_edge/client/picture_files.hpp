// PICTURES BY FILE TYPE from the game's files: a TGA, PNG, JPEG or PCX read into RGBA by its name's
// extension (a PCX through its own palette, or the game's when it has none), and the player's
// crosshair picture (`crosshairimage`, engine/hud/crosshair.hpp) found under `crosshairs/`.
//
// - **The crosshair's picture**: `crosshairs/<name>` as a .tga, .png, .jpg or .pcx, the first the
//   files hold in that order; an extension the player wrote is dropped first. A PCX draws its last
//   colour (255) see-through, as the classic pictures do. A name that climbs out of the folder
//   (`..`, a leading slash) or names nothing is refused with one line on the console.
//
// ge:summary Pictures read by their file type, and the crosshair picture under crosshairs/
// ge:keywords decode_picture tga png jpeg pcx crosshairimage read_crosshair_image crosshairs-folder
// ge:skill ui
// ge:test tests/shooters_edge_picture_files_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "assets/image.hpp"
#include "hud/crosshair.hpp"
#include "shooters_edge/client/files.hpp"
#include "ui/image.hpp"

namespace shooters_edge::client {

// The picture `bytes` hold, by `path`'s extension (.tga, .png, .jpg or .jpeg, else a PCX:
// through `palette` when the file has none; `clear` its index drawn see-through, -1 none).
// Nullopt with `why` when it does not read.
std::optional<ge::assets::Image> decode_picture(std::span<const uint8_t> bytes,
                                                std::string_view path,
                                                std::span<const uint8_t> palette, std::string* why,
                                                int clear = -1);

// The crosshair picture `name` from `files` into `out`; false (and one console line) when none.
bool read_crosshair_image(const SearchPath& files, std::string_view name, ge::ui::Image* out);

// `s.read_image` reading from `files` (which must outlive `s`).
void add_crosshair_reader(ge::hud::CrosshairSettings& s, const SearchPath& files);

}  // namespace shooters_edge::client
