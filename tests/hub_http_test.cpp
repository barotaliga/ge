// engine/hub/http.hpp and main.hpp: a request's head parsed (whole, partial, bad, too big), the
// response's headers (length, CORS, cache, HEAD without a body), events written line by line, the
// flags read, and a hub's HTTP server answering a GET, an event stream and a file larger than the
// stream's backlog on loopback (a sandbox with no loopback socket skips those).
//
// ge:summary Test: the hub's HTTP: parsing, responses, events, flags, a loopback GET and stream
// ge:keywords test hub http parse response sse event flags loopback
// ge:skill net testing
#include "hub/http.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "hub/main.hpp"

using namespace ge::net;
using namespace ge::hub;

TEST(http_parses_a_request_head) {
  HttpRequest r;
  CHECK(http_parse("GET /v1 HTTP/1.1\r\nHost: x\r\n", r) == HttpParse::more);
  CHECK(http_parse("GET /v1/servers?mode=ffa HTTP/1.1\r\nHost: x\r\n\r\nrest", r) == HttpParse::ok);
  CHECK(r.method == "GET" && r.target == "/v1/servers?mode=ffa" && r.version == "HTTP/1.1");
  CHECK(r.head_size == std::string("GET /v1/servers?mode=ffa HTTP/1.1\r\nHost: x\r\n\r\n").size());
  CHECK(http_parse("GARBAGE\r\n\r\n", r) == HttpParse::bad);
  CHECK(http_parse("GET v1 HTTP/1.1\r\n\r\n", r) == HttpParse::bad);
  CHECK(http_parse(std::string(kMaxRequest + 10, 'a'), r) == HttpParse::too_big);
}

TEST(http_writes_responses_and_events) {
  Answer a;
  a.body = "{\"a\":1}";
  a.max_age_s = 4;
  const std::string r = http_response(a, false);
  CHECK(r.rfind("HTTP/1.1 200 OK\r\n", 0) == 0);
  CHECK(r.find("Content-Length: 7\r\n") != std::string::npos);
  CHECK(r.find("Access-Control-Allow-Origin: *\r\n") != std::string::npos);
  CHECK(r.find("Cache-Control: public, max-age=4\r\n") != std::string::npos);
  CHECK(r.size() >= 7 && r.substr(r.size() - 7) == a.body);
  const std::string h = http_response(a, true);
  CHECK(h.find("Content-Length: 7\r\n") != std::string::npos &&
        h.substr(h.size() - 4) == "\r\n\r\n");
  CHECK(http_error(404, "x", "y").rfind("HTTP/1.1 404 Not Found", 0) == 0);
  CHECK(sse_event("status", "{}", 3) == "id: 3\nevent: status\ndata: {}\n\n");
  CHECK(sse_event("m", "a\nb", 1) == "id: 1\nevent: m\ndata: a\ndata: b\n\n");
}

TEST(hub_reads_its_flags) {
  HubOptions o;
  std::string err;
  CHECK(hub_options({"--master=1.2.3.4:27900,sim:1:2", "--http=8080", "--list-every=5"}, o, err));
  CHECK(o.masters == "1.2.3.4:27900,sim:1:2" && o.http == 8080 && o.list_s == 5);
  HubOptions none;
  CHECK(!hub_options({}, none, err) && err.find("--master") != std::string::npos);
  HubOptions bad;
  CHECK(!hub_options({"--master=x", "--http=99999"}, bad, err) &&
        err.find("--http") != std::string::npos);
  std::vector<Address> masters;
  CHECK(master_list("sim:1:27900, sim:2:27900", masters, err) && masters.size() == 2);
}

namespace {

std::string fetch_until(TcpStream& c, HttpServer& s, const Poller& p, const HubSpec& spec,
                        const std::string& want, int64_t& now) {
  std::string got;
  for (int i = 0; i < 400 && got.find(want) == std::string::npos; i++) {
    now += 10000;
    http_update(s, p, spec, now);
    tcp_pump(c);
    std::vector<uint8_t> bytes;
    tcp_take_bytes(c, bytes);
    got.append(bytes.begin(), bytes.end());
  }
  return got;
}

}  // namespace

