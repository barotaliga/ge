// The classic server's connections (classic_server.hpp): starting over a loaded mod, the
// connectionless packets (ping, status, the challenge, the connect and its checks), reading the
// channels and their clc messages, the moves into the mod's frame, drops and the frame's order.
//
// ge:summary The classic server: start, connectionless answers, connect checks, reading clients
// ge:keywords classic_start classic_frame classic_drop getchallenge connect reject status ping
// ge:keywords challenge-table qport reconnect timeout move-checksum clc-dispatch
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#include "shooters_edge/net/classic_server.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string_view>

#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "progs/text.hpp"
#include "shooter/client.hpp"
#include "shooter/settings.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_record.hpp"
#include "shooters_edge/net/classic_talk.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;

int64_t classic_unix_time(const ClassicServer& s) {
  return s.cfg.unix_time != nullptr ? s.cfg.unix_time() : 0;
}

int16_t safe_strafe(StrafeGuard& g, int16_t side, int n) {
  const int8_t dir = side > 0 ? 1 : side < 0 ? -1 : 0;
  if (g.force == 0 && dir != 0 && g.dir != 0 && dir != g.dir && g.zeros < n) g.force = n - g.zeros;
  if (dir != 0) g.dir = dir;
  if (g.force > 0) {
    g.force--;
    if (g.zeros < n) g.zeros++;
    return 0;
  }
  g.zeros = dir == 0 ? g.zeros + 1 : 0;
  if (g.zeros > n) g.zeros = n;
  return side;
}

