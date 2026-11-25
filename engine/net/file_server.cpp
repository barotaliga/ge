// A game server's own web files (file_server.hpp).
//
// ge:summary The web file server: requests, ranges, digests, TLS, the fair speed caps
// ge:keywords file-server http-server range repr-digest fair-share rate-cap
// ge:skill net
// ge:test tests/net_file_server_test.cpp
#include "net/file_server.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "net/http_fetch.hpp"

namespace ge::net {

namespace {

constexpr size_t kMostHead = 8192;
constexpr double kHeadTimeoutS = 10;
constexpr size_t kMostPath = 255;
// The bytes a connection may hold in its stream's backlog: room left under kStreamBacklog for
// TLS's records around them.
constexpr size_t kQueued = size_t{192} * 1024;

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

bool number(std::string_view v, int64_t& out) {
  out = 0;
  if (v.empty() || v.size() > 15) return false;
  for (const char c : v) {
    if (c < '0' || c > '9') return false;
    out = out * 10 + (c - '0');
  }
  return true;
}

int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// `bytes=A-` or `bytes=A-B` (one range); anything else leaves the request whole.
void read_range(std::string_view v, FileRequest& r) {
  if (v.size() < 6 || !same_words(v.substr(0, 6), "bytes=")) return;
  v.remove_prefix(6);
  if (v.find(',') != std::string_view::npos) return;
  const size_t dash = v.find('-');
  if (dash == std::string_view::npos) return;
  int64_t a = 0, b = -1;
  if (!number(trim(v.substr(0, dash)), a)) return;  // a suffix range (`-N`) is not taken
  const std::string_view tail = trim(v.substr(dash + 1));
  if (!tail.empty() && (!number(tail, b) || b < a)) return;
  r.from = a;
  r.to = tail.empty() ? -1 : b;
}

std::string head_text(int code, const char* words, const std::string& server) {
  return "HTTP/1.1 " + std::to_string(code) + " " + words + "\r\nServer: " + server +
         "\r\nConnection: close\r\n";
}

std::string error_text(int code, const char* words, const std::string& server,
                       const std::string& extra = {}) {
  const std::string body = std::to_string(code) + " " + words + "\n";
  return head_text(code, words, server) + extra +
         "Content-Type: text/plain\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
         body;
}

// Plain bytes to the client, through TLS when the connection has it.
void put(FileConn& c, const uint8_t* data, size_t size) {
  if (!c.tls) {
    tcp_send_bytes(c.stream, data, size);
    return;
  }
  tls_write(*c.tls, data, size);
  std::vector<uint8_t> ignored;
  tls_step(*c.tls, ignored);
  std::vector<uint8_t> wire;
  tls_take_wire(*c.tls, wire);
  if (!wire.empty()) tcp_send_bytes(c.stream, wire.data(), wire.size());
}

void put(FileConn& c, const std::string& text) {
  put(c, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

size_t backlog(const FileConn& c) { return c.stream.out.size() - c.stream.out_head; }

Address host_of(Address a) {
  a.port = 0;
  return a;
}

// The request answered: the head (and for a GET the body queued for the caps to send).
void answer(FileServer& s, FileConn& c, const FileRequest& r) {
  const std::string& name = s.options.name;
  if (r.method != "GET" && r.method != "HEAD") {
    s.stats.bad++;
    put(c, error_text(405, "Method Not Allowed", name, "Allow: GET, HEAD\r\n"));
    return;
  }
  if (!file_path_safe(r.path)) {
    s.stats.refused++;
    put(c, error_text(403, "Forbidden", name));
    return;
  }
  auto it = s.cache.find(r.path);
  if (it == s.cache.end()) {
    std::vector<uint8_t> bytes;
    if (!s.lookup || !s.lookup(r.path, bytes)) {
      s.stats.not_found++;
      put(c, error_text(404, "Not Found", name));
      return;
    }
    FileServer::Cached k;
    uint8_t sum[32];
    sha256(bytes.data(), bytes.size(), sum);
    k.digest = base64_encode(sum, 32);
    k.bytes = std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
    it = s.cache.emplace(r.path, std::move(k)).first;
  }
  const FileServer::Cached& k = it->second;
  const auto size = static_cast<int64_t>(k.bytes->size());
  int64_t from = 0, to = size - 1;
  const bool range = r.from >= 0;
  if (range) {
    if (r.from >= size) {
      s.stats.bad++;
      put(c, error_text(416, "Range Not Satisfiable", name,
                        "Content-Range: bytes */" + std::to_string(size) + "\r\n"));
      return;
    }
    from = r.from;
    if (r.to >= 0) to = std::min(r.to, size - 1);
  }
  std::string h = range ? head_text(206, "Partial Content", name) : head_text(200, "OK", name);
  h += "Content-Type: application/octet-stream\r\nAccept-Ranges: bytes\r\n";
  h += "Content-Length: " + std::to_string(to - from + 1) + "\r\n";
  if (range)
    h += "Content-Range: bytes " + std::to_string(from) + "-" + std::to_string(to) + "/" +
         std::to_string(size) + "\r\n";
  h += "ETag: \"" + k.digest.substr(0, 22) + "\"\r\n";
  h += "Repr-Digest: sha-256=:" + k.digest + ":\r\n";
  h += "Cache-Control: no-cache\r\n\r\n";
  put(c, h);
  if (range)
    s.stats.partial++;
  else
    s.stats.served++;
  if (r.method == "HEAD" || to < from) return;
  c.file = k.bytes;
  c.at = static_cast<size_t>(from);
  c.end = static_cast<size_t>(to + 1);
}

// The request's bytes in: through TLS when the connection has it.
void take(FileConn& c, double now_s) {
  std::vector<uint8_t> bytes;
  if (tcp_take_bytes(c.stream, bytes) > 0) c.moved_s = now_s;
  if (!c.tls) {
    if (!c.answered) c.in.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return;
  }
  tls_put_wire(*c.tls, bytes.data(), bytes.size());
  std::vector<uint8_t> plain;
  const TlsState st = tls_step(*c.tls, plain);
  std::vector<uint8_t> wire;
  tls_take_wire(*c.tls, wire);
  if (!wire.empty()) tcp_send_bytes(c.stream, wire.data(), wire.size());
  if (st == TlsState::failed || st == TlsState::closed) c.stream.closed = true;
  if (!c.answered) c.in.append(reinterpret_cast<const char*>(plain.data()), plain.size());
}

}  // namespace

FileParse file_request_parse(std::string_view data, FileRequest& out) {
  out = {};
  const size_t end = data.find("\r\n\r\n");
  if (end == std::string_view::npos)
    return data.size() > kMostHead ? FileParse::too_big : FileParse::more;
  if (end + 4 > kMostHead) return FileParse::too_big;
  out.head_size = end + 4;
  std::string_view head = data.substr(0, end);
  size_t nl = head.find("\r\n");
  const std::string_view first = head.substr(0, nl);
  const size_t a = first.find(' ');
  const size_t b = a == std::string_view::npos ? a : first.find(' ', a + 1);
  if (b == std::string_view::npos || first.substr(b + 1, 5) != "HTTP/") return FileParse::bad;
  out.method = std::string(first.substr(0, a));
  std::string_view target = first.substr(a + 1, b - a - 1);
  // An absolute target (`http://host/path`) read as its path.
  if (const size_t scheme = target.find("://"); scheme != std::string_view::npos) {
    const size_t slash = target.find('/', scheme + 3);
    target = slash == std::string_view::npos ? std::string_view("/") : target.substr(slash);
  }
  if (target.empty() || target.front() != '/') return FileParse::bad;
  target = target.substr(0, target.find_first_of("?#"));
  target.remove_prefix(1);
  for (size_t i = 0; i < target.size(); ++i) {
    if (target[i] != '%') {
      out.path += target[i];
      continue;
    }
    if (i + 2 >= target.size() || hex(target[i + 1]) < 0 || hex(target[i + 2]) < 0)
      return FileParse::bad;
    out.path += static_cast<char>(hex(target[i + 1]) * 16 + hex(target[i + 2]));
    i += 2;
  }
  while (nl != std::string_view::npos) {
    head.remove_prefix(nl + 2);
    nl = head.find("\r\n");
    const std::string_view line = head.substr(0, nl);
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos) continue;
    if (same_words(trim(line.substr(0, colon)), "range"))
      read_range(trim(line.substr(colon + 1)), out);
  }
  return FileParse::ok;
}

bool file_path_safe(std::string_view path) {
  if (path.empty() || path.size() > kMostPath) return false;
  size_t at = 0;
  while (at <= path.size()) {
    size_t end = path.find('/', at);
    if (end == std::string_view::npos) end = path.size();
    const std::string_view seg = path.substr(at, end - at);
    if (seg.empty() || seg == "." || seg == "..") return false;
    for (const char ch : seg) {
      const auto c = static_cast<unsigned char>(ch);
      if (c < 0x20 || c == 0x7f || c == '\\' || c == ':') return false;
    }
    at = end + 1;
  }
  return true;
}

std::vector<int64_t> fair_shares(int64_t budget, const std::vector<int64_t>& wants) {
  std::vector<int64_t> got(wants.size(), 0);
  std::vector<size_t> open;
  for (size_t i = 0; i < wants.size(); ++i)
    if (wants[i] > 0) open.push_back(i);
  while (budget > 0 && !open.empty()) {
    const int64_t part = std::max<int64_t>(budget / static_cast<int64_t>(open.size()), 1);
    std::vector<size_t> still;
    for (const size_t i : open) {
      const int64_t give = std::min({part, wants[i] - got[i], budget});
      got[i] += give;
      budget -= give;
      if (got[i] < wants[i]) still.push_back(i);
      if (budget == 0) break;
    }
    open.swap(still);
  }
  return got;
}

bool file_server_open(FileServer& s, const Address& bind, const FileServeOptions& options,
                      FileLookup lookup, std::string& error) {
  file_server_close(s);
  s.options = options;
  s.lookup = std::move(lookup);
  if (options.cert_pem.empty() != options.key_pem.empty()) {
    error =
        "the web file server has a certificate without its key or a key without its "
        "certificate; next: set both (sv_http_cert and sv_http_key) or neither for plain HTTP";
    return false;
  }
  if (!options.cert_pem.empty()) {
    std::string why;
    if (tls_server(options.cert_pem, options.key_pem, &why) == nullptr) {
      error = "the web file server's TLS did not start: " + why +
              "; next: point sv_http_cert at the PEM certificate chain and sv_http_key at its "
              "private key";
      return false;
    }
  }
  std::string why;
  if (!tcp_listen(s.listener, bind, &why)) {
    error = "the web file server cannot listen on " + to_string(bind) + ": " + why +
            "; next: set sv_http_port to a free port, or stop what holds this one";
    return false;
  }
  return true;
}

void file_server_forget(FileServer& s) { s.cache.clear(); }

void file_server_close(FileServer& s) {
  for (FileConn& c : s.conns) tcp_close(c.stream);
  s.conns.clear();
  s.people.clear();
  s.cache.clear();
  tcp_close_listener(s.listener);
  s.tokens = 0;
  s.last_s = -1;
}

void file_server_step(FileServer& s, double now_s) {
  if (!file_server_listening(s)) return;
  const FileServeOptions& o = s.options;
  const double dt = s.last_s < 0 ? 0 : std::max(now_s - s.last_s, 0.0);
  s.last_s = now_s;
  const auto burst = [](int64_t rate) { return std::max(static_cast<double>(rate) / 16, 16384.0); };
  if (o.rate > 0) s.tokens = std::min(s.tokens + static_cast<double>(o.rate) * dt, burst(o.rate));
  if (o.rate_person > 0)
    for (FilePerson& p : s.people)
      p.tokens = std::min(p.tokens + static_cast<double>(o.rate_person) * dt, burst(o.rate_person));

  // The new connections, each counted against its address.
  for (;;) {
    FileConn c;
    if (!tcp_accept(s.listener, c.stream)) break;
    s.stats.accepted++;
    const Address who = host_of(c.stream.peer);
    size_t p = 0;
    while (p < s.people.size() && s.people[p].address != who) ++p;
    const int from_them = p < s.people.size() ? s.people[p].conns : 0;
    const bool full = static_cast<int>(s.conns.size()) >= o.max_connections;
    if (full || from_them >= o.max_per_address) {
      s.stats.refused++;
      if (o.cert_pem.empty()) {  // a TLS client hears the refusal as the connection closing
        const std::string t = full ? error_text(503, "Service Unavailable", o.name)
                                   : error_text(429, "Too Many Requests", o.name);
        put(c, t);
        tcp_pump(c.stream);
      }
      tcp_close(c.stream);
      continue;
    }
    if (!o.cert_pem.empty()) {
      c.tls = tls_server(o.cert_pem, o.key_pem, nullptr);
      if (!c.tls) {
        tcp_close(c.stream);
        continue;
      }
    }
    if (p == s.people.size()) {
      FilePerson fp;
      fp.address = who;
      fp.tokens = o.rate_person > 0 ? burst(o.rate_person) : 0;
      s.people.push_back(fp);
    }
    s.people[p].conns++;
    c.person = p;
    c.opened_s = c.moved_s = now_s;
    s.conns.push_back(std::move(c));
  }

  // Each connection's request read and answered.
  for (FileConn& c : s.conns) {
    tcp_pump(c.stream);
    take(c, now_s);
    if (c.answered) continue;
    FileRequest r;
    const FileParse got = file_request_parse(c.in, r);
    if (got == FileParse::more) {
      if (now_s - c.opened_s > kHeadTimeoutS) c.stream.closed = true;
      continue;
    }
    c.answered = true;
    if (got == FileParse::ok) {
      answer(s, c, r);
    } else {
      s.stats.bad++;
      put(c, got == FileParse::too_big ? error_text(431, "Request Header Fields Too Large", o.name)
                                       : error_text(400, "Bad Request", o.name));
    }
    c.in.clear();
  }

  // The bodies sent within the caps: each sending connection's room, a person's want the sum of
  // theirs (held to their tokens), the total shared fairly among people, a person's part fairly
  // among their connections.
  std::vector<int64_t> room(s.conns.size(), 0), want(s.people.size(), 0);
  for (size_t i = 0; i < s.conns.size(); ++i) {
    const FileConn& c = s.conns[i];
    if (!c.file || c.stream.closed || c.stream.connecting || backlog(c) >= kQueued) continue;
    room[i] = static_cast<int64_t>(std::min(c.end - c.at, kQueued - backlog(c)));
    want[c.person] += room[i];
  }
  if (o.rate_person > 0)
    for (size_t p = 0; p < s.people.size(); ++p)
      want[p] = std::min(want[p], static_cast<int64_t>(s.people[p].tokens));
  int64_t all = 0;
  for (const int64_t w : want) all += w;
  const std::vector<int64_t> part =
      fair_shares(o.rate > 0 ? std::min(all, static_cast<int64_t>(s.tokens)) : all, want);
  for (size_t p = 0; p < s.people.size(); ++p) {
    if (part[p] <= 0) continue;
    std::vector<size_t> theirs;
    std::vector<int64_t> rooms;
    for (size_t i = 0; i < s.conns.size(); ++i)
      if (s.conns[i].person == p && room[i] > 0) {
        theirs.push_back(i);
        rooms.push_back(room[i]);
      }
    const std::vector<int64_t> each = fair_shares(part[p], rooms);
    for (size_t k = 0; k < theirs.size(); ++k) {
      FileConn& c = s.conns[theirs[k]];
      const auto n = static_cast<size_t>(each[k]);
      if (n == 0) continue;
      put(c, c.file->data() + c.at, n);
      c.at += n;
      c.moved_s = now_s;
      s.stats.bytes += n;
      s.people[p].sent += n;
      if (o.rate > 0) s.tokens -= static_cast<double>(n);
      if (o.rate_person > 0) s.people[p].tokens -= static_cast<double>(n);
      if (c.at >= c.end) c.file.reset();
    }
  }

  // Closed when their answer has gone, or when nothing has moved for too long.
  for (FileConn& c : s.conns) {
    tcp_pump(c.stream);
    const bool sent = c.answered && !c.file && backlog(c) == 0;
    if (c.tls && sent) {
      std::vector<uint8_t> plain;
      tls_step(*c.tls, plain);
    }
    if (sent || now_s - c.moved_s > o.idle_s) {
      if (c.stream.out_head < c.stream.out.size()) continue;  // the last bytes still going
      c.stream.closed = true;
    }
  }
  for (size_t i = s.conns.size(); i-- > 0;) {
    FileConn& c = s.conns[i];
    if (!c.stream.closed) continue;
    tcp_close(c.stream);
    s.people[c.person].conns--;
    s.conns.erase(s.conns.begin() + static_cast<ptrdiff_t>(i));
  }
  // An address with no connection left is forgotten; the connections' indexes follow.
  for (size_t p = s.people.size(); p-- > 0;) {
    if (s.people[p].conns > 0) continue;
    s.people.erase(s.people.begin() + static_cast<ptrdiff_t>(p));
    for (FileConn& c : s.conns)
      if (c.person > p) c.person--;
  }
}

}  // namespace ge::net
