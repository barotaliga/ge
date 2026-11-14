// A DEMO'S CONTENT PACK (docs/hub.md § Watching in a browser): the files one demo draws from,
// taken out of the packs beside the viewer and sent as one small compressed pack, so a visitor
// downloads that map, its models and sounds rather than every pack the hub holds.
//
// The game names the files (`HubSpec::demo_files`, and `file_needs` the ones each file found names
// in turn, as a map keeps its textures in files of their own); the hub mounts the viewer folder's
// packs in name order (a later one wins, as a game folder's numbered packs), takes each named file
// (a name ending in '/' takes the folder), writes them as a pack and packs that with zstd. The
// result is kept in the archive's `content/` named by its own SHA-256, and the demo's hash points
// at it
// (`content/<demo sha>.txt`), so the work is done once a demo and two demos that need the same
// files share one pack. A name no pack holds is left out and counted.
//
// The Demo page hands the viewer `--pack=/content/<sha>.pak.zst` when the pack can be made, else
// every pack beside the viewer as before.
//
// EACH FILE ON ITS OWN: beside every pack the hub keeps each file in it zstd-packed and named by
// the SHA-256 of its plain bytes, and the pack's list of them (web/file_list.hpp), so a page
// fetches only the files its browser does not hold yet.
//
// A MAP'S PACK (`map_content`, docs/hub.md § Playing in a browser): the files a match on that map
// draws from (`HubSpec::map_files`), made the same way the first time the map's Play page is
// asked for, its index `content/map-<map>.txt`. `viewer_maps` lists the maps the packs beside the
// viewer hold, the Play page's list.
//
// ge:summary A demo's content pack: only the files it needs out of the hub's packs, zstd, by hash
// ge:keywords hub content pack demo files minimal download zstd pak viewer cache content-address
// ge:keywords demo_content content_answer demo_files file_needs map_content map_files viewer_maps
// ge:keywords play-page map-pack content-file file-list by-hash content_list_path
// ge:skill net
// ge:test tests/hub_content_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hub/pages.hpp"
#include "hub/spec.hpp"

namespace ge::hub {

struct ContentPack {
  std::string sha;     // the pack's SHA-256 (of its packed bytes)
  uint64_t bytes = 0;  // packed
  uint64_t raw = 0;    // the pack before zstd
  size_t files = 0;    // files in it
  size_t missing = 0;  // names no pack held
};

// The web path a pack is served at.
std::string content_path(std::string_view sha);
// The pack for the archived demo `demo_sha`: made the first time, read back after. False with the
// reason when the game names no files, the demo is not in the archive, or no pack is beside the
// viewer.
bool demo_content(const Site& site, const HubSpec& spec, std::string_view demo_sha,
                  ContentPack& out, std::string& error);
// The pack for playing on `map` (a map the packs beside the viewer hold): made the first time,
// read back after. False with the reason when the game names no map's files, the name is not a
// map's, no archive keeps packs or no pack holds the map.
bool map_content(const Site& site, const HubSpec& spec, std::string_view map, ContentPack& out,
                 std::string& error);
// A map's name as a Play page's path takes it: lower-case letters, digits, '_' and '-'.
inline constexpr size_t kMostMapName = 32;
bool map_name_ok(std::string_view map);
// The maps (`maps/<name>.bsp`) the packs beside the viewer hold, by name; the packs read again
// only when they change.
std::vector<std::string> viewer_maps(const Site& site);
// The archive's paths of a pack's file list and of one file by its hash (web/file_list.hpp).
std::string content_list_path(std::string_view sha);
std::string content_file_path(std::string_view sha);
// `/content/...` answered into `out`: a pack, its file list or one of its files by hash (each
// cached a year), or with `spec` a map's file list by the map's name (`/content/map/<map>.files`,
// made the first time, never cached); false for any other path.
bool content_answer(std::string_view path, const Site& site, Answer& out,
                    const HubSpec* spec = nullptr);

}  // namespace ge::hub
