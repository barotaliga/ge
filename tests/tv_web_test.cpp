// WebSocket viewers and finished files for a TV node (engine/tv/websocket.hpp, library.hpp): SHA-1,
// base64 and the accept key hold to the standard's own examples; frames go both ways masked and
// not, at every length form; a handshake is taken whole or refused; a loopback WebSocket viewer
// gets the whole stream from a node; a node's library is listed and a file fetched by its SHA-256,
// and a files-only viewer is not counted as watching.
//
// ge:summary Tests TV's WebSocket viewers (handshake, frames, a loopback viewer) and its files
// ge:keywords test tv websocket sha1 base64 handshake frame loopback library sha256 files
// ge:skill replay net testing
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "check.hpp"
#include "net/tcp_socket.hpp"
#include "replay/stream.hpp"
#include "tv/library.hpp"
#include "tv/node.hpp"
#include "tv/websocket.hpp"

namespace net = ge::net;
namespace rp = ge::replay;
namespace tv = ge::tv;

namespace {

std::vector<uint8_t> bytes_of(std::string_view s) { return {s.begin(), s.end()}; }

std::string hex(std::span<const uint8_t> b) {
  static const char* k = "0123456789abcdef";
  std::string o;
  for (uint8_t c : b) {
    o += k[c >> 4];
    o += k[c & 15];
  }
  return o;
}

// `blocks` one-second blocks, a keyframe every `key`, ended by the trailer.
std::vector<uint8_t> make_stream(int blocks, int key, const std::string& map) {
  std::vector<uint8_t> file;
  rp::StreamWriter w;
  rp::StreamHeader h;
  h.game = "tv-test";
  h.meta = {{"map", map}};
  h.tracks = {{1, rp::TrackKind::inputs, rp::TrackCodec::stored, "inputs"}};
  rp::stream_begin(w, h, file);
  for (int b = 0; b < blocks; b++) {
    std::vector<uint8_t> chunk(2000);
    for (size_t i = 0; i < chunk.size(); i++)
      chunk[i] = static_cast<uint8_t>(static_cast<size_t>(b) * 7 + i * 13);
    rp::stream_add(w, 1, int64_t{b} * 1000, chunk);
    rp::stream_end_block(w, b % key == 0);
  }
  rp::stream_finish(w, {{"duration", std::to_string(blocks)}});
  return file;
}

}  // namespace

TEST(sha1_base64_and_the_accept_key_match_the_standards_examples) {
  CHECK((hex(tv::sha1(bytes_of("abc")))) ==
        (std::string("a9993e364706816aba3e25717850c26c9cd0d89d")));
  CHECK((hex(tv::sha1(bytes_of("")))) == (std::string("da39a3ee5e6b4b0d3255bfef95601890afd80709")));
  CHECK((tv::base64(bytes_of("f"))) == (std::string("Zg==")));
  CHECK((tv::base64(bytes_of("fo"))) == (std::string("Zm8=")));
  CHECK((tv::base64(bytes_of("foobar"))) == (std::string("Zm9vYmFy")));
  CHECK((tv::ws_accept("dGhlIHNhbXBsZSBub25jZQ==")) ==
        (std::string("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")));
}

TEST(frames_round_trip_masked_and_not_at_every_length_form) {
  const uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
  for (size_t n : {size_t{0}, size_t{5}, size_t{125}, size_t{126}, size_t{300}, size_t{70000}}) {
    std::vector<uint8_t> payload(n);
    for (size_t i = 0; i < n; i++) payload[i] = static_cast<uint8_t>(i * 31 + 7);
    for (const uint8_t* m : {static_cast<const uint8_t*>(nullptr), mask}) {
      const auto f = tv::ws_frame(tv::WsOp::binary, payload, m);
      tv::WsFrame out;
      CHECK((tv::ws_parse({f.data(), f.size() - 1}, out)) == (0));  // one byte short: not whole
      CHECK((tv::ws_parse(f, out)) == (1));
      CHECK(out.op == tv::WsOp::binary);
      CHECK(out.fin);
      CHECK((out.size) == (f.size()));
      CHECK(out.payload == payload);
    }
  }
  std::vector<uint8_t> reserved = tv::ws_frame(tv::WsOp::binary, bytes_of("x"));
  reserved[0] |= 0x40;
  tv::WsFrame out;
  CHECK((tv::ws_parse(reserved, out)) == (-1));
}

