// AN HTTP GET, for any game that fetches a file from a web server (Quake II's downloads from the
// server's `dlserver`, a server list): one request at a time per fetch, over the OS's TCP socket
// (tcp_socket.hpp's raw bytes), non-blocking, stepped each frame.
//
// - THE URL: `http://host[:port]/path` (port 80 by default). `https://` is read but not fetched:
//   the fetch fails at once with "Unsupported protocol", so a caller falls back to its own way.
// - THROUGH A PROXY (`HttpOptions::proxy`, "host:port" or "http://host:port"): the request goes to
//   the proxy with the whole URL in its first line.
// - THE REQUEST: `GET`, HTTP/1.1, `Host`, `User-Agent`, `Referer` when given, `Connection:
//   close`; no compression asked for.
// - THE ANSWER: the status, the body by `Content-Length`, by chunks, or to the close; up to
//   `HttpOptions::redirects` redirects (301, 302, 303, 307, 308) followed to their `Location`
//   (0: none, the redirect is the answer).
// - THE FAILURES, in words a caller can print (`HttpFetch::error`): "Couldn't resolve host name",
//   "Couldn't connect to server", "Couldn't resolve proxy name", "Unsupported protocol", "URL
//   using bad/illegal format or missing URL", "Timeout was reached" (no byte for
//   `HttpOptions::stall_s`), "Number of redirects hit maximum amount", "Transferred a partial
//   file", "Maximum file size exceeded" (`HttpOptions::most_bytes`).
// - `http_status_text`: a status as words ("404 Not Found"; an unusual one by its class).
// - `url_escape`: every byte but a letter, a digit and `/-_.~` as `%XX`.
//
// ge:summary An HTTP GET over a non-blocking TCP socket: URL, proxy, redirects, chunks, failures
// ge:keywords http http-get fetch download url proxy redirect chunked content-length status
// ge:keywords url_escape http_status_text user-agent referer
// ge:skill net
// ge:test tests/net_http_fetch_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "net/tcp_socket.hpp"

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
  int redirects = 5;                    // 0: none followed, the redirect itself the answer
  double stall_s = 30;                  // no byte for this long: "Timeout was reached"
  size_t most_bytes = size_t{1} << 30;  // a body larger is refused
};

enum class HttpState : uint8_t { idle, connecting, waiting, done, failed };

struct HttpFetch {
  HttpState state = HttpState::idle;
  HttpOptions options;
  std::string url;  // the one asked now (after any redirect)
  int status = 0;   // the answer's (0: none yet)
  std::vector<uint8_t> body;
  int64_t total = -1;  // the body's length when the answer gave it
  std::string error;   // why it failed ("" while it goes or once done)
  int redirected = 0;
  double last_byte_s = 0;
  TcpStream stream;
  std::vector<uint8_t> raw;  // the answer as it came
};

// Starts `f` on `url` at `now_s` (a running one dropped). A URL it cannot fetch fails at once.
void http_start(HttpFetch& f, const std::string& url, const HttpOptions& options, double now_s);
// Moves the fetch on; true once it is over (`done` with the status and body, or `failed`).
bool http_step(HttpFetch& f, double now_s);
// Drops a running fetch.
void http_cancel(HttpFetch& f);
// The bytes of the body come so far (for a progress bar).
size_t http_received(const HttpFetch& f);

}  // namespace ge::net
