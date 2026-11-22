// A CLASSIC GAME'S DOWNLOADS FROM A WEB ADDRESS, for every classic game a client joins servers
// of: a server may name a web address its files are fetched from (Quake II's ` dlserver=<url>` in
// its answer to `connect`; our QuakeWorld server's `*dlserver` in its server info), and the client
// then fetches the files it lacks from there rather than in pieces over the game's channel,
// several at once (`cl_http_max_connections`, 1 to 4), while the level waits.
//
// - THE ADDRESS: the server's; none: `cl_http_default_url` (the player's own repository). Off
//   with `cl_http_downloads 0`, `allow_download -1` or no address; an address not `http://`,
//   `https://` or a GitHub repository is ignored, as is one of 512 characters or more. Each is
//   said ("[HTTP] Download server at <url>").
// - A GITHUB REPOSITORY (`web_base`): `github:<owner>/<repo>[@<branch or tag>][/<folder>]`, or the
//   repository's page `https://github.com/<owner>/<repo>[/tree/<ref>[/<folder>]]`, is read as its
//   raw files' address (`https://raw.githubusercontent.com/<owner>/<repo>/<ref>/<folder>/`; the
//   default branch when no ref is named), so a community keeps a game's maps and skins in a
//   repository and names it.
// - A FILE'S ADDRESS: the address, then `<game folder>/<path>` escaped (`WebConfig::base_folder`
//   when no mod is played: `baseq2`, `id1`); asked with the player's agent and `Referer:
//   <referer_scheme>://<server>`. The first file queued tells the server `download http` (when
//   the address is the server's own: `web_queue` answers true).
// - THE FILE LISTS (`cl_http_filelists`): `<game folder>.filelist` with the first file and
//   `<game folder>/maps/<map>.filelist` with each map: the server's list of what else to fetch,
//   one path a line (`@` first: looked for in the game folder only); a pack (`.pak`, `.pkz`) is
//   said and fetched before the rest, a kind the game's servers never send (`WebConfig::kinds`)
//   is refused and said.
// - THE ENDS: a file comes whole on `200 OK` and is kept as one fetched over the channel; each
//   is said with its size, its speed and the files left ("[HTTP] maps/q2dm1.bsp [1.2 MB,
//   340 kB/sec] [3 remaining files]"). A `404` from the server's address leaves that file
//   lacking; any other status, a `404` from the player's own repository, an address that does not
//   resolve or connect, or a secure one whose certificate is not trusted (`cl_http_insecure 1`
//   leaves it unchecked) or whose TLS fails turns HTTP off until the next connect and
//   hands every file not yet fetched back to the game's channel. Any other failure (a stall, a
//   short file) is said and leaves that file lacking.
//
// The window's own game (a listen server) never joins through here, so its files are never asked.
//
// ge:summary A classic game's downloads from a web address or a GitHub repository, UDP behind
// ge:keywords web-downloads http-download dlserver cl_http_downloads cl_http_filelists filelist
// ge:keywords cl_http_max_connections cl_http_proxy cl_http_default_url cl_http_insecure
// ge:keywords cl_http_debug cl_http_blocking_timeout download-http web_size_text github
// ge:keywords github-repository raw-githubusercontent web_base
// ge:skill classic net
// ge:test tests/classic_content_web_downloads_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "net/http_fetch.hpp"

namespace ge::classic::content {

// The player's `cl_http_*` settings as the window reads them at connect, and the game's own.
struct WebConfig {
  std::string proxy;        // cl_http_proxy ("": none)
  std::string default_url;  // cl_http_default_url ("": none)
  std::string gamedir;      // the server's game folder ("": `base_folder`)
  std::string base_folder = "baseq2";
  std::string referer_scheme = "quake2";
  std::string user_agent = "shooters_edge";
  // The kinds of file a file list may name (its packs aside), lower case.
  std::vector<std::string> kinds = {"bsp", "dm2", "ent", "jpg", "loc", "md2", "md3", "ogg",
                                    "pcx", "png", "sp2", "tga", "txt", "wal", "wav"};
  int max_connections = 2;  // cl_http_max_connections, held to 1..4
  bool downloads = true;    // cl_http_downloads
  bool filelists = true;    // cl_http_filelists
  bool debug = false;       // cl_http_debug: each fetch's address said as it starts
  bool insecure = false;    // cl_http_insecure: a secure address's certificate unchecked
};

// Quake's kinds of file (a file list's on a Quake or QuakeWorld server).
std::vector<std::string> quake_web_kinds();

struct WebFile {
  std::string path;
  bool filelist = false;
  bool pak = false;
};

struct WebSlot {
  WebFile file;
  ge::net::HttpFetch fetch;
  double started_s = 0;
};

struct WebDownloads {
  bool on = false;
  bool default_repo = false;  // the address is the player's own, not the server's
  std::string base;           // the address files are asked under
  std::string referer;
  bool told_server = false;  // `download http` sent
  bool asked_list = false;   // the mod's file list queued
  std::vector<WebFile> queue;
  std::vector<WebSlot> slots;
  std::set<std::string> asked;  // every path queued this connection, each tried once
  // Every download's line for the player: over HTTP, and the game's channel's it adds.
  std::vector<std::string> said;
};

// What one step finished: the files fetched whole, and on a turn to off, the files handed back to
// the game's channel.
struct WebDone {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::vector<std::string> back;
};

// The address files are asked under: a GitHub repository's raw files' (above), any other as it is.
std::string web_base(const std::string& text);
// The first `dlserver=` word's value ("" when none).
std::string web_dlserver(const std::vector<std::string>& words);
// At connect: HTTP on or off for this connection, by the server's address ("" none),
// `allow_download` (`all`) and the settings; `server` the game server's address as text.
void web_connect(WebDownloads& h, const WebConfig& cfg, const std::string& dlserver, int all,
                 const std::string& server);
// A file lacked queued (with the file lists it brings); true when the server is to be told
// `download http` now.
bool web_queue(WebDownloads& h, const WebConfig& cfg, const std::string& path);
// Whether any file is queued or coming.
bool web_busy(const WebDownloads& h);
// Moves the fetches on at `now_s`, starting the queued into free slots. `wanted` says whether a
// path a file list names is lacked and let through (`game_only`: looked for in the game folder
// alone).
void web_step(WebDownloads& h, const WebConfig& cfg, double now_s,
              const std::function<bool(const std::string& path, bool game_only)>& wanted,
              WebDone& done);
// Drops every fetch (a disconnect).
void web_stop(WebDownloads& h);
// A size in bytes as the progress lines say it: "1.2 GB", "34 MB", "1.5 MB", "340 kB",
// "1 byte", "12 bytes", "unknown size" when negative.
std::string web_size_text(int64_t bytes);
// "[3 remaining files]", "[1 remaining file]".
std::string web_files_left(int n);

}  // namespace ge::classic::content
