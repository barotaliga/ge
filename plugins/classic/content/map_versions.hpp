// A STOCK MAP IN TWO VERSIONS, for every classic game a client joins servers of: a later release of
// a game rebuilt its maps (Quake's rerelease), so `dm3` on a server may be another file than the
// player's, and a server that checks the map refuses a client whose copy differs. Both are valid:
// the client keeps every version it has met, so it can join either kind of server.
//
// The player's own copy stays where the game has it (`maps/dm3.bsp`). Another version fetched from
// a server is kept hidden beside it as `maps/versions/<name>.<key>.bsp` in the game's folder, the
// key the map's checksum in eight hex digits (`version_path`), and never overwrites the player's.
// The name a player sees stays `dm3` everywhere (servers, demos, configs).
//
// THE CHOICE: while a server is joined, the version it plays is read in place of the player's copy
// (`choose_version`: an exact rewrite of the map's path on the search path, applied before the
// links). The version a server took is remembered (`maps/versions/servers.txt`, a line of server,
// map and key each), so the next join to it sends that version first (`remembered_key`).
//
// ge:summary A stock map in two versions: another release's copy kept hidden by its checksum
// ge:keywords map-versions map-version rerelease original-map map-checksum checksum-mismatch
// ge:keywords version_path stored_versions choose_version remembered_key keep_choice
// ge:keywords map-model-file-does-not-match two-versions
// ge:skill classic vfs
// ge:test tests/classic_content_map_versions_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "classic/content/files.hpp"

namespace ge::classic::content {

// "maps/dm3.bsp" with key 0x1a2b3c4d: "maps/versions/dm3.1a2b3c4d.bsp".
std::string version_path(std::string_view map, uint32_t key);

// Every version of `map` kept in `folder` (the game's folder on disk): each one's path in the game
// ("maps/versions/dm3.1a2b3c4d.bsp") and bytes.
std::vector<std::pair<std::string, std::vector<uint8_t>>> stored_versions(
    const std::filesystem::path& folder, std::string_view map);

// Writes another version of `map` into `folder` at `version_path`; its path in the game, "" when it
// could not be written (`why` says so).
std::string keep_version(const std::filesystem::path& folder, std::string_view map, uint32_t key,
                         const std::vector<uint8_t>& bytes, std::string& why);

// `map` read as `version` on `files` from now on ("" or `map` itself: the player's own copy again).
void choose_version(SearchPath& files, std::string_view map, std::string_view version);

// The key of the version of `map` that `server` took last, if any.
std::optional<uint32_t> remembered_key(const std::filesystem::path& folder, std::string_view server,
                                       std::string_view map);
// Remembers that `server` took the version `key` of `map` (the line for that server and map
// replaced).
void keep_choice(const std::filesystem::path& folder, std::string_view server, std::string_view map,
                 uint32_t key);

}  // namespace ge::classic::content
