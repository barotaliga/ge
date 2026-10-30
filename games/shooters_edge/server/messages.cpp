// The message sink (messages.hpp): the encodings and the event names.
//
// ge:summary The message sink's encodings and event texts
// ge:keywords encode clear_frame event_text event_kind_name coord angle little-endian
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_messages_test.cpp
#include "shooters_edge/server/messages.hpp"

#include <cstring>

#include "core/num_text.hpp"

namespace shooters_edge::server {

namespace {

// Truncation toward zero to 32 bits; out of range or NaN is -2147483648 (the VM's rule).
int32_t trunc_i32(double d) {
  if (!(d > -2147483649.0 && d < 2147483648.0)) return INT32_MIN;
  return static_cast<int32_t>(d);
}

void put16(std::vector<uint8_t>& b, int32_t v) {
  const auto u = static_cast<uint32_t>(v);
  b.push_back(static_cast<uint8_t>(u & 0xff));
  b.push_back(static_cast<uint8_t>((u >> 8) & 0xff));
}

}  // namespace

void encode(std::vector<uint8_t>& buf, WriteKind kind, float number, const char* text) {
  switch (kind) {
    case WriteKind::byte_:
    case WriteKind::char_:
      buf.push_back(static_cast<uint8_t>(static_cast<uint32_t>(trunc_i32(number)) & 0xff));
      break;
    case WriteKind::short_:
    case WriteKind::entity:
      put16(buf, trunc_i32(number));
      break;
    case WriteKind::long_: {
      const auto u = static_cast<uint32_t>(trunc_i32(number));
      for (int i = 0; i < 4; i++) buf.push_back(static_cast<uint8_t>((u >> (8 * i)) & 0xff));
      break;
    }
    case WriteKind::coord:
      put16(buf, trunc_i32(static_cast<double>(number) * 8));
      break;
    case WriteKind::angle:
      buf.push_back(static_cast<uint8_t>(
          static_cast<uint32_t>(trunc_i32(static_cast<double>(number) * 256 / 360)) & 0xff));
      break;
    case WriteKind::string: {
      const size_t n = std::strlen(text);
      buf.insert(buf.end(), text, text + n);
      buf.push_back(0);
      break;
    }
  }
}

const char* event_kind_name(EventKind k) {
  switch (k) {
    case EventKind::sound:
      return "sound";
    case EventKind::stuffcmd:
      return "stuffcmd";
    case EventKind::drop:
      return "drop";
    case EventKind::print:
      return "print";
    case EventKind::centerprint:
      return "centerprint";
    case EventKind::lightstyle:
      return "lightstyle";
    case EventKind::particle:
      return "particle";
    case EventKind::ambient:
      return "ambient";
    case EventKind::static_entity:
      return "static";
    case EventKind::multicast:
      return "multicast";
    case EventKind::pause:
      return "pause";
  }
  return "?";
}

void clear_frame(MessageSink& m) {
  m.unreliable.clear();
  m.events.clear();
}

std::string event_text(const NetEvent& e) {
  std::string s = event_kind_name(e.kind);
  s += " to=" + std::to_string(e.to) + (e.reliable ? " reliable" : "");
  if (!e.text.empty()) {
    std::string t = e.text;
    for (char& c : t)
      if (c == '\n') c = '|';
    s += " \"" + t + "\"";
  }
  if (e.kind == EventKind::sound || e.kind == EventKind::particle)
    s += " at " + ge::core::num::to_fixed(e.origin.x, 1) + " " +
         ge::core::num::to_fixed(e.origin.y, 1) + " " + ge::core::num::to_fixed(e.origin.z, 1);
  if (!e.bytes.empty()) s += " " + std::to_string(e.bytes.size()) + " bytes";
  return s;
}

}  // namespace shooters_edge::server
