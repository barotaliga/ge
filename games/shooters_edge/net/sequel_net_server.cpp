// The sequel's net server (sequel_net_server.hpp): out of band, the signon by the client's
// commands, the moves into the rules, the frames out.
//
// ge:summary The sequel's net server's steps: challenges, connects, the signon, moves and frames
// ge:keywords sequel-server sequel_net_frame signon configstrings baselines begin move frame
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_net_server.hpp"

#include <cstdio>
#include <cstdlib>
#include <utility>

#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

constexpr size_t kSignonShare = 700;  // bytes of the signon one answer adds before its prompt
constexpr int kStringCmdsPerPacket = 7;
constexpr uint8_t kSvcDisconnect = 7, kSvcStuffText = 11;
constexpr size_t kLogKept = 512;

void note(SequelNetServer& s, std::string line) {
  if (s.log.size() < kLogKept) s.log.push_back(std::move(line));
}

uint64_t host_key(gn::Address a) {
  a.port = 0;
  return gn::address_hash(a);
}

bool same_host(gn::Address a, gn::Address b) {
  a.port = b.port = 0;
  return a == b;
}

void send_to(SequelNetServer& s, const gn::Address& to, const uint8_t* data, size_t size) {
  gn::transport_send(*s.transport, to, data, size, static_cast<int64_t>(s.now * 1e6));
  s.bytes_out += static_cast<int64_t>(size);
}

void send_oob(SequelNetServer& s, const gn::Address& to, const std::string& text) {
  const std::vector<uint8_t> b = sequel_oob(text);
  send_to(s, to, b.data(), b.size());
}

void refuse(SequelNetServer& s, const gn::Address& to, const std::string& why) {
  s.refused++;
  send_oob(s, to, "print\n" + why + "\n");
}

void reliable(SequelNetClient& c, const std::vector<uint8_t>& b) {
  if (!b.empty()) channel_reliable(c.ch, b.data(), b.size());
}

void stuff(SequelNetClient& c, const std::string& line) {
  std::vector<uint8_t> b = {kSvcStuffText};
  b.insert(b.end(), line.begin(), line.end());
  b.push_back(0);
  reliable(c, b);
}

void transmit(SequelNetServer& s, SequelNetClient& c, const uint8_t* data, size_t size) {
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(c.ch, data, size, s.now, out);
  send_to(s, c.address, out, n);
  c.sent = s.now;
}

void drop(SequelNetServer& s, int slot, const std::string& why) {
  SequelNetClient& c = s.clients[slot];
  if (c.stage == SequelSlot::free) return;
  note(s, c.name + " dropped: " + why);
  if (c.stage == SequelSlot::active) s.rules->drop(slot);
  // The disconnect, once, with whatever reliable data is still owed.
  const uint8_t bye[] = {kSvcDisconnect};
  channel_reliable(c.ch, bye, sizeof bye);
  transmit(s, c, nullptr, 0);
  c = SequelNetClient{};
}

std::string info_text(const SequelNetServer& s) {
  char line[96];
  std::snprintf(line, sizeof line, "%16s %8s %2d/%2d\n", s.cfg.hostname.c_str(),
                (*s.rules)->map_name.c_str(), sequel_net_clients(s), s.cfg.max_clients);
  return line;
}

std::string status_text(const SequelNetServer& s) {
  std::string info;
  info_set(info, "hostname", s.cfg.hostname);
  info_set(info, "mapname", (*s.rules)->map_name);
  info_set(info, "maxclients", std::to_string(s.cfg.max_clients));
  info_set(info, "deathmatch", "1");
  info_set(info, "protocol", std::to_string(kSequelNetProtocol));
  std::string t = info + "\n";
  for (int i = 0; i < s.cfg.max_clients && i < sh::kMaxClients; i++) {
    const SequelNetClient& c = s.clients[i];
    if (c.stage == SequelSlot::free) continue;
    const int frags = static_cast<int>((*s.rules)->world.clients[i].numbers.frags);
    t += std::to_string(frags) + " 0 \"" + c.name + "\"\n";
  }
  return t;
}

