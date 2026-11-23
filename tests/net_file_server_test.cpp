// engine/net/file_server.hpp: the request parser, the path check and the fair shares as pure
// functions; then the server on loopback fetched by net/http_fetch.hpp: a file whole with its
// digest checked, a resume by range, a 404, a path that climbs refused before the lookup sees it,
// a method refused, the same over TLS on a certificate made on the spot, the per-player speed cap
// and the client's own, timed on the test's clock. A machine with no loopback socket (a sandbox)
// skips the served cases.
//
// ge:summary Test: the web file server: parsing, paths, fair shares, ranges, digests, TLS, caps
// ge:keywords test net file-server http-server range resume repr-digest fair-share rate-cap tls
// ge:skill net testing
#include "net/file_server.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "net/http_fetch.hpp"

using namespace ge::net;

namespace {

std::vector<uint8_t> pattern(size_t n) {
  std::vector<uint8_t> b(n);
  uint32_t x = 12345;
  for (uint8_t& v : b) {
    x = x * 1103515245u + 12345u;
    v = static_cast<uint8_t>(x >> 24);
  }
  return b;
}

struct Served {
  FileServer server;
  std::vector<std::string> asked;
  std::vector<uint8_t> map = pattern(300000);
};

bool open(Served& s, const FileServeOptions& o = {}) {
  std::string err;
  const bool ok = file_server_open(
      s.server, loopback_v4(0), o,
      [&s](const std::string& path, std::vector<uint8_t>& out) {
        s.asked.push_back(path);
        if (path != "qw/maps/dm3.bsp") return false;
        out = s.map;
        return true;
      },
      err);
  if (!ok) ge::test::skip("no loopback TCP socket here: " + err);
  return ok;
}

std::string url(const Served& s, const std::string& path, bool https = false) {
  return std::string(https ? "https://localhost:" : "http://127.0.0.1:") +
         std::to_string(s.server.listener.local.port) + path;
}

// Steps the server and the fetches until every fetch is over; the clock moves 1 ms a step.
bool run(Served& s, const std::vector<HttpFetch*>& fs, double& now, int spins = 400000) {
  for (int i = 0; i < spins; ++i) {
    file_server_step(s.server, now);
    now += 0.001;
    bool over = true;
    for (HttpFetch* f : fs) over = http_step(*f, now) && over;
    if (over) return true;
  }
  return false;
}

// A raw request's whole answer.
std::string raw(Served& s, const std::string& request, double& now) {
  TcpStream c;
  std::string err;
  tcp_connect(c, s.server.listener.local, &err);
  bool sent = false;
  std::string got;
  for (int i = 0; i < 200000 && !c.closed; ++i) {
    tcp_pump(c);
    if (!sent && !c.connecting) {
      tcp_send_bytes(c, reinterpret_cast<const uint8_t*>(request.data()), request.size());
      sent = true;
    }
    file_server_step(s.server, now);
    now += 0.001;
    std::vector<uint8_t> b;
    tcp_take_bytes(c, b);
    got.append(b.begin(), b.end());
  }
  tcp_close(c);
  return got;
}

}  // namespace

TEST(file_request_parse_reads_the_path_and_the_range) {
  FileRequest r;
  CHECK(file_request_parse("GET /qw/maps/dm3.bsp HTTP/1.1\r\nHost: x\r\n", r) == FileParse::more);
  CHECK(file_request_parse("GET /qw/maps/dm%33.bsp?x=1 HTTP/1.1\r\nRange: bytes=100-\r\n\r\n", r) ==
        FileParse::ok);
  CHECK(r.method == "GET" && r.path == "qw/maps/dm3.bsp" && r.from == 100 && r.to == -1);
  CHECK(file_request_parse("HEAD /a/b HTTP/1.0\r\nrange: bytes=5-9\r\n\r\n", r) == FileParse::ok);
  CHECK(r.method == "HEAD" && r.from == 5 && r.to == 9);
  // A suffix range or several ranges: the whole file.
  CHECK(file_request_parse("GET /a/b HTTP/1.1\r\nRange: bytes=-5\r\n\r\n", r) == FileParse::ok);
  CHECK(r.from == -1);
  CHECK(file_request_parse("GET /a/b HTTP/1.1\r\nRange: bytes=0-1,4-5\r\n\r\n", r) ==
        FileParse::ok);
  CHECK(r.from == -1);
  CHECK(file_request_parse("GET http://host:8/qw/x.bsp HTTP/1.1\r\n\r\n", r) == FileParse::ok);
  CHECK(r.path == "qw/x.bsp");
  CHECK(file_request_parse("GET qw HTTP/1.1\r\n\r\n", r) == FileParse::bad);
  CHECK(file_request_parse("GET /%zz HTTP/1.1\r\n\r\n", r) == FileParse::bad);
  CHECK(file_request_parse("nonsense\r\n\r\n", r) == FileParse::bad);
  CHECK(file_request_parse("GET /" + std::string(9000, 'a'), r) == FileParse::too_big);
}

