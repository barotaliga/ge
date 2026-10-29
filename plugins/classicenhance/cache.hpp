// THE LOCAL CACHE everything the pipeline makes goes into, on the player's machine and nowhere
// else. Its root is `GE_CLASSIC_CACHE` when set, otherwise the platform's per-user cache folder
// (`$XDG_CACHE_HOME` or `~/.cache` on Linux, `~/Library/Caches` on macOS, `%LOCALAPPDATA%` on
// Windows) under `ge/classicenhance`. Inside it each verified pack has a folder named by the
// first 16 hex digits of its SHA-256:
//
//   <root>/<pack id>/
//     NOTICE.txt              what this folder is and that it is not to be passed on
//     source.txt              the route, the path and both digests the pack was verified by
//     pack/pak0.pak           the pack, unchanged (so a rerun needs no source)
//     textures/index.txt      one line a texture: name, folder, size, material, glows
//     textures/<name>/        albedo.png, height.png, normal.png, roughness.png, emissive.png
//     lightmaps/<map>.png     each map's light (lightmaps.hpp)
//     stages.txt              one line a stage that finished: its name and its key
//
// A stage's key digests its version and its options: a rerun skips a stage whose recorded key
// is the one it would write, and `--force` reruns it. Files are written whole (to a temporary
// name, then renamed), so an interrupted run never leaves half a file behind.
//
// ge:summary The local cache's root, its per-pack folder layout, stage records and whole writes
// ge:keywords cache local folder root xdg localappdata stage record key rerun atomic write
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ge::classicenhance {

// The cache's root on this machine (see above); an empty path when no home folder is known.
std::filesystem::path default_cache_root();
// The folder for the pack with digest `pack_sha256` under `root`.
std::filesystem::path pack_dir(const std::filesystem::path& root, const std::string& pack_sha256);

struct StageRecord {
  std::string stage, key;
};
std::vector<StageRecord> read_records(const std::filesystem::path& dir);
// Records `stage` as finished with `key`, replacing its earlier line.
bool write_record(const std::filesystem::path& dir, const std::string& stage,
                  const std::string& key);
// The key recorded for `stage`, or "".
std::string recorded_key(const std::filesystem::path& dir, const std::string& stage);

// `bytes` to `path` whole: written beside it under a temporary name, then renamed over it.
bool write_whole(const std::filesystem::path& path, std::span<const uint8_t> bytes);
bool write_text(const std::filesystem::path& path, const std::string& text);

// A texture's or map's name as a folder name: lower case, '*' as "liq_", '+' as "anim_", and
// anything but letters, digits, '_' and '-' as '_'.
std::string safe_name(const std::string& name);

}  // namespace ge::classicenhance
