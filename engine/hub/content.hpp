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
// ge:summary A demo's content pack: only the files it needs out of the hub's packs, zstd, by hash
// ge:keywords hub content pack demo files minimal download zstd pak viewer cache content-address
// ge:keywords demo_content content_answer demo_files file_needs
// ge:skill net
// ge:test tests/hub_content_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

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
// `/content/...` answered into `out` (the pack, cached a year); false for any other path.
bool content_answer(std::string_view path, const Site& site, Answer& out);

}  // namespace ge::hub
