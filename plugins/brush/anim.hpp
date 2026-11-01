// TEXTURE SEQUENCES: textures a map shows as frames of one picture. 29 and BSP2 tell them by the
// first two bytes of their names (below); the other families chain them (`Texture::next`, set by
// the reader), and each chain that closes is a cycle from its lowest texture, its frames in chain
// order (a chain that does not close is drawn still and named in `notes`).
//
// - A CYCLE is `+<frame><name>`: frames `0`..`9` are the main cycle, `a`..`j` (any case) the
//   alternate one a door or a button shows once it has been used. A surface shows frame
//   `floor(seconds * kFramesPerSecond) mod count` of its cycle, so every surface of one cycle
//   turns together.
// - A RANDOM SET is `-<frame><name>`, frames `0`..`9`: each surface shows one frame for good,
//   picked by its index, so a wall of tiles does not repeat one picture.
// - A cycle's frames run from 0 with no gap; a set missing a frame keeps those below the gap and
//   says so in `notes`. A texture not of either kind is its own one frame.
//
// `build_texture_anims` reads the map's texture names once; `texture_at` answers, without
// allocating, which texture a surface shows at a moment. Pure: the moment is the caller's clock.
//
// ge:summary Texture sequences: + cycles (main and alternate) and - random sets, frame at a moment
// ge:keywords texture animated animation cycle sequence frame alternate random tiling plus minus
// ge:keywords texture_at build_texture_anims frames-per-second door button
// ge:skill brush
// ge:test tests/brush_anim_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "brush/map.hpp"

namespace ge::brush {

// How many frames a cycle turns through a second.
inline constexpr int kFramesPerSecond = 5;

struct TextureSet {
  std::string name;              // the name after the two-byte prefix, as stored
  bool random = false;           // a '-' set (one frame a surface) rather than a '+' cycle
  std::vector<uint32_t> frames;  // textures, frame 0 first
  std::vector<uint32_t> alt;     // the alternate cycle's textures (a '+' cycle only)
};

struct TextureAnims {
  std::vector<int32_t> set_of;  // a texture's set, or -1
  std::vector<bool> is_alt;     // whether the texture is a frame of its set's alternate cycle
  std::vector<TextureSet> sets;
  std::vector<std::string> notes;  // a gap or an alternate without a main cycle, named
};

TextureAnims build_texture_anims(const Map& map);

// The texture surface `surface` shows `seconds` into the run when its texture is `texture`;
// `alternate` asks for the alternate cycle (one is drawn only when the set has it).
uint32_t texture_at(const TextureAnims& anims, uint32_t texture, double seconds, bool alternate,
                    uint32_t surface);

}  // namespace ge::brush
