// A BUILT ASSET: a model made offline by a builder script (headless Blender, `ge asset build`)
// and committed beside it, with a STAMP saying what it was built from and a SIZE BUDGET it must
// stay under (docs/rendering.md § Authored models). This file is the asset's rules as code: the
// manifest's form, the stamp's lines and the check `assets check` (and the `assets_check` test)
// holds every committed model to.
//
// AN ASSET is a directory with an `asset.txt` in it, named after the directory, under the home
// its model belongs to (engine/, plugins/<name>/ or games/<name>/, as any code: skill plugins):
//
//   plugins/skier/assets/skier/
//     asset.txt     the manifest (below)
//     build.py      the builder: Blender Python, run headless with the data and the output path
//     data.cpp      the game data the builder is fed (assets/data.hpp), when it has any
//     skier.glb     the model, as built: committed, loaded by the engine (assets/gltf.hpp)
//     skier.stamp   what skier.glb was built from: one hash a source
//
// THE MANIFEST, one `key: value` a line, `#` to the end of a line a comment:
//
//   builder: build.py              the builder script, from this directory (required)
//   data: skier                    the GE_ASSET_DATA set its JSON comes from (optional)
//   source: ../../app/rig.cpp      another file the model is made from (optional, repeatable)
//   budget-kib: 512                the most the committed model may weigh, KiB (required)
//
// THE STAMP is the hash (FNV-1a 64 over the text, carriage returns dropped, so a checkout's line
// endings do not move it) of the builder, every source and the data's JSON, one line each, and
// the Blender that built it. A model is STALE when any line differs from what the sources hash to
// now: its stamp names which, and the next action is `ge asset build <name>`. Only the sources
// are hashed, never the model, so a model is rebuilt only when what it is made from moves.
//
// ge:summary A built asset's manifest, its stamp of source hashes and its size budget, checked
// ge:keywords assets asset manifest asset-txt stamp source-hash stale budget size-budget builder
// ge:keywords blender ge-asset read_manifest find_manifests check_asset stamp_lines fnv1a
// ge:skill assets
// ge:test tests/assets_manifest_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ge::assets {

namespace fs = std::filesystem;

inline constexpr std::string_view kManifestFile = "asset.txt";

struct AssetManifest {
  std::string name;  // the directory's name; the model is <name>.glb, its stamp <name>.stamp
  fs::path dir;
  std::string builder;               // relative to dir
  std::vector<std::string> sources;  // relative to dir
  std::string data;                  // a GE_ASSET_DATA name, or ""
  uint64_t budget_bytes = 0;

  fs::path model_path() const { return dir / (name + ".glb"); }
  fs::path stamp_path() const { return dir / (name + ".stamp"); }
};

struct ManifestRead {
  bool ok = false;
  AssetManifest manifest;
  std::string error;  // what is wrong, on which line, and the fix
};
ManifestRead read_manifest(const fs::path& file);
ManifestRead parse_manifest(std::string_view text, const fs::path& dir);

// Every asset.txt under `root`, sorted, skipping build output and dependencies.
std::vector<fs::path> find_manifests(const fs::path& root);

// FNV-1a 64 over the text with every '\r' dropped, as 16 lower-case hex digits.
std::string hash_text(std::string_view text);

struct StampLine {
  std::string kind;  // builder, source, data
  std::string key;   // the path from the asset's directory, or the data's name
  std::string hash;
};

// The stamp's lines as the sources hash now. `data_json` is the data's text when the manifest
// names data. False (with `error`) when a file cannot be read.
bool stamp_lines(const AssetManifest& m, std::string_view data_json, std::vector<StampLine>& out,
                 std::string& error);

// The stamp file's text for these lines and the Blender version that built the model.
std::string stamp_text(const AssetManifest& m, const std::vector<StampLine>& lines,
                       std::string_view blender);

struct Stamp {
  std::vector<StampLine> lines;
  std::string blender;
};
Stamp parse_stamp(std::string_view text);

struct AssetCheck {
  uint64_t bytes = 0;                 // the committed model's size (0: none)
  std::string blender;                // the stamp's
  std::vector<std::string> problems;  // each a sentence: what, where, the next action
};
// The committed model against its sources and its budget. Loading it is the caller's
// (assets/gltf.hpp), so this reads no file format.
AssetCheck check_asset(const AssetManifest& m, std::string_view data_json);

}  // namespace ge::assets