void connect(SequelNetServer& s, const gn::Address& from, const std::vector<std::string>& w) {
  if (w.size() < 5) return;
  if (std::atoi(w[1].c_str()) != kSequelNetProtocol)
    return refuse(s, from, "Server is protocol version 34.");
  const auto qport = static_cast<uint16_t>(std::atoi(w[2].c_str()));
  const auto at = s.challenges.find(host_key(from));
  if (at == s.challenges.end()) return refuse(s, from, "No challenge for address.");
  if (at->second != std::atoi(w[3].c_str())) return refuse(s, from, "Bad challenge.");
  const std::string& userinfo = w[4];
  for (const SequelNetClient& c : s.clients)
    if (c.stage != SequelSlot::free && same_host(c.address, from) &&
        (c.qport == qport || c.address.port == from.port))
      return;  // already in: a repeat
  const bool spectator =
      !info_get(userinfo, "spectator").empty() && info_get(userinfo, "spectator") != "0";
  if (!spectator && !s.cfg.password.empty() && s.cfg.password != "none" &&
      info_get(userinfo, "password") != s.cfg.password)
    return refuse(s, from, "Password required or incorrect.\nConnection refused.");
  int slot = -1;
  for (int i = 0; i < s.cfg.max_clients && i < sh::kMaxClients; i++)
    if (s.clients[i].stage == SequelSlot::free) {
      slot = i;
      break;
    }
  if (slot < 0) return refuse(s, from, "Server is full.");
  SequelNetClient& c = s.clients[slot];
  c = SequelNetClient{};
  c.stage = SequelSlot::signon;
  c.address = from;
  c.qport = qport;
  c.userinfo = userinfo;
  c.name = info_get(userinfo, "name");
  if (c.name.empty()) c.name = "unnamed";
  c.spectator = spectator;
  c.heard = s.now;
  channel_setup(c.ch, false, qport, s.now);
  channel_set_rate(c.ch, 0);
  send_oob(s, from, "client_connect");
  note(s, c.name + " connected");
}

void read_oob(SequelNetServer& s, const gn::Address& from, const uint8_t* data, size_t size) {
  const std::string text = sequel_oob_text(data, size);
  const std::vector<std::string> w = sequel_words(text);
  if (w.empty()) return;
  if (w[0] == "getchallenge") {
    auto at = s.challenges.find(host_key(from));
    if (at == s.challenges.end()) {
      s.next_challenge = s.next_challenge * 1103515245u + 12345u;
      at = s.challenges
               .emplace(host_key(from), static_cast<int32_t>((s.next_challenge >> 16) & 0x7FFFu))
               .first;
    }
    send_oob(s, from, "challenge " + std::to_string(at->second) + " p=34");
  } else if (w[0] == "connect") {
    connect(s, from, w);
  } else if (w[0] == "info") {
    if (w.size() > 1 && std::atoi(w[1].c_str()) != kSequelNetProtocol)
      send_oob(s, from, "info\n" + s.cfg.hostname + ": wrong version\n");
    else
      send_oob(s, from, "info\n" + info_text(s));
  } else if (w[0] == "status") {
    send_oob(s, from, "print\n" + status_text(s));
  } else if (w[0] == "ping") {
    send_oob(s, from, "ack");
  }
}

void start_signon(SequelNetServer& s, int slot) {
  SequelNetClient& c = s.clients[slot];
  if (c.stage == SequelSlot::active) {
    s.rules->drop(slot);
    c.stage = SequelSlot::signon;
  }
  sv::SequelWireOptions opt = s.cfg.wire;
  opt.attract_loop = 0;
  sv::sequel_signon_parts(c.wire, **s.rules, slot, opt, c.signon);
  reliable(c, c.signon.server_data);
  stuff(c, "cmd configstrings " + std::to_string(s.count) + " 0\n");
}

