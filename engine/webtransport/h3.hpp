// THE HTTP/3 A BROWSER'S WEBTRANSPORT NEEDS (docs/hub.md § Playing in a browser), and nothing
// more: a browser opens a session with an extended CONNECT on HTTP/3, and from then on its
// datagrams carry the session's id in front. So this is a small layer of pure functions over
// bytes, no state and no I/O: the endpoint (endpoint.hpp) feeds it a stream's bytes and sends
// what it makes.
//
//   varints                 QUIC's variable-length integers (1, 2, 4 or 8 bytes)
//   frames                  type, length, payload; `FrameReader` takes a stream's bytes in pieces
//   SETTINGS                what each side says first on its control stream: a server here says
//                           extended CONNECT, HTTP datagrams and WebTransport are on, and that it
//                           keeps no QPACK table
//   QPACK                   header blocks with the static table only (a peer told our table holds
//                           nothing never refers to one), literals plain or Huffman-coded
//   request and response    the CONNECT read into its method, protocol and path; the answer made
//   datagram                the quarter stream id in front of the payload
//
// The tables (QPACK's static table, HPACK's Huffman code) are the published ones, typed in
// h3_tables.cpp.
//
// ge:summary The HTTP/3 a WebTransport session needs: varints, frames, SETTINGS, static QPACK
// ge:keywords webtransport http3 h3 quic varint frame settings qpack huffman extended-connect
// ge:keywords datagram quarter-stream-id control-stream
// ge:skill net
// ge:test tests/webtransport_h3_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::webtransport::h3 {

// --- varints ------------------------------------------------------------------------------------
constexpr uint64_t kMaxVarint = (uint64_t{1} << 62) - 1;
size_t varint_size(uint64_t v);
// Appends `v` (at most kMaxVarint) in the fewest bytes.
void put_varint(std::vector<uint8_t>& out, uint64_t v);
// Reads one at `p`, moving it; false (and `p` unmoved) when the bytes end first.
bool get_varint(const uint8_t*& p, const uint8_t* end, uint64_t& v);

// --- frames and streams -------------------------------------------------------------------------
constexpr uint64_t kFrameData = 0x00;
constexpr uint64_t kFrameHeaders = 0x01;
constexpr uint64_t kFrameSettings = 0x04;
constexpr uint64_t kFrameGoaway = 0x07;
// A unidirectional stream's first varint says what it is.
constexpr uint64_t kStreamControl = 0x00;
constexpr uint64_t kStreamQpackEncoder = 0x02;
constexpr uint64_t kStreamQpackDecoder = 0x03;
constexpr uint64_t kStreamWebTransport = 0x54;
// A bidirectional stream a WebTransport session opens starts with this signal instead of a frame.
constexpr uint64_t kSignalWebTransport = 0x41;

void put_frame(std::vector<uint8_t>& out, uint64_t type, const uint8_t* payload, size_t size);

// Frames out of a stream's bytes as they arrive. `take` adds bytes; `next` gives the next whole
// frame (false while it is incomplete). A frame longer than `max_payload` sets `failed`.
struct FrameReader {
  std::vector<uint8_t> pending;
  size_t max_payload = size_t{64} * 1024;
  bool failed = false;
  void take(const uint8_t* data, size_t size) { pending.insert(pending.end(), data, data + size); }
  bool next(uint64_t& type, std::vector<uint8_t>& payload);
};

// --- SETTINGS -----------------------------------------------------------------------------------
constexpr uint64_t kSettingQpackMaxTableCapacity = 0x01;
constexpr uint64_t kSettingQpackBlockedStreams = 0x07;
constexpr uint64_t kSettingEnableConnectProtocol = 0x08;
constexpr uint64_t kSettingH3Datagram = 0x33;
constexpr uint64_t kSettingEnableWebTransport = 0x2b603742;  // the draft browsers speak today
constexpr uint64_t kSettingWebTransportMaxSessions = 0xc671706a;

struct Setting {
  uint64_t id = 0;
  uint64_t value = 0;
};
// The control stream a WebTransport server opens: its type, then SETTINGS.
std::vector<uint8_t> server_control_stream();
// The same for a client (this module's own test client).
std::vector<uint8_t> client_control_stream();
// A SETTINGS frame's payload as pairs; false when it is cut short.
bool parse_settings(const uint8_t* data, size_t size, std::vector<Setting>& out);
// The value of `id` in `settings`, or `fallback`.
uint64_t setting(const std::vector<Setting>& settings, uint64_t id, uint64_t fallback);

// --- QPACK, static table only -------------------------------------------------------------------
struct Field {
  std::string name;
  std::string value;
};
// A Huffman-coded string (HPACK's code); false on a bad code or padding.
bool huffman_decode(const uint8_t* data, size_t size, std::string& out);
// A field section; false with `error` when it refers to a dynamic table or is malformed.
bool qpack_decode(const uint8_t* data, size_t size, std::vector<Field>& out, std::string* error);
// A field section from the static table where it can, plain literals where not.
std::vector<uint8_t> qpack_encode(const std::vector<Field>& fields);
// The static table's size and its entries (index 0 to 98).
size_t static_table_size();
const char* static_name(size_t index);
const char* static_value(size_t index);

// --- the session's request and answer -----------------------------------------------------------
struct Request {
  std::string method;     // "CONNECT"
  std::string protocol;   // "webtransport"
  std::string scheme;     // "https"
  std::string authority;  // host:port the page named
  std::string path;       // "/play" and its query
  std::string origin;     // the page's origin
};
// The fields of a request into `out`; false with `error` when a field the session needs is
// missing or not a WebTransport CONNECT.
bool read_request(const std::vector<Field>& fields, Request& out, std::string* error);
// The HEADERS frame (with its type and length) that answers with `status`.
std::vector<uint8_t> response_frame(int status);
// The HEADERS frame of a client's CONNECT (the test client's).
std::vector<uint8_t> request_frame(std::string_view authority, std::string_view path,
                                   std::string_view origin);
// A response's status from its fields, or 0.
int response_status(const std::vector<Field>& fields);

// --- datagrams ----------------------------------------------------------------------------------
// The session's id (its CONNECT stream's id, a multiple of 4) in front of `payload`.
void put_datagram(std::vector<uint8_t>& out, uint64_t session_stream, const uint8_t* payload,
                  size_t size);
// The session's stream id and where the payload starts; false when the id is cut short.
bool read_datagram(const uint8_t* data, size_t size, uint64_t& session_stream,
                   size_t& payload_offset);

}  // namespace ge::webtransport::h3
