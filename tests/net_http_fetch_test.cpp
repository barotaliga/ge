// engine/net/http_fetch.hpp: a GET against a loopback server that answers with canned replies
// (a body by length, by chunks and to the close; a redirect; a 404; too many redirects; a stall;
// a partial body), the URL read and escaped, the status words, and https refused at once. A
// machine with no loopback socket (a sandbox) skips the served cases.
//
// ge:summary Test: HTTP GET over loopback: lengths, chunks, redirects, 404, failures, URLs
// ge:keywords test net http http-get fetch chunked redirect url_escape loopback
// ge:skill net testing
#include "net/http_fetch.hpp"

#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::net;

namespace {

// A server on loopback: each connection it accepts gets the next canned reply, then is closed
// (or held open when `hold` is set).
struct Server {
  TcpListener listener;
  std::vector<std::string> replies;
  std::vector<std::string> requests;
  std::vector<TcpStream> streams;
  size_t next = 0;
  bool hold = false;
};

void serve(Server& s) {
  for (TcpStream in; tcp_accept(s.listener, in); in = {}) {  // accept closes the stream it fills
    s.streams.push_back(in);
    s.requests.emplace_back();
  }
  for (size_t i = 0; i < s.streams.size(); ++i) {
    TcpStream& t = s.streams[i];
    if (t.handle == -1) continue;
    tcp_pump(t);
    std::vector<uint8_t> got;
    tcp_take_bytes(t, got);
    s.requests[i].append(got.begin(), got.end());
    if (s.requests[i].find("\r\n\r\n") == std::string::npos) continue;
    if (s.next < s.replies.size() && t.out.empty()) {
      const std::string& r = s.replies[s.next++];
      tcp_send_bytes(t, reinterpret_cast<const uint8_t*>(r.data()), r.size());
      for (int k = 0; k < 100000 && !t.out.empty(); ++k) tcp_pump(t);
      if (!s.hold) tcp_close(t);
    }
  }
}

// Runs `f` against `s` until it is over (or `spins` steps went by); the clock moves 1 ms a step.
bool run(HttpFetch& f, Server& s, double& now, int spins = 200000) {
  for (int i = 0; i < spins; ++i) {
    serve(s);
    now += 0.001;
    if (http_step(f, now)) return true;
  }
  return false;
}

std::string url_of(const Server& s, const std::string& path) {
  return "http://127.0.0.1:" + std::to_string(s.listener.local.port) + path;
}

std::string text(const std::vector<uint8_t>& b) { return {b.begin(), b.end()}; }

bool open(Server& s) {
  std::string err;
  if (!tcp_listen(s.listener, loopback_v4(0), &err)) {
    ge::test::skip("no loopback TCP socket here: " + err);
    return false;
  }
  return true;
}

void close(Server& s) {
  for (TcpStream& t : s.streams) tcp_close(t);
  tcp_close_listener(s.listener);
}

}  // namespace

TEST(http_url_read_escape_and_status_words) {
  HttpUrl u;
  CHECK(read_http_url("http://example.org/q2/maps/q2dm1.bsp", u));
  CHECK(!u.https && u.host == "example.org" && u.port == 80 && u.path == "/q2/maps/q2dm1.bsp");
  CHECK(read_http_url("HTTP://host:8080", u) && u.port == 8080 && u.path == "/");
  CHECK(read_http_url("https://host/a?b=1#frag", u) && u.https && u.port == 443);
  CHECK(u.path == "/a?b=1");
  CHECK(!read_http_url("ftp://host/x", u));
  CHECK(!read_http_url("http://", u));
  CHECK(!read_http_url("http://host:99999/", u));
  CHECK(url_escape("baseq2/maps/my map+1.bsp") == "baseq2/maps/my%20map%2B1.bsp");
  CHECK(url_escape("a-b_c.d~e") == "a-b_c.d~e");
  CHECK(http_status_text(404) == "404 Not Found");
  CHECK(http_status_text(200) == "200 OK");
  CHECK(http_status_text(418) == "418 Client Error");
}

TEST(http_refuses_what_it_cannot_fetch_at_once) {
  HttpFetch f;
  http_start(f, "https://example.org/x", {}, 0);
  CHECK(f.state == HttpState::failed && f.error == "Unsupported protocol");
  CHECK(http_step(f, 0));
  http_start(f, "not a url", {}, 0);
  CHECK(f.state == HttpState::failed);
  CHECK(f.error == "URL using bad/illegal format or missing URL");
}