// The signon's parts from `from` on, about 700 bytes of them, and the next prompt.
template <typename Parts>
void signon_share(SequelNetClient& c, const Parts& parts, int from, const std::string& more,
                  const std::string& done) {
  std::vector<uint8_t> b;
  size_t i = 0;
  while (i < parts.size() && parts[i].first < from) i++;
  for (; i < parts.size() && b.size() < kSignonShare; i++)
    b.insert(b.end(), parts[i].second.begin(), parts[i].second.end());
  reliable(c, b);
  stuff(c, i < parts.size() ? more + std::to_string(parts[i].first) + "\n" : done);
}

void string_command(SequelNetServer& s, int slot, const std::string& line) {
  SequelNetClient& c = s.clients[slot];
  const std::vector<std::string> w = sequel_words(line);
  if (w.empty()) return;
  const std::string& cmd = w[0];
  const bool signon = cmd == "configstrings" || cmd == "baselines" || cmd == "begin";
  if (signon && (w.size() < 2 || std::atoi(w[1].c_str()) != s.count)) {
    note(s, c.name + " from a different level: the signon starts again");
    return start_signon(s, slot);
  }
  const int at = w.size() > 2 ? std::atoi(w[2].c_str()) : 0;
  const std::string count = std::to_string(s.count);
  if (cmd == "new") {
    if (c.stage == SequelSlot::signon) start_signon(s, slot);
  } else if (cmd == "configstrings") {
    if (c.stage == SequelSlot::signon)
      signon_share(c, c.signon.config_strings, at, "cmd configstrings " + count + " ",
                   "cmd baselines " + count + " 0\n");
  } else if (cmd == "baselines") {
    if (c.stage == SequelSlot::signon)
      signon_share(c, c.signon.baselines, at, "cmd baselines " + count + " ",
                   "precache " + count + "\n");
  } else if (cmd == "begin") {
    if (c.stage != SequelSlot::signon) return;
    s.rules->connect(slot, c.name, c.spectator, false);
    c.stage = SequelSlot::active;
    c.acked = -1;
    sv::sequel_resync(c.wire);
    note(s, c.name + " entered the game");
  } else if (cmd == "disconnect") {
    drop(s, slot, "left");
  } else if (c.stage == SequelSlot::active) {
    s.rules->client_command(slot, line);
  }
}

// One packet's messages from client `slot`; its moves onto `packets`.
void read_messages(SequelNetServer& s, int slot, const uint8_t* data, size_t size,
                   std::vector<sh::Packet>& packets) {
  SequelNetClient& c = s.clients[slot];
  gn::Reader r = gn::make_reader(data, size);
  int commands = 0;
  bool moved = false;
  while (gn::remaining(r) > 0 && c.stage != SequelSlot::free) {
    const int id = read_byte(r);
    if (id == kClcNop) continue;
    if (id == kClcMove) {
      if (moved) return;  // one move a packet: the rest is ignored
      moved = true;
      SequelMove m;
      if (!read_sequel_move(r, c.ch.incoming_sequence, m)) return drop(s, slot, "a cut move");
      if (c.stage != SequelSlot::active) continue;
      c.acked = m.frame;
      if (s.cfg.verify_checksum && !m.checksum_ok) {
        c.bad_checksums++;
        return;
      }
      c.moves++;
      sh::Packet p;
      p.client = slot;
      p.sequence = c.ch.incoming_sequence;
      for (int k = 0; k < 3; k++) p.cmds[k] = move_cmd(m.cmds[k]);
      packets.push_back(p);
    } else if (id == kClcUserInfo) {
      c.userinfo = read_string(r);
      const std::string name = info_get(c.userinfo, "name");
      if (!name.empty() && name != c.name) {
        c.name = name;
        if (c.stage == SequelSlot::active) s.rules->user_info(slot, "name", name);
      }
    } else if (id == kClcStringCmd) {
      const std::string line = read_string(r);
      if (++commands <= kStringCmdsPerPacket) string_command(s, slot, line);
    } else {
      return drop(s, slot, "a message " + std::to_string(id) + " it does not know");
    }
    if (r.bad) return drop(s, slot, "a message cut short");
  }
}

