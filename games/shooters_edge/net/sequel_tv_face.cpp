// Our TV node's sequel face (sequel_tv_face.hpp): the source's blocks read for what a viewer needs
// to start, viewers' challenges and connects, the signon, the blocks out on the reliable channel.
//
// ge:summary The sequel face's steps: blocks read and kept, viewers connected, signon, blocks out
// ge:keywords tv-node sequel-face sequel_tv_frame signon whole-frame stufftext reliable viewers
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_tv_face_test.cpp
#include "shooters_edge/net/sequel_tv_face.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/sequel_net.hpp"

namespace shooters_edge::net {

namespace {

constexpr uint8_t kSvcDisconnect = 7, kSvcReconnect = 8, kSvcStuffText = 11, kSvcServerData = 12,
                  kSvcConfigString = 13, kSvcBaseline = 14, kSvcFrame = 20;
constexpr size_t kUnit = 1200;  // the most bytes one reliable message carries
constexpr size_t kLogKept = 256;
constexpr int kStringCmdsPerPacket = 7;

void note(SequelTvFace& f, std::string line) {
  if (f.log.size() < kLogKept) f.log.push_back(std::move(line));
}

uint64_t host_key(gn::Address a) {
  a.port = 0;
  return gn::address_hash(a);
}

bool same_host(gn::Address a, gn::Address b) {
  a.port = b.port = 0;
  return a == b;
}

int32_t i32_at(std::span<const uint8_t> b, size_t at) {
  uint32_t v = 0;
  if (at + 4 <= b.size()) std::memcpy(&v, b.data() + at, 4);  // little-endian
  return static_cast<int32_t>(v);
}

void send_to(SequelTvFace& f, const gn::Address& to, const uint8_t* data, size_t size) {
  gn::transport_send(*f.transport, to, data, size, static_cast<int64_t>(f.now * 1e6));
  f.bytes_out += static_cast<int64_t>(size);
}

void send_oob(SequelTvFace& f, const gn::Address& to, const std::string& text) {
  const std::vector<uint8_t> b = sequel_oob(text);
  send_to(f, to, b.data(), b.size());
}

void refuse(SequelTvFace& f, const gn::Address& to, const std::string& why) {
  f.refused++;
  send_oob(f, to, "print\n" + why + "\n");
}

// `msg` onto the viewer's queue, joined to the last message while the two fit one unit.
void queue(SequelViewer& v, std::span<const uint8_t> msg) {
  if (msg.empty()) return;
  if (!v.queue.empty() && v.queue.back().size() + msg.size() <= kUnit) {
    v.queue.back().insert(v.queue.back().end(), msg.begin(), msg.end());
    return;
  }
  v.queue.emplace_back(msg.begin(), msg.end());
}

void transmit(SequelTvFace& f, SequelViewer& v) {
  const bool may = v.stage == SequelViewerStage::active || v.go > 0;
  if (channel_reliable_backlog(v.ch) == 0 && !v.queue.empty() && may) {
    if (v.stage != SequelViewerStage::active) v.go--;
    const std::vector<uint8_t>& m = v.queue.front();
    channel_reliable(v.ch, m.data(), m.size());
    v.queue.pop_front();
  }
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(v.ch, nullptr, 0, f.now, out);
  send_to(f, v.address, out, n);
  v.sent = f.now;
}

void drop(SequelTvFace& f, SequelViewer& v, const std::string& why) {
  if (v.stage == SequelViewerStage::free) return;
  note(f, v.name + " stopped watching: " + why);
  f.dropped++;
  const uint8_t bye[] = {kSvcDisconnect};
  v.queue.clear();
  channel_reliable(v.ch, bye, sizeof bye);
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(v.ch, nullptr, 0, f.now, out);
  send_to(f, v.address, out, n);
  v = SequelViewer{};
}

// The signon as the source stands now, then the prompt for `begin`.
// The signon as the source stands now, in units: each but the last ends with a prompt the client
// echoes back (`cmd configstrings <count> <n>`), and the face sends the next unit on that echo,
// since a client in its signon sends a packet only when it has something to say. The last unit
// ends with `precache`, which the client answers with `begin`.
void send_signon(SequelTvFace& f, SequelViewer& v) {
  v.signed_on = true;
  v.queue.clear();
  v.go = 1;
  const std::string count = std::to_string(f.count);
  const size_t room = kUnit - 48;  // a prompt's room left in every unit
  std::vector<uint8_t> unit;
  auto close = [&](const std::string& line) {
    unit.push_back(kSvcStuffText);
    unit.insert(unit.end(), line.begin(), line.end());
    unit.push_back(0);
    v.queue.push_back(std::move(unit));
    unit.clear();
  };
  auto add = [&](const std::vector<uint8_t>& msg) {
    if (!unit.empty() && unit.size() + msg.size() > room)
      close("cmd configstrings " + count + " " + std::to_string(v.queue.size() + 1) + "\n");
    unit.insert(unit.end(), msg.begin(), msg.end());
  };
  add(f.server_data);
  for (const auto& [index, msg] : f.strings) add(msg);
  for (const auto& msg : f.baselines) add(msg);
  close("precache " + count + "\n");
}

std::string status_text(const SequelTvFace& f) {
  std::string info;
  info_set(info, "hostname", f.cfg.name);
  info_set(info, "mapname", f.map);
  info_set(info, "maxclients", std::to_string(f.cfg.max_viewers));
  info_set(info, "protocol", std::to_string(kSequelNetProtocol));
  std::string t = info + "\n";
  for (const SequelViewer& v : f.viewers)
    if (v.stage != SequelViewerStage::free) t += "0 0 \"" + v.name + "\"\n";
  return t;
}

void connect(SequelTvFace& f, const gn::Address& from, const std::vector<std::string>& w) {
  if (w.size() < 5) return;
  if (std::atoi(w[1].c_str()) != kSequelNetProtocol)
    return refuse(f, from, "Server is protocol version 34.");
  const auto qport = static_cast<uint16_t>(std::atoi(w[2].c_str()));
  const auto at = f.challenges.find(host_key(from));
  if (at == f.challenges.end()) return refuse(f, from, "No challenge for address.");
  if (at->second != std::atoi(w[3].c_str())) return refuse(f, from, "Bad challenge.");
  const std::string& userinfo = w[4];
  for (const SequelViewer& v : f.viewers)
    if (v.stage != SequelViewerStage::free && same_host(v.address, from) &&
        (v.qport == qport || v.address.port == from.port))
      return;  // already in: a repeat
  if (!f.cfg.password.empty() && info_get(userinfo, "password") != f.cfg.password)
    return refuse(f, from, "Password required or incorrect.\nConnection refused.");
  SequelViewer* slot = nullptr;
  for (int i = 0; i < f.cfg.max_viewers && i < static_cast<int>(f.viewers.size()); i++)
    if (f.viewers[static_cast<size_t>(i)].stage == SequelViewerStage::free) {
      slot = &f.viewers[static_cast<size_t>(i)];
      break;
    }
  if (slot == nullptr) return refuse(f, from, "This broadcast is full.");
  SequelViewer& v = *slot;
  v = SequelViewer{};
  v.stage = SequelViewerStage::signon;
  v.address = from;
  v.qport = qport;
  v.name = info_get(userinfo, "name");
  if (v.name.empty()) v.name = "unnamed";
  v.heard = f.now;
  channel_setup(v.ch, false, qport, f.now);
  channel_set_rate(v.ch, 0);
  send_oob(f, from, "client_connect");
  note(f, v.name + " is watching");
}

void read_oob(SequelTvFace& f, const gn::Address& from, const uint8_t* data, size_t size) {
  const std::vector<std::string> w = sequel_words(sequel_oob_text(data, size));
  if (w.empty()) return;
  if (w[0] == "getchallenge") {
    auto at = f.challenges.find(host_key(from));
    if (at == f.challenges.end()) {
      f.next_challenge = f.next_challenge * 1103515245u + 12345u;
      at = f.challenges
               .emplace(host_key(from), static_cast<int32_t>((f.next_challenge >> 16) & 0x7FFFu))
               .first;
    }
    send_oob(f, from, "challenge " + std::to_string(at->second) + " p=34");
  } else if (w[0] == "connect") {
    connect(f, from, w);
  } else if (w[0] == "info") {
    char line[96];
    std::snprintf(line, sizeof line, "%16s %8s %2d/%2d\n", f.cfg.name.c_str(), f.map.c_str(),
                  sequel_tv_viewers(f), f.cfg.max_viewers);
    send_oob(f, from, std::string("info\n") + line);
  } else if (w[0] == "status") {
    send_oob(f, from, "print\n" + status_text(f));
  } else if (w[0] == "ping") {
    send_oob(f, from, "ack");
  }
}

void string_command(SequelTvFace& f, SequelViewer& v, const std::string& line) {
  const std::vector<std::string> w = sequel_words(line);
  if (w.empty()) return;
  if (w[0] == "new") {
    v.stage = SequelViewerStage::signon;
    v.asked = true;
    if (!f.since_whole.empty()) send_signon(f, v);  // else once the level's first whole frame
  } else if (w[0] == "configstrings" || w[0] == "baselines") {
    // A prompt echoed: the next unit of the signon (a stale count is the last level's).
    if (v.stage == SequelViewerStage::signon && v.signed_on && w.size() > 1 &&
        std::atoi(w[1].c_str()) == f.count)
      v.go = 1;
  } else if (w[0] == "begin") {
    if (v.stage != SequelViewerStage::signon || !v.signed_on) return;
    if (w.size() > 1 && std::atoi(w[1].c_str()) != f.count) {
      send_signon(f, v);  // from another level: the signon again
      return;
    }
    v.stage = SequelViewerStage::active;
    for (const auto& b : f.since_whole) queue(v, b);
    v.blocks += static_cast<int64_t>(f.since_whole.size());
  } else if (w[0] == "disconnect") {
    drop(f, v, "left");
  }
}

void read_messages(SequelTvFace& f, SequelViewer& v, const uint8_t* data, size_t size) {
  gn::Reader r = gn::make_reader(data, size);
  int commands = 0;
  while (gn::remaining(r) > 0 && v.stage != SequelViewerStage::free) {
    const int id = read_byte(r);
    if (id == kClcNop) continue;
    if (id == kClcMove) return;  // a viewer's moves move nothing: the rest of the packet is too
    if (id == kClcUserInfo) {
      read_string(r);
    } else if (id == kClcStringCmd) {
      const std::string line = read_string(r);
      if (++commands <= kStringCmdsPerPacket) string_command(f, v, line);
    } else {
      return drop(f, v, "a message " + std::to_string(id) + " it does not know");
    }
    if (r.bad) return drop(f, v, "a message cut short");
  }
}

// A new level: everything kept starts over, and every viewer is told the level changed.
void new_level(SequelTvFace& f) {
  f.strings.clear();
  f.baselines.clear();
  f.since_whole.clear();
  f.levels++;
  for (SequelViewer& v : f.viewers) {
    if (v.stage == SequelViewerStage::free) continue;
    v.queue.clear();
    // As a server changing levels: the client keeps its connection and asks for the signon again
    // with `new`; it may be sent this one message before it does.
    static constexpr char kAgain[] = "\x0b" "changing\n\0" "\x0b" "reconnect\n";
    queue(v, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(kAgain), sizeof kAgain));
    v.stage = SequelViewerStage::signon;
    v.asked = false;
    v.signed_on = false;
    v.go = 1;
  }
}

}  // namespace