namespace {

bool same_host(const gn::Address& a, const gn::Address& b) {
  gn::Address x = a, y = b;
  x.port = y.port = 0;
  return x == y;
}

int32_t new_challenge(ClassicServer& s, const gn::Address& from) {
  int slot = 0;
  for (int i = 0; i < 64; i++) {
    // One challenge an address (its port too), so clients sharing a host (players behind one
    // router, bots from one machine) do not replace each other's before they connect.
    if (s.challenges[i].made > 0 && s.challenges[i].address == from) {
      slot = i;
      break;
    }
    if (s.challenges[i].made < s.challenges[slot].made) slot = i;
  }
  Challenge& c = s.challenges[slot];
  c.address = from;
  c.value = static_cast<int32_t>(ge::core::next(s.rng) * 2147483647.0);
  c.made = ++s.challenges_made;
  return c.value;
}

void reply_challenge(ClassicServer& s, const gn::Address& from) {
  const int32_t value = new_challenge(s, from);
  uint8_t buf[128];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  gn::write_u32(w, 0xffffffffu);
  const std::string text = "c" + std::to_string(value);
  write_string(w, text);
  for (ExtFamily f : {ExtFamily::x, ExtFamily::x2, ExtFamily::mvd})
    if (s.cfg.offered[f] != 0) {
      gn::write_u32(w, family_magic(f));
      gn::write_u32(w, s.cfg.offered[f]);
    }
  send_oob(s, from, buf, w.size);
}

void reject(ClassicServer& s, const gn::Address& from, const std::string& why) {
  s.counts.rejects++;
  s.log.push_back("reject " + gn::to_string(from) + ": " + why);
  send_oob_print(s, from, why + "\n");
}

// A decimal or 0x-prefixed hex number filling the whole word; false for anything else.
bool parse_number(const std::string& t, uint32_t& out) {
  const bool hex = t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X');
  const char* first = t.data() + (hex ? 2 : 0);
  const char* last = t.data() + t.size();
  const auto [end, ec] = std::from_chars(first, last, out, hex ? 16 : 10);
  return ec == std::errc{} && end == last && first != last;
}

// The extension lines after a connect's first line: `0x<magic> 0x<bits>` each.
bool read_ext_lines(const std::string& text, Ext& asked) {
  bool any = false;
  size_t at = text.find('\n');
  while (at != std::string::npos && at + 1 < text.size()) {
    const size_t end = text.find('\n', at + 1);
    const std::string line = text.substr(at + 1, end == std::string::npos ? end : end - at - 1);
    pg::Words w;
    pg::tokenize_words(line.c_str(), w);
    uint32_t magic = 0, bits = 0;
    ExtFamily f{};
    if (w.count >= 2 && parse_number(pg::word(w, 0), magic) && parse_number(pg::word(w, 1), bits) &&
        family_of_magic(magic, f)) {
      asked[f] = bits;
      any = true;
    }
    at = end;
  }
  return any;
}

int rate_of(const ClassicServer& s, const std::string& info) {
  const std::string r = info_get(info, "rate");
  if (r.empty()) return 99999;
  int rate = std::atoi(r.c_str());
  if (rate == 0) return 2500;
  if (s.cfg.maxrate > 0 && rate > s.cfg.maxrate) rate = s.cfg.maxrate;
  return rate < 500 ? 500 : rate > 300000 ? 300000 : rate;
}

// A connecting client's VIP level (ServerConfig's notes): by a password word, by the address.
int vip_level(const ClassicServer& s, const gn::Address& from, const std::string& spec,
              const std::string& password) {
  int level = 0;
  for (size_t i = 0; i < s.cfg.vip_passwords.size(); i++) {
    const std::string& word = s.cfg.vip_passwords[i];
    if (word.empty() || word == "none" || (word != spec && word != password)) continue;
    level = i < s.cfg.vip_values.size() ? s.cfg.vip_values[i] : static_cast<int>(i) + 1;
    break;
  }
  if (const gs::BanEntry* e = gs::ban_match(s.vips, from, classic_unix_time(s)))
    level = std::max(level, e->level);
  return level;
}

void handle_connect(ClassicServer& s, const gn::Address& from, const std::string& text) {
  if (gs::ban_refuses(s.bans, s.cfg.ban_mode, from, classic_unix_time(s))) {
    reject(s, from, "You are banned from this server.");
    return;
  }
  pg::Words w;
  pg::tokenize_words(text.substr(0, text.find('\n')).c_str(), w);
  const int version = std::atoi(pg::word(w, 1));
  if (version != kProtocolVersion) {
    reject(s, from, "Server is version " + std::to_string(kProtocolVersion) + ".");
    return;
  }
  const auto qport = static_cast<uint16_t>(std::atoi(pg::word(w, 2)));
  const int32_t challenge = std::atoi(pg::word(w, 3));
  std::string userinfo = pg::word(w, 4);
  int found = -1;
  // The host's challenge that matches; any of the host's when none does (its reason a bad one). A
  // port that moved between the two asks still finds its challenge by the host and the value.
  for (int i = 0; i < 64; i++)
    if (s.challenges[i].made > 0 && same_host(s.challenges[i].address, from) &&
        (found < 0 || s.challenges[i].value == challenge))
      found = i;
  if (found < 0) {
    reject(s, from, "No challenge for address.");
    return;
  }
  if (s.challenges[found].value != challenge) {
    reject(s, from, "Bad challenge.");
    return;
  }
  if (userinfo.find("&c") != std::string::npos || userinfo.find("&r") != std::string::npos ||
      !info_valid(userinfo)) {
    reject(s, from, "Invalid userinfo. Restart your client.");
    return;
  }
  bool spectator = false;
  const std::string spec = info_get(userinfo, "spectator");
  const bool watching = !spec.empty() && spec != "0";
  const int vip = vip_level(s, from, watching ? spec : "", info_get(userinfo, "password"));
  if (watching) {
    if (vip == 0 && !s.cfg.spectator_password.empty() && s.cfg.spectator_password != "none" &&
        spec != s.cfg.spectator_password) {
      reject(s, from, "requires a spectator password");
      return;
    }
    spectator = true;
  } else if (vip == 0 && !s.cfg.password.empty() && s.cfg.password != "none" &&
             info_get(userinfo, "password") != s.cfg.password) {
    reject(s, from, "server requires a password");
    return;
  }
  info_remove(userinfo, "*VIP");
  if (vip > 0) info_set(userinfo, "*VIP", std::to_string(vip));
  info_remove(userinfo, "spectator");
  info_remove(userinfo, "password");
  if (spectator) info_set(userinfo, "*spectator", "1");

  // A reconnect from the same address and qport drops the old slot, unless that slot connected
  // too short a time ago (sv_reconnectwait): then the storm gets no answer at all.
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClassicClient& c = s.clients[i];
    if (c.state == ConnState::free || c.local || !same_host(c.address, from) ||
        (c.ch.qport != qport && c.address.port != from.port))
      continue;
    if (s.cfg.reconnect_wait_s > 0 && s.game->sv.time - c.connect_time < s.cfg.reconnect_wait_s) {
      s.counts.rejects++;
      s.log.push_back("reject " + gn::to_string(from) + ": reconnected too soon");
      return;
    }
    classic_drop(s, i, "reconnected");
  }
  const int maxclients = s.game->sv.set.maxclients;
  // Spectators in their own slots (sv_maxspectators): the players' and theirs counted apart.
  if (s.cfg.max_spectators > 0 || s.cfg.max_vip_spectators > 0) {
    int players = 0, watchers = 0;
    for (int i = 0; i < maxclients && i < sh::kMaxClients; i++) {
      const ClassicClient& c = s.clients[i];
      if (c.state != ConnState::free)
        (c.spectator ? watchers : players)++;
      else if (s.game->clients[i].active)
        players++;
    }
    // A VIP may watch from the VIP spectator slots too (sv_maxvipspectators).
    const int vip_seats = std::max(0, s.cfg.max_vip_spectators);
    const int most = maxclients - s.cfg.max_spectators - vip_seats;
    const int watch_seats = s.cfg.max_spectators + (vip > 0 ? vip_seats : 0);
    if (!spectator && players >= most) {
      const int svf = std::atoi(info_get(userinfo, "svf").c_str());
      const bool forced = watchers < watch_seats && ((s.cfg.full_spectate == 1 && (svf & 2) == 0) ||
                                                     (s.cfg.full_spectate == 2 && (svf & 1) != 0));
      if (!forced) {
        reject(s, from, "server is full");
        return;
      }
      send_oob_print(s, from, "server is full: connecting as spectator\n");
      spectator = true;
      info_set(userinfo, "*spectator", "1");
    } else if (spectator && watchers >= watch_seats) {
      reject(s, from, "server is full");
      return;
    }
  }
  int slot = -1;
  for (int i = 0; i < maxclients && i < sh::kMaxClients; i++)
    if (s.clients[i].state == ConnState::free && !s.game->clients[i].active) {
      slot = i;
      break;
    }
  if (s.cfg.most_players > 0) {
    int seated = 0;
    for (int i = 0; i < maxclients && i < sh::kMaxClients; i++)
      if (s.clients[i].state != ConnState::free || s.game->clients[i].active) seated++;
    if (seated >= s.cfg.most_players) slot = -1;
  }
  if (slot < 0) {
    reject(s, from, "server is full");
    return;
  }

