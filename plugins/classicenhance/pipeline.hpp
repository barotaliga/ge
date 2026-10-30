// THE PIPELINE: a verified pack (source.hpp) enhanced into the local cache (cache.hpp), stage by
// stage, in a fixed order:
//
// 1. **pack**: the pack itself, unchanged, with `source.txt` (how it was found and verified) and
//    `NOTICE.txt`.
// 2. **textures**: every texture of every map in the pack (`maps/*.bsp`), once each by name,
//    enhanced (texture.hpp) into `textures/<name>/`, with `textures/index.txt`. Textures nobody
//    sees drawn (`never_drawn`) are left out.
// 3. **lightmaps**: each map's light (lightmaps.hpp) into `lightmaps/`.
// 4. **sounds**: every sound in the pack (`sound/**.wav`) enhanced (sound.hpp) to 48 kHz 16-bit
//    WAV under `sounds/` at the same relative path, with `sounds/index.txt`.
// 5. **models**: every model and sprite in the pack (`progs/*.mdl`, `progs/*.spr`; models.hpp)
//    as glTF with enhanced skins under `models/`, sprites' frames under `sprites/`, and
//    `replaced.txt`: what the engine draws in their place (replace.hpp).
// 6. **pictures**: the pack's 2D art (pictures.hpp: the wad's pictures and font sheet, the
//    `gfx/` pictures) in the palette's colours under `pictures/<name>.png`, unscaled, with
//    `pictures/index.txt`.
//
// A stage whose recorded key (its version and options) matches is skipped; `RunOptions::force`
// reruns it. `RunOptions::only` runs the named stages alone (the ones before them must have run).
// A report says per stage whether it ran, what it wrote and how long it took. While it runs it
// reports each stage's start, items and end to `RunOptions::progress` (progress.hpp draws them),
// and stops between items once `RunOptions::cancel` is set.
//
// ge:summary The enhancement pipeline's stages, pack to pictures, into the cache
// ge:keywords pipeline stage pack textures lightmaps sounds models run skip force cache enhance
// ge:keywords pictures
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "classicenhance/models.hpp"
#include "classicenhance/sound.hpp"
#include "classicenhance/source.hpp"
#include "classicenhance/texture.hpp"

namespace ge::classicenhance {

enum class Stage : uint8_t { pack, textures, lightmaps, sounds, models, pictures };
inline constexpr Stage kStages[] = {Stage::pack,   Stage::textures, Stage::lightmaps,
                                    Stage::sounds, Stage::models,   Stage::pictures};
const char* stage_name(Stage s);
// The stage named `name`; false when none is.
bool stage_named(const std::string& name, Stage* out);

inline constexpr int kTextureStageVersion = 1;
inline constexpr int kSoundStageVersion = 1;
inline constexpr int kPictureStageVersion = 1;

// What the pipeline reports while it works (progress.hpp's board reads it): a stage started
// (with its item count), an item done, a stage finished, skipped or failed.
struct Progress {
  enum class Kind : uint8_t { started, item, finished, skipped, failed };
  Kind kind = Kind::started;
  Stage stage = Stage::pack;
  uint32_t done = 0, total = 0;  // items: done so far (item, finished), in the stage (started)
  std::string item;              // the item's name (item), the reason (skipped, failed)
  const assets::Image* before = nullptr;  // a texture's item: the pack's picture
  const assets::Image* after = nullptr;   // and its enhanced albedo
};

struct RunOptions {
  EnhanceOptions textures;
  SoundOptions sounds;
  ModelOptions models;
  bool force = false;
  std::vector<Stage> only;  // empty: every stage
  // At most this many textures (0: all), for a quick look; recorded in the stage's key.
  uint32_t texture_limit = 0;
  // Called on the pipeline's thread with each report; none: no reports.
  std::function<void(const Progress&)> progress;
  // Checked between items: once true, the stage running fails with "cancelled".
  const std::atomic<bool>* cancel = nullptr;
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
