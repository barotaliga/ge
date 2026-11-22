// The definitions behind q2_mvd_server.hpp.
//
// ge:summary Quake II's multiview recorder: the recording file, the TCP viewers, the words
// ge:keywords mvd2 mvdrecord gtv relay-stream tcp hello stream-data zombie overflowed
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_mvd_server_test.cpp
#include "shooters_edge/net/q2_mvd_server.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace shooters_edge::net {

namespace gn = ge::net;
using Stage = Q2MvdViewer::Stage;

namespace {

constexpr size_t kFrameMost = 32767;  // a stream-data payload: the length word counts the id too

void say(const Q2Mvd& m, const std::string& line) {
  if (m.log) m.log(line);
}

int64_t clock_now(const Q2Mvd& m) { return m.unix_time ? m.unix_time() : 0; }

std::string addr_text(const gn::Address& a) { return gn::to_string(a); }

size_t unsent(const Q2MvdViewer& v) { return v.stream.out.size() - v.stream.out_head; }

// One message (its length word, its id, its payload) to `v`, deflated when the viewer took it.
bool send(Q2Mvd& m, Q2MvdViewer& v, uint8_t id, const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> msg;
  msg.reserve(payload.size() + 3);
  const size_t len = payload.size() + 1;
  msg.push_back(static_cast<uint8_t>(len & 0xFF));
  msg.push_back(static_cast<uint8_t>(len >> 8));
  msg.push_back(id);
  msg.insert(msg.end(), payload.begin(), payload.end());
  std::vector<uint8_t> wire;
  if (v.z.started) {
    ge::core::deflate_sync(v.z, msg, wire);
  } else {
    wire = std::move(msg);
  }
  const size_t room = size_t(std::clamp(m.cfg.bufsize, 1, 4)) * kGtvServerMost;
  if (unsent(v) + wire.size() > room) return false;
  return gn::tcp_send_bytes(v.stream, wire.data(), wire.size());
}

void drop(Q2Mvd& m, Q2MvdViewer& v, const std::string& reason, double now, bool quiet = false) {
  if (v.stage == Stage::zombie) return;
  if (v.magic && !quiet)
    say(m, "TCP client " + v.name + "[" + addr_text(v.stream.peer) + "] dropped: " + reason);
  if (v.z.started) {
    std::vector<uint8_t> tail;
    ge::core::deflate_finish(v.z, tail);
    gn::tcp_send_bytes(v.stream, tail.data(), tail.size());
  }
  v.stage = Stage::zombie;
  v.dropped_at = now;
}

void put_long(std::vector<uint8_t>& b, uint32_t v) {
  for (int k = 0; k < 4; k++) b.push_back(static_cast<uint8_t>((v >> (8 * k)) & 0xFF));
}

// The recording's bytes appended (gzipped when it is).
void file_write(Q2Mvd& m, const std::vector<uint8_t>& bytes) {
  if (m.file == nullptr) return;
  std::vector<uint8_t> out;
  if (m.gzip)
    ge::core::deflate_sync(m.gz, bytes, out);
  else
    out = bytes;
  if (!out.empty() && std::fwrite(out.data(), 1, out.size(), m.file) != out.size()) {
    say(m, "Couldn't write local MVD: the disk refused the write");
    std::fclose(m.file);
    m.file = nullptr;
    return;
  }
  m.written += static_cast<int64_t>(bytes.size());
}

void file_close(Q2Mvd& m) {
  if (m.file == nullptr) return;
  std::vector<uint8_t> end;
  sv::mvd2_write_end(end);
  file_write(m, end);
  if (m.file != nullptr && m.gzip) {
    std::vector<uint8_t> tail;
    ge::core::deflate_finish(m.gz, tail);
    std::fwrite(tail.data(), 1, tail.size(), m.file);
  }
  if (m.file != nullptr) std::fclose(m.file);
  m.file = nullptr;
}

std::vector<uint8_t> with_length(const std::vector<uint8_t>& block) {
  std::vector<uint8_t> b;
  b.reserve(block.size() + 2);
  b.push_back(static_cast<uint8_t>(block.size() & 0xFF));
  b.push_back(static_cast<uint8_t>(block.size() >> 8));
  b.insert(b.end(), block.begin(), block.end());
  return b;
}

// Everything the recorder writes ends here: the streaming viewers, then the recording.
void emit(Q2Mvd& m, const std::vector<uint8_t>& block, double now) {
  for (Q2MvdViewer& v : m.viewers)
    if (v.stage == Stage::sending && !send(m, v, kGtsStreamData, block))
      drop(m, v, "overflowed", now);
  file_write(m, with_length(block));
}

void switch_on(Q2Mvd& m, double now) {
  if (m.on) return;
  m.on = true;
  m.active = false;
  m.unused_since = now;
  m.idle_since = now;
}

bool someone_watches(const Q2Mvd& m) {
  if (m.file != nullptr) return true;
  return std::any_of(m.viewers.begin(), m.viewers.end(),
                     [](const Q2MvdViewer& v) { return v.stage == Stage::sending; });
}

void stop_recording(Q2Mvd& m, const std::string& line) {
  file_close(m);
  say(m, line);
}

// The game state as it stands: every settings string and the last frame whole, so the blocks
// after it delta from what it holds.
std::vector<uint8_t> gamestate(const Q2Mvd& m) {
  sv::Mvd2Level level = m.level;
  level.config_strings = m.live.sent;
  level.base = m.last;
  std::vector<uint8_t> block;
  sv::mvd2_write_level(block, level);
  return std::vector<uint8_t>(block.begin() + 2, block.end());
}

// A recording started: the magic, and the game state when the recorder is not suspended.
void start_file(Q2Mvd& m) {
  std::vector<uint8_t> magic;
  sv::mvd2_write_magic(magic);
  file_write(m, magic);
  if (m.active && m.have_level) file_write(m, with_length(gamestate(m)));
}

// A level's start (or the recorder resumed): the game state to everyone watching.
void send_level(Q2Mvd& m, const sv::RulesCommon& c, const sv::Q2WireOptions& opt,
                const sv::Mvd2Capture& cap, double now) {
  sv::q2_mvd2_level(c, opt, cap, m.live, m.level);
  m.last = m.level.base;
  m.have_level = true;
  if (!m.on || !m.active) return;
  const std::vector<uint8_t> state = gamestate(m);
  if (state.size() > kFrameMost) {
    say(m, "Fatal MVD server error: gamestate overflowed");
    q2_mvd_shutdown(m, false);
    return;
  }
  emit(m, state, now);
}

void read_viewer(Q2Mvd& m, Q2MvdViewer& v, double now);

}  // namespace

