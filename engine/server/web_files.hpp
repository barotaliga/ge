// A SERVER'S FILES OVER HTTPS, for every game whose clients download what they lack: the dedicated
// server sends a map, a skin or a sound from its own web file server (net/file_server.hpp) on a
// TCP port beside its game port, many times faster than in pieces over the game's own connection,
// with the speed held for each player and for everyone together.
//
// - THE SETTINGS (`kWebFilesRules`, read by `web_files_config`): `sv_http_port` turns it on;
//   `sv_http_rate` (kB a second to every player together, 20000 by default) and
//   `sv_http_rate_player` (kB a second to one player, 5000 by default; a player's parallel
//   fetches share it, and the total is shared fairly among the players fetching at the time);
//   `sv_http_cert` and `sv_http_key` (PEM files) for a certificate the operator has; `sv_http_url`
//   the address clients are told; `sv_http_plain 1` for an operator's own TLS proxy in front.
// - SECURE WITH NOTHING SET UP: with no certificate given the server makes its own when it starts
//   (P-256, `tls_self_signed`) and tells its clients that certificate's SHA-256 beside the address
//   (`WebFiles::pin`), so a client of this engine checks the server's files' connection against
//   it: the same trust as the game connection the address came over, and no one between them can
//   change a byte. A client of another engine cannot check a certificate it was not told about,
//   so for those an operator sets a certificate a public authority signed and `sv_http_url` to the
//   host name it names.
// - THE ADDRESS TOLD (`WebFiles::url`): `sv_http_url` when set; else `https://<host>:<port>/`,
//   the host `sv_publicaddr`'s when it is set, else `{server}`, which a client of this engine
//   reads as the host it joined (`web_files_told` says whether an address names a real host, the
//   kind a client of another engine can use).
// - WHAT IS SERVED is the game's: the lookup it passes (its folders and kinds of file, its own
//   download switches); the file server refuses any path that climbs before the lookup sees it.
//
// ge:summary A dedicated server's files over HTTPS: the sv_http_ settings, its own certificate
// ge:keywords web-files sv_http_port sv_http_rate sv_http_rate_player sv_http_cert sv_http_key
// ge:keywords sv_http_url sv_http_plain https downloads fast-downloads certificate-pin self-signed
// ge:keywords web_files_open web_files_frame download-rate total-rate per-player-rate
// ge:skill net
// ge:test tests/server_web_files_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "net/address.hpp"
#include "net/file_server.hpp"
#include "server/server.hpp"

namespace ge::server {

inline constexpr GameRule kWebFilesRules[] = {
    {"sv_http_port",
     "The TCP port the server sends players the files they lack from, over HTTPS: far faster "
     "than over the game's own connection.",
     nullptr, "0: off (the default); 1..65535: that port", false},
    {"sv_http_rate",
     "The most kilobytes a second the server's web files go out at, to every player together.",
     "20000", "0: no cap; 1..N: that many kB a second (20000 the default)", false},
    {"sv_http_rate_player",
     "The most kilobytes a second one player's web files come at, all their downloads together.",
     "5000", "0: no cap; 1..N: that many kB a second (5000 the default)", false},
    {"sv_http_cert", "The PEM file with the certificate chain the web files are sent under.",
     nullptr,
     "\"\": the server makes its own and tells its players how to trust it (the default); TEXT: "
     "the file's path",
     false},
    {"sv_http_key", "The PEM file with sv_http_cert's private key.", nullptr,
     "\"\": none (the default); TEXT: the file's path", false},
    {"sv_http_url", "The address players are told to fetch the web files from.", nullptr,
     "\"\": this server's own address and sv_http_port (the default); TEXT: an https:// address, "
     "the host name sv_http_cert names or a proxy in front",
     false},
    {"sv_http_plain", "Whether the web files go out as plain HTTP, for a TLS proxy in front.",
     nullptr, "0: HTTPS (the default); 1: plain HTTP", false},
};

struct WebFilesConfig {
  int port = 0;
  int64_t rate_kb = 20000;
  int64_t rate_player_kb = 5000;
  std::string cert_file, key_file;
  std::string url;
  std::string public_host;  // sv_publicaddr's host ("": none)
  bool plain = false;
};

// The settings read from `c`'s rules; false with `error` (the setting, its value, the next action).
bool web_files_config(const ServerConfig& c, WebFilesConfig& out, std::string& error);

struct WebFiles {
  net::FileServer server;
  WebFilesConfig cfg;
  std::string url;  // the address told ("": off)
  std::string pin;  // the server's own certificate's SHA-256 in hex ("": a given one, or plain)
};

// Starts serving on `ip`'s address at `cfg.port` (`cfg.port` 0: off, and true): the certificate
// read or made, the address worked out. False with `error` (a file that does not read, a port
// held).
bool web_files_open(WebFiles& w, const WebFilesConfig& cfg, const net::Address& ip,
                    net::FileLookup lookup, std::string& error);
void web_files_frame(WebFiles& w, double now_s);
void web_files_close(WebFiles& w);
// Whether `url` names a real host (not `{server}`): one any client can use.
bool web_files_told(const std::string& url);
// `url` with `{server}` read as `host`.
std::string web_files_at(const std::string& url, const std::string& host);

}  // namespace ge::server
