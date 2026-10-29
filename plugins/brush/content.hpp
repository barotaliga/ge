// A CONTENT SET'S ASSETS: every sound and model a set of classic files ships or names, read from
// the files themselves and from every map's entities, so a game that replaces that content knows
// what it must replace and where each one is used.
//
// `scan_content` takes the set's files (from packs and loose directories alike, by path):
// - **Shipped**: a `.wav` is a sound, named by its path under `sound/` (the name a map's keys and
//   the game logic use); a `.mdl`, `.md2`, `.md3`, `.spr` or `.sp2` is a model, named by its path.
// - **Named by a map**: every `.bsp` is read (`read_bsp`) and each entity's `noise`, `noise1` ..
//   `noise4` names a sound, its `model` a model (an inline `*<n>` brush model is the map's own and
//   is skipped). A `.bsp` a map names as a model is a model too.
// - **Picked by number**: a `sounds` key (any class but `worldspawn`, whose `sounds` is a music
//   track) and worldspawn's `worldtype` pick one of a class's numbered sound sets; each pairing a
//   map uses is listed with the maps that use it.
// What the game logic plays or shows (its bytecode's calls) is not read here: that needs the
// progs reader. A map that does not read is listed with its reason and the scan carries on.
//
// `parse_asset_map` reads a game's ASSET MAP (`<set>.assetmap`): which of its own names stands in
// for each classic sound and model, and for each numbered set. Lines: `sound <classic path>
// <ours>`, `model <classic path> <ours>`, `set <classname> <key>=<value> <field> <ours>`; a `#` at
// a line's start or after whitespace starts a comment (a trailing `# pin` marks a line tools never
// rewrite). `compare_content` holds a scan to a map: each asset and set mapped or not; a world's
// type is mapped by every class's `worldtype` rows for it (the locks and keys that pick by it).
//
// Pure: the same answer from the same files on every platform, in path order.
//
// ge:summary Every sound and model a classic content set ships or its maps name; the asset map
// ge:keywords content scan assets sounds models noise sounds worldtype numbered-set assetmap
// ge:keywords replace registry pak entities classic-names
// ge:skill brush free-assets
// ge:test tests/brush_content_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::brush {

struct ContentFile {
  std::string path;  // as stored: `sound/<name>.wav`, `progs/<name>.mdl`, `maps/<name>.bsp`
  std::span<const uint8_t> bytes;
};

enum class AssetKind : uint8_t { sound, model };

// Where a map names an asset: the map's path, the entity's class and the key.
struct AssetUse {
  std::string map, classname, key;
};

struct ScannedAsset {
  AssetKind kind = AssetKind::sound;
  std::string name;            // a sound's path under sound/, a model's path
  bool shipped = false;        // the set holds the file
  std::vector<AssetUse> uses;  // in map, then entity order
};

// A numbered set a map picks: `func_door` with `sounds=2`, `worldspawn` with `worldtype=1`.
struct SetUse {
  std::string classname, key, value;
  std::vector<std::string> maps;
};

struct ContentScan {
  std::vector<std::string> maps;                            // every .bsp read, in path order
  std::vector<std::pair<std::string, std::string>> unread;  // a map and why it did not read
  std::vector<ScannedAsset> assets;                         // sounds, then models, each by name
  std::vector<SetUse> sets;  // by class, key, then value (numerically when both are numbers)
};

ContentScan scan_content(const std::vector<ContentFile>& files);

struct AssetMapLine {
  AssetKind kind = AssetKind::sound;
  std::string classic;                       // `sound` and `model` lines
  std::string classname, key, value, field;  // `set` lines
  std::string ours;
  bool set = false, pinned = false;
  int line = 0;
};

struct AssetMap {
  std::vector<AssetMapLine> lines;  // in file order
};

// The asset map in `text`; false with the line and the reason in `*error`.
bool parse_asset_map(std::string_view text, AssetMap* out, std::string* error);

// A scan held to an asset map: each asset's and each numbered set's own name, "" when unmapped.
struct ContentMatch {
  std::vector<std::string> asset_ours;  // per ContentScan::assets
  std::vector<std::string> set_ours;    // per ContentScan::sets: "noise1=<ours> noise2=<ours>"
};

ContentMatch compare_content(const ContentScan& scan, const AssetMap& map);

// THE FIXTURE SET the tests and the `content-scan` lab use, made up here so no one's content is
// needed: a pack of two sounds, a model and two maps (29 and 38) whose entities name shipped and
// missing sounds and models, an inline brush model, numbered door, button and world sets, and the
// world's music track; and an asset map for it that leaves some of each unmapped.
struct ContentFixture {
  std::vector<uint8_t> pak;  // the pack archive
  std::string asset_map;     // its asset map's text
};
bool content_fixture(ContentFixture* out, std::string* error);

}  // namespace ge::brush