TEST(http_serves_a_get_and_an_event_stream_on_loopback) {
  HttpServer s;
  std::string err;
  if (!http_open(s, loopback_v4(0), err)) {
    ge::test::skip("no loopback TCP socket here: " + err);
    return;
  }
  HubSpec spec;
  spec.game = "Test";
  Poller p;
  HubNode n;
  n.address = sim_address(5, 27500);
  n.have = n.listed = true;
  n.status.name = "first";
  n.changes = 1;
  p.nodes.push_back(n);
  int64_t now = 0;

  TcpStream c;
  CHECK(tcp_connect(c, s.listener.local, &err));
  const std::string req = "GET /v1/servers HTTP/1.1\r\nHost: hub\r\n\r\n";
  tcp_send_bytes(c, reinterpret_cast<const uint8_t*>(req.data()), req.size());
  std::string got = fetch_until(c, s, p, spec, "\"first\"", now);
  CHECK(got.rfind("HTTP/1.1 200 OK", 0) == 0 && got.find("\"count\":1") != std::string::npos);
  tcp_close(c);

  TcpStream e;
  CHECK(tcp_connect(e, s.listener.local, &err));
  const std::string ev = "GET /v1/servers/sim:5:27500/events HTTP/1.1\r\n\r\n";
  tcp_send_bytes(e, reinterpret_cast<const uint8_t*>(ev.data()), ev.size());
  got = fetch_until(e, s, p, spec, "\"first\"", now);
  CHECK(got.find("text/event-stream") != std::string::npos);
  CHECK(got.find("event: status") != std::string::npos);
  p.nodes[0].status.name = "second";
  p.nodes[0].changes = 2;
  got = fetch_until(e, s, p, spec, "\"second\"", now);
  CHECK(got.find("id: 2") != std::string::npos);
  p.nodes.clear();
  got = fetch_until(e, s, p, spec, "event: gone", now);
  CHECK(got.find("event: gone") != std::string::npos);
  CHECK(s.stats.answered == 1 && s.stats.streams == 1 && s.stats.events == 2);
  tcp_close(e);
  http_close(s);
}

TEST(http_serves_an_answer_larger_than_the_streams_backlog_whole) {
  HttpServer s;
  std::string err;
  if (!http_open(s, loopback_v4(0), err)) {
    ge::test::skip("no loopback TCP socket here: " + err);
    return;
  }
  // A viewer's file four times the stream's backlog (a wasm is megabytes).
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge-hub-http-big";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  std::string big(kStreamBacklog * 4, '\0');
  for (size_t i = 0; i < big.size(); i++) big[i] = static_cast<char>('a' + i % 26);
  std::ofstream(dir / "big.wasm", std::ios::binary) << big;
  s.viewer_dir = dir.string();
  HubSpec spec;
  spec.game = "Test";
  const Poller p;
  TcpStream c;
  CHECK(tcp_connect(c, s.listener.local, &err));
  const std::string req = "GET /viewer/big.wasm HTTP/1.1\r\nHost: hub\r\n\r\n";
  tcp_send_bytes(c, reinterpret_cast<const uint8_t*>(req.data()), req.size());
  std::string got;
  int64_t now = 0;
  for (int i = 0; i < 5000 && (got.find("\r\n\r\n") == std::string::npos ||
                               got.size() - got.find("\r\n\r\n") - 4 < big.size());
       i++) {
    now += 1000;
    http_update(s, p, spec, now);
    tcp_pump(c);
    std::vector<uint8_t> bytes;
    tcp_take_bytes(c, bytes);
    got.append(bytes.begin(), bytes.end());
  }
  const size_t head = got.find("\r\n\r\n");
  CHECK(got.rfind("HTTP/1.1 200 OK", 0) == 0 && head != std::string::npos);
  CHECK(got.substr(head + 4) == big);
  tcp_close(c);
  http_close(s);
  std::filesystem::remove_all(dir);
}
