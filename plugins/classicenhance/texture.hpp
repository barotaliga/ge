// ONE CLASSIC TEXTURE ENHANCED: the palette-indexed picture a map carries, turned into the
// surface set a modern material reads, all of it tileable because the textures repeat across a
// wall:
//
// - **albedo**: the picture upscaled (`EnhanceOptions::scale`, 4 by default) by anisotropic
//   kernel regression: each output texel is a weighted mean of the 4 × 4 source texels round it,
//   the weights an ellipse stretched along the local edge (the structure tensor of the source's
//   luminance), so edges stay sharp and are not stair-stepped while flat areas stay smooth; then
//   a light unsharp mask at the output's scale, and fine grain in proportion to the source's own
//   local contrast, so the larger picture keeps the original's busy, painted surface instead of
//   going soft. Worked in linear light.
// - **height**: the albedo's luminance band-passed (the shapes a few texels to a few dozen texels
//   across, such as bricks, panels and rivets, over the finest grain), stretched to 0..1 between
//   its 2nd and 98th percentiles. Dark reads as low: a crevice, a seam, mortar.
// - **normal**: the height's slopes (Sobel) in tangent space, +Y up the picture, encoded
//   `n * 0.5 + 0.5`.
// - **roughness**: a base by the material the texture's name suggests (`material_of`), rougher in
//   the crevices.
// - **emissive**: the colour of the texels drawn from the palette's last 32 entries, which the
//   classic renderer draws at full brightness whatever the light, upscaled by the same kernel;
//   black where nothing glows (`Enhanced::glows` false).
//
// A liquid (a name starting '*') gets a flattened relief and a smooth surface; a sky (a name
// starting "sky") gets the albedo only. Pure and deterministic: the grain is hashed from the
// texel and the name, and the work is split in bands whose outputs never overlap.
//
// ge:summary One classic texture upscaled with its height, normal, roughness and glow maps
// ge:keywords texture upscale super-resolution kernel-regression anisotropic structure-tensor
// ge:keywords albedo normal-map height-map roughness emissive fullbright palette tileable wrap
// ge:skill classicenhance
// ge:test tests/classicenhance_texture_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "assets/image.hpp"

namespace ge::classicenhance {

// 256 RGB triples, as the content's palette file stores them.
struct Palette {
  std::array<uint8_t, 768> rgb{};
};
// The palette's entries from here on are drawn at full brightness by the classic renderer.
inline constexpr int kFullbrightFirst = 224;

enum class Material : uint8_t { stone, metal, wood, liquid, sky, other };
const char* material_name(Material m);
// What a texture's name suggests it is made of ('*' liquids, "sky", and words such as metal,
// tech, wood, brick, rock, stone); `other` when nothing matches.
Material material_of(std::string_view name);
// A texture no one ever sees drawn (trigger volumes, clip brushes): not worth enhancing.
bool never_drawn(std::string_view name);

struct Upscaler;
class UpscaleDevice;

struct EnhanceOptions {
  int scale = 4;         // output texels a source texel, along each side (1..8)
  double sharpen = 0.3;  // the unsharp mask's amount
  double grain = 0.35;   // fine grain, as a share of the source's local contrast
  double relief = 1.0;   // the normal map's strength
  // The trained upscaler (upscaler.hpp): when set and its factor is `scale`, the albedo is its
  // picture (the texture wrapped round a margin so its edges tile, then cropped) in place of the
  // kernel's, with no sharpening or grain added; the glow is still the kernel's.
  const Upscaler* network = nullptr;
  // Where the network runs: the CPU when null, else this device (a picture it refuses runs on the
  // CPU).
  UpscaleDevice* device = nullptr;
};

struct Enhanced {
  assets::Image albedo, height, normal, roughness, emissive;
  bool glows = false;  // some texel is fullbright
  Material material = Material::other;
};

// The picture as the classic renderer shows it: each index through the palette, opaque.
assets::Image to_rgba(std::span<const uint8_t> indexed, int width, int height,
                      const Palette& palette);

// The surface set for texture `name`; `indexed` holds width × height palette indices.
Enhanced enhance_texture(std::string_view name, std::span<const uint8_t> indexed, int width,
                         int height, const Palette& palette, const EnhanceOptions& options = {});

}  // namespace ge::classicenhance
