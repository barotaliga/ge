// Our TV node's face for classic clients (classic_tv_udp.hpp): each stream's packets read for what
// a viewer needs and rewritten plain, viewers' challenges and connects, the signon in pages, the
// frames and the reliable messages out, a viewer's own player picked with its track keys.
//
// ge:summary The classic clients' face's steps: packets read plain, viewers, signon, frames out
// ge:keywords tv-node classic-face udp_tv_frame signon prespawn ptrack full-entity-list plain
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_udp_test.cpp
#include "shooters_edge/net/classic_tv_udp.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "progs/text.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_svc.hpp"
#include "shooters_edge/net/classic_wire.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;

namespace {

constexpr size_t kLogKept = 256;
constexpr size_t kPage = kMaxMsgLen / 2;  // a signon page's most
constexpr size_t kUnit = 700;             // a reliable message's most in the game: room for a frame
constexpr int kStringCmdsPerPacket = 7;
constexpr int kMostEntity = 511, kMostModel = 255;
const char* const kLeftKeys[] = {"*z_ext", "*ge_ext"};  // extension keys a viewer never agreed

void note(ClassicTvUdp& f, std::string line) {
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

WireCtx plain_ctx() { return WireCtx{}; }

void send_to(ClassicTvUdp& f, const gn::Address& to, const uint8_t* data, size_t size) {
  gn::transport_send(*f.transport, to, data, size, static_cast<int64_t>(f.now * 1e6));
  f.bytes_out += static_cast<int64_t>(size);
}

void send_oob(ClassicTvUdp& f, const gn::Address& to, const std::string& text) {
  std::vector<uint8_t> b = {0xff, 0xff, 0xff, 0xff};
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
  send_to(f, to, b.data(), b.size());
}

void send_oob_print(ClassicTvUdp& f, const gn::Address& to, const std::string& text) {
  size_t at = 0;
  do {
    const size_t n = std::min(text.size() - at, kMinMtu - 10);
    send_oob(f, to, "n" + text.substr(at, n));
    at += n;
  } while (at < text.size());
}

void refuse(ClassicTvUdp& f, const gn::Address& to, const std::string& why) {
  f.refused++;
  send_oob_print(f, to, why + "\n");
}

std::vector<uint8_t> encode(const SvcMsg& m) {
  uint8_t buf[kMaxMsgLen];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  if (!write_svc(w, m, plain_ctx()) || w.overflow) return {};
  return {buf, buf + w.size};
}

std::vector<uint8_t> stuff_msg(const std::string& line) {
  SvcMsg m;
  m.id = svc::stufftext;
  m.strings = {line};
  return encode(m);
}

std::string without_ext_keys(std::string info) {
  for (const char* k : kLeftKeys) info_remove(info, k);
  return info;
}

// `msg` onto the viewer's queue, joined to the last message while the two fit `most`.
void queue(UdpViewer& v, const std::vector<uint8_t>& msg, size_t most = kUnit) {
  if (msg.empty()) return;
  if (!v.queue.empty() && v.queue.back().size() + msg.size() <= most) {
    v.queue.back().insert(v.queue.back().end(), msg.begin(), msg.end());
    return;
  }
  v.queue.push_back(msg);
}

// A message every viewer of stream `s` that is in the game is sent reliably.
void to_watchers(ClassicTvUdp& f, int s, const std::vector<uint8_t>& msg) {
  for (UdpViewer& v : f.viewers)
    if (v.stage == UdpViewerStage::active && v.stream == s) queue(v, msg);
}

void transmit(ClassicTvUdp& f, UdpViewer& v, const std::vector<uint8_t>* dgram) {
  if (channel_reliable_backlog(v.ch) == 0 && !v.queue.empty()) {
    const std::vector<uint8_t>& m = v.queue.front();
    channel_reliable(v.ch, m.data(), m.size());
    v.queue.pop_front();
  }
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(v.ch, dgram != nullptr ? dgram->data() : nullptr,
                                    dgram != nullptr ? dgram->size() : 0, f.now, out);
  send_to(f, v.address, out, n);
  v.sent = f.now;
}

void drop(ClassicTvUdp& f, UdpViewer& v, const std::string& why) {
  if (v.stage == UdpViewerStage::free) return;
  note(f, v.name + " stopped watching: " + why);
  f.dropped++;
  const uint8_t bye[] = {svc::disconnect};
  v.queue.clear();
  channel_reliable(v.ch, bye, sizeof bye);
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(v.ch, nullptr, 0, f.now, out);
  send_to(f, v.address, out, n);
  v = UdpViewer{};
}

// --- the signon, each step on the viewer's echo ---

void send_new(ClassicTvUdp& f, UdpViewer& v) {
  const UdpTvStream& st = f.streams[static_cast<size_t>(v.stream)];
  SvcMsg m;
  m.id = svc::serverdata;
  // Seated in the player's view: his slot, and no speed of its own (the face turns its view).
  const int seat = st.pov >= 0 ? st.pov : (f.viewer_slot | 0x80);
  m.ints = {kProtocolVersion, st.count, seat};
  m.strings = {st.gamedir, st.title};
  m.floats.assign(st.movevars, st.movevars + 10);
  for (const int k : {0, 2, 3, 4, 5, 6}) m.floats[static_cast<size_t>(k)] = 0;  // no own move
  queue(v, encode(m), kPage);
  SvcMsg cd;
  cd.id = svc::cdtrack;
  cd.ints = {st.cdtrack};
  queue(v, encode(cd), kPage);
  queue(v, stuff_msg("fullserverinfo \"" + st.serverinfo + "\"\n"), kPage);
}

void send_list(ClassicTvUdp& f, UdpViewer& v, bool models, int start) {
  const UdpTvStream& st = f.streams[static_cast<size_t>(v.stream)];
  const std::vector<std::string>& names = models ? st.models : st.sounds;
  const int limit = std::min<int>(256, static_cast<int>(names.size()));
  if (start < 0 || start >= limit) start = std::max(0, limit - 1);
  SvcMsg m;
  m.id = models ? svc::modellist : svc::soundlist;
  size_t bytes = 4;
  int last = start;
  while (last + 1 < limit) {
    const std::string& n = names[static_cast<size_t>(last) + 1];
    if (bytes + n.size() + 1 > kPage && (last & 0xff) != 0) break;
    bytes += n.size() + 1;
    m.strings.push_back(n);
    last++;
  }
  m.ints = {start, last + 1 < limit ? (last & 0xff) : 0};
  v.queue.push_back(encode(m));
}

std::vector<uint8_t> baseline_msg(const EntityState& b) {
  SvcMsg m;
  m.id = svc::spawnbaseline;
  m.ints = {b.number, b.modelindex, b.frame, b.colormap, b.skin};
  m.floats = {b.origin.x, b.angles.x, b.origin.y, b.angles.y, b.origin.z, b.angles.z};
  return encode(m);
}

// One index space: the statics, the baselines with a model, the static sounds.
void send_prespawn(ClassicTvUdp& f, UdpViewer& v, int n) {
  const UdpTvStream& st = f.streams[static_cast<size_t>(v.stream)];
  std::vector<const EntityState*> bases;
  for (const EntityState& b : st.baselines)
    if (b.modelindex != 0) bases.push_back(&b);
  const int ns = static_cast<int>(st.statics.size()), nb = static_cast<int>(bases.size());
  const int total = ns + nb + static_cast<int>(st.static_sounds.size());
  std::vector<uint8_t> page;
  int i = std::max(0, n);
  for (; i < total && page.size() < kPage; i++) {
    const std::vector<uint8_t> m = i < ns ? st.statics[static_cast<size_t>(i)]
                                   : i < ns + nb
                                       ? baseline_msg(*bases[static_cast<size_t>(i - ns)])
                                       : st.static_sounds[static_cast<size_t>(i - ns - nb)];
    page.insert(page.end(), m.begin(), m.end());
  }
  const std::string count = std::to_string(st.count);
  const std::vector<uint8_t> next =
      stuff_msg(i < total ? "cmd prespawn " + count + " " + std::to_string(i) + "\n"
                          : "cmd spawn " + count + " 0\n");
  page.insert(page.end(), next.begin(), next.end());
  v.queue.push_back(std::move(page));
}

std::vector<uint8_t> slot_lines(const UdpTvSlot& p, int slot) {
  std::vector<uint8_t> out;
  auto add = [&](const SvcMsg& m) {
    const std::vector<uint8_t> b = encode(m);
    out.insert(out.end(), b.begin(), b.end());
  };
  SvcMsg m;
  m.id = svc::updatefrags;
  m.ints = {slot, p.frags};
  add(m);
  m.id = svc::updateping;
  m.ints = {slot, p.ping};
  add(m);
  m.id = svc::updatepl;
  m.ints = {slot, p.loss};
  add(m);
  m.id = svc::updateentertime;
  m.ints = {slot};
  m.floats = {p.in_for};
  add(m);
  m.id = svc::updateuserinfo;
  m.ints = {slot, p.userid};
  m.floats.clear();
  m.strings = {p.info};
  add(m);
  return out;
}

std::vector<uint8_t> stat_msg(int stat, int32_t value) {
  SvcMsg m;
  m.id = value >= 0 && value < 256 ? svc::updatestat : svc::updatestatlong;
  m.ints = {stat, value};
  return encode(m);
}

void send_spawn(ClassicTvUdp& f, UdpViewer& v) {
  const UdpTvStream& st = f.streams[static_cast<size_t>(v.stream)];
  for (int j = 0; j < 32; j++)
    if (st.slots[static_cast<size_t>(j)].used)
      queue(v, slot_lines(st.slots[static_cast<size_t>(j)], j), kPage);
  for (int i = 0; i < 64; i++) {
    SvcMsg m;
    m.id = svc::lightstyle;
    m.ints = {i};
    m.strings = {st.lightstyles[static_cast<size_t>(i)]};
    queue(v, encode(m), kPage);
  }
  for (int i = 0; i < 32; i++) queue(v, stat_msg(i, st.stats[i]), kPage);
  queue(v, stuff_msg("skins\n"), kPage);
}

// The viewer moves to stream `s`: at once when the two are on the same level with the same
// lists (a multiview match's views are), else by the level's change.
void move_to(ClassicTvUdp& f, UdpViewer& v, int s) {
  if (s == v.stream || s < 0 || s >= static_cast<int>(f.streams.size())) return;
  const UdpTvStream& a = f.streams[static_cast<size_t>(v.stream)];
  const UdpTvStream& b = f.streams[static_cast<size_t>(s)];
  if (!b.level) return;
  f.switches++;
  v.stream = s;
  if (v.stage != UdpViewerStage::active) return;
  if (a.level && a.pov == b.pov && a.count == b.count && a.models == b.models &&
      a.sounds == b.sounds) {
    for (int i = 0; i < 32; i++) queue(v, stat_msg(i, b.stats[i]));
    return;
  }
  v.queue.clear();
  queue(v, stuff_msg("changing\n"));
  queue(v, stuff_msg("reconnect\n"));
  v.stage = UdpViewerStage::connected;
  v.asked = false;
}

std::string status_text(const ClassicTvUdp& f) {
  const UdpTvStream& st = f.streams[static_cast<size_t>(f.follow)];
  std::string info = st.serverinfo;
  info_set(info, "hostname", f.cfg.name);
  std::string t = info + "\n";
  for (int i = 0; i < 32; i++) {
    const UdpTvSlot& p = st.slots[static_cast<size_t>(i)];
    if (!p.used || p.info.empty()) continue;
    const bool watching = !info_get(p.info, "*spectator").empty();
    char line[256];
    std::snprintf(line, sizeof line, "%d %d %d %d \"%s\" \"%s\" %d %d\n", p.userid,
                  watching ? -9999 : p.frags, static_cast<int>(p.in_for / 60),
                  watching ? -p.ping : p.ping, info_get(p.info, "name").c_str(),
                  info_get(p.info, "skin").c_str(), std::atoi(info_get(p.info, "topcolor").c_str()),
                  std::atoi(info_get(p.info, "bottomcolor").c_str()));
    t += line;
  }
  return t;
}

void connect(ClassicTvUdp& f, const gn::Address& from, const std::string& text) {
  pg::Words w;
  pg::tokenize_words(text.substr(0, text.find('\n')).c_str(), w);
  if (std::atoi(pg::word(w, 1)) != kProtocolVersion)
    return refuse(f, from, "Server is version " + std::to_string(kProtocolVersion) + ".");
  const auto qport = static_cast<uint16_t>(std::atoi(pg::word(w, 2)));
  const auto at = f.challenges.find(host_key(from));
  if (at == f.challenges.end()) return refuse(f, from, "No challenge for address.");
  if (at->second != std::atoi(pg::word(w, 3))) return refuse(f, from, "Bad challenge.");
  const std::string userinfo = pg::word(w, 4);
  for (const UdpViewer& v : f.viewers)
    if (v.stage != UdpViewerStage::free && same_host(v.address, from) && v.qport == qport) {
      send_oob(f, from, "j");  // a repeat: its answer was lost
      return;
    }
  if (!f.cfg.password.empty() && info_get(userinfo, "password") != f.cfg.password &&
      info_get(userinfo, "spectator") != f.cfg.password)
    return refuse(f, from, "This broadcast needs a password.");
  UdpViewer* slot = nullptr;
  for (int i = 0; i < f.cfg.max_viewers && i < static_cast<int>(f.viewers.size()); i++)
    if (f.viewers[static_cast<size_t>(i)].stage == UdpViewerStage::free) {
      slot = &f.viewers[static_cast<size_t>(i)];
      break;
    }
  if (slot == nullptr) return refuse(f, from, "This broadcast is full.");
  UdpViewer& v = *slot;
  v = UdpViewer{};
  v.stage = UdpViewerStage::connected;
  v.address = from;
  v.qport = qport;
  v.name = info_get(userinfo, "name");
  if (v.name.empty()) v.name = "unnamed";
  v.heard = f.now;
  v.stream = f.follow;
  channel_setup(v.ch, false, qport, f.now);
  channel_set_rate(v.ch, 0);
  send_oob(f, from, "j");
  note(f, v.name + " is watching");
}

void read_oob(ClassicTvUdp& f, const gn::Address& from, const uint8_t* data, size_t size) {
  const std::string text(reinterpret_cast<const char*>(data) + 4,
                         strnlen(reinterpret_cast<const char*>(data) + 4, size - 4));
  pg::Words w;
  pg::tokenize_words(text.substr(0, text.find('\n')).c_str(), w);
  const std::string cmd = pg::word(w, 0);
  if (cmd == "getchallenge") {
    auto at = f.challenges.find(host_key(from));
    if (at == f.challenges.end()) {
      f.next_challenge = f.next_challenge * 1103515245u + 12345u;
      at = f.challenges
               .emplace(host_key(from), static_cast<int32_t>((f.next_challenge >> 8) & 0x7FFFFFu))
               .first;
    }
    send_oob(f, from, "c" + std::to_string(at->second));
  } else if (cmd == "connect") {
    connect(f, from, text);
  } else if (cmd == "status") {
    send_oob_print(f, from, status_text(f));
  } else if (cmd == "ping" || cmd == "k") {
    send_oob(f, from, "l");
  }
}

void string_command(ClassicTvUdp& f, UdpViewer& v, const std::string& line) {
  pg::Words w;
  pg::tokenize_words(line.c_str(), w);
  const std::string cmd = pg::word(w, 0);
  const UdpTvStream& st = f.streams[static_cast<size_t>(v.stream)];
  const bool stale = w.count >= 2 && std::atoi(pg::word(w, 1)) != st.count;
  const bool signon = v.stage == UdpViewerStage::connected && st.level && !stale;
  if (cmd == "new") {
    if (v.stage == UdpViewerStage::active) return;
    v.stage = UdpViewerStage::connected;
    v.asked = true;
    if (st.level) send_new(f, v);
  } else if (cmd == "soundlist" || cmd == "modellist") {
    if (signon) send_list(f, v, cmd == "modellist", std::atoi(pg::word(w, 2)));
  } else if (cmd == "prespawn") {
    if (signon) send_prespawn(f, v, std::atoi(pg::word(w, 2)));
  } else if (cmd == "spawn") {
    if (signon) send_spawn(f, v);
  } else if (cmd == "begin") {
    if (!signon) return;
    v.stage = UdpViewerStage::active;
    v.asked = false;
  } else if (cmd == "ptrack") {
    // The spectator's track keys: the player it follows, and his view's stream when the face has
    // one; no slot: back to the one the TV follows.
    if (w.count < 2) {
      v.chose = false;
      move_to(f, v, f.follow);
      return;
    }
    const int want = std::atoi(pg::word(w, 1));
    for (size_t s = 0; s < f.streams.size(); s++)
      if (f.streams[s].pov == want && f.streams[s].level) {
        v.chose = true;
        v.want = -1;
        move_to(f, v, static_cast<int>(s));
        return;
      }
    if (f.views_on_request && want >= 0 && want < 32) {  // the source turns a stream to it
      v.chose = true;
      v.want = want;
    }
  } else if (cmd == "download") {
    // The broadcast offers no files: the client goes on without the one it lacks.
    SvcMsg m;
    m.id = svc::download;
    m.ints = {-1, 0};
    queue(v, encode(m), kPage);
  } else if (cmd == "drop") {
    drop(f, v, "left");
  }
  // The rest (chat, user info, pings) is a viewer's: the broadcast does not take it.
}

// The fire button: the next player's view the face has; with views on request, the next player
// of the recorded game, his view asked for when no stream carries it yet.
void next_view(ClassicTvUdp& f, UdpViewer& v) {
  const int n = static_cast<int>(f.streams.size());
  if (f.views_on_request) {
    const UdpTvStream& st = f.streams[static_cast<size_t>(v.stream)];
    const int from = v.want >= 0 ? v.want : st.pov;
    for (int k = 1; k <= 32; k++) {
      const int slot = (from + k + 32) % 32;
      const UdpTvSlot& p = st.slots[static_cast<size_t>(slot)];
      if (!p.used || info_get(p.info, "name").empty() || !info_get(p.info, "*spectator").empty())
        continue;
      if (slot == st.pov) {
        v.want = -1;
        return;
      }
      for (int s = 0; s < n; s++)
        if (f.streams[static_cast<size_t>(s)].pov == slot &&
            f.streams[static_cast<size_t>(s)].level) {
          v.chose = true;
          v.want = -1;
          move_to(f, v, s);
          return;
        }
      v.chose = true;
      v.want = slot;
      return;
    }
    return;
  }
  for (int k = 1; k < n; k++) {
    const int s = (v.stream + k) % n;
    if (f.streams[static_cast<size_t>(s)].level) {
      v.chose = true;
      move_to(f, v, s);
      return;
    }
  }
}

void read_messages(ClassicTvUdp& f, UdpViewer& v, const uint8_t* data, size_t size) {
  gn::Reader r = gn::make_reader(data, size);
  int commands = 0;
  while (gn::remaining(r) > 0 && v.stage != UdpViewerStage::free) {
    ClcMsg m;
    std::string error;
    if (!read_clc(r, m, error)) return drop(f, v, "a message it sent did not read: " + error);
    if (m.id == clc::stringcmd && ++commands <= kStringCmdsPerPacket) string_command(f, v, m.text);
    if (m.id == clc::move && v.stage == UdpViewerStage::active) {
      const bool fire = (m.cmds[2].buttons & 1) != 0;
      if (fire && !v.fire) next_view(f, v);
      v.fire = fire;
    }
  }
}

// --- the source's packets ---

void new_level(ClassicTvUdp& f, int s) {
  UdpTvStream& st = f.streams[static_cast<size_t>(s)];
  const int64_t packets = st.packets, frames = st.frames, levels = st.levels + 1;
  st = UdpTvStream{};
  st.packets = packets;
  st.frames = frames;
  st.levels = levels;
  st.baselines.resize(kMostEntity + 1);
  for (UdpViewer& v : f.viewers) {
    if (v.stage == UdpViewerStage::free || v.stream != s) continue;
    v.queue.clear();
    // As a server changing levels: the client keeps its connection and signs on again.
    queue(v, stuff_msg("changing\n"));
    queue(v, stuff_msg("reconnect\n"));
    v.stage = UdpViewerStage::connected;
    v.asked = false;
  }
}

// The slot no recorded player holds, highest first, for the viewers.
void pick_viewer_slot(ClassicTvUdp& f) {
  for (int k = 31; k >= 0; k--) {
    bool used = false;
    for (const UdpTvStream& st : f.streams) used = used || st.slots[static_cast<size_t>(k)].used;
    if (!used) {
      f.viewer_slot = k;
      return;
    }
  }
}

EntityState state_of_layout(const SvcMsg& m, size_t first) {
  EntityState b;
  b.modelindex = m.ints.size() > first ? m.ints[first] : 0;
  b.frame = m.ints.size() > first + 1 ? m.ints[first + 1] : 0;
  b.colormap = m.ints.size() > first + 2 ? m.ints[first + 2] : 0;
  b.skin = m.ints.size() > first + 3 ? m.ints[first + 3] : 0;
  if (m.floats.size() >= 6) {
    b.origin = {m.floats[0], m.floats[2], m.floats[4]};
    b.angles = {m.floats[1], m.floats[3], m.floats[5]};
  }
  return b;
}

std::vector<uint8_t> static_msg(const EntityState& b) {
  SvcMsg m;
  m.id = svc::spawnstatic;
  m.ints = {b.modelindex, b.frame, b.colormap, b.skin};
  m.floats = {b.origin.x, b.angles.x, b.origin.y, b.angles.y, b.origin.z, b.angles.z};
  return encode(m);
}

bool plain_entity(const EntityState& e) {
  return e.number > 0 && e.number <= kMostEntity && e.modelindex <= kMostModel;
}

// One message of stream `s` read off `r` (its id next): kept, rewritten plain, sent on or left out.
// False when it does not read.
bool read_one(ClassicTvUdp& f, int s, gn::Reader& r, uint32_t sequence,
              std::vector<uint8_t>& players, std::vector<uint8_t>& ents, bool& framed,
              std::string& error) {
  UdpTvStream& st = f.streams[static_cast<size_t>(s)];
  WireCtx ctx;
  ctx.ext = st.ext;
  ctx.chunked = st.ext.has(ExtFamily::x, kXChunkedDl);
  const size_t at = r.at;
  const int id = r.at < r.size ? r.data[r.at] : -1;
  if (id == svc::packetentities || id == svc::deltapacketentities) {
    read_byte(r);
    const std::vector<EntityState>* from = nullptr;
    if (id == svc::deltapacketentities) {
      const int fb = read_byte(r);
      const size_t k = static_cast<size_t>(fb & 63);
      if ((f.streams[static_cast<size_t>(s)].ring_seq[k] & 0xff) != static_cast<uint32_t>(fb)) {
        error = "an entity list deltas from frame " + std::to_string(fb) + ", which it never had";
        return false;
      }
      from = &st.ring[k];
    }
    std::vector<EntityState> out;
    if (!read_packet_entities(r, from, st.baselines, st.ext, out) || r.bad) {
      error = "an entity list did not read";
      return false;
    }
    std::vector<EntityState> shown;
    for (const EntityState& e : out)
      if (plain_entity(e)) shown.push_back(e);
    f.left_out += static_cast<int64_t>(out.size() - shown.size());
    st.ring[sequence & 63] = std::move(out);
    st.ring_seq[sequence & 63] = sequence;
    uint8_t buf[kMaxMsgLen];
    gn::Writer w = gn::make_writer(buf, sizeof buf);
    write_byte(w, svc::packetentities);
    write_packet_entities(w, nullptr, shown, st.baselines, Ext{});
    if (w.overflow) {
      error = "an entity list grew past a packet written plain";
      return false;
    }
    ents.assign(buf, buf + w.size);
    framed = true;
    return true;
  }
  if (id == svc::playerinfo) {
    read_byte(r);
    const PlayerState p = read_playerinfo(r, st.ext);
    if (r.bad) {
      error = "a player's state did not read";
      return false;
    }
    uint8_t buf[256];
    gn::Writer w = gn::make_writer(buf, sizeof buf);
    write_byte(w, svc::playerinfo);
    write_playerinfo(w, p, Ext{});
    players.insert(players.end(), buf, buf + w.size);
    framed = true;
    return true;
  }
  SvcMsg m;
  if (!read_svc(r, m, ctx, error)) return false;
  (void)at;
  auto slot_of = [&]() -> UdpTvSlot* {
    const int k = m.ints.empty() ? -1 : m.ints[0];
    return k >= 0 && k < 32 ? &st.slots[static_cast<size_t>(k)] : nullptr;
  };
  auto reliable = [&]() { to_watchers(f, s, encode(m)); };
  auto event = [&]() {
    const std::vector<uint8_t> b = encode(m);
    st.events.insert(st.events.end(), b.begin(), b.end());
  };
  switch (m.id) {
    case svc::serverdata: {
      if (st.level || st.packets > 1) new_level(f, s);
      st.level = true;
      st.ext = Ext{};
      for (const auto& [magic, bits] : m.ext) {
        ExtFamily fam{};
        if (family_of_magic(magic, fam)) st.ext[fam] = bits;
      }
      st.count = (m.ints.size() > 1 ? m.ints[1] : 0) + f.count_raise;
      st.pov = m.ints.size() > 2 ? (m.ints[2] & 0x7f) : -1;
      if (m.ints.size() > 2 && (m.ints[2] & 0x80) != 0) st.pov = -1;  // a spectator's: no one's
      st.gamedir = m.strings.empty() ? "" : m.strings[0];
      st.title = m.strings.size() > 1 ? m.strings[1] : "";
      for (size_t i = 0; i < 10 && i < m.floats.size(); i++) st.movevars[i] = m.floats[i];
      st.sounds = {""};
      st.models = {""};
      for (UdpViewer& v : f.viewers)  // asked before the level came: its signon now
        if (v.stage == UdpViewerStage::connected && v.stream == s && v.asked) send_new(f, v);
      return true;
    }
    case svc::soundlist:
    case svc::modellist:
    case svc::soundlistshort:
    case svc::modellistshort: {
      const bool models = m.id == svc::modellist || m.id == svc::modellistshort;
      std::vector<std::string>& names = models ? st.models : st.sounds;
      const size_t start = static_cast<size_t>(std::max(0, m.ints.empty() ? 0 : m.ints[0]));
      if (names.size() < start + 1 + m.strings.size()) names.resize(start + 1 + m.strings.size());
      for (size_t i = 0; i < m.strings.size(); i++) names[start + 1 + i] = m.strings[i];
      return true;
    }
    case svc::spawnstatic:
      st.statics.push_back(encode(m));
      return true;
    case svc::spawnstatic2:
      if (!m.ents.empty() && m.ents[0].state.modelindex <= kMostModel)
        st.statics.push_back(static_msg(m.ents[0].state));
      else
        f.left_out++;
      return true;
    case svc::spawnbaseline:
    case svc::spawnbaseline2: {
      EntityState b = m.id == svc::spawnbaseline ? state_of_layout(m, 1)
                      : m.ents.empty()           ? EntityState{}
                                                 : m.ents[0].state;
      b.number = m.ints.empty() ? 0 : m.ints[0];
      if (b.number >= 0 && b.number <= kMostEntity && b.modelindex <= kMostModel)
        st.baselines[static_cast<size_t>(b.number)] = b;
      else
        f.left_out++;
      return true;
    }
    case svc::spawnstaticsound:
      st.static_sounds.push_back(encode(m));
      return true;
    case svc::spawnstaticsound2:
      if (m.ints.size() >= 4 && m.ints[1] <= 255) {
        m.id = svc::spawnstaticsound;
        m.ints = {m.ints[1], m.ints[2], m.ints[3]};
        st.static_sounds.push_back(encode(m));
      } else {
        f.left_out++;
      }
      return true;
    case svc::lightstyle:
      if (!m.ints.empty() && m.ints[0] >= 0 && m.ints[0] < 64 && !m.strings.empty())
        st.lightstyles[static_cast<size_t>(m.ints[0])] = m.strings[0];
      reliable();
      return true;
    case svc::updateuserinfo:
      if (UdpTvSlot* p = slot_of()) {
        p->used = !m.strings.empty() && !m.strings[0].empty();
        p->userid = m.ints.size() > 1 ? m.ints[1] : 0;
        p->info = m.strings.empty() ? "" : m.strings[0];
        pick_viewer_slot(f);
      }
      reliable();
      return true;
    case svc::setinfo:
      if (UdpTvSlot* p = slot_of(); p != nullptr && m.strings.size() >= 2)
        info_set(p->info, m.strings[0], m.strings[1]);
      reliable();
      return true;
    case svc::updatefrags:
      if (UdpTvSlot* p = slot_of()) p->frags = m.ints.size() > 1 ? m.ints[1] : 0;
      reliable();
      return true;
    case svc::updateping:
      if (UdpTvSlot* p = slot_of()) p->ping = m.ints.size() > 1 ? m.ints[1] : 0;
      reliable();
      return true;
    case svc::updatepl:
      if (UdpTvSlot* p = slot_of()) p->loss = m.ints.size() > 1 ? m.ints[1] : 0;
      reliable();
      return true;
    case svc::updateentertime:
      if (UdpTvSlot* p = slot_of()) p->in_for = m.floats.empty() ? 0 : m.floats[0];
      reliable();
      return true;
    case svc::serverinfo:
      if (m.strings.size() >= 2) {
        for (const char* k : kLeftKeys)
          if (m.strings[0] == k) return true;
        info_set(st.serverinfo, m.strings[0], m.strings[1]);
      }
      reliable();
      return true;
    case svc::stufftext: {
      // The recorder's server's lines to its own client: only the server info is the level's.
      const std::string& t = m.strings.empty() ? std::string() : m.strings[0];
      if (t.rfind("fullserverinfo", 0) == 0) {
        pg::Words w;
        pg::tokenize_words(t.c_str(), w);
        const std::string info = pg::word(w, 1);
        const std::string z = info_get(info, "*z_ext");
        st.ext[ExtFamily::z] = static_cast<uint32_t>(std::atoi(z.c_str()));
        st.serverinfo = without_ext_keys(info);
      }
      return true;
    }
    case svc::updatestat:
    case svc::updatestatlong:
      if (m.ints.size() >= 2 && m.ints[0] >= 0 && m.ints[0] < 32) {
        st.stats[m.ints[0]] = m.ints[1];
        to_watchers(f, s, stat_msg(m.ints[0], m.ints[1]));
      }
      return true;
    case svc::cdtrack:
      st.cdtrack = m.ints.empty() ? 0 : m.ints[0];
      reliable();
      return true;
    case svc::print:
    case svc::centerprint:
    case svc::intermission:
    case svc::finale:
    case svc::sellscreen:
    case svc::killedmonster:
    case svc::foundsecret:
    case svc::setpause:
      reliable();
      return true;
    case svc::soundextended:
      if (m.ints.size() >= 5 && m.ints[0] < 2048 && m.ints[4] <= 255) {
        m.id = svc::sound;
        event();
      } else {
        f.left_out++;
      }
      return true;
    case svc::damage:
    case svc::smallkick:
    case svc::bigkick:
    case svc::setangle:  // the player's own view: the viewer sits in it
      if (m.id == svc::setangle && st.ext.has(ExtFamily::mvd, kMvdTeleport)) m.ints.clear();
      event();
      return true;
    case svc::sound:
    case svc::temp_entity:
    case svc::nails:
    case svc::muzzleflash:
    case svc::stopsound:
      event();
      return true;
    default:
      // The recorder's own view (damage, kicks, turns, its speed), the channel's (nop, choke,
      // downloads, the end) and the extensions' own: not a viewer's.
      if (m.id != svc::nop && m.id != svc::disconnect && m.id != svc::chokecount) f.left_out++;
      return true;
  }
}

}  // namespace

int udp_tv_viewers(const ClassicTvUdp& f) {
  int n = 0;
  for (const UdpViewer& v : f.viewers) n += v.stage != UdpViewerStage::free ? 1 : 0;
  return n;
}

void udp_tv_start(ClassicTvUdp& f, gn::Transport& t, const UdpTvConfig& cfg, int streams) {
  f.cfg = cfg;
  f.transport = &t;
  f.streams.assign(static_cast<size_t>(std::max(1, streams)), UdpTvStream{});
  for (UdpTvStream& st : f.streams) st.baselines.resize(kMostEntity + 1);
  f.follow = 0;
}

bool udp_tv_packet(ClassicTvUdp& f, int s, std::span<const uint8_t> packet) {
  if (s < 0 || s >= static_cast<int>(f.streams.size()) || packet.size() < 8) return false;
  UdpTvStream& st = f.streams[static_cast<size_t>(s)];
  uint32_t sequence = 0;
  std::memcpy(&sequence, packet.data(), 4);  // little-endian
  if (sequence == 0xffffffffu) return true;  // out of band: nothing of the game
  sequence &= 0x7fffffffu;
  st.packets++;
  gn::Reader r = gn::make_reader(packet.data() + 8, packet.size() - 8);
  std::vector<uint8_t> players, ents;
  bool framed = false;
  std::string error;
  while (gn::remaining(r) > 0) {
    if (!read_one(f, s, r, sequence, players, ents, framed, error)) {
      f.error = error;
      note(f, "a packet that did not read: " + error);
      return false;
    }
  }
  UdpTvStream& now = f.streams[static_cast<size_t>(s)];  // read_one may have started it over
  (void)st;
  if (framed) {
    now.frame = std::move(players);
    now.frame.insert(now.frame.end(), ents.begin(), ents.end());
    now.fresh = true;
    now.frames++;
  }
  return true;
}

void udp_tv_view(ClassicTvUdp& f, int s, sh::Vec3f angles) {
  if (s < 0 || s >= static_cast<int>(f.streams.size())) return;
  f.streams[static_cast<size_t>(s)].view = angles;
  f.streams[static_cast<size_t>(s)].has_view = true;
}

void udp_tv_follow(ClassicTvUdp& f, int s) {
  if (s < 0 || s >= static_cast<int>(f.streams.size())) return;
  f.follow = s;
  for (UdpViewer& v : f.viewers)
    if (v.stage != UdpViewerStage::free && !v.chose) move_to(f, v, s);
}

void udp_tv_clear(ClassicTvUdp& f, int s) {
  if (s < 0 || s >= static_cast<int>(f.streams.size())) return;
  UdpTvStream& st = f.streams[static_cast<size_t>(s)];
  st = UdpTvStream{};
  st.baselines.resize(kMostEntity + 1);
}

std::vector<int> udp_tv_wanted(const ClassicTvUdp& f) {
  std::vector<int> out;
  for (const UdpViewer& v : f.viewers)
    if (v.stage != UdpViewerStage::free && v.want >= 0 &&
        std::find(out.begin(), out.end(), v.want) == out.end())
      out.push_back(v.want);
  return out;
}

int udp_tv_watching(const ClassicTvUdp& f, int s) {
  if (s < 0 || s >= static_cast<int>(f.streams.size())) return 0;
  const int pov = f.streams[static_cast<size_t>(s)].pov;
  int n = 0;
  for (const UdpViewer& v : f.viewers)
    if (v.stage != UdpViewerStage::free && (v.stream == s || (pov >= 0 && v.want == pov))) n++;
  return n;
}

void udp_tv_frame(ClassicTvUdp& f, int64_t now_us) {
  f.now = static_cast<double>(now_us) / 1e6;
  // A view asked for has come: its viewers move to it.
  for (UdpViewer& v : f.viewers) {
    if (v.stage == UdpViewerStage::free || v.want < 0) continue;
    for (size_t s = 0; s < f.streams.size(); s++)
      if (f.streams[s].pov == v.want && f.streams[s].level) {
        v.want = -1;
        move_to(f, v, static_cast<int>(s));
        break;
      }
  }
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
    UdpViewer* v = nullptr;
    for (UdpViewer& c : f.viewers)
      if (c.stage != UdpViewerStage::free && c.qport == qport && same_host(c.address, from)) {
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
  for (UdpViewer& v : f.viewers) {
    if (v.stage == UdpViewerStage::free) continue;
    if (f.now - v.heard > f.cfg.timeout) {
      drop(f, v, "timed out");
      continue;
    }
    if (v.ch.overflow || v.queue.size() > f.cfg.most_behind) {
      drop(f, v, "fell behind");
      continue;
    }
    UdpTvStream& st = f.streams[static_cast<size_t>(v.stream)];
    if (v.stage == UdpViewerStage::active) {
      // Every packet carries the newest frame (the sounds and effects since the last first).
      if (!st.fresh && f.now - v.sent < 1.0) continue;
      std::vector<uint8_t> dgram = st.fresh ? st.events : std::vector<uint8_t>{};
      if (dgram.size() + st.frame.size() > kMaxPacket - 16 - kUnit) dgram.clear();
      dgram.insert(dgram.end(), st.frame.begin(), st.frame.end());
      if (st.has_view) {  // the player's view's angles: the viewer looks where he looks
        SvcMsg a;
        a.id = svc::setangle;
        a.floats = {st.view.x, st.view.y, st.view.z};
        const std::vector<uint8_t> b = encode(a);
        dgram.insert(dgram.end(), b.begin(), b.end());
      }
      transmit(f, v, &dgram);
      v.frames++;
    } else {
      // In the signon: a packet whenever the viewer has spoken since the last one (its ack moves
      // the reliable message on), soon when something waits, and once a second besides.
      const bool waiting = !v.queue.empty() || channel_reliable_backlog(v.ch) > 0;
      if (v.ch.last_received >= v.sent || (waiting && f.now - v.sent >= 0.05) ||
          f.now - v.sent >= 1.0)
        transmit(f, v, nullptr);
    }
  }
  for (UdpTvStream& st : f.streams) {
    st.fresh = false;
    st.events.clear();
  }
}

void udp_tv_stop(ClassicTvUdp& f) {
  for (UdpViewer& v : f.viewers) drop(f, v, "the broadcast stopped");
}

}  // namespace shooters_edge::net