  ClassicClient& c = s.clients[slot];
  c = ClassicClient{};
  c.state = ConnState::connected;
  c.address = from;
  c.spectator = spectator;
  c.userid = s.next_userid;
  s.next_userid = s.next_userid % 99 + 1;
  channel_setup(c.ch, false, qport, s.now);
  Ext asked;
  const bool named = read_ext_lines(text, asked);
  const std::string z = info_get(userinfo, "*z_ext");
  asked[ExtFamily::z] = z.empty() ? 0u : static_cast<uint32_t>(std::atoi(z.c_str()));
  info_remove(userinfo, "*z_ext");
  const std::string ge = info_get(userinfo, "*ge_ext");
  asked[ExtFamily::ge] = ge.empty() ? 0u : static_cast<uint32_t>(std::atoi(ge.c_str()));
  info_remove(userinfo, "*ge_ext");
  // A player aid's grant (classic_admin.hpp) is the server's to give.
  for (const ge::server::AidKind& k : classic_aids()) info_remove(userinfo, k.key);
  c.ext = agree(s.cfg.offered, asked);
  c.coords = coords_of(c.ext);
  const bool proxy = !info_get(userinfo, "Qizmo").empty() || !info_get(userinfo, "*qtv").empty();
  c.need_pext = !named && !proxy;
  std::vector<std::string> taken;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (i != slot && s.clients[i].state != ConnState::free) taken.push_back(s.clients[i].name);
  c.name = unique_name(sanitize_name(info_get(userinfo, "name"), s.cfg.default_name), taken);
  if (s.name_rule != nullptr) s.name_rule(s.name_ctx, slot, c.name, true);
  info_set(userinfo, "name", c.name);
  c.userinfo = userinfo;
  const std::string msg = info_get(userinfo, "msg");
  c.msg_level = msg.empty() ? 1 : std::atoi(msg.c_str());
  channel_set_rate(c.ch, rate_of(s, userinfo), std::atoi(info_get(userinfo, "dupe").c_str()));
  c.connect_time = s.game->sv.time;
  c.voice_to = s.cfg.voice_route == VoiceRoute::all ? VoiceTo::all : VoiceTo::team;
  c.datagram.reserve(kMaxMsgLen);
  s.counts.connects++;
  s.log.push_back("connect " + std::to_string(slot) + " " + c.name + " " + gn::to_string(from) +
                  " ext " + describe_ext(c.ext));
  const uint8_t accept[5] = {0xff, 0xff, 0xff, 0xff, 'j'};
  send_oob(s, from, accept, sizeof accept);
  full_client_update(s, slot, -1);
}

void handle_oob(ClassicServer& s, const gn::Address& from, const uint8_t* data, size_t size) {
  s.counts.oob_in++;
  const std::string text(reinterpret_cast<const char*>(data) + 4,
                         strnlen(reinterpret_cast<const char*>(data) + 4, size - 4));
  pg::Words w;
  pg::tokenize_words(text.substr(0, text.find('\n')).c_str(), w);
  const std::string cmd = pg::word(w, 0);
  // A ping: "ping", or `k` alone or before a newline (a forwarder's probe).
  if (cmd == "ping" || cmd == "k") {
    const uint8_t l = 'l';
    send_oob(s, from, &l, 1);
  } else if (cmd == "status") {
    send_oob_print(s, from, status_text(s, std::atoi(pg::word(w, 1))));
  } else if (cmd == "getchallenge") {
    reply_challenge(s, from);
  } else if (cmd == "connect") {
    handle_connect(s, from, text);
  } else if (cmd == "rcon") {
    classic_rcon(s, from, text.substr(0, text.find('\n')));
  }
  // `ip`, `log` and the server extras are not answered.
}

// The packet's commands the loop will run (the lost ones' copies first, as it runs them) through
// the safe strafe; the two it ran last packet stay as guarded, so the copies agree.
void guard_strafes(int n, StrafeGuard& g, sh::Packet& p) {
  const uint32_t lost = g.seq == 0 ? 0 : p.sequence - g.seq - 1;
  if (lost == 0 && g.seq != 0) {
    p.cmds[0] = g.last[0];
    p.cmds[1] = g.last[1];
  } else {
    if (lost > 1) p.cmds[0].side = safe_strafe(g, p.cmds[0].side, n);
    if (lost > 0) p.cmds[1].side = safe_strafe(g, p.cmds[1].side, n);
  }
  p.cmds[2].side = safe_strafe(g, p.cmds[2].side, n);
  g.seq = p.sequence;
  g.last[0] = p.cmds[1];
  g.last[1] = p.cmds[2];
}

// A move: the checksum's verdict, then the loop's packet.
void read_move(ClassicServer& s, int slot, const ClcMsg& m, const uint8_t* payload, bool& moved) {
  ClassicClient& c = s.clients[slot];
  if (moved) return;  // at most one move a packet
  moved = true;
  c.moves++;
  if (s.cfg.verify_move_checksum) {
    const uint8_t want =
        move_checksum(payload + m.body_at, m.body_end - m.body_at, c.ch.incoming_sequence);
    if (want != m.checksum) {
      c.moves_bad_checksum++;
      return;
    }
  } else {
    c.moves_unchecked++;
  }
  c.loss_pct = m.loss;
  if (c.state != ConnState::spawned || !s.game->sv.clients[static_cast<size_t>(slot)].active)
    return;
  sh::Packet p;
  p.client = slot;
  p.sequence = c.ch.incoming_sequence;
  for (int i = 0; i < 3; i++) p.cmds[i] = m.cmds[i];
  if (s.cfg.safe_strafe > 0 && !c.spectator) guard_strafes(s.cfg.safe_strafe, c.strafe, p);
  s.packets.push_back(p);
}