TEST(file_path_safe_refuses_climbing_and_odd_bytes) {
  CHECK(file_path_safe("qw/maps/dm3.bsp"));
  CHECK(file_path_safe("id1/sound/a b.wav"));
  for (const char* bad : {"", "../x", "qw/../../etc/passwd", "qw/./maps/x", "qw//x", "/qw/x",
                          "qw/x/", "qw\\x", "c:/x", "qw/\x01"})
    CHECK_MSG(!file_path_safe(bad), bad);
  CHECK(!file_path_safe(std::string(256, 'a')));
  CHECK(!file_path_safe(std::string("qw/a\0b", 6)));
}

TEST(fair_shares_split_evenly_and_pass_on_what_one_does_not_need) {
  CHECK((fair_shares(300, {100, 100, 100}) == std::vector<int64_t>{100, 100, 100}));
  CHECK((fair_shares(300, {1000, 1000, 1000}) == std::vector<int64_t>{100, 100, 100}));
  CHECK((fair_shares(300, {50, 1000, 1000}) == std::vector<int64_t>{50, 125, 125}));
  CHECK((fair_shares(300, {0, 1000}) == std::vector<int64_t>{0, 300}));
  CHECK((fair_shares(0, {10, 10}) == std::vector<int64_t>{0, 0}));
  const std::vector<int64_t> odd = fair_shares(10, {9, 9, 9});
  CHECK(odd[0] + odd[1] + odd[2] == 10);
  for (const int64_t v : odd) CHECK(v >= 3 && v <= 4);
  // Niclas's example: 20000 kB/s shared by five players who could each take 5000: 4000 each.
  CHECK((fair_shares(20000, {5000, 5000, 5000, 5000, 5000}) ==
         std::vector<int64_t>{4000, 4000, 4000, 4000, 4000}));
}

TEST(file_server_serves_a_file_whole_by_range_and_refuses_the_rest) {
  Served s;
  if (!open(s)) return;
  double now = 0;
  HttpFetch f;
  http_start(f, url(s, "/qw/maps/dm3.bsp"), {}, now);
  CHECK(run(s, {&f}, now));
  CHECK_MSG(f.state == HttpState::done, f.error);
  CHECK(f.status == 200 && f.body == s.map && f.offset == 0);
  CHECK(f.sha256.size() == 32 && http_digest_ok(f, s.map));
  std::vector<uint8_t> other = s.map;
  other[7] ^= 1;
  CHECK(!http_digest_ok(f, other));

  // A resume: the rest from byte 123456, the file's whole size known.
  HttpOptions o;
  o.from = 123456;
  http_start(f, url(s, "/qw/maps/dm3.bsp"), o, now);
  CHECK(run(s, {&f}, now));
  CHECK_MSG(f.state == HttpState::done, f.error);
  CHECK(f.status == 200 && f.offset == 123456 && f.total == 300000);
  CHECK(f.body.size() == 300000 - 123456);
  std::vector<uint8_t> joined(s.map.begin(), s.map.begin() + 123456);
  joined.insert(joined.end(), f.body.begin(), f.body.end());
  CHECK(joined == s.map && http_digest_ok(f, joined));
  o.from = 300000;  // past the end
  http_start(f, url(s, "/qw/maps/dm3.bsp"), o, now);
  CHECK(run(s, {&f}, now));
  CHECK(f.state == HttpState::failed && f.error == "Range not satisfiable");

  http_start(f, url(s, "/qw/maps/none.bsp"), {}, now);
  CHECK(run(s, {&f}, now));
  CHECK(f.state == HttpState::done && f.status == 404);
  const size_t asked = s.asked.size();
  CHECK(raw(s, "GET /qw/../../etc/passwd HTTP/1.1\r\n\r\n", now).rfind("HTTP/1.1 403", 0) == 0);
  CHECK(raw(s, "GET /qw/%2e%2e/x HTTP/1.1\r\n\r\n", now).rfind("HTTP/1.1 403", 0) == 0);
  CHECK(s.asked.size() == asked);  // the lookup never saw them
  CHECK(raw(s, "POST /qw/maps/dm3.bsp HTTP/1.1\r\n\r\n", now).rfind("HTTP/1.1 405", 0) == 0);
  const std::string head = raw(s, "HEAD /qw/maps/dm3.bsp HTTP/1.1\r\n\r\n", now);
  CHECK(head.rfind("HTTP/1.1 200", 0) == 0 && head.find("Content-Length: 300000") != head.npos);
  CHECK(head.size() < 1000);  // no body
  CHECK(s.server.stats.served >= 2 && s.server.stats.partial == 1);
  CHECK(s.server.conns.empty() && s.server.people.empty());
  file_server_close(s.server);
}

