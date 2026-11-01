// Recorded games read frame by frame (demo.hpp): the formats told apart, the records of each
// walked, the message reader both formats' messages are read with.
//
// ge:summary The demo file's records: the format detected, blocks and records walked, the reader
// ge:keywords open_demo next_frame demo-msg detect block record usercmd-size gzip track
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/inflate.hpp"

namespace shooters_edge::client {

namespace {

constexpr size_t kMostMessage = 1u << 20;  // a block or packet larger than this is damage, bytes

uint32_t u32_at(const std::vector<uint8_t>& b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 | static_cast<uint32_t>(b[at + 3]) << 24;
}

float f32_at(const std::vector<uint8_t>& b, size_t at) {
  const uint32_t u = u32_at(b, at);
  float f = 0;
  std::memcpy(&f, &u, sizeof f);
  return f;
}

// The single format's first line: the music track as an optional sign and digits, a newline.
size_t single_header(const std::vector<uint8_t>& b) {
  size_t i = 0;
  while (i < b.size() && i < 16 && b[i] == ' ') i++;
  if (i < b.size() && b[i] == '-') i++;
  const size_t digits = i;
  while (i < b.size() && i < 16 && b[i] >= '0' && b[i] <= '9') i++;
  if (i == digits || i >= b.size() || b[i] != '\n') return 0;
  return i + 1;
}

// A plausible client record at `at`: a finite time and a type the format has.
bool record_at(const std::vector<uint8_t>& b, size_t at) {
  if (at + 5 > b.size()) return at == b.size();  // the file's end is a record boundary too
  const float t = f32_at(b, at);
  return std::isfinite(t) && std::fabs(t) < 1e7f && b[at + 4] <= 2;
}

// The client records from `at` read with commands of `cmd_size` bytes: true when the next
// `count` records (or all up to the file's end) have plausible times, types and lengths.
bool chain_ok(const std::vector<uint8_t>& b, size_t at, int cmd_size, int count) {
  for (int i = 0; i < count && at < b.size(); i++) {
    if (!record_at(b, at) || at + 5 > b.size()) return at == b.size();
    const uint8_t type = b[at + 4];
    at += 5;
    if (type == 0) {
      at += static_cast<size_t>(cmd_size) + 12;
    } else if (type == 2) {
      at += 8;
    } else {
      if (at + 4 > b.size()) return true;  // cut short: the last record
      const uint32_t len = u32_at(b, at);
      if (len < 8 || len > kMostMessage) return false;
      at += 4 + len;
    }
  }
  return true;
}

std::string at_offset(size_t pos) { return " at byte " + std::to_string(pos); }

// The multiview format's tracked player: the one asked for, else the one already tracked while he
// plays, else the lowest slot playing; his stats become the view's.
void track_player(Demo& d) {
  DemoState& s = d.state;
  const auto playing = [&](int k) {
    return k >= 0 && static_cast<size_t>(k) < s.players.size() &&
           s.players[static_cast<size_t>(k)].present;
  };
  if (d.track >= 0) {
    s.player = d.track;
  } else if (!playing(s.player)) {
    for (int k = 0; k < static_cast<int>(s.players.size()); k++) {
      if (playing(k)) {
        s.player = k;
        break;
      }
    }
  }
  if (static_cast<size_t>(s.player) < d.player_stats.size()) {
    const auto& st = d.player_stats[static_cast<size_t>(s.player)];
    std::copy(st.begin(), st.end(), s.stats);
  }
  if (playing(s.player)) s.angles = s.players[static_cast<size_t>(s.player)].angles;
}

void finish_frame(Demo& d) {
  DemoState& s = d.state;
  s.frames++;
  if (s.format == DemoFormat::multiview) track_player(d);
  if (s.intermission) return;
  if (s.format == DemoFormat::single) {
    if (s.view_entity >= 0 && static_cast<size_t>(s.view_entity) < s.entities.size()) {
      s.eye = s.entities[static_cast<size_t>(s.view_entity)].origin;
      s.eye.z += s.view_height;
    }
  } else if (s.player >= 0 && static_cast<size_t>(s.player) < s.players.size() &&
             s.players[static_cast<size_t>(s.player)].present) {
    s.eye = s.players[static_cast<size_t>(s.player)].origin;
    s.eye.z += s.view_height;
  }
}

DemoRead next_single(Demo& d, std::string& error) {
  const std::vector<uint8_t>& b = d.bytes;
  if (d.pos + 16 > b.size()) return DemoRead::end;
  const int32_t len = static_cast<int32_t>(u32_at(b, d.pos));
  if (len < 0 || static_cast<size_t>(len) > kMostMessage) {
    error = "the block length " + std::to_string(len) + at_offset(d.pos) +
            " is not a block's; next: the file is damaged, or not a demo";
    return DemoRead::error;
  }
  if (d.pos + 16 + static_cast<size_t>(len) > b.size()) return DemoRead::end;  // cut short
  d.state.angles = {f32_at(b, d.pos + 4), f32_at(b, d.pos + 8), f32_at(b, d.pos + 12)};
  DemoMsg m{std::span<const uint8_t>(b).subspan(d.pos + 16, static_cast<size_t>(len))};
  const size_t block = d.pos;
  d.pos += 16 + static_cast<size_t>(len);
  if (!read_single_messages(d, m, error)) {
    error += " (in the block" + at_offset(block) + ")";
    return DemoRead::error;
  }
  finish_frame(d);
  return d.ended ? DemoRead::end : DemoRead::frame;
}

DemoRead next_client(Demo& d, std::string& error) {
  const std::vector<uint8_t>& b = d.bytes;
  for (;;) {
    if (d.pos + 5 > b.size()) return DemoRead::end;
    const size_t rec = d.pos;
    const float t = f32_at(b, rec);
    const uint8_t type = b[rec + 4];
    size_t at = rec + 5;
    if (type == 0) {
      if (d.cmd_size == 0) d.cmd_size = chain_ok(b, rec, 24, 16) ? 24 : 28;
      const size_t n = static_cast<size_t>(d.cmd_size) + 12;
      if (at + n > b.size()) return DemoRead::end;
      at += static_cast<size_t>(d.cmd_size);
      d.state.angles = {f32_at(b, at), f32_at(b, at + 4), f32_at(b, at + 8)};
      d.pos = at + 12;
    } else if (type == 2) {
      if (at + 8 > b.size()) return DemoRead::end;
      d.pos = at + 8;
    } else if (type == 1) {
      if (at + 4 > b.size()) return DemoRead::end;
      const int32_t len = static_cast<int32_t>(u32_at(b, at));
      if (len < 8 || static_cast<size_t>(len) > kMostMessage) {
        error = "the packet length " + std::to_string(len) + at_offset(rec) +
                " is not a packet's; next: the file is damaged, or not a client recording";
        return DemoRead::error;
      }
      at += 4;
      if (at + static_cast<size_t>(len) > b.size()) return DemoRead::end;  // cut short
      d.pos = at + static_cast<size_t>(len);
      const uint32_t seq = u32_at(b, at);
      if (seq == 0xFFFFFFFFu) continue;  // out of band: no messages for the game
      d.incoming = seq & 0x7FFFFFFFu;
      DemoMsg m{std::span<const uint8_t>(b).subspan(at + 8, static_cast<size_t>(len) - 8)};
      if (!read_client_messages(d, m, error)) {
        error += " (in the packet" + at_offset(rec) + ")";
        return DemoRead::error;
      }
      d.state.time = t;
      finish_frame(d);
      return d.ended ? DemoRead::end : DemoRead::frame;
    } else {
      error = "the record type " + std::to_string(type) + at_offset(rec) +
              " is none of command, packet or sequences; next: the file is damaged";
      return DemoRead::error;
    }
  }
}

}  // namespace

int DemoMsg::u8() {
  if (at + 1 > b.size()) {
    bad = true;
    at = b.size();
    return 0;
  }
  return b[at++];
}

int DemoMsg::i8() { return static_cast<int8_t>(static_cast<uint8_t>(u8())); }

int DemoMsg::u16() {
  if (at + 2 > b.size()) {
    bad = true;
    at = b.size();
    return 0;
  }
  const int v = b[at] | b[at + 1] << 8;
  at += 2;
  return v;
}

int DemoMsg::i16() { return static_cast<int16_t>(static_cast<uint16_t>(u16())); }

int32_t DemoMsg::i32() {
  if (at + 4 > b.size()) {
    bad = true;
    at = b.size();
    return 0;
  }
  const uint32_t v = static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
                     static_cast<uint32_t>(b[at + 2]) << 16 |
                     static_cast<uint32_t>(b[at + 3]) << 24;
  at += 4;
  return static_cast<int32_t>(v);
}

float DemoMsg::f32() {
  const uint32_t u = static_cast<uint32_t>(i32());
  float f = 0;
  std::memcpy(&f, &u, sizeof f);
  return f;
}

std::string DemoMsg::str() {
  std::string s;
  while (at < b.size()) {
    const uint8_t c = b[at++];
    if (c == 0) return s;
    if (c != 0xFF) s.push_back(static_cast<char>(c));
  }
  return s;  // a string the message's end cut: as read
}

void DemoMsg::skip(size_t n) {
  if (at + n > b.size()) {
    bad = true;
    at = b.size();
    return;
  }
  at += n;
}

std::string map_of_model(const std::string& model) {
  constexpr std::string_view kPre = "maps/", kExt = ".bsp";
  if (model.size() <= kPre.size() + kExt.size() || model.compare(0, kPre.size(), kPre) != 0 ||
      model.compare(model.size() - kExt.size(), kExt.size(), kExt) != 0)
    return "";
  return model.substr(kPre.size(), model.size() - kPre.size() - kExt.size());
}

std::vector<uint8_t> voice_header(DemoFormat base) {
  return {kVoiceMagic[0],
          kVoiceMagic[1],
          kVoiceMagic[2],
          kVoiceMagic[3],
          1,
          static_cast<uint8_t>(base == DemoFormat::multiview ? 2 : 1),
          kVoiceOpus,
          0};
}

std::string voice_extension(DemoFormat base) {
  return base == DemoFormat::multiview ? ".mvdv" : ".qwdv";
}

bool voice_name(const std::string& path) {
  if (path.size() < 5) return false;
  std::string ext = path.substr(path.size() - 5);
  for (char& c : ext) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  return ext == voice_extension(DemoFormat::client) ||
         ext == voice_extension(DemoFormat::multiview);
}

bool open_demo(std::vector<uint8_t> bytes, Demo& out, std::string& error) {
  out = Demo{};
  if (bytes.size() >= 2 && bytes[0] == 0x1f && bytes[1] == 0x8b) {
    auto unpacked = ge::core::inflate_gzip(bytes);
    if (!unpacked) {
      error =
          "the file is a gzip file that does not unpack (torn, or its checksum is off); next: "
          "download it again, or unpack it and name the recording inside";
      return false;
    }
    bytes = std::move(*unpacked);
  }
  int voice_base = 0;  // the voice header's base format (1 client, 2 multiview), 0: none
  if (bytes.size() >= kVoiceHeader && std::equal(kVoiceMagic, kVoiceMagic + 4, bytes.begin())) {
    if (bytes[4] != 1 || (bytes[5] != 1 && bytes[5] != 2) || bytes[6] != kVoiceOpus) {
      error = "the voice recording's header names version " + std::to_string(bytes[4]) +
              ", base format " + std::to_string(bytes[5]) + " and codec " +
              std::to_string(bytes[6]) +
              "; only version 1 of a client (1) or multiview (2) recording with Opus (2) is "
              "read; next: re-record it with this build";
      return false;
    }
    voice_base = bytes[5];
    bytes.erase(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(kVoiceHeader));
  }
  if (is_multiview(bytes)) {
    out.state.format = DemoFormat::multiview;
    out.state.view_height = 22;
    out.ring.resize(1);
    out.player_stats.resize(32);
  } else if (const size_t head = single_header(bytes); head != 0) {
    out.state.format = DemoFormat::single;
    out.pos = head;
  } else if (!bytes.empty() && record_at(bytes, 0) && bytes.size() >= 5) {
    out.state.format = DemoFormat::client;
    out.state.view_height = 22;
    out.ring.resize(64);
  } else {
    error =
        "the file is not a single-player demo (it opens with the music track: digits and a "
        "newline), a client recording (a time and a record type 0..2) or a multiview recording "
        "(a milliseconds byte, a record type and the server data); next: check the file, or "
        "name another";
    return false;
  }
  if (voice_base != 0) {
    const DemoFormat said = voice_base == 2 ? DemoFormat::multiview : DemoFormat::client;
    if (out.state.format != said) {
      error = std::string("the voice recording's header says a ") +
              (said == DemoFormat::multiview ? "multiview" : "client") +
              " recording follows, and the records are not one; next: the file is damaged";
      return false;
    }
    out.state.voice_format = true;
  }
  out.state.players.resize(32);
  out.bytes = std::move(bytes);
  return true;
}

bool read_voice(DemoMsg& m, DemoVoice& out) {
  out.sender = m.u8();
  out.gen = m.u8();
  out.seq = m.u8();
  const int n = m.u16();
  if (m.bad || m.at + static_cast<size_t>(n) > m.b.size()) {
    m.skip(static_cast<size_t>(n));
    return false;
  }
  out.data.assign(m.b.begin() + static_cast<std::ptrdiff_t>(m.at),
                  m.b.begin() + static_cast<std::ptrdiff_t>(m.at + static_cast<size_t>(n)));
  m.at += static_cast<size_t>(n);
  return true;
}

bool voice_frames(const DemoVoice& v, std::vector<std::span<const uint8_t>>& out) {
  out.clear();
  size_t at = 0;
  while (at < v.data.size()) {
    if (at + 2 > v.data.size()) return false;
    const size_t n = static_cast<size_t>(v.data[at] | v.data[at + 1] << 8);
    at += 2;
    if (at + n > v.data.size()) return false;
    out.emplace_back(v.data.data() + at, n);
    at += n;
  }
  return true;
}

DemoRead next_frame(Demo& d, std::string& error) {
  if (d.ended) return DemoRead::end;
  d.state.started.clear();
  d.state.stopped.clear();
  d.state.voice.clear();
  d.state.prints.clear();
  d.state.centre.clear();
  if (d.state.format == DemoFormat::client) return next_client(d, error);
  if (d.state.format == DemoFormat::multiview) {
    const DemoRead r = next_multiview(d, error);
    if (r == DemoRead::frame) finish_frame(d);
    return r;
  }
  // A single-player entity is seen only in the frames whose message updates it.
  for (DemoEntity& e : d.state.entities) e.present = false;
  return next_single(d, error);
}

}  // namespace shooters_edge::client
