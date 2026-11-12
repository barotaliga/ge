// THE HUB'S HTTP: HTTP/1.1 over the OS's TCP (net/tcp_socket.hpp, raw bytes), enough for a hub
// and no more (docs/hub.md § Hosting it). GET and HEAD, one request a connection then close,
// answers from the pages and the JSON API (pages.hpp, api.hpp) with CORS open and their cache
// time, and server-sent events for a node's record (`/v1/servers/{address}/events`: one
// connection kept open, an event each time the record changes, a comment line every 15 s so
// proxies keep it). No TLS: an operator's reverse proxy does that. The request parser and the
// response writer are pure functions over bytes, tested without a socket; `HttpServer` is the
// thin loop that carries them.
//
// ge:summary The hub's HTTP/1.1: request parsing, responses, server-sent events, the server loop
// ge:keywords hub http http-1-1 get head request response cors cache-control sse event-stream
// ge:keywords server-sent-events keepalive tcp listen
// ge:skill net
// ge:test tests/hub_http_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hub/api.hpp"
#include "net/tcp_socket.hpp"

namespace ge::hub {

constexpr size_t kMaxRequest = 8192;             // a request's head: more is refused (431)
constexpr int64_t kRequestTimeoutUs = 10000000;  // a request not whole by then: closed
constexpr int64_t kEventPingUs = 15000000;       // an open event stream's comment line
constexpr int kMaxConnections = 256;

struct HttpRequest {
  std::string method, target, version;
  size_t head_size = 0;  // the bytes the head took, the blank line with it
};

enum class HttpParse : uint8_t { more, ok, bad, too_big };

// The head of a request at the start of `data`: `more` until the blank line has come.
HttpParse http_parse(std::string_view data, HttpRequest& out);

// A whole response: the status line, the headers (type, length, cache, CORS, close), the body
// unless `head_only`.
std::string http_response(const Answer& a, bool head_only);
// An error response: `code` with a one-line JSON body saying what was wrong and what to do.
std::string http_error(int code, std::string_view what, std::string_view next);
// An event stream's head, and one event (`data` on one line; an id the browser resumes from).
std::string sse_head();
std::string sse_event(std::string_view name, std::string_view data, uint64_t id);

struct HttpConn {
  net::TcpStream stream;
  std::string in;
  int64_t opened_us = 0;
  bool answered = false;  // the answer is queued: close once it has gone
  bool events = false;    // an open event stream
  net::Address node;
  uint64_t sent_changes = 0;
  int64_t sent_us = 0;
  uint64_t event_id = 0;
};

struct HttpStats {
  uint64_t accepted = 0, answered = 0, errors = 0, events = 0, streams = 0, refused = 0;
};

struct HttpServer {
  net::TcpListener listener;
  std::vector<HttpConn> conns;
  HttpStats stats;
};

// Listen on `bind`; false with `error` (the address, the OS's reason, the next action).
bool http_open(HttpServer& s, const net::Address& bind, std::string& error);
void http_close(HttpServer& s);
// Accept, read, answer, push events, close what is done. One call a frame.
void http_update(HttpServer& s, const Poller& p, const HubSpec& spec, int64_t now_us);

}  // namespace ge::hub
