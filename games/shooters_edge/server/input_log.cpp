// The input log's bytes: written and read field by field, little-endian (input_log.hpp).
//
// ge:summary The input log's file: written and read field by field, refused when damaged
// ge:keywords input-log write read bytes little-endian refuse damaged
// ge:skill shooter
// ge:test tests/shooters_edge_host_test.cpp
#include "shooters_edge/server/input_log.hpp"

#include <cstring>

namespace shooters_edge::server {

namespace {

constexpr char kMagic[4] = {'G', 'E', 'S', 'L'};

struct Writer {
  std::vector<uint8_t>& out;
  void u8(uint8_t v) { out.push_back(v); }
  void u16(uint16_t v) {
    for (int i = 0; i < 2; i++) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  void u32(uint32_t v) {
    for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  void u64(uint64_t v) {
    for (int i = 0; i < 8; i++) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
  }
  void f32(float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    u32(bits);
  }
};

struct Reader {
  const std::vector<uint8_t>& in;
  size_t at = 0;
  bool short_read = false;
  bool need(size_t n) {
    if (at + n <= in.size()) return true;
    short_read = true;
    return false;
  }
  uint64_t le(int n) {
    if (!need(static_cast<size_t>(n))) return 0;
    uint64_t v = 0;
    for (int i = 0; i < n; i++)
      v |= static_cast<uint64_t>(in[at + static_cast<size_t>(i)]) << (8 * i);
    at += static_cast<size_t>(n);
    return v;
  }
  uint8_t u8() { return static_cast<uint8_t>(le(1)); }
  uint16_t u16() { return static_cast<uint16_t>(le(2)); }
  uint32_t u32() { return static_cast<uint32_t>(le(4)); }
  uint64_t u64() { return le(8); }
  float f32() {
    const uint32_t bits = u32();
    float v = 0;
    std::memcpy(&v, &bits, 4);
    return v;
  }
};

void write_cmd(Writer& w, const sh::MoveCmd& c) {
  w.u8(c.msec);
  w.f32(c.angles.x);
  w.f32(c.angles.y);
  w.f32(c.angles.z);
  w.u16(static_cast<uint16_t>(c.forward));
  w.u16(static_cast<uint16_t>(c.side));
  w.u16(static_cast<uint16_t>(c.up));
  w.u8(c.buttons);
  w.u8(c.impulse);
}

bool same_cmd(const sh::MoveCmd& a, const sh::MoveCmd& b) {
  return a.msec == b.msec && a.angles.x == b.angles.x && a.angles.y == b.angles.y &&
         a.angles.z == b.angles.z && a.forward == b.forward && a.side == b.side && a.up == b.up &&
         a.buttons == b.buttons && a.impulse == b.impulse;
}

// A packet whose two older commands are its slot's previous packet's two newer ones (every packet
// of a client that lost none) is written as its newest command alone.
bool shifted(const sh::Packet& p, const sh::Packet& prev) {
  return same_cmd(p.cmds[0], prev.cmds[1]) && same_cmd(p.cmds[1], prev.cmds[2]);
}

sh::MoveCmd read_cmd(Reader& r) {
  sh::MoveCmd c;
  c.msec = r.u8();
  c.angles.x = r.f32();
  c.angles.y = r.f32();
  c.angles.z = r.f32();
  c.forward = static_cast<int16_t>(r.u16());
  c.side = static_cast<int16_t>(r.u16());
  c.up = static_cast<int16_t>(r.u16());
  c.buttons = r.u8();
  c.impulse = r.u8();
  return c;
}

}  // namespace

std::vector<uint8_t> write_input_log(const InputLog& log) {
  std::vector<uint8_t> out;
  out.reserve(64 + log.frames.size() * 11 + log.events.size() * 3 + log.packets.size() * 28);
  Writer w{out};
  for (const char c : kMagic) w.u8(static_cast<uint8_t>(c));
  w.u32(kInputLogVersion);
  w.u16(static_cast<uint16_t>(log.settings.maxclients));
  w.u16(static_cast<uint16_t>(log.settings.max_edicts));
  w.u32(log.settings.seed);
  const size_t name_len = log.settings.map.size() < 255 ? log.settings.map.size() : 255;
  w.u8(static_cast<uint8_t>(name_len));
  for (size_t i = 0; i < name_len; i++) w.u8(static_cast<uint8_t>(log.settings.map[i]));
  w.u32(static_cast<uint32_t>(log.frames.size()));
  sh::Packet prev[sh::kMaxClients];
  for (const LogFrame& f : log.frames) {
    w.u8(static_cast<uint8_t>(f.events));
    w.u16(static_cast<uint16_t>(f.packets));
    w.u64(f.hash);
    for (uint32_t i = 0; i < f.events; i++) {
      const SlotEvent& e = log.events[f.first_event + i];
      w.u8(static_cast<uint8_t>(e.kind));
      w.u8(e.slot);
      w.u8(e.flags);
    }
    for (uint32_t i = 0; i < f.packets; i++) {
      const sh::Packet& p = log.packets[f.first_packet + i];
      sh::Packet& last = prev[p.client];
      const bool shift = shifted(p, last);
      w.u8(static_cast<uint8_t>(p.client));
      w.u8(shift ? 1 : 0);
      w.u32(p.sequence);
      if (shift)
        write_cmd(w, p.cmds[2]);
      else
        for (const sh::MoveCmd& c : p.cmds) write_cmd(w, c);
      last = p;
    }
  }
  return out;
}

bool read_input_log(const std::vector<uint8_t>& bytes, InputLog& log, std::string& error) {
  log = InputLog{};
  Reader r{bytes};
  char magic[4] = {};
  for (char& c : magic) c = static_cast<char>(r.u8());
  if (r.short_read || std::memcmp(magic, kMagic, 4) != 0) {
    error =
        "not a server input log (no GESL magic at byte 0); next: pass a file written by "
        "shooters_edge_server --log=<path>";
    return false;
  }
  const uint32_t version = r.u32();
  if (version != kInputLogVersion) {
    error = "input log version " + std::to_string(version) + ", this build reads " +
            std::to_string(kInputLogVersion) + "; next: record the run again with this build";
    return false;
  }
  log.settings.maxclients = r.u16();
  log.settings.max_edicts = r.u16();
  log.settings.seed = r.u32();
  const uint8_t name_len = r.u8();
  if (r.need(name_len)) {
    log.settings.map.assign(reinterpret_cast<const char*>(bytes.data() + r.at), name_len);
    r.at += name_len;
  }
  if (log.settings.maxclients < 1 || log.settings.maxclients > sh::kMaxClients) {
    error = "input log names maxclients " + std::to_string(log.settings.maxclients) + " (1.." +
            std::to_string(sh::kMaxClients) + " allowed); the file is damaged";
    return false;
  }
  const uint32_t frames = r.u32();
  sh::Packet prev[sh::kMaxClients];
  for (uint32_t f = 0; f < frames && !r.short_read; f++) {
    LogFrame lf;
    lf.events = r.u8();
    lf.packets = r.u16();
    lf.hash = r.u64();
    lf.first_event = static_cast<uint32_t>(log.events.size());
    lf.first_packet = static_cast<uint32_t>(log.packets.size());
    for (uint32_t i = 0; i < lf.events && !r.short_read; i++) {
      SlotEvent e;
      e.kind = static_cast<SlotEventKind>(r.u8());
      e.slot = r.u8();
      e.flags = r.u8();
      if (e.slot >= log.settings.maxclients ||
          (e.kind != SlotEventKind::connect && e.kind != SlotEventKind::disconnect)) {
        error = "input log frame " + std::to_string(f) + ": a slot event with slot " +
                std::to_string(e.slot) + " and kind " + std::to_string(static_cast<int>(e.kind)) +
                " is out of range; the file is damaged";
        return false;
      }
      log.events.push_back(e);
    }
    for (uint32_t i = 0; i < lf.packets && !r.short_read; i++) {
      sh::Packet p;
      p.client = r.u8();
      const uint8_t shift = r.u8();
      p.sequence = r.u32();
      if (p.client >= log.settings.maxclients || shift > 1) {
        error = "input log frame " + std::to_string(f) + ": a packet for slot " +
                std::to_string(p.client) + " (flags " + std::to_string(shift) +
                ") past maxclients or with unknown flags; the file is damaged";
        return false;
      }
      sh::Packet& last = prev[p.client];
      if (shift != 0) {
        p.cmds[0] = last.cmds[1];
        p.cmds[1] = last.cmds[2];
        p.cmds[2] = read_cmd(r);
      } else {
        for (sh::MoveCmd& c : p.cmds) c = read_cmd(r);
      }
      last = p;
      log.packets.push_back(p);
    }
    log.frames.push_back(lf);
  }
  if (r.short_read) {
    error = "input log ends early at byte " + std::to_string(bytes.size()) + " (" +
            std::to_string(log.frames.size()) + " of " + std::to_string(frames) +
            " frames read); the file is cut short";
    return false;
  }
  return true;
}

}  // namespace shooters_edge::server
