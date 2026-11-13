// The HTTP/3 layer's functions (h3.hpp): varints, frames, SETTINGS, QPACK with the static table,
// the request and its answer, and the datagram's session id.
//
// ge:summary HTTP/3 for WebTransport: varints, frames, SETTINGS, static QPACK, Huffman
// ge:keywords webtransport http3 qpack huffman settings frame varint extended-connect
// ge:skill net
// ge:test tests/webtransport_h3_test.cpp
#include "webtransport/h3.hpp"

#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include "webtransport/h3_tables.hpp"

namespace ge::webtransport::h3 {

// --- varints ------------------------------------------------------------------------------------

size_t varint_size(uint64_t v) {
  if (v < 64) return 1;
  if (v < 16384) return 2;
  if (v < 1073741824) return 4;
  return 8;
}

void put_varint(std::vector<uint8_t>& out, uint64_t v) {
  const size_t n = varint_size(v);
  const uint8_t prefix = n == 1 ? 0x00 : n == 2 ? 0x40 : n == 4 ? 0x80 : 0xc0;
  for (size_t i = 0; i < n; ++i) {
    uint8_t b = static_cast<uint8_t>(v >> (8 * (n - 1 - i)));
    if (i == 0) b = static_cast<uint8_t>((b & 0x3f) | prefix);
    out.push_back(b);
  }
}

bool get_varint(const uint8_t*& p, const uint8_t* end, uint64_t& v) {
  if (p >= end) return false;
  const size_t n = size_t{1} << (p[0] >> 6);
  if (static_cast<size_t>(end - p) < n) return false;
  uint64_t x = p[0] & 0x3f;
  for (size_t i = 1; i < n; ++i) x = (x << 8) | p[i];
  v = x;
  p += n;
  return true;
}

// --- frames -------------------------------------------------------------------------------------

void put_frame(std::vector<uint8_t>& out, uint64_t type, const uint8_t* payload, size_t size) {
  put_varint(out, type);
  put_varint(out, size);
  if (size) out.insert(out.end(), payload, payload + size);
}

bool FrameReader::next(uint64_t& type, std::vector<uint8_t>& payload) {
  if (failed) return false;
  const uint8_t* p = pending.data();
  const uint8_t* end = p + pending.size();
  uint64_t t = 0, len = 0;
  if (!get_varint(p, end, t) || !get_varint(p, end, len)) return false;
  if (len > max_payload) {
    failed = true;
    return false;
  }
  if (static_cast<uint64_t>(end - p) < len) return false;
  type = t;
  payload.assign(p, p + len);
  pending.erase(pending.begin(), pending.begin() + ((p - pending.data()) + len));
  return true;
}

// --- SETTINGS -----------------------------------------------------------------------------------

namespace {

std::vector<uint8_t> control_stream(const std::vector<Setting>& settings) {
  std::vector<uint8_t> payload;
  for (const Setting& s : settings) {
    put_varint(payload, s.id);
    put_varint(payload, s.value);
  }
  std::vector<uint8_t> out;
  put_varint(out, kStreamControl);
  put_frame(out, kFrameSettings, payload.data(), payload.size());
  return out;
}

}  // namespace

std::vector<uint8_t> server_control_stream() {
  return control_stream({{kSettingQpackMaxTableCapacity, 0},
                         {kSettingQpackBlockedStreams, 0},
                         {kSettingEnableConnectProtocol, 1},
                         {kSettingH3Datagram, 1},
                         {kSettingEnableWebTransport, 1},
                         {kSettingWebTransportMaxSessions, 1}});
}

std::vector<uint8_t> client_control_stream() {
  return control_stream({{kSettingQpackMaxTableCapacity, 0},
                         {kSettingQpackBlockedStreams, 0},
                         {kSettingH3Datagram, 1},
                         {kSettingEnableWebTransport, 1}});
}

bool parse_settings(const uint8_t* data, size_t size, std::vector<Setting>& out) {
  const uint8_t* p = data;
  const uint8_t* end = data + size;
  out.clear();
  while (p < end) {
    Setting s;
    if (!get_varint(p, end, s.id) || !get_varint(p, end, s.value)) return false;
    out.push_back(s);
  }
  return true;
}

uint64_t setting(const std::vector<Setting>& settings, uint64_t id, uint64_t fallback) {
  for (const Setting& s : settings) {
    if (s.id == id) return s.value;
  }
  return fallback;
}

// --- QPACK --------------------------------------------------------------------------------------

size_t static_table_size() { return kStaticTableSize; }
const char* static_name(size_t index) { return kStaticTable[index].name; }
const char* static_value(size_t index) { return kStaticTable[index].value; }

namespace {

// HPACK's prefixed integer: the low `bits` of the first byte, then 7 bits a byte.
bool get_prefixed(const uint8_t*& p, const uint8_t* end, int bits, uint64_t& v) {
  if (p >= end) return false;
  const uint64_t max = (uint64_t{1} << bits) - 1;
  v = *p++ & max;
  if (v < max) return true;
  for (int shift = 0; shift < 56; shift += 7) {
    if (p >= end) return false;
    const uint8_t b = *p++;
    v += uint64_t{b & 0x7fu} << shift;
    if (!(b & 0x80)) return true;
  }
  return false;
}

void put_prefixed(std::vector<uint8_t>& out, uint8_t first, int bits, uint64_t v) {
  const uint64_t max = (uint64_t{1} << bits) - 1;
  if (v < max) {
    out.push_back(static_cast<uint8_t>(first | v));
    return;
  }
  out.push_back(static_cast<uint8_t>(first | max));
  v -= max;
  while (v >= 128) {
    out.push_back(static_cast<uint8_t>((v & 0x7f) | 0x80));
    v >>= 7;
  }
  out.push_back(static_cast<uint8_t>(v));
}

// A string literal whose length has a `bits`-bit prefix and an H flag just above it.
bool get_string(const uint8_t*& p, const uint8_t* end, int bits, std::string& out) {
  if (p >= end) return false;
  const bool huffman = (*p >> bits) & 1;
  uint64_t len = 0;
  if (!get_prefixed(p, end, bits, len) || static_cast<uint64_t>(end - p) < len) return false;
  bool ok = true;
  if (huffman) {
    ok = huffman_decode(p, static_cast<size_t>(len), out);
  } else {
    out.assign(reinterpret_cast<const char*>(p), static_cast<size_t>(len));
  }
  p += len;
  return ok;
}

void put_string(std::vector<uint8_t>& out, uint8_t first, int bits, std::string_view s) {
  put_prefixed(out, first, bits, s.size());
  out.insert(out.end(), s.begin(), s.end());
}

uint64_t huffman_key(uint32_t code, int bits) { return (uint64_t(bits) << 32) | code; }

const std::unordered_map<uint64_t, int>& huffman_lookup() {
  static const std::unordered_map<uint64_t, int> table = [] {
    std::unordered_map<uint64_t, int> t;
    for (int s = 0; s < 257; ++s) t.emplace(huffman_key(kHuffman[s].code, kHuffman[s].bits), s);
    return t;
  }();
  return table;
}

bool fail(std::string* error, const char* what) {
  if (error) *error = what;
  return false;
}

}  // namespace

bool huffman_decode(const uint8_t* data, size_t size, std::string& out) {
  const auto& lookup = huffman_lookup();
  out.clear();
  uint32_t code = 0;
  int bits = 0;
  for (size_t i = 0; i < size; ++i) {
    for (int b = 7; b >= 0; --b) {
      code = (code << 1) | ((data[i] >> b) & 1);
      ++bits;
      if (bits < 5) continue;
      if (bits > 30) return false;
      const auto it = lookup.find(huffman_key(code, bits));
      if (it == lookup.end()) continue;
      if (it->second == 256) return false;  // EOS inside a string
      out.push_back(static_cast<char>(it->second));
      code = 0;
      bits = 0;
    }
  }
  // What is left is padding: fewer than 8 bits, all ones.
  return bits < 8 && code == (uint32_t{1} << bits) - 1;
}

bool qpack_decode(const uint8_t* data, size_t size, std::vector<Field>& out, std::string* error) {
  const uint8_t* p = data;
  const uint8_t* end = data + size;
  uint64_t insert_count = 0, base = 0;
  if (!get_prefixed(p, end, 8, insert_count) || !get_prefixed(p, end, 7, base)) {
    return fail(error, "qpack: the field section's prefix is cut short");
  }
  if (insert_count != 0) {
    return fail(error,
                "qpack: the field section refers to a dynamic table this side never allowed");
  }
  out.clear();
  while (p < end) {
    const uint8_t b = *p;
    Field f;
    if (b & 0x80) {  // indexed field line
      if (!(b & 0x40)) return fail(error, "qpack: an indexed line refers to the dynamic table");
      uint64_t index = 0;
      if (!get_prefixed(p, end, 6, index) || index >= kStaticTableSize) {
        return fail(error, "qpack: an indexed line's static index is out of the table");
      }
      f.name = kStaticTable[index].name;
      f.value = kStaticTable[index].value;
    } else if (b & 0x40) {  // literal with a name reference
      if (!(b & 0x10)) return fail(error, "qpack: a name reference points at the dynamic table");
      uint64_t index = 0;
      if (!get_prefixed(p, end, 4, index) || index >= kStaticTableSize) {
        return fail(error, "qpack: a name reference's static index is out of the table");
      }
      f.name = kStaticTable[index].name;
      if (!get_string(p, end, 7, f.value)) return fail(error, "qpack: a value is malformed");
    } else if (b & 0x20) {  // literal with a literal name
      if (!get_string(p, end, 3, f.name) || !get_string(p, end, 7, f.value)) {
        return fail(error, "qpack: a literal name or value is malformed");
      }
    } else {
      return fail(error, "qpack: a post-base line refers to the dynamic table");
    }
    out.push_back(std::move(f));
  }
  return true;
}

std::vector<uint8_t> qpack_encode(const std::vector<Field>& fields) {
  std::vector<uint8_t> out{0x00, 0x00};
  for (const Field& f : fields) {
    int exact = -1, named = -1;
    for (size_t i = 0; i < kStaticTableSize; ++i) {
      if (f.name != kStaticTable[i].name) continue;
      if (named < 0) named = static_cast<int>(i);
      if (f.value == kStaticTable[i].value) {
        exact = static_cast<int>(i);
        break;
      }
    }
    if (exact >= 0) {
      put_prefixed(out, 0xc0, 6, static_cast<uint64_t>(exact));
    } else if (named >= 0) {
      put_prefixed(out, 0x50, 4, static_cast<uint64_t>(named));
      put_string(out, 0x00, 7, f.value);
    } else {
      put_string(out, 0x20, 3, f.name);
      put_string(out, 0x00, 7, f.value);
    }
  }
  return out;
}

// --- the request and its answer -----------------------------------------------------------------

bool read_request(const std::vector<Field>& fields, Request& out, std::string* error) {
  out = Request{};
  for (const Field& f : fields) {
    if (f.name == ":method")
      out.method = f.value;
    else if (f.name == ":protocol")
      out.protocol = f.value;
    else if (f.name == ":scheme")
      out.scheme = f.value;
    else if (f.name == ":authority")
      out.authority = f.value;
    else if (f.name == ":path")
      out.path = f.value;
    else if (f.name == "origin")
      out.origin = f.value;
  }
  if (out.method != "CONNECT") {
    return fail(error, "webtransport: the request is not a CONNECT; only WebTransport is served");
  }
  if (out.protocol != "webtransport") {
    return fail(error, "webtransport: the CONNECT's :protocol is not webtransport");
  }
  if (out.path.empty() || out.path[0] != '/') {
    return fail(error, "webtransport: the CONNECT has no :path");
  }
  return true;
}

std::vector<uint8_t> response_frame(int status) {
  std::vector<Field> fields{{":status", std::to_string(status)}};
  // The draft today's browsers speak wants it named in the answer.
  if (status == 200) fields.push_back({"sec-webtransport-http3-draft", "draft02"});
  const std::vector<uint8_t> block = qpack_encode(fields);
  std::vector<uint8_t> out;
  put_frame(out, kFrameHeaders, block.data(), block.size());
  return out;
}

std::vector<uint8_t> request_frame(std::string_view authority, std::string_view path,
                                   std::string_view origin) {
  const std::vector<uint8_t> block = qpack_encode({{":method", "CONNECT"},
                                                   {":protocol", "webtransport"},
                                                   {":scheme", "https"},
                                                   {":authority", std::string(authority)},
                                                   {":path", std::string(path)},
                                                   {"origin", std::string(origin)},
                                                   {"sec-webtransport-http3-draft02", "1"}});
  std::vector<uint8_t> out;
  put_frame(out, kFrameHeaders, block.data(), block.size());
  return out;
}

int response_status(const std::vector<Field>& fields) {
  for (const Field& f : fields) {
    if (f.name == ":status") return std::atoi(f.value.c_str());
  }
  return 0;
}

// --- datagrams ----------------------------------------------------------------------------------

void put_datagram(std::vector<uint8_t>& out, uint64_t session_stream, const uint8_t* payload,
                  size_t size) {
  put_varint(out, session_stream / 4);
  if (size) out.insert(out.end(), payload, payload + size);
}

bool read_datagram(const uint8_t* data, size_t size, uint64_t& session_stream,
                   size_t& payload_offset) {
  const uint8_t* p = data;
  uint64_t quarter = 0;
  if (!get_varint(p, data + size, quarter)) return false;
  session_stream = quarter * 4;
  payload_offset = static_cast<size_t>(p - data);
  return true;
}

}  // namespace ge::webtransport::h3
