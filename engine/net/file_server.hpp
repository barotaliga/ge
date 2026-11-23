// A GAME SERVER'S OWN WEB FILES, for every game whose clients fetch the files they lack (a map, a
// skin, a sound) from a web address rather than in pieces over the game's channel: a small HTTP/1.1
// file server inside the game server's process, over TLS when the operator gives it a certificate
// and its key, held to a speed for each player and for everyone at once.
//
// - WHAT IT SERVES: `GET` and `HEAD` of a path the caller's `FileLookup` answers with its bytes;
//   the server refuses every other method (405) and every path that is not plain before the lookup
//   ever sees it (`file_path_safe`: no `..` or `.` segment, no backslash, colon, control byte or
//   empty segment, at most 255 bytes; 403). The lookup is the allow list (the game's folders and
//   kinds of file); a path it does not answer is a 404. Each file's bytes are read once and kept
//   with their SHA-256 until `file_server_forget` (a map change).
// - THE ANSWER: `200` with `Content-Length`, `Accept-Ranges: bytes`, `ETag` and `Repr-Digest:
//   sha-256=:<base64>:` (RFC 9530), so a client checks the whole file before it uses it; a resume
//   (`Range: bytes=A-` or `A-B`) gets `206` with `Content-Range`, a range past the end `416`.
//   One request a connection, then closed (`Connection: close`).
// - THE SPEED (`FileServeOptions::rate`, `rate_person`, bytes a second, 0 no cap): a player is
//   every connection from one address, so parallel fetches share the player's cap; the total is
//   shared fairly among the players sending at the time (`fair_shares`: each gets an equal part,
//   what one does not need goes to the others), and a player's part fairly among their
//   connections. A burst is at most a sixteenth of a second of the cap (a frame or so).
// - THE GUARDS: at most `max_connections` at once (more: `503`), `max_per_address` from one
//   address (more: `429`), a request's head at most 8 KB (`431`) and whole within 10 s, a
//   connection with no byte moving for `idle_s` closed.
// - TLS: `cert_pem` and `key_pem` both set: every connection is TLS (1.2 or 1.3, tls_stream.hpp);
//   both empty: plain HTTP (behind an operator's own TLS proxy, or on a LAN).
//
// The request parser, the path check and the shares are pure functions, tested without a socket.
//
// ge:summary A game server's own web files: HTTP/1.1 and TLS, ranges, digests, fair speed caps
// ge:keywords file-server http-server https web-files downloads range resume repr-digest sha-256
// ge:keywords rate-cap fair-share per-player-rate total-rate tls-server allow-list path-traversal
// ge:keywords file_server_open file_server_step fair_shares file_path_safe
// ge:skill net
// ge:test tests/net_file_server_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "net/tls_stream.hpp"

namespace ge::net {

struct FileServeOptions {
  int64_t rate = 0;         // the most bytes a second to everyone at once (0: no cap)
  int64_t rate_person = 0;  // the most bytes a second to one address (0: no cap)
  int max_connections = 64;
  int max_per_address = 6;
  double idle_s = 20;    // no byte moving for this long: closed
  std::string cert_pem;  // TLS: the certificate chain, leaf first ("" with `key_pem`: plain HTTP)
  std::string key_pem;
  std::string name = "ge";  // the `Server` header
};

// The bytes of the file at `path` (decoded, no leading slash, already checked plain); false: 404.
using FileLookup = std::function<bool(const std::string& path, std::vector<uint8_t>& out)>;

struct FileRequest {
  std::string method;
  std::string path;   // decoded, the leading `/` and any query dropped
  int64_t from = -1;  // `Range: bytes=from-to` (-1: none asked)
  int64_t to = -1;    // inclusive (-1: to the end)
  size_t head_size = 0;
};

enum class FileParse : uint8_t { more, ok, bad, too_big };

// The head of a request at the start of `data`: `more` until its blank line has come.
FileParse file_request_parse(std::string_view data, FileRequest& out);
// Whether a decoded path may reach a lookup (above).
bool file_path_safe(std::string_view path);
// `budget` shared among `wants` fairly (max-min: equal parts, a part one does not need given to
// the rest); each share at most its want, the sum at most `budget`.
std::vector<int64_t> fair_shares(int64_t budget, const std::vector<int64_t>& wants);

struct FileConn {
  TcpStream stream;
  std::shared_ptr<TlsStream> tls;
  std::string in;  // the request's bytes so far (plain)
  double opened_s = 0, moved_s = 0;
  bool answered = false;
  std::shared_ptr<const std::vector<uint8_t>> file;  // the body being sent (null: none)
  size_t at = 0, end = 0;                            // ... these bytes of it, from `at`
  size_t person = 0;                                 // `FileServer::people`' index
};

struct FilePerson {
  Address address;  // the port zeroed
  double tokens = 0;
  int conns = 0;
  uint64_t sent = 0;
};

struct FileServeStats {
  uint64_t accepted = 0, served = 0, partial = 0, not_found = 0, refused = 0, bad = 0;
  uint64_t bytes = 0;  // body bytes sent, ever
};

struct FileServer {
  TcpListener listener;
  FileServeOptions options;
  FileLookup lookup;
  std::vector<FileConn> conns;
  std::vector<FilePerson> people;  // every address with a connection open
  struct Cached {
    std::shared_ptr<const std::vector<uint8_t>> bytes;
    std::string digest;  // base64 SHA-256
  };
  std::unordered_map<std::string, Cached> cache;
  double tokens = 0, last_s = -1;
  FileServeStats stats;
};

// Listens on `bind`; false with `error` (what failed, the next action) when it cannot, or when
// the certificate and key do not read or do not match.
bool file_server_open(FileServer& s, const Address& bind, const FileServeOptions& options,
                      FileLookup lookup, std::string& error);
// Accepts, reads, answers and sends within the caps; one call a frame.
void file_server_step(FileServer& s, double now_s);
// Drops every file read (they are read again when next asked).
void file_server_forget(FileServer& s);
void file_server_close(FileServer& s);
// Whether it listens.
inline bool file_server_listening(const FileServer& s) { return s.listener.handle != -1; }

}  // namespace ge::net