int find_client(const SequelNetServer& s, const gn::Address& from, uint16_t qport) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    const SequelNetClient& c = s.clients[i];
    if (c.stage != SequelSlot::free && c.qport == qport && same_host(c.address, from)) return i;
  }
  return -1;
}

void send_frames(SequelNetServer& s) {
  const int64_t frame = s.game->frame_number;
  const bool fresh = frame != s.written_frame;
  s.written_frame = frame;
  std::vector<uint8_t> block, rel;
  for (int i = 0; i < sh::kMaxClients; i++) {
    SequelNetClient& c = s.clients[i];
    if (c.stage == SequelSlot::free) continue;
    if (c.ch.overflow) {
      drop(s, i, "overflowed");
      continue;
    }
    if (c.stage == SequelSlot::signon) {
      if (channel_reliable_backlog(c.ch) > 0 || s.now - c.sent >= 1.0) transmit(s, c, nullptr, 0);
      continue;
    }
    if (!fresh) continue;
    if (c.acked != c.wire.last_frame || c.acked <= 0) {
      sv::sequel_resync(c.wire);
      c.full_frames++;
    }
    rel.clear();
    sv::sequel_frame(c.wire, **s.rules, static_cast<int32_t>(frame), &s.game->players[i].move,
                     block, &rel);
    reliable(c, rel);
    transmit(s, c, block.data(), block.size());
    c.frames++;
  }
}

}  // namespace

int sequel_net_clients(const SequelNetServer& s) {
  int n = 0;
  for (const SequelNetClient& c : s.clients) n += c.stage != SequelSlot::free ? 1 : 0;
  return n;
}

bool sequel_net_start(SequelNetServer& s, const sv::RulesRunner& rules, gn::Transport& t,
                      const SequelNetConfig& cfg, std::string& error) {
  s.cfg = cfg;
  s.rules = &rules;
  s.game = sv::sequel_of(rules);
  s.transport = &t;
  if (s.game == nullptr) {
    error = "the runner is not the sequel's: the sequel's net server serves only its rules";
    return false;
  }
  s.count = 0x10000 + cfg.wire.server_count;
  s.written_frame = s.game->frame_number;
  return true;
}

void sequel_net_frame(SequelNetServer& s, int64_t now_us) {
  s.now = static_cast<double>(now_us) / 1e6;
  gn::transport_pump(*s.transport, now_us);
  std::vector<sh::Packet> packets;
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  for (;;) {
    const int n = gn::transport_receive(*s.transport, buf, sizeof buf, from, now_us);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    s.datagrams_in++;
    s.bytes_in += n;
    if (is_out_of_band(buf, size)) {
      read_oob(s, from, buf, size);
      continue;
    }
    uint16_t qport = 0;
    if (!peek_qport(buf, size, qport)) continue;
    const int slot = find_client(s, from, qport);
    if (slot < 0) continue;
    SequelNetClient& c = s.clients[slot];
    c.address = from;  // a router may have moved its port
    size_t at = 0;
    if (!channel_process(c.ch, buf, size, s.now, at)) continue;
    c.heard = s.now;
    read_messages(s, slot, buf + at, size - at, packets);
  }
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].stage != SequelSlot::free && s.now - s.clients[i].heard > s.cfg.timeout)
      drop(s, i, "timed out");
  s.rules->frame(packets.data(), static_cast<int>(packets.size()));
  send_frames(s);
}

void sequel_net_stop(SequelNetServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) drop(s, i, "the server stopped");
}

}  // namespace shooters_edge::net