// A client's voice relayed to those who hear it, and into a multiview recording.
void relay_voice(ClassicServer& s, int slot, const ClcMsg& m) {
  const ClassicClient& c = s.clients[slot];
  if (!s.cfg.voice || !c.ext.has(ExtFamily::x2, kX2Voice) || c.state != ConnState::spawned ||
      m.data.size() > kMostVoiceBytes) {
    s.counts.voice_refused++;
    return;
  }
  s.counts.voice_in++;
  SvcMsg v;
  v.id = svc::voice;
  v.ints = {slot, m.gen, m.seq};
  v.data = m.data;
  uint8_t buf[kMaxMsgLen + 8];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  write_svc(w, v, WireCtx{});
  for (int j = 0; j < sh::kMaxClients && !w.overflow; j++) {
    ClassicClient& d = s.clients[j];
    if ((j == slot && !s.cfg.voice_echo) || d.state != ConnState::spawned ||
        !d.ext.has(ExtFamily::x2, kX2Voice) || !d.voice_heard || (d.voice_ignored >> slot & 1u))
      continue;
    // By the talker's target (classic_talk.hpp): everyone, his team, the players near, one.
    if (j != slot && !talk_hears(s, slot, j, c.voice_to, c.voice_one)) continue;
    if (d.local) {  // the listen server's own player: handed over, no packet carries it
      if (s.local_voice == nullptr) continue;
      s.local_voice(s.local_ctx, j, v);
      s.counts.voice_out++;
      continue;
    }
    if (d.datagram.size() + w.size > kMaxMsgLen / 2) {
      s.counts.voice_dropped++;
      continue;
    }
    d.datagram.insert(d.datagram.end(), buf, buf + w.size);
    s.counts.voice_out++;
  }
  if (s.mvd) mvd_voice(*s.mvd, v);
  for (MultiviewRecorder* r : s.streams) mvd_voice(*r, v);
}

void handle_packet(ClassicServer& s, int slot, const uint8_t* data, size_t size) {
  ClassicClient& c = s.clients[slot];
  size_t at = 0;
  if (!channel_process(c.ch, data, size, s.now, at)) return;
  c.heard = true;
  ServerFrame& acked = c.frames[c.ch.incoming_acknowledged & 63];
  if (acked.sequence == c.ch.incoming_acknowledged && acked.sequence != 0 && acked.sent > 0) {
    const double ping = s.now - acked.sent;
    c.ping_s = c.ping_samples == 0 ? ping : c.ping_s + (ping - c.ping_s) / 16.0;
    c.ping_samples++;
    acked.sent = -acked.sent;  // counted once
  }
  // The hold toward the minimum ping (sv_minping), 1 ms a packet; never for a spectator.
  if (s.cfg.min_ping_ms > 0 && !c.spectator && !s.game->paused && c.ping_samples > 0) {
    const double ms = c.ping_s * 1000;
    if (ms < s.cfg.min_ping_ms) c.hold_s = std::min(1.0, c.hold_s + 0.001);
    if (ms > s.cfg.min_ping_ms + 1) c.hold_s = std::max(0.0, c.hold_s - 0.001);
  } else if (s.cfg.min_ping_ms <= 0 || c.spectator) {
    c.hold_s = 0;
  }
  gn::Reader r = gn::make_reader(data, size);
  r.at = at;
  bool moved = false;
  while (remaining(r) > 0) {
    ClcMsg m;
    std::string error;
    if (!read_clc(r, m, error)) {
      classic_drop(s, slot, error);
      return;
    }
    switch (m.id) {
      case clc::move:
        read_move(s, slot, m, data, moved);
        break;
      case clc::stringcmd:
        string_command(s, slot, m.text);
        if (c.state == ConnState::free) return;
        break;
      case clc::delta:
        c.delta_byte = m.delta;
        break;
      case clc::tmove:
        if (c.spectator && c.rules_active)
          s.game->sv.edicts[static_cast<size_t>(slot) + 1].origin = m.origin;
        break;
      case clc::upload:
        client_print(s, slot, 2, "Uploads are not taken.\n");
        break;
      case clc::voice:
        relay_voice(s, slot, m);
        break;
      default:
        break;
    }
  }
}

// Moves each other live human player to where the shooter saw him (begin) or back (end). The
// saved origins live in the server: one lagged trace at a time, restored before it returns.
void lag_rewind(void* ctx, int shooter, bool begin) {
  ClassicServer& s = *static_cast<ClassicServer*>(ctx);
  const int me = shooter - 1;
  if (me < 0 || me >= sh::kMaxClients) return;
  if (!begin) {
    for (int j = 0; j < sh::kMaxClients; j++)
      if (s.lag_moved[j]) sh::set_origin(s.game->sv, j + 1, s.lag_saved[j]);
    for (bool& m : s.lag_moved) m = false;
    return;
  }
  const ClassicClient& sc = s.clients[me];
  if (sc.state != ConnState::spawned || sc.local) return;  // a bot aims at where players are
  const ServerFrame& f = sc.frames[sc.ch.incoming_acknowledged & 63];
  const double acked = f.sequence == sc.ch.incoming_acknowledged ? f.server_time : s.game->sv.time;
  const std::string maxfps = info_get(s.serverinfo, "maxfps");
  const double target =
      antilag_target(acked, sc.ping_s, maxfps.empty() ? 77 : std::atoi(maxfps.c_str()),
                     s.game->sv.time, s.cfg.antilag_exact);
  s.lag_rewinds++;
  for (int j = 0; j < sh::kMaxClients; j++) {
    if (j == me || !s.clients[j].rules_active || s.clients[j].spectator) continue;
    sh::Edict& e = s.game->sv.edicts[static_cast<size_t>(j) + 1];
    if (e.health <= 0) continue;
    sh::Vec3f at;
    if (!antilag_position(s.antilag.players[j], target, e.velocity, at)) continue;
    s.lag_saved[j] = e.origin;
    s.lag_moved[j] = true;
    sh::set_origin(s.game->sv, j + 1, at);
  }
}

}  // namespace