int sequel_tv_viewers(const SequelTvFace& f) {
  int n = 0;
  for (const SequelViewer& v : f.viewers) n += v.stage != SequelViewerStage::free ? 1 : 0;
  return n;
}

void sequel_tv_start(SequelTvFace& f, gn::Transport& t, const SequelTvConfig& cfg) {
  f.cfg = cfg;
  f.transport = &t;
  f.reader = client::Demo{};
  client::sequel_stream_begin(f.reader);
  f.reader.spans = &f.spans;
}

void sequel_tv_block(SequelTvFace& f, std::span<const uint8_t> block) {
  if (block.empty()) return;
  // The reader walks the block as a recording of one block.
  const size_t from = f.reader.bytes.size();
  const auto len = static_cast<uint32_t>(block.size());
  for (int k = 0; k < 4; k++) f.reader.bytes.push_back(static_cast<uint8_t>(len >> (8 * k)));
  f.reader.bytes.insert(f.reader.bytes.end(), block.begin(), block.end());
  f.reader.pos = from;
  std::string error;
  const client::DemoRead got = client::next_sequel(f.reader, error);
  f.reader.bytes.clear();  // one block at a time: nothing kept but the reader's state
  f.reader.pos = 0;
  f.reader.stuffed.clear();
  f.reader.state.prints.clear();
  f.reader.state.effects.clear();
  if (got == client::DemoRead::error) {
    f.error = error;
    note(f, "a block that did not read: " + error);
    return;
  }
  f.blocks++;
  std::vector<uint8_t> kept;  // the block without its stuffed lines
  bool whole = false, framed = false;
  for (const client::DemoSpan& s : f.spans) {
    const std::span<const uint8_t> msg = block.subspan(s.at, s.end - s.at);
    if (s.id == kSvcStuffText || s.id == kSvcReconnect) continue;
    if (s.id == kSvcServerData) {
      if (!f.server_data.empty()) new_level(f);
      f.server_data.assign(msg.begin(), msg.end());
      if (f.server_data.size() > 9) f.server_data[9] = 1;  // attract loop: a recording played
      f.count = i32_at(msg, 5);
      continue;
    }
    if (s.id == kSvcConfigString) {
      const int index = msg.size() >= 3 ? (msg[1] | msg[2] << 8) : -1;
      f.strings[index].assign(msg.begin(), msg.end());
      if (index == 33 && msg.size() > 3) {  // the map's model, "maps/<name>.bsp"
        std::string m(reinterpret_cast<const char*>(msg.data() + 3));
        if (m.rfind("maps/", 0) == 0) m = m.substr(5);
        if (m.size() > 4) m.resize(m.size() - 4);
        f.map = m;
      }
    } else if (s.id == kSvcBaseline) {
      f.baselines.emplace_back(msg.begin(), msg.end());
      continue;
    } else if (s.id == kSvcFrame) {
      framed = true;
      whole = i32_at(msg, 5) <= 0;
    }
    kept.insert(kept.end(), msg.begin(), msg.end());
  }
  if (!framed) {
    // Signon or reliable messages alone: viewers in the game get what they carry.
    if (!f.since_whole.empty()) f.since_whole.push_back(kept);
  } else {
    if (whole) {
      f.since_whole.clear();
      f.whole_frames++;
    }
    if (whole || !f.since_whole.empty()) f.since_whole.push_back(kept);
  }
  for (SequelViewer& v : f.viewers)  // asked before the level came: its signon now
    if (v.stage == SequelViewerStage::signon && v.asked && !v.signed_on && !f.since_whole.empty())
      send_signon(f, v);
  if (kept.empty()) return;
  for (SequelViewer& v : f.viewers) {
    if (v.stage != SequelViewerStage::active) continue;
    queue(v, kept);
    v.blocks++;
  }
}

