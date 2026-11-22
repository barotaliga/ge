// An HTTP GET over a non-blocking TCP socket (http_fetch.hpp).
//
// ge:summary An HTTP GET over a non-blocking TCP socket: the request, the answer, redirects
// ge:keywords http http-get fetch download chunked redirect
// ge:skill net
// ge:test tests/net_http_fetch_test.cpp
#include "net/http_fetch.hpp"

#include <utility>

#include "net/udp_socket.hpp"

namespace ge::net {

namespace {

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool same_words(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (lower(a[i]) != lower(b[i])) return false;
  return true;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

bool starts_with_words(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && same_words(s.substr(0, prefix.size()), prefix);
}

void fail(HttpFetch& f, const char* why) {
  tcp_close(f.stream);
  f.state = HttpState::failed;
  f.error = why;
}

// The answer's head as read: the status, the framing, the redirect.
struct Head {
  size_t end = 0;  // where the body starts
  int status = 0;
  int64_t length = -1;
  bool chunked = false;
  std::string location;
};

// The head of `raw` when it is whole; false while it is not. `bad` when it is not HTTP.
bool read_head(const std::vector<uint8_t>& raw, Head& h, bool& bad) {
  const std::string_view text(reinterpret_cast<const char*>(raw.data()), raw.size());
  const size_t end = text.find("\r\n\r\n");
  if (end == std::string_view::npos) return false;
  h = {};
  h.end = end + 4;
  std::string_view head = text.substr(0, end);
  size_t line_end = head.find("\r\n");
  const std::string_view first = head.substr(0, line_end);
  if (!starts_with_words(first, "HTTP/") || first.find(' ') == std::string_view::npos) {
    bad = true;
    return true;
  }
  const std::string_view code = first.substr(first.find(' ') + 1);
  for (size_t i = 0; i < code.size() && i < 3; ++i) {
    if (code[i] < '0' || code[i] > '9') {
      bad = true;
      return true;
    }
    h.status = h.status * 10 + (code[i] - '0');
  }
  while (line_end != std::string_view::npos) {
    head.remove_prefix(line_end + 2);
    line_end = head.find("\r\n");
    const std::string_view line = head.substr(0, line_end);
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos) continue;
    const std::string_view name = trim(line.substr(0, colon));
    const std::string_view value = trim(line.substr(colon + 1));
    if (same_words(name, "content-length")) {
      int64_t n = 0;
      bool digits = !value.empty();
      for (const char c : value) {
        if (c < '0' || c > '9' || n > (int64_t{1} << 50)) {
          digits = false;
          break;
        }
        n = n * 10 + (c - '0');
      }
      if (digits) h.length = n;
    } else if (same_words(name, "transfer-encoding")) {
      h.chunked = value.size() >= 7 && same_words(value.substr(value.size() - 7), "chunked");
    } else if (same_words(name, "location")) {
      h.location = std::string(value);
    }
  }
  return true;
}

// The chunked body from `at` decoded into `out`: 1 whole, 0 more to come, -1 not chunks.
int read_chunks(const std::vector<uint8_t>& raw, size_t at, std::vector<uint8_t>& out,
                size_t most) {
  out.clear();
  while (true) {
    size_t line = at;
    while (line + 1 < raw.size() && !(raw[line] == '\r' && raw[line + 1] == '\n')) ++line;
    if (line + 1 >= raw.size()) return 0;
    size_t n = 0;
    size_t digits = 0;
    for (size_t i = at; i < line; ++i) {
      const char c = static_cast<char>(raw[i]);
      int v = -1;
      if (c >= '0' && c <= '9') v = c - '0';
      if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
      if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
      if (v < 0) break;  // an extension after `;`
      if (n > most) return -1;
      n = n * 16 + static_cast<size_t>(v);
      ++digits;
    }
    if (digits == 0) return -1;
    at = line + 2;
    if (n == 0) return 1;  // trailers are not read: the stream closes after
    if (raw.size() < at + n + 2) return 0;
    out.insert(out.end(), raw.begin() + static_cast<ptrdiff_t>(at),
               raw.begin() + static_cast<ptrdiff_t>(at + n));
    if (out.size() > most) return -1;
    at += n + 2;
  }
}

void send(HttpFetch& f, const std::string& text) {
  tcp_send_bytes(f.stream, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

// The TLS session for `host` over the stream, `request` its first plain bytes.
void start_tls(HttpFetch& f, const std::string& host, const std::string& request) {
  TlsOptions o;
  o.verify = f.options.verify;
  o.ca_pem = f.options.ca_pem;
  f.tls = tls_client(host, o);
  tls_write(*f.tls, reinterpret_cast<const uint8_t*>(request.data()), request.size());
}

// The proxy's answer to `CONNECT` read from `f.raw`: 1 the tunnel is open (its bytes taken), 0
// more to come, -1 refused.
int read_tunnel(HttpFetch& f) {
  const std::string_view text(reinterpret_cast<const char*>(f.raw.data()), f.raw.size());
  const size_t end = text.find("\r\n\r\n");
  if (end == std::string_view::npos) return f.raw.size() > 65536 ? -1 : 0;
  const size_t space = text.find(' ');
  if (space == std::string_view::npos || space > end || text.substr(space + 1, 1) != "2") return -1;
  f.raw.erase(f.raw.begin(), f.raw.begin() + static_cast<ptrdiff_t>(end + 4));
  return 1;
}

// Opens the request for `f.url`: false and failed when it cannot.
bool open(HttpFetch& f, double now_s) {
  tcp_close(f.stream);
  f.stream = {};
  f.tls.reset();
  f.tunnel = false;
  f.request.clear();
  f.raw.clear();
  f.body.clear();
  f.status = 0;
  f.total = -1;
  HttpUrl u;
  if (!read_http_url(f.url, u)) {
    fail(f, "URL using bad/illegal format or missing URL");
    return false;
  }
  Address to;
  std::string why;
  if (!f.options.proxy.empty()) {
    std::string_view proxy = f.options.proxy;
    if (starts_with_words(proxy, "http://")) proxy.remove_prefix(7);
    while (!proxy.empty() && proxy.back() == '/') proxy.remove_suffix(1);
    if (!resolve_ipv4(proxy, 8080, to, &why)) {
      fail(f, "Couldn't resolve proxy name");
      return false;
    }
  } else if (!resolve_ipv4(u.host, u.port, to, &why)) {
    fail(f, "Couldn't resolve host name");
    return false;
  }
  if (!tcp_connect(f.stream, to, &why)) {
    fail(f, "Couldn't connect to server");
    return false;
  }
  std::string host = u.host;
  if (u.port != (u.https ? 443 : 80)) host += ":" + std::to_string(u.port);
  std::string request = "GET ";
  request += f.options.proxy.empty() || u.https ? u.path : "http://" + host + u.path;
  request += " HTTP/1.1\r\nHost: " + host + "\r\nUser-Agent: " + f.options.user_agent + "\r\n";
  if (!f.options.referer.empty()) request += "Referer: " + f.options.referer + "\r\n";
  request += "Accept: */*\r\nConnection: close\r\n\r\n";
  if (!u.https) {
    send(f, request);
  } else if (!f.options.proxy.empty()) {
    std::string target = u.host;
    target += ":" + std::to_string(u.port);
    send(f, "CONNECT " + target + " HTTP/1.1\r\nHost: " + target + "\r\n\r\n");
    f.request = std::move(request);
    f.tunnel = true;
  } else {
    start_tls(f, u.host, request);
  }
  f.state = HttpState::connecting;
  f.last_byte_s = now_s;
  return true;
}

// The URL a redirect's `location` names, read against `from`.
std::string resolve_location(const std::string& from, const std::string& location) {
  if (starts_with_words(location, "http://") || starts_with_words(location, "https://"))
    return location;
  HttpUrl u;
  if (!read_http_url(from, u)) return location;
  std::string base = std::string(u.https ? "https://" : "http://") + u.host;
  if (u.port != (u.https ? 443 : 80)) base += ":" + std::to_string(u.port);
  if (!location.empty() && location.front() == '/') return base + location;
  std::string dir = u.path.substr(0, u.path.find('?'));
  dir = dir.substr(0, dir.rfind('/') + 1);
  return base + dir + location;
}

}  // namespace

bool read_http_url(std::string_view text, HttpUrl& out) {
  out = {};
  text = trim(text);
  if (starts_with_words(text, "http://")) {
    text.remove_prefix(7);
  } else if (starts_with_words(text, "https://")) {
    text.remove_prefix(8);
    out.https = true;
    out.port = 443;
  } else {
    return false;
  }
  const size_t slash = text.find_first_of("/?#");
  std::string_view host = text.substr(0, slash);
  if (slash != std::string_view::npos) {
    std::string path(text.substr(slash));
    const size_t hash = path.find('#');
    if (hash != std::string::npos) path.resize(hash);
    if (path.empty() || path.front() != '/') path.insert(0, "/");
    out.path = path;
  }
  const size_t at = host.rfind('@');
  if (at != std::string_view::npos) host.remove_prefix(at + 1);
  const size_t colon = host.rfind(':');
  if (colon != std::string_view::npos) {
    const std::string_view digits = host.substr(colon + 1);
    uint32_t port = 0;
    if (digits.empty()) return false;
    for (const char c : digits) {
      if (c < '0' || c > '9') return false;
      port = port * 10 + static_cast<uint32_t>(c - '0');
      if (port > 65535) return false;
    }
    if (port == 0) return false;
    out.port = static_cast<uint16_t>(port);
    host = host.substr(0, colon);
  }
  if (host.empty()) return false;
  out.host = std::string(host);
  return true;
}

std::string url_escape(std::string_view text) {
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += ch;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

std::string http_status_text(int code) {
  const char* words = nullptr;
  switch (code) {
    case 200:
      words = "OK";
      break;
    case 204:
      words = "No Content";
      break;
    case 206:
      words = "Partial Content";
      break;
    case 301:
      words = "Moved Permanently";
      break;
    case 302:
      words = "Found";
      break;
    case 303:
      words = "See Other";
      break;
    case 304:
      words = "Not Modified";
      break;
    case 307:
      words = "Temporary Redirect";
      break;
    case 308:
      words = "Permanent Redirect";
      break;
    case 400:
      words = "Bad Request";
      break;
    case 401:
      words = "Unauthorized";
      break;
    case 403:
      words = "Forbidden";
      break;
    case 404:
      words = "Not Found";
      break;
    case 408:
      words = "Request Timeout";
      break;
    case 410:
      words = "Gone";
      break;
    case 429:
      words = "Too Many Requests";
      break;
    case 500:
      words = "Internal Server Error";
      break;
    case 501:
      words = "Not Implemented";
      break;
    case 502:
      words = "Bad Gateway";
      break;
    case 503:
      words = "Service Unavailable";
      break;
    case 504:
      words = "Gateway Timeout";
      break;
    default:
      if (code >= 500)
        words = "Server Error";
      else if (code >= 400)
        words = "Client Error";
      else if (code >= 300)
        words = "Redirection";
      else if (code >= 200)
        words = "Success";
      else
        words = "Informational";
  }
  return std::to_string(code) + " " + words;
}

void http_start(HttpFetch& f, const std::string& url, const HttpOptions& options, double now_s) {
  http_cancel(f);
  f.options = options;
  f.url = url;
  f.error.clear();
  f.redirected = 0;
  open(f, now_s);
}

bool http_step(HttpFetch& f, double now_s) {
  if (f.state == HttpState::done || f.state == HttpState::failed) return true;
  if (f.state == HttpState::idle) return false;
  tcp_pump(f.stream);
  if (f.state == HttpState::connecting) {
    if (f.stream.connecting) {
      if (now_s - f.last_byte_s > f.options.stall_s) fail(f, "Timeout was reached");
      return f.state == HttpState::failed;
    }
    if (f.stream.closed && f.stream.received == 0) {
      fail(f, "Couldn't connect to server");
      return true;
    }
    f.state = HttpState::waiting;
  }
  bool closed = f.stream.closed;
  if (f.tunnel || f.tls) {
    std::vector<uint8_t> wire;
    if (tcp_take_bytes(f.stream, wire) > 0) f.last_byte_s = now_s;
    if (f.tunnel) {
      f.raw.insert(f.raw.end(), wire.begin(), wire.end());
      wire.clear();
      const int t = read_tunnel(f);
      if (t < 0 || (t == 0 && closed)) {
        fail(f, "Couldn't connect to server");
        return true;
      }
      if (t == 0) {
        if (now_s - f.last_byte_s > f.options.stall_s) fail(f, "Timeout was reached");
        return f.state == HttpState::failed;
      }
      f.tunnel = false;
      wire.swap(f.raw);  // a server's first bytes that came with the proxy's answer
      HttpUrl u;
      read_http_url(f.url, u);
      start_tls(f, u.host, f.request);
      f.request.clear();
    }
    tls_put_wire(*f.tls, wire.data(), wire.size());
    const TlsState st = tls_step(*f.tls, f.raw);
    wire.clear();
    tls_take_wire(*f.tls, wire);
    if (!wire.empty()) {
      tcp_send_bytes(f.stream, wire.data(), wire.size());
      tcp_pump(f.stream);
    }
    if (st == TlsState::failed) {
      fail(f, tls_error(*f.tls).c_str());
      return true;
    }
    closed = closed || st == TlsState::closed;
    if (closed && st == TlsState::handshaking) {
      fail(f, "SSL connect error");
      return true;
    }
  } else if (tcp_take_bytes(f.stream, f.raw) > 0) {
    f.last_byte_s = now_s;
  }
  Head h;
  bool bad = false;
  if (!read_head(f.raw, h, bad)) {
    if (f.raw.size() > 65536) bad = true;
    if (bad || closed) {
      fail(f, "Transferred a partial file");
      return true;
    }
    if (now_s - f.last_byte_s > f.options.stall_s) fail(f, "Timeout was reached");
    return f.state == HttpState::failed;
  }
  if (bad) {
    fail(f, "Transferred a partial file");
    return true;
  }
  f.status = h.status;
  f.total = h.length;
  const bool redirect = (h.status == 301 || h.status == 302 || h.status == 303 || h.status == 307 ||
                         h.status == 308) &&
                        !h.location.empty() && f.options.redirects > 0;
  if (redirect) {
    if (f.redirected >= f.options.redirects) {
      fail(f, "Number of redirects hit maximum amount");
      return true;
    }
    ++f.redirected;
    f.url = resolve_location(f.url, h.location);
    open(f, now_s);
    return f.state == HttpState::failed;
  }
  if (h.length >= 0 && static_cast<uint64_t>(h.length) > f.options.most_bytes) {
    fail(f, "Maximum file size exceeded");
    return true;
  }
  const size_t have = f.raw.size() - h.end;
  bool whole = false;
  if (h.status == 204 || h.status == 304 || (h.status >= 100 && h.status < 200)) {
    whole = true;
    f.body.clear();
  } else if (h.chunked) {
    const int r = read_chunks(f.raw, h.end, f.body, f.options.most_bytes);
    if (r < 0) {
      fail(f, f.body.size() > f.options.most_bytes ? "Maximum file size exceeded"
                                                   : "Transferred a partial file");
      return true;
    }
    whole = r == 1;
  } else if (h.length >= 0) {
    whole = have >= static_cast<size_t>(h.length);
    if (whole)
      f.body.assign(f.raw.begin() + static_cast<ptrdiff_t>(h.end),
                    f.raw.begin() + static_cast<ptrdiff_t>(h.end + h.length));
  } else {
    if (have > f.options.most_bytes) {
      fail(f, "Maximum file size exceeded");
      return true;
    }
    whole = closed;
    if (whole) f.body.assign(f.raw.begin() + static_cast<ptrdiff_t>(h.end), f.raw.end());
  }
  if (whole) {
    tcp_close(f.stream);
    f.raw.clear();
    f.raw.shrink_to_fit();
    f.state = HttpState::done;
    return true;
  }
  if (closed) {
    fail(f, "Transferred a partial file");
    return true;
  }
  if (now_s - f.last_byte_s > f.options.stall_s) fail(f, "Timeout was reached");
  return f.state == HttpState::failed;
}

void http_cancel(HttpFetch& f) {
  tcp_close(f.stream);
  f.stream = {};
  f.tls.reset();
  f.tunnel = false;
  f.request.clear();
  f.raw.clear();
  f.body.clear();
  f.status = 0;
  f.total = -1;
  f.state = HttpState::idle;
}

size_t http_received(const HttpFetch& f) {
  if (f.state == HttpState::done) return f.body.size();
  const std::string_view text(reinterpret_cast<const char*>(f.raw.data()), f.raw.size());
  const size_t end = text.find("\r\n\r\n");
  return end == std::string_view::npos ? 0 : f.raw.size() - end - 4;
}

}  // namespace ge::net
