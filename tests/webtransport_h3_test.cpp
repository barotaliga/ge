// engine/webtransport/h3.hpp: the HTTP/3 a WebTransport session needs. QUIC's varints (the
// published examples), frames in pieces, SETTINGS, the Huffman code (the published example),
// QPACK with the static table both ways and its refusals, the CONNECT and its answer, datagrams.
//
// ge:summary Test: varints, frames, SETTINGS, Huffman, static QPACK, CONNECT, datagrams
// ge:keywords test webtransport http3 qpack huffman varint settings extended-connect
// ge:skill net testing
#include "webtransport/h3.hpp"

#include <string>

#include "check.hpp"

using namespace ge::webtransport::h3;

namespace {
uint64_t read_one(std::vector<uint8_t> bytes) {
  const uint8_t* p = bytes.data();
  uint64_t v = 0;
  if (!get_varint(p, p + bytes.size(), v)) return ~uint64_t{0};
  return p == bytes.data() + bytes.size() ? v : ~uint64_t{0};
}
}  // namespace

TEST(h3_varints) {
  CHECK(read_one({0xc2, 0x19, 0x7c, 0x5e, 0xff, 0x14, 0xe8, 0x8c}) == 151288809941952652ull);
  CHECK(read_one({0x9d, 0x7f, 0x3e, 0x7d}) == 494878333);
  CHECK(read_one({0x7b, 0xbd}) == 15293);
  CHECK(read_one({0x25}) == 37);
  CHECK(read_one({0x40, 0x25}) == 37);
  for (uint64_t v : std::initializer_list<uint64_t>{0, 63, 64, 16383, 16384, 1073741823, 1073741824,
                                                    kMaxVarint}) {
    std::vector<uint8_t> out;
    put_varint(out, v);
    CHECK(out.size() == varint_size(v));
    CHECK(read_one(out) == v);
  }
  const uint8_t cut[2] = {0x9d, 0x7f};
  const uint8_t* p = cut;
  uint64_t v = 0;
  CHECK(!get_varint(p, cut + 2, v) && p == cut);
}

TEST(h3_frames_in_pieces) {
  std::vector<uint8_t> bytes;
  const uint8_t a[3] = {1, 2, 3};
  put_frame(bytes, kFrameData, a, 3);
  put_frame(bytes, kFrameHeaders, nullptr, 0);
  FrameReader r;
  uint64_t type = 99;
  std::vector<uint8_t> payload;
  for (size_t i = 0; i < bytes.size(); ++i) {
    r.take(&bytes[i], 1);
    if (i + 1 < 5) CHECK(!r.next(type, payload));
  }
  CHECK(r.next(type, payload) && type == kFrameData && payload.size() == 3 && payload[2] == 3);
  CHECK(r.next(type, payload) && type == kFrameHeaders && payload.empty());
  CHECK(!r.next(type, payload));
  FrameReader big;
  big.max_payload = 8;
  std::vector<uint8_t> huge;
  put_varint(huge, kFrameData);
  put_varint(huge, 9);
  big.take(huge.data(), huge.size());
  CHECK(!big.next(type, payload) && big.failed);
}

TEST(h3_server_settings) {
  const std::vector<uint8_t> stream = server_control_stream();
  const uint8_t* p = stream.data();
  uint64_t kind = 9;
  CHECK(get_varint(p, p + stream.size(), kind) && kind == kStreamControl);
  FrameReader r;
  r.take(p, static_cast<size_t>(stream.data() + stream.size() - p));
  uint64_t type = 0;
  std::vector<uint8_t> payload;
  CHECK(r.next(type, payload) && type == kFrameSettings);
  std::vector<Setting> s;
  CHECK(parse_settings(payload.data(), payload.size(), s));
  CHECK(setting(s, kSettingEnableConnectProtocol, 0) == 1);
  CHECK(setting(s, kSettingH3Datagram, 0) == 1);
  CHECK(setting(s, kSettingEnableWebTransport, 0) == 1);
  CHECK(setting(s, kSettingQpackMaxTableCapacity, 9) == 0);
  CHECK(setting(s, 0x1234, 7) == 7);
  CHECK(!parse_settings(payload.data(), payload.size() - 1, s));
}

TEST(h3_huffman) {
  // RFC 7541 C.4.1: "www.example.com".
  const uint8_t code[] = {0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff};
  std::string out;
  CHECK(huffman_decode(code, sizeof code, out));
  CHECK(out == "www.example.com");
  // "no-cache" (C.4.2).
  const uint8_t nc[] = {0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf};
  CHECK(huffman_decode(nc, sizeof nc, out) && out == "no-cache");
  // Padding that is not all ones, and a whole byte of padding, are refused.
  const uint8_t bad[] = {0xf1, 0xe3, 0xc2, 0xe5, 0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xfe};
  CHECK(!huffman_decode(bad, sizeof bad, out));
  const uint8_t pad[] = {0xa8, 0xeb, 0x10, 0x64, 0x9c, 0xbf, 0xff};
  CHECK(!huffman_decode(pad, sizeof pad, out));
}

