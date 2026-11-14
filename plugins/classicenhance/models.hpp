// THE MODELS STAGE: every vertex-animated model and sprite in the pack, enhanced into the cache
// so a game can load them like any other asset.
//
// - A model (`progs/<name>.mdl`) becomes `models/<name>.glb`: its skins enhanced by the texture
//   enhancer (glowing texels as an emissive picture), its mesh smooth-shaded with every frame's
//   own normals, every frame a morph target and its frames played as clips (`morph::write_glb`),
//   in metres. Rounding the facets (`morph::smooth_model`) is an option, off: smooth shading
//   alone keeps the silhouettes the artists drew.
// - A sprite (`progs/<name>.spr`) becomes `sprites/<name>/<frame>.png`, each frame with its
//   transparent texels (index 255) clear, at the source size.
// - The second family's models and sprites keep their pictures outside, in the game's picture
//   files (`skin_paths`, a frame's `picture`): each is read out of the pack as the game reads it.
// - A model's or sprite's name in the cache is `model_name` of its path: the path under its
//   kind's folder (`progs/armor.mdl` is `armor`, `models/items/armor/tris.md2` is
//   `items/armor/tris`), so the sequel's many `tris` models never land on one another;
//   `safe_name` (cache.hpp) makes it a folder name.
// - `models/index.txt` lists every model and sprite, and `replaced.txt` what the engine draws in
//   their place (`replace.hpp`); a replaced model with glowing parts loses those pieces, and
//   one that is all glow (a bonfire's flame) writes no file, only its index line.
//
// ge:summary The models stage: the pack's models as glTF with enhanced skins, sprites as PNGs
// ge:keywords models stage mdl spr gltf glb skin enhance sprite replaced index morph cache
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>

#include "classicenhance/profile.hpp"
#include "classicenhance/texture.hpp"
#include "morph/model.hpp"

namespace ge::classicenhance {

// The stage's version: a rerun redoes the stage when it moves.
// 2: each skin also as models/<name>/skin<k>.png; 3: named by `model_name`, outside skins read.
inline constexpr int kModelStageVersion = 3;

// A model's or sprite's name in the cache, from its path in the game's files: the path without
// its first folder and its extension ("progs/armor.mdl" -> "armor").
std::string model_name(std::string_view path);

struct ModelOptions {
  // Called with done 0 and the file count before the first file, then after each file with its
  // name; false stops the stage (its error already set by the caller). None: not called.
  std::function<bool(const std::string& name, uint32_t done, uint32_t total)> each;
  int skin_scale = 4;  // the skins' upscale (1..8); the rest of the texture options apply
  int round = 0;       // 0: smooth shading only; n > 1: every triangle rounded into n * n
};

struct ModelStats {
  uint32_t models = 0, sprites = 0, files = 0, replaced = 0;
};

// The model without its glowing pieces: a piece is a set of triangles joined through shared
// vertices, and it glows when the first skin is fullbright at the middle of at least a quarter of
// its triangles (a painted flame's pieces glow over half; a torch's stick and cage not at all).
// Vertices no triangle uses any more are dropped.
morph::Model drop_glowing(const morph::Model& m);

// Every model and sprite in the pack (the profile's kinds) into `dir` (the pack's cache folder);
// false with the reason.
bool enhance_models(std::span<const uint8_t> pack, const Palette& palette,
                    const std::filesystem::path& dir, const ModelOptions& opts,
                    const EnhanceOptions& textures, ModelStats* stats, std::string* error,
                    const Profile& profile = classic_profile());

}  // namespace ge::classicenhance