void sequel_tv_frame(SequelTvFace& f, int64_t now_us) {
  f.now = static_cast<double>(now_us) / 1e6;
  gn::transport_pump(*f.transport, now_us);
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  for (;;) {
    const int n = gn::transport_receive(*f.transport, buf, sizeof buf, from, now_us);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    if (is_out_of_band(buf, size)) {
      read_oob(f, from, buf, size);
      continue;
    }
    uint16_t qport = 0;
    if (!peek_qport(buf, size, qport)) continue;
    SequelViewer* v = nullptr;
    for (SequelViewer& c : f.viewers)
      if (c.stage != SequelViewerStage::free && c.qport == qport && same_host(c.address, from)) {
        v = &c;
        break;
      }
    if (v == nullptr) continue;
    v->address = from;
    size_t at = 0;
    if (!channel_process(v->ch, buf, size, f.now, at)) continue;
    v->heard = f.now;
    read_messages(f, *v, buf + at, size - at);
  }
  for (SequelViewer& v : f.viewers) {
    if (v.stage == SequelViewerStage::free) continue;
    if (f.now - v.heard > f.cfg.timeout) {
      drop(f, v, "timed out");
      continue;
    }
    if (v.ch.overflow || v.queue.size() > f.cfg.most_behind) {
      drop(f, v, "fell behind");
      continue;
    }
    // A packet whenever the viewer has spoken since the last one (its ack moves the reliable
    // message on), and once a second besides.
    if (v.ch.last_received >= v.sent || f.now - v.sent >= 1.0) transmit(f, v);
  }
}

void sequel_tv_stop(SequelTvFace& f) {
  for (SequelViewer& v : f.viewers) drop(f, v, "the broadcast stopped");
}

}  // namespace shooters_edge::net
