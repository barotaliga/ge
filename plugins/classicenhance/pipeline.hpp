// THE PIPELINE: a verified pack (source.hpp) enhanced into the local cache (cache.hpp), stage by
// stage, in a fixed order:
//
// 1. **pack**: the pack itself, unchanged, with `source.txt` (how it was found and verified) and
//    `NOTICE.txt`.
// 2. **textures**: every texture of every map in the pack (`maps/*.bsp`), once each by name,
//    enhanced (texture.hpp) into `textures/<name>/`, with `textures/index.txt`. Textures nobody
//    sees drawn (`never_drawn`) are left out.
// 3. **lightmaps**: each map's light (lightmaps.hpp) into `lightmaps/`.
//
// A stage whose recorded key (its version and options) matches is skipped; `RunOptions::force`
// reruns it. `RunOptions::only` runs the named stages alone (the ones before them must have run).
// A report says per stage whether it ran, what it wrote and how long it took.
//
// ge:summary The enhancement pipeline's stages (pack, textures, lightmaps) run into the cache
// ge:keywords pipeline stage pack textures lightmaps run skip force report cache enhance
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "classicenhance/source.hpp"
#include "classicenhance/texture.hpp"

namespace ge::classicenhance {

enum class Stage : uint8_t { pack, textures, lightmaps };
inline constexpr Stage kStages[] = {Stage::pack, Stage::textures, Stage::lightmaps};
const char* stage_name(Stage s);
// The stage named `name`; false when none is.
bool stage_named(const std::string& name, Stage* out);

inline constexpr int kTextureStageVersion = 1;

struct RunOptions {
  EnhanceOptions textures;
  bool force = false;
  std::vector<Stage> only;  // empty: every stage
  // At most this many textures (0: all), for a quick look; recorded in the stage's key.
  uint32_t texture_limit = 0;
};

struct StageResult {
  Stage stage = Stage::pack;
  bool ran = false;    // false: skipped, up to date (or not asked for)
  uint32_t files = 0;  // files written
  double seconds = 0;
  std::string note;  // what it did, in a line
};

struct RunReport {
  std::filesystem::path dir;  // the pack's cache folder
  std::vector<StageResult> stages;
};

// The pipeline over `source` into `root` (default_cache_root() when empty); false with the
// reason in `*error`, the stages done so far kept.
bool run_pipeline(const Source& source, const std::filesystem::path& root, const RunOptions& opts,
                  RunReport* report, std::string* error);

// One map's textures, as the textures stage reads them out of the pack.
struct PackTexture {
  std::string name, map;  // the texture's name and the first map that carries it
  int width = 0, height = 0;
  std::vector<uint8_t> indexed;
};
// The palette and every drawn texture of every map in `pack`, once each by name, in the order
// the maps (sorted by path) and their texture lumps give; false with the reason.
bool pack_textures(std::span<const uint8_t> pack, Palette* palette, std::vector<PackTexture>* out,
                   std::string* error);

}  // namespace ge::classicenhance