bool q2_mvd_listen(Q2Mvd& m, const gn::Address& bind) {
  if (m.cfg.enable < 2 || m.listening) return m.listening;
  std::string error;
  if (!gn::tcp_listen(m.listener, bind, &error)) {
    say(m, "Error opening server TCP port.");
    m.cfg.enable = 1;
    return false;
  }
  m.listening = true;
  return true;
}

void q2_mvd_pump(Q2Mvd& m, double now) {
  m.now = now;
  if (m.listening) {
    gn::TcpStream s;
    while (gn::tcp_accept(m.listener, s)) {
      const std::string at = "[" + addr_text(s.peer) + "]";
      int same = 0;
      for (const Q2MvdViewer& v : m.viewers) {
        const bool v4 = v.stream.peer.family != gn::Family::ipv6;
        const bool match = v4 ? std::memcmp(v.stream.peer.bytes, s.peer.bytes, 4) == 0
                              : std::memcmp(v.stream.peer.bytes, s.peer.bytes, 6) == 0;
        if (v.stream.peer.family == s.peer.family && match && v.stage != Stage::zombie) same++;
      }
      const int slots = std::clamp(m.cfg.maxclients, 1, 256);
      if (m.cfg.iplimit > 0 && same >= m.cfg.iplimit) {
        say(m, "TCP client " + at + " rejected: too many connections");
        gn::tcp_close(s);
        continue;
      }
      if (static_cast<int>(m.viewers.size()) >= slots) {
        say(m, "TCP client " + at + " rejected: no free slots");
        gn::tcp_close(s);
        continue;
      }
      Q2MvdViewer v;
      v.stream = s;
      v.heard = now;
      m.viewers.push_back(std::move(v));
      s = gn::TcpStream{};
    }
  }
  for (Q2MvdViewer& v : m.viewers) {
    gn::tcp_pump(v.stream);
    if (v.stage == Stage::zombie) continue;
    if (v.stream.closed) {
      drop(m, v, "EOF from client", now);
      continue;
    }
    read_viewer(m, v, now);
    if (v.stage == Stage::zombie) continue;
    const double silent = now - v.heard;
    if (v.stage == Stage::assigned || v.stage == Stage::connected) {
      if (silent > std::min(m.cfg.ghost, m.cfg.timeout)) drop(m, v, "request timed out", now);
    } else if (silent > m.cfg.timeout) {
      drop(m, v, "connection timed out", now);
    }
  }
  // The dropped drain their last bytes, for `zombietime` at most.
  for (size_t i = m.viewers.size(); i-- > 0;) {
    Q2MvdViewer& v = m.viewers[i];
    if (v.stage != Stage::zombie) continue;
    gn::tcp_pump(v.stream);
    if (unsent(v) == 0 || v.stream.closed || now - v.dropped_at > m.cfg.zombie) {
      gn::tcp_close(v.stream);
      m.viewers.erase(m.viewers.begin() + static_cast<std::ptrdiff_t>(i));
    }
  }
}

