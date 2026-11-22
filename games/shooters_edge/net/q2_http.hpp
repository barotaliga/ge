// QUAKE II'S DOWNLOADS OVER HTTP: a server may name a web address its files are fetched from
// (` dlserver=<url>` in its answer to `connect`, our server's `sv_downloadserver`), and the
// client then fetches the files it lacks from there rather than in pieces over the game's
// channel, several at once (`cl_http_max_connections`, 1 to 4), while the level waits.
//
// - THE ADDRESS: the first `dlserver=` word of the answer; none: `cl_http_default_url` (the
//   player's own repository). Off with `cl_http_downloads 0`, `allow_download -1` or no address;
//   an address not `http://` or `https://` is ignored, as is one of 512 characters or more. Each
//   is said ("[HTTP] Download server at <url>").
// - A FILE'S ADDRESS: the address, then `<game folder>/<path>` escaped (`baseq2` when no mod is
//   played); asked with the player's agent and `Referer: quake2://<server>`. The first file
//   queued tells the server `download http` (when the address is the server's own).
// - THE FILE LISTS (`cl_http_filelists`): `<game folder>.filelist` with the first file and
//   `<game folder>/maps/<map>.filelist` with each map: the server's list of what else to fetch,
//   one path a line (`@` first: looked for in the game folder only); a pack (`.pak`, `.pkz`) is
//   said and fetched before the rest, a kind a server never sends is refused and said.
// - THE ENDS: a file comes whole on `200 OK` and is kept as one fetched over the channel; each
//   is said with its size, its speed and the files left ("[HTTP] maps/q2dm1.bsp [1.2 MB,
//   340 kB/sec] [3 remaining files]"). A `404` from the server's address leaves that file
//   lacking; any other status, a `404` from the player's own repository, an address that does not
//   resolve or connect, or one the client cannot fetch (`https://`) turns HTTP off until the next
//   connect and hands every file not yet fetched back to the game's channel. Any other failure
//   (a stall, a short file) is said and leaves that file lacking.
//
// The window's own game (a listen server) never joins through here, so its files are never asked.
//
// ge:summary Quake II's downloads over HTTP: the server's dlserver, file lists, fall back to UDP
// ge:keywords q2-http http-download dlserver cl_http_downloads cl_http_filelists filelist
// ge:keywords cl_http_max_connections cl_http_proxy cl_http_default_url cl_http_insecure
// ge:keywords cl_http_debug cl_http_blocking_timeout download-http q2_size_text
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_http_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "net/http_fetch.hpp"

namespace shooters_edge::net {

// The player's `cl_http_*` settings, as the window reads them at connect.
struct Q2HttpConfig {
  std::string proxy;        // cl_http_proxy ("": none)
  std::string default_url;  // cl_http_default_url ("": none)
  std::string gamedir;      // the server's game folder ("": baseq2)
  std::string user_agent = "shooters_edge";
  int max_connections = 2;  // cl_http_max_connections, held to 1..4
  bool downloads = true;    // cl_http_downloads
  bool filelists = true;    // cl_http_filelists
  bool debug = false;       // cl_http_debug: each fetch's address said as it starts
};

struct Q2HttpFile {
  std::string path;
  bool filelist = false;
  bool pak = false;
};

struct Q2HttpSlot {
  Q2HttpFile file;
  ge::net::HttpFetch fetch;
  double started_s = 0;
};

struct Q2Http {
  bool on = false;
  bool default_repo = false;  // the address is the player's own, not the server's
  std::string base;           // the address files are asked under
  std::string referer;
  bool told_server = false;  // `download http` sent
  bool asked_list = false;   // the mod's file list queued
  std::vector<Q2HttpFile> queue;
  std::vector<Q2HttpSlot> slots;
  std::set<std::string> asked;  // every path queued this connection, each tried once
  std::vector<std::string> said;
};

// What one step finished: the files fetched whole, and on a turn to off, the files handed back to
// the game's channel.
struct Q2HttpDone {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::vector<std::string> back;
};

// The first `dlserver=` word's value of the answer to `connect` ("" when none).
std::string q2_http_dlserver(const std::vector<std::string>& words);
// At connect: HTTP on or off for this connection, by the server's `dlserver` ("" none),
// `allow_download` (`all`) and the settings; `server` the game server's address as text.
void q2_http_connect(Q2Http& h, const Q2HttpConfig& cfg, const std::string& dlserver, int all,
                     const std::string& server);
// A file lacked queued (with the file lists it brings); true when the server is to be told
// `download http` now.
bool q2_http_queue(Q2Http& h, const Q2HttpConfig& cfg, const std::string& path);
// Whether any file is queued or coming.
bool q2_http_busy(const Q2Http& h);
// Moves the fetches on at `now_s`, starting the queued into free slots. `wanted` says whether a
// path a file list names is lacked and let through (`game_only`: looked for in the game folder
// alone).
void q2_http_step(Q2Http& h, const Q2HttpConfig& cfg, double now_s,
                  const std::function<bool(const std::string& path, bool game_only)>& wanted,
                  Q2HttpDone& done);
// Drops every fetch (a disconnect).
void q2_http_stop(Q2Http& h);
// A size in bytes as the progress lines say it: "1.2 GB", "34 MB", "1.5 MB", "340 kB",
// "1 byte", "12 bytes", "unknown size" when negative.
std::string q2_size_text(int64_t bytes);

}  // namespace shooters_edge::net
