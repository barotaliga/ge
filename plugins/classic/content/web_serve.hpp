// A CLASSIC GAME SERVER'S FILES ON THE WEB, for every classic game's dedicated server: the lookup
// its web file server answers from (engine/server/web_files.hpp), so a client fetches over HTTPS
// what it would otherwise take in pieces over the game's channel. A client asks
// `<game folder>/<path>` (the layout every classic client builds from a server's web address); the
// lookup answers only
//
// - the game folder the server plays (`WebServe::game_folder`) or its base folder,
// - a kind of file the game's servers send (`WebServe::kinds`: Quake's `quake_web_kinds`, Quake
//   II's `WebConfig::kinds`), a pack never,
// - a path the server's own download switches let through (`WebServe::allowed`: the same
//   `sv_download_*` a client asking over the channel is held to),
//
// read from the server's search path (packs and loose files alike), so the web never gives what
// the game's channel would refuse.
//
// ge:summary A classic server's web files: the game folder, the kinds and its download switches
// ge:keywords web-serve web-files sv_http_port classic-server fast-downloads allow-list
// ge:keywords web_serve_lookup
// ge:skill classic net
// ge:test tests/classic_content_web_serve_test.cpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "classic/content/files.hpp"
#include "net/file_server.hpp"

namespace ge::classic::content {

struct WebServe {
  std::string game_folder;  // the mod's folder the server plays, or the base's
  std::string base_folder;  // `id1`, `qw`, `baseq2`
  std::vector<std::string> kinds;
  std::function<bool(const std::string& path)> allowed;  // null: every path of those kinds
};

// The lookup over `files` (kept by reference: it must outlive the server).
ge::net::FileLookup web_serve_lookup(const SearchPath& files, WebServe rules);
// Whether `path` (`<game folder>/<file>`) is one `rules` serves; `file` the game's own path.
bool web_serve_allows(const WebServe& rules, const std::string& path, std::string& file);

}  // namespace ge::classic::content