TEST(h3_qpack_static_table) {
  CHECK(static_table_size() == 99);
  CHECK(std::string(static_name(15)) == ":method" && std::string(static_value(15)) == "CONNECT");
  CHECK(std::string(static_name(25)) == ":status" && std::string(static_value(25)) == "200");
  CHECK(std::string(static_name(98)) == "x-frame-options");
}

TEST(h3_qpack_both_ways) {
  const std::vector<Field> fields{{":method", "CONNECT"},                    // exact entry
                                  {":path", "/play?to=1"},                   // name entry
                                  {"sec-webtransport-http3-draft02", "1"}};  // literal
  const std::vector<uint8_t> block = qpack_encode(fields);
  CHECK(block[2] == (0xc0 | 15));
  std::vector<Field> back;
  std::string err;
  CHECK_MSG(qpack_decode(block.data(), block.size(), back, &err), err);
  CHECK(back.size() == 3);
  CHECK(back[1].name == ":path" && back[1].value == "/play?to=1");
  CHECK(back[2].name == "sec-webtransport-http3-draft02" && back[2].value == "1");
  // A Huffman-coded value from a browser: :authority (static 0) "www.example.com".
  const std::vector<uint8_t> h{0x00, 0x00, 0x50, 0x8c, 0xf1, 0xe3, 0xc2, 0xe5,
                               0xf2, 0x3a, 0x6b, 0xa0, 0xab, 0x90, 0xf4, 0xff};
  CHECK(qpack_decode(h.data(), h.size(), back, &err) && back.size() == 1);
  CHECK(back[0].name == ":authority" && back[0].value == "www.example.com");
}

TEST(h3_qpack_refuses_the_dynamic_table) {
  std::vector<Field> out;
  std::string err;
  const std::vector<uint8_t> insert_count{0x01, 0x00, 0xd1};
  CHECK(!qpack_decode(insert_count.data(), insert_count.size(), out, &err));
  CHECK(err.find("dynamic table") != std::string::npos);
  const std::vector<uint8_t> dyn_index{0x00, 0x00, 0x80};
  CHECK(!qpack_decode(dyn_index.data(), dyn_index.size(), out, &err));
  const std::vector<uint8_t> post_base{0x00, 0x00, 0x10};
  CHECK(!qpack_decode(post_base.data(), post_base.size(), out, &err));
  const std::vector<uint8_t> out_of_table{0x00, 0x00, 0xff, 0x30};
  CHECK(!qpack_decode(out_of_table.data(), out_of_table.size(), out, &err));
}

TEST(h3_connect_and_answer) {
  const std::vector<uint8_t> frame = request_frame("127.0.0.1:4433", "/play?to=x", "https://hub");
  FrameReader r;
  r.take(frame.data(), frame.size());
  uint64_t type = 0;
  std::vector<uint8_t> payload;
  CHECK(r.next(type, payload) && type == kFrameHeaders);
  std::vector<Field> fields;
  CHECK(qpack_decode(payload.data(), payload.size(), fields, nullptr));
  Request req;
  std::string err;
  CHECK_MSG(read_request(fields, req, &err), err);
  CHECK(req.path == "/play?to=x" && req.authority == "127.0.0.1:4433" &&
        req.origin == "https://hub");
  fields[0].value = "GET";
  CHECK(!read_request(fields, req, &err) && err.find("not a CONNECT") != std::string::npos);

  const std::vector<uint8_t> ok = response_frame(200);
  FrameReader rr;
  rr.take(ok.data(), ok.size());
  CHECK(rr.next(type, payload) && type == kFrameHeaders);
  CHECK(qpack_decode(payload.data(), payload.size(), fields, nullptr));
  CHECK(response_status(fields) == 200);
  CHECK(fields.size() == 2 && fields[1].value == "draft02");
}

TEST(h3_datagrams) {
  std::vector<uint8_t> d;
  const uint8_t m[2] = {7, 8};
  put_datagram(d, uint64_t{400}, m, 2);
  uint64_t stream = 0;
  size_t at = 0;
  CHECK(read_datagram(d.data(), d.size(), stream, at));
  CHECK(stream == 400 && at == 2 && d[at] == 7 && d.size() - at == 2);
  CHECK(!read_datagram(d.data(), 0, stream, at));
}