TEST(http_body_by_length_chunks_and_close) {
  Server s;
  if (!open(s)) return;
  s.replies = {
      "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-Other: y\r\n\r\nhello",
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nWiki\r\n6;x=1\r\npedia!\r\n"
      "0\r\n\r\n",
      "HTTP/1.0 200 OK\r\n\r\nto the close",
  };
  double now = 0;
  HttpFetch f;
  HttpOptions o;
  o.user_agent = "test-agent";
  o.referer = "quake2://127.0.0.1:27910";
  http_start(f, url_of(s, "/baseq2/maps/a.bsp"), o, now);
  CHECK(run(f, s, now));
  CHECK_MSG(f.state == HttpState::done, f.error);
  CHECK(f.status == 200 && text(f.body) == "hello" && f.total == 5);
  CHECK(http_received(f) == 5);
  const std::string& req = s.requests[0];
  CHECK(req.rfind("GET /baseq2/maps/a.bsp HTTP/1.1\r\n", 0) == 0);
  CHECK(req.find("\r\nUser-Agent: test-agent\r\n") != std::string::npos);
  CHECK(req.find("\r\nReferer: quake2://127.0.0.1:27910\r\n") != std::string::npos);
  CHECK(req.find("\r\nHost: 127.0.0.1:") != std::string::npos);

  http_start(f, url_of(s, "/b"), {}, now);
  CHECK(run(f, s, now));
  CHECK_MSG(f.state == HttpState::done, f.error);
  CHECK(text(f.body) == "Wikipedia!");

  http_start(f, url_of(s, "/c"), {}, now);
  CHECK(run(f, s, now));
  CHECK_MSG(f.state == HttpState::done, f.error);
  CHECK(text(f.body) == "to the close" && f.total == -1);
  close(s);
}

TEST(http_redirects_404_and_failures) {
  Server s;
  if (!open(s)) return;
  s.replies = {
      "HTTP/1.1 302 Found\r\nLocation: /real/file.pcx\r\nContent-Length: 0\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc",
      "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nnot found",
      "HTTP/1.1 301 Moved\r\nLocation: again\r\n\r\n",
      "HTTP/1.1 301 Moved\r\nLocation: again\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort",
      "HTTP/1.1 200 OK\r\nContent-Length: 4000\r\n\r\n",
  };
  double now = 0;
  HttpFetch f;
  http_start(f, url_of(s, "/old/file.pcx"), {}, now);
  CHECK(run(f, s, now));
  CHECK_MSG(f.state == HttpState::done, f.error);
  CHECK(text(f.body) == "abc" && f.redirected == 1);
  CHECK(f.url == url_of(s, "/real/file.pcx"));
  CHECK(s.requests[1].rfind("GET /real/file.pcx ", 0) == 0);

  http_start(f, url_of(s, "/gone"), {}, now);
  CHECK(run(f, s, now));
  CHECK(f.state == HttpState::done && f.status == 404);
  CHECK(http_status_text(f.status) == "404 Not Found");

  HttpOptions one;
  one.redirects = 1;
  http_start(f, url_of(s, "/dir/loop"), one, now);
  CHECK(run(f, s, now));
  CHECK(f.state == HttpState::failed && f.error == "Number of redirects hit maximum amount");
  CHECK(s.requests[4].rfind("GET /dir/again ", 0) == 0);

  http_start(f, url_of(s, "/short"), {}, now);
  CHECK(run(f, s, now));
  CHECK(f.state == HttpState::failed && f.error == "Transferred a partial file");

  HttpOptions small;
  small.most_bytes = 1000;
  http_start(f, url_of(s, "/big"), small, now);
  CHECK(run(f, s, now));
  CHECK(f.state == HttpState::failed && f.error == "Maximum file size exceeded");
  close(s);
}

TEST(http_stall_times_out) {
  Server s;
  if (!open(s)) return;
  s.hold = true;
  s.replies = {"HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\npart"};
  double now = 0;
  HttpFetch f;
  HttpOptions o;
  o.stall_s = 0.5;
  http_start(f, url_of(s, "/slow"), o, now);
  CHECK(run(f, s, now, 2000));
  CHECK(f.state == HttpState::failed && f.error == "Timeout was reached");
  CHECK(http_received(f) == 4);
  close(s);
}

TEST(http_two_at_once) {
  Server s;
  if (!open(s)) return;
  s.replies = {"HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\na",
               "HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nb"};
  double now = 0;
  HttpFetch a, b;
  http_start(a, url_of(s, "/a"), {}, now);
  http_start(b, url_of(s, "/b"), {}, now);
  bool da = false, db = false;
  for (int i = 0; i < 200000 && !(da && db); ++i) {
    serve(s);
    now += 0.001;
    da = da || http_step(a, now);
    db = db || http_step(b, now);
  }
  CHECK_MSG(a.state == HttpState::done, a.error);
  CHECK_MSG(b.state == HttpState::done, b.error);
  close(s);
}
