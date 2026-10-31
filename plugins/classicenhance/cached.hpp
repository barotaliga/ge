// THE CACHE READ BACK BY A GAME: where the enhanced copy of the packs a game is playing lies on
// this machine, and its pictures handed to the renderers in place of the pack's own: a wall's
// texture upscaled, a model's skin upscaled. Each is looked up by the pack's own name and checked
// against the size the game has for it, so a stale or foreign cache never draws a wrong picture:
// anything missing or mismatched leaves the pack's own.
//
// - `find_cache`: the folder of the pack (cache.hpp's `pack_dir` of its SHA-256 under the root),
//   when its textures' stage has finished there.
// - `cached_texture`: textures/<name>/albedo.png at 1, 2, 4 or 8 times the pack's size, as sRGB
//   RGBA with alpha 255 where emissive.png glows (the classic's fullbright texels: lit by nothing
//   but themselves). The liquids' and the sky's textures (names starting '*' or "sky") are left
//   to the pack: the renderers draw those their own way.
// - `cached_surface`: the same texture's relief for a modern renderer to light it with, packed into
//   one RGBA picture the albedo's size: the normal's x and y (tangent space, +Y up the picture),
//   the height (255 the top) and the roughness, from normal.png, height.png and roughness.png.
// - `cached_skins`: models/<name>/skin<k>.png, each skin of a model, the same size rule.
// - `cached_rebake`: lightmaps/<map>.gerb, the map's light rebaked (lightmaps.hpp), when it is
//   whole and made for a map of the same vertices.
//
// ge:summary The enhanced cache read back by a game: the pack's folder, textures and skins by name
// ge:keywords classicenhance cache read find_cache cached_texture cached_skins albedo emissive
// ge:keywords cached_surface normal-map height-map roughness relief parallax
// ge:keywords upscaled texture skin replace renderer atlas cached_rebake gerb rebake bounce
// ge:skill classicenhance
// ge:test tests/classicenhance_cached_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "assets/image.hpp"
#include "brush/rebake.hpp"

namespace ge::classicenhance {

// The cache folder of the pack with these bytes under `root` (default_cache_root() when empty);
// an empty path when its textures were never enhanced there.
std::filesystem::path find_cache(std::span<const uint8_t> pack,
                                 const std::filesystem::path& root = {});

// The enhanced copy of texture `name` (the pack's w x h) into `rgba`: (w k) x (h k) x 4, k the
// cache's scale. False (and `rgba` untouched) when there is none of a size that fits.
bool cached_texture(const std::filesystem::path& dir, std::string_view name, uint32_t w, uint32_t h,
                    std::vector<uint8_t>* rgba);

// The relief of texture `name` whose enhanced albedo is `w` x `h` (cached_texture's size) into
// `rgba`: w x h x 4, each texel the normal's x and y (0.5 flat), the height (1 the top) and the
// roughness, as bytes. False (and `rgba` untouched) when any of the three maps is missing, of
// another size, or the texture is one cached_texture leaves to the pack.
bool cached_surface(const std::filesystem::path& dir, std::string_view name, uint32_t w, uint32_t h,
                    std::vector<uint8_t>* rgba);

// The enhanced skins of model `name` (the file's stem, "soldier" for progs/soldier.mdl), each
// at a multiple of the pack's `w` x `h`; false when the cache has none.
bool cached_skins(const std::filesystem::path& dir, std::string_view name, uint32_t w, uint32_t h,
                  std::vector<assets::Image>* skins);

// The rebaked light of map `name` (the file's stem, "e1m1" for maps/e1m1.bsp) made for a map of
// `vertices` vertices; false (and `out` untouched) when the cache has none, it does not read, or it
// was made for another map.
bool cached_rebake(const std::filesystem::path& dir, std::string_view name, size_t vertices,
                   brush::Rebaked* out);

}  // namespace ge::classicenhance