namespace {

void read_viewer(Q2Mvd& m, Q2MvdViewer& v, double now) {
  gn::tcp_take_bytes(v.stream, v.in);
  size_t at = 0;
  if (!v.magic) {
    if (v.in.size() < 4) return;
    if (std::memcmp(v.in.data(), "MVD2", 4) != 0) {
      drop(m, v, "not a MVD/GTV stream", now);
      return;
    }
    v.magic = true;
    v.stage = Stage::connected;
    gn::tcp_send_bytes(v.stream, v.in.data(), 4);
    at = 4;
  }
  while (v.in.size() - at >= 2 && v.stage != Stage::zombie) {
    const size_t len = size_t(v.in[at]) | size_t(v.in[at + 1]) << 8;
    if (len == 0) {
      drop(m, v, "end of stream", now);
      break;
    }
    if (len > kGtvClientMost) {
      drop(m, v, "oversize message", now);
      break;
    }
    if (v.in.size() - at - 2 < len) break;
    const uint8_t* p = v.in.data() + at + 2;
    const uint8_t* end = p + len;
    at += 2 + len;
    v.heard = now;
    const uint8_t id = *p++;
    bool bad = false;
    const auto u16 = [&]() -> uint32_t {
      if (end - p < 2) {
        bad = true;
        return 0;
      }
      const uint32_t r = uint32_t(p[0]) | uint32_t(p[1]) << 8;
      p += 2;
      return r;
    };
    const auto u32 = [&]() -> uint32_t {
      const uint32_t lo = u16();
      return lo | u16() << 16;
    };
    const auto str = [&]() {
      std::string s;
      while (p < end && *p != 0) s.push_back(static_cast<char>(*p++));
      if (p >= end)
        bad = true;
      else
        p++;
      return s;
    };
    if (id == kGtvHello) {
      if (v.stage != Stage::connected) {
        drop(m, v, "duplicated hello message", now);
        break;
      }
      if (unsent(v) != 0) {
        drop(m, v, "send buffer not empty", now);
        break;
      }
      const uint32_t version = u16();
      if (!bad && version != kGtvProtocol) {
        send(m, v, kGtsBadRequest, {});
        drop(m, v, "bad protocol version", now);
        break;
      }
      uint32_t flags = u32();
      u32();
      std::string name = str();
      const std::string password = str();
      const std::string client_version = str();
      if (bad) {
        drop(m, v, "read past end of message", now);
        break;
      }
      v.name = name.substr(0, 15);
      v.version = client_version;
      bool allowed = false;
      if (ge::classic::addr_list_hit(m.hosts, v.stream.peer, clock_now(m)) != nullptr)
        allowed = true;
      else if (ge::classic::addr_list_hit(m.bans, v.stream.peer, clock_now(m)) != nullptr)
        allowed = false;
      else
        allowed = m.cfg.password.empty() || password == m.cfg.password;
      if (!allowed) {
        send(m, v, kGtsNoAccess, {});
        drop(m, v, "not authorized", now);
        break;
      }
      if (m.cfg.allow_stufftext >= 0) flags &= ~kGtfStringCmds;
      v.flags = flags;
      std::vector<uint8_t> hello;
      put_long(hello, flags);
      send(m, v, kGtsHello, hello);
      if ((flags & kGtfDeflate) != 0) {
        v.z = ge::core::DeflateStream{};
        v.z.wrap = ge::core::DeflateWrap::zlib;
        std::vector<uint8_t> header;
        ge::core::deflate_sync(v.z, {}, header);  // the zlib header and an empty flush
        gn::tcp_send_bytes(v.stream, header.data(), header.size());
      }
      v.stage = Stage::primed;
      say(m, "Accepted MVD client " + v.name + "[" + addr_text(v.stream.peer) + "]");
    } else if (id == kGtvPing) {
      if (v.stage == Stage::primed || v.stage == Stage::sending) send(m, v, kGtsPong, {});
    } else if (id == kGtvStreamStart) {
      if (v.stage != Stage::primed) {
        drop(m, v, "unexpected stream start message", now);
        break;
      }
      u16();  // the frames it would hold back: every message here is flushed whole
      if (bad) {
        drop(m, v, "read past end of message", now);
        break;
      }
      switch_on(m, now);
      v.stage = Stage::sending;
      send(m, v, kGtsStreamStart, {});
      const bool whole = m.active && m.have_level;
      const std::vector<uint8_t> state = whole ? gamestate(m) : std::vector<uint8_t>{};
      if (state.size() > kFrameMost) {
        drop(m, v, "gamestate overflowed", now);
        break;
      }
      if (!send(m, v, kGtsStreamData, state)) drop(m, v, "overflowed", now);
    } else if (id == kGtvStreamStop) {
      if (v.stage != Stage::sending) {
        drop(m, v, "unexpected stream stop message", now);
        break;
      }
      v.stage = Stage::primed;
      send(m, v, kGtsStreamStop, {});
    } else if (id == kGtvCmd) {
      if (v.stage == Stage::connected) {
        drop(m, v, "unexpected stringcmd message", now);
        break;
      }
      // Run as the dummy's own command only when the relay was let send them (the string-commands
      // flag, kept only while sv_mvd_allow_stufftext is below 0) and a dummy plays.
      std::string text(reinterpret_cast<const char*>(p), static_cast<size_t>(end - p));
      if (const size_t z = text.find('\0'); z != std::string::npos) text.resize(z);
      if ((v.flags & kGtfStringCmds) != 0 && m.dummy >= 0 && !text.empty())
        m.dummy_forwarded.push_back(text);
    } else {
      drop(m, v, "unknown command byte", now);
      break;
    }
  }
  v.in.erase(v.in.begin(), v.in.begin() + static_cast<std::ptrdiff_t>(std::min(at, v.in.size())));
}

}  // namespace