void send_oob(ClassicServer& s, const gn::Address& to, const uint8_t* data, size_t size) {
  gn::transport_send(*s.transport, to, data, size, static_cast<int64_t>(s.now * 1e6));
  s.counts.oob_out++;
  s.counts.datagrams_out++;
  s.counts.bytes_out += static_cast<int64_t>(size);
}

void send_oob_print(ClassicServer& s, const gn::Address& to, const std::string& text) {
  // A long text goes in several packets, each under the smallest MTU.
  size_t at = 0;
  do {
    const size_t n = text.size() - at < kMinMtu - 10 ? text.size() - at : kMinMtu - 10;
    std::vector<uint8_t> buf = {0xff, 0xff, 0xff, 0xff, 'n'};
    buf.insert(buf.end(), text.begin() + static_cast<std::ptrdiff_t>(at),
               text.begin() + static_cast<std::ptrdiff_t>(at + n));
    buf.push_back(0);
    send_oob(s, to, buf.data(), buf.size());
    at += n;
  } while (at < text.size());
}

void reliable_bytes(ClassicServer& s, int slot, const uint8_t* data, size_t size) {
  ClassicClient& c = s.clients[slot];
  if (c.state == ConnState::free || c.local) return;
  channel_reliable(c.ch, data, size);
}

void reliable_svc(ClassicServer& s, int slot, const SvcMsg& m) {
  uint8_t buf[kMaxMsgLen];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  WireCtx ctx;
  ctx.ext = s.clients[slot].ext;
  ctx.chunked = ctx.ext.has(ExtFamily::x, kXChunkedDl);
  write_svc(w, m, ctx);
  if (!w.overflow) reliable_bytes(s, slot, buf, w.size);
}

void stuff(ClassicServer& s, int slot, const std::string& text) {
  SvcMsg m;
  m.id = svc::stufftext;
  m.strings = {text};
  reliable_svc(s, slot, m);
}

void client_print(ClassicServer& s, int slot, int level, const std::string& text) {
  if (s.clients[slot].local) {
    if (s.local_print != nullptr) s.local_print(s.local_ctx, slot, text);
    return;
  }
  if (level < s.clients[slot].msg_level && level < 3) return;
  SvcMsg m;
  m.id = svc::print;
  m.ints = {level};
  m.strings = {text};
  reliable_svc(s, slot, m);
}

// What the endpoint reads off a map's server once: the player model, the mod's fields, the
// map's static sounds and signon writes, the baselines and the server info.
bool bind_map(ClassicServer& s, const sv::RulesRunner& rules, std::string& error) {
  if (!rules || rules->models.empty()) {
    error = "the server has no map running";
    return false;
  }
  s.rules = &rules;
  s.game = rules.common;
  sv::RulesCommon& qc = *rules.common;
  s.player_model = 0;
  s.static_sounds.clear();
  s.signon.clear();
  for (size_t i = 1; i < qc.models.size(); i++)
    if (qc.models[i] == "progs/player.mdl") s.player_model = static_cast<int>(i);
  // What the map's load wrote: its static sounds and signon writes; its statics are qc.statics.
  for (const sv::NetEvent& ev : qc.out.events)
    if (ev.kind == sv::EventKind::ambient) {
      int index = 0;
      for (size_t i = 1; i < qc.sounds.size(); i++)
        if (qc.sounds[i] == ev.text) index = static_cast<int>(i);
      s.static_sounds.push_back(
          {ev.origin, index, static_cast<int>(ev.values[0]), static_cast<int>(ev.values[1])});
    }
  if (!qc.out.init.empty()) {
    if (qc.out.init.size() > kMaxMsgLen / 2) {
      error = "classic_start: the map's signon writes are " + std::to_string(qc.out.init.size()) +
              " bytes, more than one signon buffer carries (" + std::to_string(kMaxMsgLen / 2) +
              ")";
      return false;
    }
    s.signon.push_back(qc.out.init);
  }
  s.statics_known = static_cast<int>(qc.statics.size());
  sv::clear_frame(qc.out);
  s.baselines.assign(static_cast<size_t>(qc.sv.set.max_edicts), EntityState{});
  for (int e = 0; e < qc.sv.num_edicts; e++) {
    s.baselines[static_cast<size_t>(e)].number = e;
    if (!qc.sv.edicts[static_cast<size_t>(e)].free)
      s.baselines[static_cast<size_t>(e)] = entity_state(s, e);
  }
  for (size_t e = static_cast<size_t>(qc.sv.num_edicts); e < s.baselines.size(); e++)
    s.baselines[e].number = static_cast<int>(e);

  s.serverinfo.clear();
  info_set(s.serverinfo, "*version", "ShootersEdge 0.1", kServerInfoBytes);
  info_set(s.serverinfo, "*z_ext", std::to_string(s.cfg.offered[ExtFamily::z]), kServerInfoBytes);
  if (s.cfg.offered[ExtFamily::ge] != 0)
    info_set(s.serverinfo, "*ge_ext", std::to_string(s.cfg.offered[ExtFamily::ge]),
             kServerInfoBytes);
  info_set(s.serverinfo, "hostname", s.cfg.hostname, kServerInfoBytes);
  info_set(s.serverinfo, "maxclients", std::to_string(qc.sv.set.maxclients), kServerInfoBytes);
  info_set(s.serverinfo, "maxfps", std::to_string(s.cfg.maxfps), kServerInfoBytes);
  info_set(s.serverinfo, "sv_antilag", s.cfg.antilag ? "1" : "0", kServerInfoBytes);
  // What a server browser lists (classic_browse.hpp): the map, the rules' kind and whether
  // joining takes a password.
  info_set(s.serverinfo, "map", qc.map_name, kServerInfoBytes);
  // The skill only where monsters play it (a match's skill is the mod's leftover default).
  bool monsters = true;
  for (const char* rule : {"deathmatch", "teamplay", "coop"}) {
    const ge::console::Cvar* c = qc.cvars.find(rule);
    if (c == nullptr || c->value == "0") continue;
    info_set(s.serverinfo, rule, c->value, kServerInfoBytes);
    if (std::string_view(rule) == "deathmatch") monsters = false;
  }
  if (const ge::console::Cvar* c = qc.cvars.find("skill"); c != nullptr && monsters)
    info_set(s.serverinfo, "skill", c->value, kServerInfoBytes);
  // A cooperative campaign says `deathmatch 0` outright, as other coop servers do.
  if (const ge::console::Cvar* coop = qc.cvars.find("coop"); coop != nullptr && coop->on())
    info_set(s.serverinfo, "deathmatch", "0", kServerInfoBytes);
  const int needpass =
      (s.cfg.password.empty() ? 0 : 1) | (s.cfg.spectator_password.empty() ? 0 : 2);
  if (needpass != 0) info_set(s.serverinfo, "needpass", std::to_string(needpass), kServerInfoBytes);
  // The move's switches a client predicts with, when not at their defaults (pm_ktjump 0).
  for (const auto& [k, v] : sh::changed_info(qc.sv.set))
    info_set(s.serverinfo, k, v, kServerInfoBytes);
  for (const auto& [k, v] : qc.serverinfo) info_set(s.serverinfo, k, v, kServerInfoBytes);
  for (const auto& [k, v] : s.cfg.info)
    if (!v.empty()) info_set(s.serverinfo, k, v, kServerInfoBytes);
  if (s.cfg.antilag) {
    qc.lag_rewind = &lag_rewind;
    qc.lag_ctx = &s;
  }
  return true;
}

