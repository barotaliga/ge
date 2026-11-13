// The hub's HTTP, defined: the request head parsed, responses and events written, the server loop.
//
// ge:summary The hub's HTTP defined: the head parsed, responses and events written, the loop
// ge:keywords hub http parse response sse event loop accept close
// ge:skill net
// ge:test tests/hub_http_test.cpp
#include "hub/http.hpp"

#include <algorithm>
#include <utility>

#include "core/json_write.hpp"
#include "hub/pages.hpp"

namespace ge::hub {

namespace gn = ge::net;

namespace {

std::string lower_ascii(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

const char* reason(int code) {
  switch (code) {
    case 200:
      return "OK";
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 431:
      return "Request Header Fields Too Large";
    case 503:
      return "Service Unavailable";
    default:
      return "Error";
  }
}

constexpr size_t kFeedPiece = size_t{64} * 1024;  // an answer goes to the stream this much at once

// As much of the answer's bytes as the stream's backlog takes now (a viewer's file or a pack is
// megabytes: the rest goes as the client reads it, over the next updates).
void feed(HttpConn& c) {
  while (c.out_at < c.out.size()) {
    const size_t n = std::min(kFeedPiece, c.out.size() - c.out_at);
    if (!gn::tcp_send_bytes(c.stream, reinterpret_cast<const uint8_t*>(c.out.data() + c.out_at), n))
      break;
    c.out_at += n;
  }
  if (c.out_at >= c.out.size()) {
    c.out.clear();
    c.out_at = 0;
  }
}

void send(HttpConn& c, const std::string& bytes) {
  c.out += bytes;
  feed(c);
}

bool drained(const HttpConn& c) {
  return c.out_at >= c.out.size() && c.stream.out_head >= c.stream.out.size();
}

}  // namespace

HttpParse http_parse(std::string_view data, HttpRequest& out) {
  const size_t end = data.find("\r\n\r\n");
  if (end == std::string_view::npos) {
    return data.size() > kMaxRequest ? HttpParse::too_big : HttpParse::more;
  }
  if (end + 4 > kMaxRequest) return HttpParse::too_big;
  const std::string_view line = data.substr(0, data.find("\r\n"));
  const size_t a = line.find(' ');
  const size_t b = a == std::string_view::npos ? a : line.find(' ', a + 1);
  if (b == std::string_view::npos) return HttpParse::bad;
  out.method = std::string(line.substr(0, a));
  out.target = std::string(line.substr(a + 1, b - a - 1));
  out.version = std::string(line.substr(b + 1));
  out.head_size = end + 4;
  if (out.target.empty() || out.target[0] != '/' || out.version.rfind("HTTP/1.", 0) != 0) {
    return HttpParse::bad;
  }
  out.host.clear();
  out.gzip = false;
  const std::string_view head = data.substr(0, end);
  for (size_t at = head.find("\r\n"); at != std::string_view::npos && at < head.size();) {
    const size_t from = at + 2;
    const size_t eol = std::min(head.find("\r\n", from), head.size());
    const std::string_view h = head.substr(from, eol - from);
    if (h.size() > 5 && (h[0] == 'H' || h[0] == 'h') &&
        (h.substr(1, 4) == "ost:" || h.substr(1, 4) == "OST:")) {
      std::string_view v = h.substr(5);
      while (!v.empty() && v.front() == ' ') v.remove_prefix(1);
      while (!v.empty() && v.back() == ' ') v.remove_suffix(1);
      out.host = std::string(v);
    }
    if (h.size() > 16 && lower_ascii(h.substr(0, 16)) == "accept-encoding:" &&
        lower_ascii(h.substr(16)).find("gzip") != std::string::npos) {
      out.gzip = true;
    }
    at = eol;
  }
  return HttpParse::ok;
}

std::string http_response(const Answer& a, bool head_only) {
  std::string r = "HTTP/1.1 " + std::to_string(a.code) + " " + reason(a.code) + "\r\n";
  const bool text = a.type.rfind("text/", 0) == 0 || a.type == "application/json";
  r += "Content-Type: " + a.type + (text ? "; charset=utf-8\r\n" : "\r\n");
  r += "Content-Length: " + std::to_string(a.body.size()) + "\r\n";
  if (!a.encoding.empty()) r += "Content-Encoding: " + a.encoding + "\r\nVary: Accept-Encoding\r\n";
  r += a.max_age_s > 0 ? "Cache-Control: public, max-age=" + std::to_string(a.max_age_s) + "\r\n"
                       : std::string("Cache-Control: no-store\r\n");
  r += "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
  if (!head_only) r += a.body;
  return r;
}

std::string http_error(int code, std::string_view what, std::string_view next) {
  Answer a;
  a.code = code;
  a.body =
      ge::core::json_obj({{"error", ge::core::json_str(what)}, {"next", ge::core::json_str(next)}});
  return http_response(a, false);
}

std::string sse_head() {
  return "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=utf-8\r\n"
         "Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: "
         "keep-alive\r\n\r\n";
}

std::string sse_event(std::string_view name, std::string_view data, uint64_t id) {
  std::string e = "id: " + std::to_string(id) + "\nevent: " + std::string(name) + "\n";
  size_t at = 0;
  while (at <= data.size()) {  // one data line per line of the payload
    const size_t nl = data.find('\n', at);
    e +=
        "data: " + std::string(data.substr(at, nl == std::string_view::npos ? nl : nl - at)) + "\n";
    if (nl == std::string_view::npos) break;
    at = nl + 1;
  }
  return e + "\n";
}

bool http_open(HttpServer& s, const gn::Address& bind, std::string& error) {
  std::string why;
  if (!gn::tcp_listen(s.listener, bind, &why)) {
    error = "the hub cannot listen for HTTP on " + gn::to_string(bind) + ": " + why +
            "; next: pass another --http port, or stop what holds this one";
    return false;
  }
  return true;
}

void http_close(HttpServer& s) {
  for (HttpConn& c : s.conns) gn::tcp_close(c.stream);
  s.conns.clear();
  gn::tcp_close_listener(s.listener);
}

void http_update(HttpServer& s, const Poller& p, const HubSpec& spec, int64_t now_us) {
  for (;;) {
    HttpConn c;
    if (!gn::tcp_accept(s.listener, c.stream)) break;
    s.stats.accepted++;
    if (static_cast<int>(s.conns.size()) >= kMaxConnections) {
      s.stats.refused++;
      send(c, http_error(503, "the hub has too many open connections", "try again in a moment"));
      gn::tcp_pump(c.stream);
      gn::tcp_close(c.stream);
      continue;
    }
    c.opened_us = now_us;
    s.conns.push_back(std::move(c));
  }

  for (HttpConn& c : s.conns) {
    gn::tcp_pump(c.stream);
    feed(c);
    std::vector<uint8_t> bytes;
    if (gn::tcp_take_bytes(c.stream, bytes) > 0 && !c.answered && !c.events) {
      c.in.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    if (!c.answered && !c.events) {
      HttpRequest req;
      const HttpParse r = http_parse(c.in, req);
      if (r == HttpParse::more) {
        if (now_us - c.opened_us > kRequestTimeoutUs) c.stream.closed = true;
        continue;
      }
      c.answered = true;
      if (r != HttpParse::ok) {
        s.stats.errors++;
        send(c, r == HttpParse::too_big
                    ? http_error(431, "the request's head is over 8 KB", "send a shorter request")
                    : http_error(400, "the request line is not 'GET /path HTTP/1.1'",
                                 "send a GET for / or a path under /v1"));
      } else if (req.method != "GET" && req.method != "HEAD") {
        s.stats.errors++;
        send(c, http_error(405, "the hub answers GET and HEAD only", "send a GET"));
      } else {
        const Answer a = hub_answer(p, spec, req.target, now_us,
                                    Site{s.archive, s.unix_s, req.host, s.viewer_dir, req.gzip});
        if (a.events && a.code == 200) {
          const HubNode* n = poller_find(p, a.node);
          c.answered = false;
          c.events = true;
          c.node = a.node;
          c.sent_changes = n ? n->changes : 0;
          c.sent_us = now_us;
          s.stats.streams++;
          send(c, sse_head());
          send(c, sse_event("status", a.body, ++c.event_id));
          s.stats.events++;
        } else {
          s.stats.answered++;
          send(c, http_response(a, req.method == "HEAD"));
        }
      }
      gn::tcp_pump(c.stream);
      continue;
    }
    if (c.events) {
      const HubNode* n = poller_find(p, c.node);
      if (!n) {
        send(c, sse_event("gone", "{}", ++c.event_id));
        c.events = false;
        c.answered = true;
      } else if (n->changes != c.sent_changes) {
        send(c, sse_event("status", node_json(spec, *n, now_us), ++c.event_id));
        c.sent_changes = n->changes;
        c.sent_us = now_us;
        s.stats.events++;
      } else if (now_us - c.sent_us >= kEventPingUs) {
        send(c, ": ping\n\n");
        c.sent_us = now_us;
      }
      gn::tcp_pump(c.stream);
    }
  }

  s.conns.erase(std::remove_if(s.conns.begin(), s.conns.end(),
                               [](HttpConn& c) {
                                 const bool done = c.stream.closed || (c.answered && drained(c));
                                 if (done) gn::tcp_close(c.stream);
                                 return done;
                               }),
                s.conns.end());
}

}  // namespace ge::hub