void q2_mvd_level(Q2Mvd& m, const sv::RulesCommon& c, const sv::Q2WireOptions& opt,
                  const sv::Mvd2Capture& cap, double now) {
  if (m.on && m.file != nullptr && m.have_level) {
    m.maps++;
    if (m.cfg.maxmaps > 0 && m.maps >= m.cfg.maxmaps)
      stop_recording(m, "Stopping MVD recording, maximum number of level changes reached.");
  }
  send_level(m, c, opt, cap, now);
}

void q2_mvd_frame(Q2Mvd& m, const sv::RulesCommon& c, const sv::Q2WireOptions& opt,
                  const sv::Mvd2Capture& cap, double now) {
  m.now = now;
  if (!m.on || !m.have_level) return;
  // Switched off after `sv_mvd_disconnect_time` with no recording and no streaming viewer.
  if (someone_watches(m)) {
    m.unused_since = now;
  } else if (m.cfg.disconnect_min > 0 && now - m.unused_since > m.cfg.disconnect_min * 60.0) {
    m.on = false;
    m.active = false;
    return;
  }
  bool playing = false;
  for (int i = 0; i < 32; i++)
    if (c.clients[i].active && !c.clients[i].spectator) playing = true;
  if (playing || m.cfg.suspend_min <= 0) m.idle_since = now;
  const bool idle = m.cfg.suspend_min > 0 && now - m.idle_since > m.cfg.suspend_min * 60.0;
  if (m.active && idle) {
    m.active = false;
    for (Q2MvdViewer& v : m.viewers)
      if (v.stage == Stage::sending && !send(m, v, kGtsStreamData, {}))
        drop(m, v, "overflowed", now);
    return;
  }
  if (!m.active) {
    if (idle) return;
    m.active = true;  // resumed: a fresh game state
    send_level(m, c, opt, cap, now);
    return;
  }
  sv::Mvd2Frame now_frame;
  sv::q2_mvd2_frame(c, opt, cap, m.live, now_frame);
  std::vector<uint8_t> block;
  sv::mvd2_write_frame(block, m.last, now_frame);
  m.last = std::move(now_frame);
  const std::vector<uint8_t> payload(block.begin() + 2, block.end());
  if (payload.size() > kFrameMost) {
    say(m, "Fatal MVD server error: frame overflowed");
    q2_mvd_shutdown(m, false);
    return;
  }
  emit(m, payload, now);
  if (m.file != nullptr) {
    m.frames++;
    if (m.cfg.maxsize_kb > 0 && m.written > m.cfg.maxsize_kb * 1000) {
      stop_recording(m, "Stopping MVD recording, maximum size reached.");
    } else if (m.cfg.maxtime_min > 0 && m.frames > m.cfg.maxtime_min * 600) {
      stop_recording(m, "Stopping MVD recording, maximum duration reached.");
    }
  }
}