TEST(file_server_over_tls_with_the_operators_certificate) {
  Served s;
  FileServeOptions o;
  CHECK(tls_self_signed("localhost", o.cert_pem, o.key_pem));
  if (!open(s, o)) return;
  double now = 0;
  HttpFetch f;
  HttpOptions h;
  h.ca_pem = o.cert_pem;
  http_start(f, url(s, "/qw/maps/dm3.bsp", true), h, now);
  CHECK(run(s, {&f}, now));
  CHECK_MSG(f.state == HttpState::done, f.error);
  CHECK(f.body == s.map && http_digest_ok(f, f.body));
  HttpOptions untrusted;  // the system's roots: the self-made certificate is not trusted
  std::string other, other_key;
  CHECK(tls_self_signed("localhost", other, other_key));
  untrusted.ca_pem = other;
  http_start(f, url(s, "/qw/maps/dm3.bsp", true), untrusted, now);
  CHECK(run(s, {&f}, now));
  CHECK(f.state == HttpState::failed);
  // Pinned: the server's own certificate trusted by its digest, at its bare address (the name
  // is not the certificate's), and a certificate with another digest refused.
  HttpOptions pinned;
  pinned.pin.resize(32);
  CHECK(tls_cert_sha256(o.cert_pem, pinned.pin.data()));
  http_start(
      f, "https://127.0.0.1:" + std::to_string(s.server.listener.local.port) + "/qw/maps/dm3.bsp",
      pinned, now);
  CHECK(run(s, {&f}, now));
  CHECK_MSG(f.state == HttpState::done, f.error);
  CHECK(f.body == s.map);
  pinned.pin[0] ^= 1;
  http_start(f, url(s, "/qw/maps/dm3.bsp", true), pinned, now);
  CHECK(run(s, {&f}, now));
  CHECK(f.state == HttpState::failed &&
        f.error == "SSL peer certificate or SSH remote key was not OK");
  file_server_close(s.server);

  FileServer bad;
  FileServeOptions half;
  half.cert_pem = o.cert_pem;
  std::string err;
  CHECK(!file_server_open(bad, loopback_v4(0), half, nullptr, err));
  CHECK(err.find("next:") != std::string::npos);
  half.key_pem = "not a key";
  CHECK(!file_server_open(bad, loopback_v4(0), half, nullptr, err));
  CHECK(err.find("sv_http_key") != std::string::npos);
}

TEST(file_server_holds_a_player_to_the_cap_across_parallel_fetches) {
  Served s;
  FileServeOptions o;
  o.rate_person = 100000;  // 100 kB/s: two fetches of 300 kB from one address take ~6 s
  if (!open(s, o)) return;
  double now = 0;
  HttpFetch a, b;
  http_start(a, url(s, "/qw/maps/dm3.bsp"), {}, now);
  http_start(b, url(s, "/qw/maps/dm3.bsp"), {}, now);
  const double t0 = now;
  CHECK(run(s, {&a, &b}, now, 2000000));
  CHECK(a.state == HttpState::done && b.state == HttpState::done);
  CHECK(a.body == s.map && b.body == s.map);
  const double took = now - t0;
  CHECK_MSG(took > 5.5 && took < 6.5, std::to_string(took));
  file_server_close(s.server);
}

TEST(file_server_total_cap_and_the_clients_own_cap) {
  Served s;
  FileServeOptions o;
  o.rate = 200000;  // 200 kB/s for everyone
  if (!open(s, o)) return;
  double now = 0;
  HttpFetch a;
  const double t0 = now;
  http_start(a, url(s, "/qw/maps/dm3.bsp"), {}, now);
  CHECK(run(s, {&a}, now, 2000000));
  CHECK(a.state == HttpState::done && a.body == s.map);
  const double took = now - t0;
  CHECK_MSG(took > 1.3 && took < 1.7, std::to_string(took));  // 300 kB at 200 kB/s, a burst first

  HttpOptions slow;  // the client's own cap is lower than the server's
  slow.rate = 100000;
  const double t1 = now;
  http_start(a, url(s, "/qw/maps/dm3.bsp"), slow, now);
  CHECK(run(s, {&a}, now, 2000000));
  CHECK(a.state == HttpState::done && a.body == s.map);
  const double took2 = now - t1;
  // The socket's own buffers hold some bytes beyond the cap's reads, so the bound is loose.
  CHECK_MSG(took2 > 2.0 && took2 < 3.5, std::to_string(took2));
  file_server_close(s.server);
}