bool classic_start(ClassicServer& s, const sv::RulesRunner& rules, gn::Transport& t,
                   const ServerConfig& cfg, std::string& error) {
  s.cfg = cfg;
  s.transport = &t;
  s.rng = ge::core::make_rng(cfg.seed);
  if (cfg.voice) s.cfg.offered[ExtFamily::x2] |= kX2Voice;
  if (!bind_map(s, rules, error)) return false;
  s.packets.reserve(static_cast<size_t>(sh::kMaxClients) * 4);
  return true;
}

bool classic_change(ClassicServer& s, const sv::RulesRunner& next, std::string& error) {
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].state != ConnState::free) stuff(s, i, "changing\n");
  if (s.game != nullptr) {
    s.game->lag_rewind = nullptr;
    s.game->lag_ctx = nullptr;
  }
  if (!bind_map(s, next, error)) return false;
  s.spawncount++;
  for (int i = 0; i < sh::kMaxClients; i++) {
    ClassicClient& c = s.clients[i];
    antilag_clear(s.antilag, i);
    if (c.state == ConnState::free || c.local) continue;  // the game seats its own on `next`
    c.state = ConnState::connected;
    c.rules_active = false;
    c.delta_byte = -1;
    for (ServerFrame& f : c.frames) f = ServerFrame{};
    for (bool& known : c.stats_known) known = false;
    c.statics_sent = 0;
    c.dl = ServerDownload{};
    stuff(s, i, "reconnect\n");
  }
  s.log.push_back("change " + next->map_name);
  return true;
}

void classic_unbind(ClassicServer& s) {
  if (s.game != nullptr) {
    s.game->lag_rewind = nullptr;
    s.game->lag_ctx = nullptr;
  }
  s.game = nullptr;
  s.rules = nullptr;
}

void classic_drop(ClassicServer& s, int slot, const std::string& why) {
  ClassicClient& c = s.clients[slot];
  if (c.state == ConnState::free) return;
  uint8_t buf[kMaxPacket];
  const uint8_t bye = svc::disconnect;
  for (int i = 0; i < (c.local ? 0 : 2); i++) {
    const size_t n = channel_transmit(c.ch, &bye, 1, s.now, buf);
    gn::transport_send(*s.transport, c.address, buf, n, static_cast<int64_t>(s.now * 1e6));
    s.counts.datagrams_out++;
    s.counts.bytes_out += static_cast<int64_t>(n);
  }
  if (c.rules_active && s.unseat != nullptr)
    s.unseat(s.seat_ctx, slot);
  else if (c.rules_active && s.rules != nullptr)
    s.rules->drop(slot);
  antilag_clear(s.antilag, slot);
  std::string line = "drop " + std::to_string(slot);
  line += ' ';
  line += c.name;
  line += ": ";
  line += why;
  s.log.push_back(line);
  s.counts.drops++;
  c = ClassicClient{};
  full_client_update(s, slot, -1);
}

bool classic_add_local(ClassicServer& s, int slot, const std::string& name, std::string& error,
                       bool bot) {
  if (slot < 0 || slot >= sh::kMaxClients || s.clients[slot].state != ConnState::free) {
    error = "slot " + std::to_string(slot) + " is taken by a player";
    return false;
  }
  ClassicClient& c = s.clients[slot];
  c = ClassicClient{};
  c.state = ConnState::spawned;
  c.local = true;
  c.rules_active = true;
  c.name = name;
  c.userid = s.next_userid;
  s.next_userid = s.next_userid % 99 + 1;
  c.connect_time = s.game->sv.time;
  info_set(c.userinfo, "name", name);
  if (bot) info_set(c.userinfo, "*bot", "1");
  if (!bot && s.cfg.voice) c.ext[ExtFamily::x2] |= kX2Voice;  // the window's player talks, hears
  c.voice_to = s.cfg.voice_route == VoiceRoute::all ? VoiceTo::all : VoiceTo::team;
  c.voice_heard = !bot;  // no client to send `unmuteall`: he listens from the start
  s.log.push_back("local " + std::to_string(slot) + " " + name);
  full_client_update(s, slot, -1);
  return true;
}