void q2_mvd_shutdown(Q2Mvd& m, bool restart) {
  file_close(m);
  for (Q2MvdViewer& v : m.viewers) {
    if (v.stage == Stage::primed || v.stage == Stage::sending)
      send(m, v, restart ? kGtsReconnect : kGtsDisconnect, {});
    if (v.z.started && !v.z.finished) {
      std::vector<uint8_t> tail;
      ge::core::deflate_finish(v.z, tail);
      gn::tcp_send_bytes(v.stream, tail.data(), tail.size());
    }
    for (int k = 0; k < 64 && v.stream.out.size() > v.stream.out_head && !v.stream.closed; k++)
      gn::tcp_pump(v.stream);
    gn::tcp_close(v.stream);
  }
  m.viewers.clear();
  m.on = false;
  m.active = false;
  m.have_level = false;
}

bool q2_mvd_record(Q2Mvd& m, std::string name, bool gzip, std::string& path, std::string& out) {
  if (m.file != nullptr) {
    out = "Already recording a local MVD.\n";
    return false;
  }
  if (name.find("..") != std::string::npos || name.find('/') != std::string::npos ||
      name.find('\\') != std::string::npos) {
    out = "Couldn't open " + name + ": the name may not leave the demos folder\n";
    return false;
  }
  const std::string ext = ".mvd2";
  if (name.size() < ext.size() || name.compare(name.size() - ext.size(), ext.size(), ext) != 0)
    name += ext;
  if (gzip) name += ".gz";
  const std::filesystem::path dir = m.cfg.demos.empty() ? "demos" : m.cfg.demos;
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  path = (dir / name).string();
  m.file = std::fopen(path.c_str(), "wb");
  if (m.file == nullptr) {
    out = "Couldn't open " + path + ": the file could not be made\n";
    return false;
  }
  m.path = path;
  m.gzip = gzip;
  m.gz = ge::core::DeflateStream{};
  m.gz.wrap = ge::core::DeflateWrap::gzip;
  m.written = 0;
  m.frames = 0;
  m.maps = 0;
  switch_on(m, m.now);
  start_file(m);
  return true;
}

bool q2_mvd_stop(Q2Mvd& m) {
  if (m.file == nullptr) return false;
  file_close(m);
  return true;
}

void q2_mvd_switch_on(Q2Mvd& m, double now) { switch_on(m, now); }

void q2_mvd_dummy_refused(Q2Mvd& m) {
  file_close(m);
  for (Q2MvdViewer& v : m.viewers)
    if (v.stage == Stage::sending) {
      send(m, v, kGtsError, {});
      drop(m, v, "couldn't create MVD dummy", m.now);
    }
  m.on = false;
  m.active = false;
}

