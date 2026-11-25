// AN HTTP GET, for any game that fetches a file from a web server (Quake II's downloads from the
// server's `dlserver`, a server list): one request at a time per fetch, over the OS's TCP socket
// (tcp_socket.hpp's raw bytes), non-blocking, stepped each frame.
//
// - THE URL: `http://host[:port]/path` (port 80 by default). `https://` is read but not fetched:
//   the fetch fails at once with "Unsupported protocol", so a caller falls back to its own way.
// - THROUGH A PROXY (`HttpOptions::proxy`, "host:port" or "http://host:port"): the request goes to
//   the proxy with the whole URL in its first line.
// - THE REQUEST: `GET`, HTTP/1.1, `Host`, `User-Agent`, `Referer` when given, `Connection:
//   close`; `Accept-Encoding` when `HttpOptions::accept_encoding` names one, the body then left
//   packed as it came (`HttpFetch::encoding` says how) for the caller to unpack.
// - THE ANSWER: the status, the body by `Content-Length`, by chunks, or to the close; up to
//   `HttpOptions::redirects` redirects (301, 302, 303, 307, 308) followed to their `Location`
//   (0: none, the redirect is the answer).
// - THE FAILURES, in words a caller can print (`HttpFetch::error`): "Couldn't resolve host name",
//   "Couldn't connect to server", "Couldn't resolve proxy name", "URL using bad/illegal format or
//   missing URL", TLS's own (tls_stream.hpp: a certificate not trusted, no roots, the handshake),
//   "Timeout was reached" (no byte for `HttpOptions::stall_s`), "Number of redirects hit maximum
//   amount", "Transferred a partial file", "Maximum file size exceeded"
//   (`HttpOptions::most_bytes`), "Range not satisfiable" (a resume the server could not start).
// - A RESUME (`HttpOptions::from` > 0): `Range: bytes=<from>-` asked; a `206` whose
//   `Content-Range` starts there gives the rest alone (`HttpFetch::offset` = `from`, `total` the
//   whole file's size), a `200` the whole file again (`offset` 0).
// - A SPEED CAP (`HttpOptions::rate`, bytes a second; 0 none): the socket read no faster, so the
//   server's sending slows to it (the OS's buffer fills and TCP's window closes).
// - THE FILE'S DIGEST: `Repr-Digest: sha-256=:<base64>:` (RFC 9530) or the older `Digest:
//   SHA-256=<base64>` read into `HttpFetch::sha256` (empty when the answer gave none) for the
//   caller to check the whole file against (`http_digest_ok`).
// - `http_status_text`: a status as words ("404 Not Found"; an unusual one by its class).
// - `url_escape`: every byte but a letter, a digit and `/-_.~` as `%XX`.
//
// ge:summary An HTTP GET over a non-blocking TCP socket: URL, proxy, redirects, chunks, failures
// ge:keywords http http-get fetch download url proxy redirect chunked content-length status
// ge:keywords url_escape http_status_text user-agent referer https tls connect-tunnel
// ge:keywords resume range content-range 206 rate-cap repr-digest sha-256 digest
// ge:skill net
// ge:test tests/net_http_fetch_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "net/tcp_socket.hpp"
#include "net/tls_stream.hpp"

namespace ge::net {

struct HttpUrl {
  bool https = false;
  std::string host;
  uint16_t port = 80;
  std::string path = "/";  // with its query
};

// `text` read as a URL; false when it is not `http://` or `https://` with a host.
bool read_http_url(std::string_view text, HttpUrl& out);
std::string url_escape(std::string_view text);
std::string http_status_text(int code);

struct HttpOptions {
  std::string user_agent = "ge";
  std::string referer;                  // "": none sent
  std::string proxy;                    // "": straight to the server
  std::string ca_pem;                   // https: "" the system's roots; else these alone
  std::string accept_encoding;          // "": none asked; else sent as `Accept-Encoding`
  int redirects = 5;                    // 0: none followed, the redirect itself the answer
  double stall_s = 30;                  // no byte for this long: "Timeout was reached"
  size_t most_bytes = size_t{1} << 30;  // a body larger is refused
  bool verify = true;                   // https: the server's certificate and name checked
  std::vector<uint8_t> pin;             // https: 32 bytes, the one certificate trusted (tls)
  int64_t from = 0;                     // > 0: a resume, the bytes from here asked (`Range`)
  int64_t rate = 0;                     // the most bytes a second read (0: no cap)
};

enum class HttpState : uint8_t { idle, connecting, waiting, done, failed };

struct HttpFetch {
  HttpState state = HttpState::idle;
  bool tunnel = false;  // https through a proxy: the proxy's answer to `CONNECT` awaited
  HttpOptions options;
  std::string url;  // the one asked now (after any redirect)
  int status = 0;   // the answer's (0: none yet)
  std::vector<uint8_t> body;
  int64_t total = -1;    // the body's length when the answer gave it
  std::string error;     // why it failed ("" while it goes or once done)
  std::string encoding;  // the answer's `Content-Encoding`, lower case ("": none; the body as sent)
  int redirected = 0;
  double last_byte_s = 0;
  int64_t offset = 0;           // a resume's: where `body` starts in the file (0: the whole file)
  std::vector<uint8_t> sha256;  // the answer's digest of the whole file (32 bytes; empty: none)
  // The answer's head once read (0: not yet), its framing and its body's length (-1 unknown).
  size_t head_end = 0;
  bool chunked = false;
  int64_t length = -1;
  double tokens = 0;     // the speed cap's bytes in hand
  double rate_at_s = 0;  // ... as of then
  size_t seen = 0;       // the raw bytes looked at for a whole chunked body
  TcpStream stream;
  std::shared_ptr<TlsStream> tls;  // https: the session over `stream`
  std::string request;             // https: sent once the tunnel is open
  std::vector<uint8_t> raw;        // the answer as it came (plain)
};

// Starts `f` on `url` at `now_s` (a running one dropped). A URL it cannot fetch fails at once.
void http_start(HttpFetch& f, const std::string& url, const HttpOptions& options, double now_s);
// Moves the fetch on; true once it is over (`done` with the status and body, or `failed`).
bool http_step(HttpFetch& f, double now_s);
// Drops a running fetch.
void http_cancel(HttpFetch& f);
// The bytes of the body come so far (for a progress bar), a resume's `offset` not counted.
size_t http_received(const HttpFetch& f);
// The body's bytes come so far when a resume could go on from them (a body by length or to the
// close, as sent: not packed by the server); false when none could.
bool http_partial(const HttpFetch& f, std::vector<uint8_t>& out);
// Whether `bytes` (the whole file) match the answer's digest; true when it gave none.
bool http_digest_ok(const HttpFetch& f, const std::vector<uint8_t>& bytes);
// `text` read as standard base64 (padding optional); false when it is not.
bool base64_decode(std::string_view text, std::vector<uint8_t>& out);
std::string base64_encode(const uint8_t* data, size_t size);

}  // namespace ge::net