void classic_local_voice(ClassicServer& s, int slot, const ClcMsg& m) {
  if (slot < 0 || slot >= sh::kMaxClients || !s.clients[slot].local) return;
  relay_voice(s, slot, m);
}

void classic_set_info(ClassicServer& s, int slot, const std::string& key,
                      const std::string& value) {
  ClassicClient& c = s.clients[slot];
  if (c.state == ConnState::free || info_get(c.userinfo, key) == value) return;
  if (!info_set(c.userinfo, key, value)) return;
  if (key == "name") c.name = value;
  if (c.rules_active) s.rules->user_info(slot, key, value);
  for (int j = 0; j < sh::kMaxClients; j++) {
    if (s.clients[j].state == ConnState::free) continue;
    SvcMsg m;
    m.id = svc::setinfo;
    m.ints = {slot};
    m.strings = {key, value};
    reliable_svc(s, j, m);
  }
}

void classic_print_all(ClassicServer& s, const std::string& text) {
  for (int j = 0; j < sh::kMaxClients; j++)
    if (s.clients[j].state == ConnState::spawned) client_print(s, j, 2, text);
}

int classic_count(const ClassicServer& s, ConnState at_least) {
  int n = 0;
  for (const ClassicClient& c : s.clients)
    if (c.state != ConnState::free && static_cast<int>(c.state) >= static_cast<int>(at_least)) n++;
  return n;
}

namespace {

std::string quoted(const std::string& s) {
  std::string q;
  q.reserve(s.size() + 2);
  q += '"';
  q += s;
  q += '"';
  return q;
}

}  // namespace

std::string status_text(const ClassicServer& s, int opt) {
  std::string t = s.serverinfo + "\n";
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClassicClient& c = s.clients[i];
    if (c.state == ConnState::free) continue;
    int frags = 0;
    if (c.rules_active) frags = static_cast<int>(s.game->world.clients[i].numbers.frags);
    const int ping = c.state != ConnState::spawned ? 999
                     : c.ping_samples == 0         ? 9999
                                                   : static_cast<int>(c.ping_s * 1000);
    const int minutes = static_cast<int>((s.game->sv.time - c.connect_time) / 60);
    auto colour = [&](const char* key) {
      int v = std::atoi(info_get(c.userinfo, key).c_str());
      return v < 0 ? 0 : v > 16 ? 16 : v;
    };
    std::string name = c.name;
    std::string fr = std::to_string(frags);
    if (c.spectator) {
      name.insert(0, "\\s\\");
      fr = (opt & 8) ? "S" : "-9999";
    }
    const std::string fields[] = {std::to_string(c.userid),
                                  fr,
                                  std::to_string(minutes),
                                  std::to_string(c.spectator ? -ping : ping),
                                  quoted(name),
                                  quoted(info_get(c.userinfo, "skin")),
                                  std::to_string(colour("topcolor")),
                                  std::to_string(colour("bottomcolor"))};
    for (size_t k = 0; k < std::size(fields); k++) {
      if (k > 0) t += ' ';
      t += fields[k];
    }
    if (opt & 16) {
      t += ' ';
      t += quoted(info_get(c.userinfo, "team"));
    }
    if (opt & 64) t += " \"\"";
    if (opt & 128) t += " \"h\"";
    t += "\n";
  }
  return t;
}

namespace {

// A look value as a byte: below `lo` it is `lo`, above 255 it is 255, the fraction dropped.
int look_byte(float v, int lo) {
  return v < static_cast<float>(lo) ? lo : v > 255 ? 255 : static_cast<int>(v);
}

}  // namespace

void entity_look(EntityState& st, float alpha, float scale, const float colormod[3]) {
  if (alpha != 1) {
    const int v = static_cast<int>(alpha * 254);
    st.alpha = alpha >= 1 ? 0 : v < 1 ? 1 : v > 254 ? 254 : v;
  }
  st.scale = scale == 1 ? 16 : look_byte(scale * 16, 1);
  for (int c = 0; c < 3; c++) st.colormod[c] = look_byte(colormod[c] * 32, 0);
}

void status_players(const ClassicServer& s, std::vector<gn::StatusPlayer>& out) {
  out.clear();
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClassicClient& c = s.clients[i];
    if (c.state == ConnState::free) continue;
    gn::StatusPlayer p;
    p.name = c.name;
    p.team = info_get(c.userinfo, "team");
    p.score = c.rules_active ? static_cast<int32_t>(s.game->world.clients[i].numbers.frags) : 0;
    const int ping = c.state != ConnState::spawned || c.ping_samples == 0
                         ? 0
                         : static_cast<int>(c.ping_s * 1000);
    p.ping_ms = static_cast<uint16_t>(ping < 0 ? 0 : ping > 9999 ? 9999 : ping);
    const double minutes = (s.game->sv.time - c.connect_time) / 60;
    p.minutes = static_cast<uint16_t>(minutes < 0 ? 0 : minutes > 65535 ? 65535 : minutes);
    p.flags = static_cast<uint8_t>((c.local ? gn::kPlayerBot : 0) |
                                   (c.spectator ? gn::kPlayerSpectator : 0));
    out.push_back(std::move(p));
  }
}