TEST(a_handshake_is_taken_whole_or_refused) {
  const std::string req =
      "GET /tv HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
  std::string reply, error;
  size_t used = 0;
  const auto whole = bytes_of(req);
  CHECK((tv::ws_handshake({whole.data(), whole.size() - 2}, reply, used, error)) == (0));
  CHECK((tv::ws_handshake(whole, reply, used, error)) == (1));
  CHECK((used) == (whole.size()));
  CHECK(reply.find("101 Switching Protocols") != std::string::npos);
  CHECK(reply.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
  const auto plain = bytes_of("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
  CHECK((tv::ws_handshake(plain, reply, used, error)) == (-1));
  CHECK(error.find("not a WebSocket upgrade") != std::string::npos);
}

TEST(a_websocket_viewer_on_loopback_gets_the_whole_stream) {
  const std::vector<uint8_t> file = make_stream(8, 3, "hall");
  tv::Node node;
  node.config.delay_ms = 1000;  // the blocks go out one by one, so the viewer starts at the first
  bool fed = false;
  tv::TvWeb web;
  std::string error;
  if (!tv::tv_web_listen(web, net::loopback_v4(0), error)) {
    std::printf("SKIP: no loopback TCP here: %s\n", error.c_str());
    return;
  }
  net::TcpStream client;
  CHECK(net::tcp_connect(client, web.listener.local, &error));
  const std::string req =
      "GET /tv HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
  const uint8_t mask[4] = {1, 2, 3, 4};
  tv::Watch watch;
  std::vector<uint8_t> in;
  bool sent_request = false, upgraded = false, sent_hello = false, pinged = false, ponged = false;
  const auto send = [&](int link, std::span<const uint8_t> m) {
    return tv::tv_web_send(web, link, m);
  };
  for (int i = 0; i < 5000 && !watch.ended; i++) {
    net::tcp_pump(client);
    if (!sent_request && !client.connecting) {
      net::tcp_send_bytes(client, reinterpret_cast<const uint8_t*>(req.data()), req.size());
      sent_request = true;
    }
    tv::tv_web_receive(web, node);
    if (!fed && !node.viewers.empty() && node.viewers[0].greeted) {
      CHECK(tv::tv_feed(node, file, int64_t{i} * 100));
      fed = true;
    }
    tv::tv_update(node, int64_t{i} * 100, send);
    tv::tv_web_flush(web);
    net::tcp_pump(client);
    net::tcp_take_bytes(client, in);
    if (!upgraded) {
      const std::string text(in.begin(), in.end());
      const size_t end = text.find("\r\n\r\n");
      if (end != std::string::npos) {
        CHECK(text.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
        in.erase(in.begin(), in.begin() + static_cast<std::ptrdiff_t>(end + 4));
        upgraded = true;
      }
    }
    if (upgraded && !sent_hello) {
      const auto f = tv::ws_frame(tv::WsOp::binary, tv::tv_hello("browser", false), mask);
      const auto p = tv::ws_frame(tv::WsOp::ping, bytes_of("hi"), mask);
      net::tcp_send_bytes(client, f.data(), f.size());
      net::tcp_send_bytes(client, p.data(), p.size());
      sent_hello = pinged = true;
    }
    for (;;) {
      tv::WsFrame f;
      if (!upgraded || tv::ws_parse(in, f) != 1) break;
      in.erase(in.begin(), in.begin() + static_cast<std::ptrdiff_t>(f.size));
      if (f.op == tv::WsOp::pong) ponged = f.payload == bytes_of("hi");
      if (f.op == tv::WsOp::binary) CHECK(tv::watch_receive(watch, f.payload));
    }
    if (!watch.ended) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(pinged && ponged);
  CHECK(watch.ended);
  CHECK((watch.blocks) == (int64_t{8}));
  CHECK((watch.viewers) == (int64_t{1}));
  // The watch holds the header and every block; the file adds only its trailer.
  CHECK(watch.stream.size() < file.size());
  CHECK(std::equal(watch.stream.begin(), watch.stream.end(), file.begin()));
  net::tcp_close(client);
  tv::tv_web_close(web);
}

TEST(a_nodes_library_is_listed_and_a_file_fetched_by_its_sha256) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "ge_tv_web_test_library";
  fs::remove_all(dir);
  fs::create_directories(dir);
  const std::vector<uint8_t> a = make_stream(40, 5, "hall");  // over two pieces
  const std::vector<uint8_t> b = make_stream(3, 1, "yard");
  for (const auto& [name, bytes] : {std::pair{"a.ger", &a}, std::pair{"b.ger", &b}}) {
    std::ofstream f(dir / name, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes->data()),
            static_cast<std::streamsize>(bytes->size()));
  }
  {
    std::ofstream junk(dir / "notes.ger");
    junk << "not a replay";
  }
  tv::Node node;
  std::string error;
  CHECK(tv::library_scan(dir.string(), node.library, error));
  CHECK((node.library.size()) == (size_t{2}));
  CHECK((node.library[0].name) == (std::string("a.ger")));
  CHECK((node.library[0].bytes) == (static_cast<uint64_t>(a.size())));
  CHECK((node.library[0].game) == (std::string("tv-test")));
  CHECK((node.library[1].meta[0].second) == (std::string("yard")));
  CHECK((node.library[0].sha256.size()) == (size_t{64}));

  std::map<int, tv::Watch> w;
  const tv::Send send = [&](int link, std::span<const uint8_t> m) {
    return tv::watch_receive(w[link], m);
  };
  tv::tv_join(node, 1);
  CHECK(tv::tv_receive(node, 1, tv::tv_hello_files("archive")));
  CHECK(tv::tv_receive(node, 1, tv::tv_list()));
  CHECK(tv::tv_receive(node, 1, tv::tv_get(node.library[0].sha256)));
  tv::tv_update(node, 0, send);
  CHECK(w[1].have_list);
  CHECK((w[1].files.size()) == (size_t{2}));
  CHECK((w[1].files[1].name) == (std::string("b.ger")));
  CHECK(w[1].fetch_done && !w[1].fetch_missing);
  CHECK(w[1].fetched == a);
  CHECK(!w[1].have_header);  // files only: no live stream
  CHECK((tv::tv_viewer_count(node)) == (int64_t{0}));

  CHECK(tv::tv_receive(node, 1, tv::tv_get(std::string(64, '0'))));
  tv::tv_update(node, 1, send);
  CHECK(w[1].fetch_done && w[1].fetch_missing);
  fs::remove_all(dir);
}