bool q2_mvd_command(Q2Mvd& m, const std::vector<std::string>& w, const std::string& line,
                    bool in_game, std::string& out) {
  out.clear();
  if (w.empty()) return false;
  const std::string& cmd = w[0];
  const std::string args = line.size() > cmd.size() ? line.substr(line.find(cmd) + cmd.size()) : "";
  if (cmd == "mvdrecord") {
    bool gzip = false, help = false;
    std::string name;
    for (size_t i = 1; i < w.size(); i++) {
      const std::string& a = w[i];
      if (a == "-h" || a == "--help") {
        help = true;
      } else if (a == "-z" || a == "--compress") {
        gzip = true;
      } else if (a == "-hz" || a == "-zh") {
        help = true;
        gzip = true;
      } else if (name.empty()) {
        name = a;
      }
    }
    if (help) {
      out =
          "Usage: mvdrecord [-hz] <filename>\nBegin local MVD recording.\n\nAvailable options:\n"
          "-h | --help     | display this message\n"
          "-z | --compress | compress file with gzip\n\n";
      return true;
    }
    if (!in_game) {
      out = "No server running.\n";
      return true;
    }
    if (name.empty()) {
      out = "Missing filename argument.\nTry 'mvdrecord --help' for more information.\n";
      return true;
    }
    if (m.cfg.enable <= 0) {
      out = "MVD recording is disabled on this server.\n";
      return true;
    }
    std::string path;
    if (!q2_mvd_record(m, name, gzip, path, out)) return true;
    out = "Recording local MVD to " + path + "\n";
    return true;
  }
  if (cmd == "mvdstop") {
    out = q2_mvd_stop(m) ? "Stopped local MVD recording.\n" : "Not recording a local MVD.\n";
    return true;
  }
  if (cmd == "mvdstuff") {
    if (m.dummy < 0) {
      out = "Can't '" + cmd + "', dummy MVD client is not active\n";
      return true;
    }
    std::string text = args;
    while (!text.empty() && text.front() == ' ') text.erase(0, 1);
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
      text = text.substr(1, text.size() - 2);
    m.dummy_cmds += text + "\n";
    return true;
  }
  if (cmd == "addgtvhost" || cmd == "addgtvban") {
    ge::classic::addr_list_add(cmd == "addgtvhost" ? m.hosts : m.bans, cmd, args, out);
    return true;
  }
  if (cmd == "delgtvhost" || cmd == "delgtvban") {
    ge::classic::addr_list_del(cmd == "delgtvhost" ? m.hosts : m.bans, cmd, args, out);
    return true;
  }
  if (cmd == "listgtvhosts" || cmd == "listgtvbans") {
    out = ge::classic::addr_list_text(cmd == "listgtvhosts" ? m.hosts : m.bans);
    return true;
  }
  return false;
}

std::string q2_mvd_status(const Q2Mvd& m, bool versions, double now) {
  if (m.viewers.empty()) return "No TCP clients.\n";
  const auto pad = [](std::string s, size_t n) {
    if (s.size() < n) s.append(n - s.size(), ' ');
    return s;
  };
  std::string t;
  if (versions) {
    t = "num name             version\n--- ---------------- "
        "----------------------------------------\n";
    int i = 0;
    for (const Q2MvdViewer& v : m.viewers)
      t += pad(std::to_string(i++), 3) + " " + pad(v.name, 16) + " " + v.version.substr(0, 40) +
           "\n";
    return t;
  }
  t = "num name             buf lastmsg address               state\n"
      "--- ---------------- --- ------- --------------------- -----\n";
  int i = 0;
  for (const Q2MvdViewer& v : m.viewers) {
    const char* state = v.stage == Stage::zombie      ? "ZMBI"
                        : v.stage == Stage::assigned  ? "ASGN"
                        : v.stage == Stage::connected ? "CNCT"
                        : v.stage == Stage::primed    ? "PRIM"
                                                      : "SEND";
    t += pad(std::to_string(i++), 3) + " " + pad(v.name, 16) + " " +
         pad(std::to_string(unsent(v)), 3) + " " +
         pad(std::to_string(static_cast<int64_t>((now - v.heard) * 1000)), 7) + " " +
         pad(addr_text(v.stream.peer), 21) + " " + state + "\n";
  }
  return t;
}

}  // namespace shooters_edge::net