EntityState entity_state(const ClassicServer& s, int e) {
  EntityState st;
  st.number = e;
  const sv::WorldEntity& row = s.game->world.entities[static_cast<size_t>(e)];
  st.modelindex = row.modelindex;
  st.frame = row.frame;
  st.colormap = row.colormap;
  st.skin = row.skin;
  st.effects = row.effects;
  st.origin = row.origin;
  st.angles = row.angles;
  entity_look(st, row.alpha, row.scale, row.colormod);
  return st;
}

namespace {

// A score that moved this frame told to every client, as the scoreboard reads it.
void tell_frags(ClassicServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    ClassicClient& c = s.clients[i];
    if (!c.rules_active) continue;
    const int frags = static_cast<int>(s.game->world.clients[i].numbers.frags);
    if (frags == c.frags_told) continue;
    c.frags_told = frags;
    SvcMsg m;
    m.id = svc::updatefrags;
    m.ints = {i, frags};
    for (int j = 0; j < sh::kMaxClients; j++)
      if (s.clients[j].state != ConnState::free) reliable_svc(s, j, m);
  }
}

}  // namespace

void classic_frame(ClassicServer& s, int64_t now_us) {
  s.now = static_cast<double>(now_us) / 1e6;
  s.frames++;
  s.packets.clear();
  gn::transport_pump(*s.transport, now_us);
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  while (true) {
    const int n = gn::transport_receive(*s.transport, buf, sizeof buf, from, now_us);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    s.counts.datagrams_in++;
    s.counts.bytes_in += n;
    if (is_out_of_band(buf, size)) {
      handle_oob(s, from, buf, size);
      continue;
    }
    uint16_t qport = 0;
    if (!peek_qport(buf, size, qport)) continue;
    int slot = -1;
    for (int i = 0; i < sh::kMaxClients; i++) {
      ClassicClient& c = s.clients[i];
      if (c.state == ConnState::free || !same_host(c.address, from) || c.ch.qport != qport)
        continue;
      c.address = from;  // a NAT may have moved the port
      slot = i;
      break;
    }
    if (slot < 0) continue;
    ClassicClient& c = s.clients[slot];
    if (c.hold_s > 0 || !c.held.empty()) {  // held back for the minimum ping
      c.held.push_back({s.now + c.hold_s, std::vector<uint8_t>(buf, buf + size)});
      continue;
    }
    handle_packet(s, slot, buf, size);
  }
  for (int i = 0; i < sh::kMaxClients; i++) {  // the held packets whose time has come, in order
    ClassicClient& c = s.clients[i];
    size_t let = 0;
    while (let < c.held.size() && c.held[let].at <= s.now) let++;
    if (let == 0) continue;
    std::vector<ClassicClient::Held> due(std::make_move_iterator(c.held.begin()),
                                         std::make_move_iterator(c.held.begin() + let));
    c.held.erase(c.held.begin(), c.held.begin() + static_cast<std::ptrdiff_t>(let));
    for (const ClassicClient::Held& h : due) {
      if (s.clients[i].state == ConnState::free) break;
      handle_packet(s, i, h.bytes.data(), h.bytes.size());
    }
  }
  for (const sh::Packet& p : s.local_moves)  // the game's own seats, after the network's
    if (p.client >= 0 && p.client < sh::kMaxClients && s.clients[p.client].local &&
        s.game->sv.clients[static_cast<size_t>(p.client)].active)
      s.packets.push_back(p);
  s.local_moves.clear();

  s.rules->frame(s.packets.data(), static_cast<int>(s.packets.size()));
  if (s.local_tap != nullptr) s.local_tap(s.local_ctx, *s.game);
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].rules_active)
      antilag_record(s.antilag, i, s.game->sv.time,
                     s.game->sv.edicts[static_cast<size_t>(i) + 1].origin);
  route_events(s);
  tell_frags(s);
  if (s.mvd) mvd_frame(s, *s.mvd);
  for (MultiviewRecorder* r : s.streams) mvd_frame(s, *r);

  for (int i = 0; i < sh::kMaxClients; i++) {
    ClassicClient& c = s.clients[i];
    if (c.state == ConnState::free || c.local) continue;
    if (c.ch.overflow)
      classic_drop(s, i, "reliable channel overflowed");
    else if (s.now - c.ch.last_received > s.cfg.timeout_s) {
      const std::string name = c.name;
      classic_drop(s, i, "timed out");
      classic_print_all(s, name + " timed out\n");
    } else if (c.rules_active && s.game->clients[i].drop)
      classic_drop(s, i, "dropped by the mod");
  }
  if (s.now - s.last_ping_update >= 2) {
    s.last_ping_update = s.now;
    for (int i = 0; i < sh::kMaxClients; i++) {
      ClassicClient& c = s.clients[i];
      if (c.state == ConnState::free) continue;
      const uint64_t acc = c.ch.accepted - c.last_accepted, drop = c.ch.dropped - c.last_dropped;
      c.loss_pct = acc + drop ? static_cast<int>(100 * drop / (acc + drop)) : 0;
      c.last_accepted = c.ch.accepted;
      c.last_dropped = c.ch.dropped;
      for (int j = 0; j < sh::kMaxClients; j++) {
        if (s.clients[j].state != ConnState::spawned) continue;
        SvcMsg m;
        m.id = svc::updateping;
        m.ints = {i, static_cast<int>(c.ping_s * 1000)};
        reliable_svc(s, j, m);
        m.id = svc::updatepl;
        m.ints = {i, c.loss_pct};
        reliable_svc(s, j, m);
      }
    }
  }
  send_frames(s);
}

}  // namespace shooters_edge::net
