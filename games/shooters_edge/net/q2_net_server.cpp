// Quake II's net server (q2_net_server.hpp): out of band, the signon by the client's
// commands, the moves into the rules, the frames out.
//
// ge:summary Quake II's net server's steps: challenges, connects, the signon, moves and frames
// ge:keywords q2-server q2_net_frame signon configstrings baselines begin move frame
// ge:keywords rate suppress-count rate-hold sv_maxrate sv_minrate
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#include "shooters_edge/net/q2_net_server.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <span>
#include <utility>

#include "classic/content/web_downloads.hpp"
#include "classic/rulesets.hpp"
#include "core/deflate.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"
#include "server/admission.hpp"
#include "server/operator.hpp"
#include "server/rate.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/q2_admin.hpp"
#include "shooters_edge/net/q2_mvd_dummy.hpp"
#include "shooters_edge/net/q2_net_download.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::net {

namespace {

namespace pg = ge::progs;

constexpr size_t kSignonShare = 700;  // bytes of the signon one answer adds before its prompt
constexpr int kStringCmdsPerPacket = 7;
constexpr uint8_t kSvcZPacket = 21;   // a compressed block of messages (35 and 36)
constexpr uint8_t kSvcReconnect = 8;  // a full restart: connect again (`recycle`)
constexpr uint8_t kSvcDisconnect = 7, kSvcPrint = 10, kSvcStuffText = 11, kSvcDownload = 16;
constexpr uint8_t kSvcGeMoves = 124;       // the game's own: the other players' moves
constexpr uint8_t kSvcGeShare = 126;       // the game's own: the easy jump's share
constexpr uint8_t kSvcGeAccountKey = 127;  // the game's own: the logins' key tag
constexpr uint8_t kSvcGeFeel = 122;        // the game's own: the move's feel
constexpr uint8_t kSvcGeBack = 119;        // the game's own: an item taken and when it comes back
constexpr uint8_t kSvcGeArsenal = 118;     // the game's own: the weapons held, the ammunition
constexpr size_t kVoiceTickBytes = 1200;   // the voices one client is sent a tick, at most
constexpr size_t kMovesBlockMost = 1200;   // the frame with the moves, at most (under a datagram)
constexpr size_t kLogKept = 512;
// Quake II's reading of a client's `rate`: none 5000 (held to no limit), 0 the least, and 100 up
// to the engine's most (sv_minrate and sv_maxrate, 15000 and 60000 by default, hold it nearer).
constexpr ge::server::RateBounds kRateBounds{
    .none = 5000, .zero = 100, .low = 100, .none_held = false};
constexpr int kRateFrames = 10;
// sv_nodelta 0: the whole frames a client may ask for before it is dropped.
constexpr int kNoDeltaMost = 64;  // the frames whose sizes are summed (a second at 10 Hz)
constexpr int kRateSlots = 60;    // the most: a second at `sv_fps` 60

// The move's feel as svc 122 carries it: a count of 32-bit floats (little-endian) and the floats
// (the top speed, the friction, the air acceleration, the swimmer's share, the water's friction,
// the water jump's speed), then a byte of flags (1 the landing pause, 2 the step climbed in the
// air, 4 no surfacing lift). A knob added later goes after the floats so far (and the count grows)
// or in a spare flag: a client reads the floats it knows and passes over the rest.
std::vector<uint8_t> q2_feel_message(const ge::classic::Q2Feel& f) {
  const float floats[] = {f.max_speed,  f.friction,       f.air_accelerate,
                          f.swim_scale, f.water_friction, f.water_jump};
  std::vector<uint8_t> b;
  b.push_back(static_cast<uint8_t>(std::size(floats)));
  for (float v : floats) {
    uint32_t u = 0;
    std::memcpy(&u, &v, 4);
    for (int k = 0; k < 4; k++) b.push_back(static_cast<uint8_t>(u >> (8 * k)));
  }
  b.push_back(static_cast<uint8_t>((f.land_pause ? 1 : 0) | (f.air_step ? 2 : 0) |
                                   (f.surface_lift ? 0 : 4)));
  return b;
}

void note(Q2NetServer& s, std::string line) {
  if (s.log.size() < kLogKept) s.log.push_back(std::move(line));
}

void send_to(Q2NetServer& s, const gn::Address& to, const uint8_t* data, size_t size) {
  gn::transport_send(*s.transport, to, data, size, static_cast<int64_t>(s.now * 1e6));
  s.bytes_out += static_cast<int64_t>(size);
}

void send_oob(Q2NetServer& s, const gn::Address& to, const std::string& text) {
  const std::vector<uint8_t> b = q2_oob(text);
  send_to(s, to, b.data(), b.size());
}

void refuse(Q2NetServer& s, const gn::Address& to, const std::string& why) {
  s.refused++;
  send_oob(s, to, "print\n" + why + "\n");
}

// The rate bounds a client from `from` is held to (sv_lanrate frees the LAN's).
ge::server::RateLimits limits_of(const Q2NetServer& s, const gn::Address& from) {
  return ge::server::rate_limits_for({s.cfg.min_rate, s.cfg.max_rate}, s.cfg.lan_rate, from);
}

int rate_of(const Q2NetServer& s, const std::string& info, const gn::Address& from) {
  return ge::server::rate_in_force(info_get(info, "rate"), limits_of(s, from), kRateBounds);
}

void reliable(Q2NetClient& c, const std::vector<uint8_t>& b) {
  if (!b.empty()) channel_reliable(c.ch, b.data(), b.size());
}

void stuff(Q2NetClient& c, const std::string& line) {
  std::vector<uint8_t> b = {kSvcStuffText};
  b.insert(b.end(), line.begin(), line.end());
  b.push_back(0);
  reliable(c, b);
}

// A client whose `rate` is past sv_maxrate or under sv_minrate told so and set back.
void rate_check(Q2NetServer& s, int slot) {
  Q2NetClient& c = s.clients[slot];
  const ge::server::RateVerdict v =
      ge::server::rate_verdict(info_get(c.userinfo, "rate"), limits_of(s, c.address));
  if (!v.out) return;
  q2_net_print(s, slot, v.said);
  stuff(c, "rate " + std::to_string(v.rate) + "\n");
}

void transmit(Q2NetServer& s, Q2NetClient& c, const uint8_t* data, size_t size) {
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(c.ch, data, size, s.now, out);
  send_to(s, c.address, out, n);
  if (c.packet_dup > 0) c.last_out.assign(out, out + n);
  c.sent = s.now;
}

// A newcomer sent elsewhere (`sv_redirect`, server/admission.hpp): told he is connected, as a stock
// client must be before it reads a message, then in one packet told why and to connect there.
void redirect(Q2NetServer& s, const gn::Address& to, uint16_t qport, std::string_view where,
              bool full) {
  s.redirected++;
  Q2NetClient c;  // a seat for this packet alone
  c.address = to;
  channel_setup(c.ch, false, qport, s.now);
  send_oob(s, to, "client_connect");
  const std::string why = std::string(full ? "Server is full. " : "") + "Redirecting you to " +
                          std::string(where) + "...\n";
  std::vector<uint8_t> b = {kSvcPrint, 2};
  b.insert(b.end(), why.begin(), why.end());
  b.push_back(0);
  reliable(c, b);
  stuff(c, "connect " + std::string(where) + "\n");
  transmit(s, c, nullptr, 0);
  note(s, gn::to_string(to) + " sent on to " + std::string(where));
}

void drop(Q2NetServer& s, int slot, const std::string& why, bool logged = false) {
  Q2NetClient& c = s.clients[slot];
  if (!q2_held(c)) return;
  if (!logged) note(s, c.name + " dropped: " + why);
  // A client signing on again after a level change still holds its slot in the rules.
  if ((**s.rules).clients[slot].active) s.rules->drop(slot);
  // The disconnect, once, with whatever reliable data is still owed.
  const uint8_t bye[] = {kSvcDisconnect};
  channel_reliable(c.ch, bye, sizeof bye);
  transmit(s, c, nullptr, 0);
  // A zombie until `zombie_time` after its last packet: the disconnect resent, nothing read.
  c.stage = Q2Slot::zombie;
  c.downloading = false;
  c.download.clear();
}

// How a drop is told (q2_net_server.hpp): `<name> was dropped: <reason>`, `<name> <reason>` (was
// kicked, timed out), or to the server's console alone.
enum class Told : uint8_t { dropped, plain, silent };

// Client `slot` dropped and told so: the line to everyone in the game (when it was) and to the
// client itself before its disconnect, and `<name>[<address>] ...` on the console.
void drop_told(Q2NetServer& s, int slot, const std::string& reason, Told how, bool logged = true) {
  Q2NetClient& c = s.clients[slot];
  if (!q2_held(c)) return;
  const std::string tail = how == Told::plain ? " " + reason : " was dropped: " + reason;
  if (how != Told::silent) {
    if (c.stage == Q2Slot::active)
      for (int k = 0; k < sh::kMaxClients; k++)
        if (k != slot && s.clients[k].stage == Q2Slot::active)
          q2_net_print(s, k, c.name + tail + "\n");
    q2_net_print(s, slot, c.name + tail + "\n");
  }
  if (logged) note(s, c.name + "[" + gn::to_string(c.address) + "]" + tail);
  drop(s, slot, reason, true);
}

// A name a player can be shown: something but spaces and control characters in its first 15.
bool shown_name(const std::string& name) {
  for (size_t i = 0; i < name.size() && i < 15; i++)
    if (name[i] > 32 && name[i] < 127) return true;
  return false;
}

// The name a server keeps: its first 15 characters, each one's high bit cleared.
std::string cut_name(const std::string& name) {
  std::string out = name.substr(0, 15);
  for (char& ch : out) ch = static_cast<char>(static_cast<unsigned char>(ch) & 0x7f);
  return out;
}

// The wider limits the hosted game asked for (Q2Raw::ext): 0 for our own rules.
int ext_of(const Q2NetServer& s) { return s.native != nullptr ? s.native->raw.ext : 0; }

// A password setting that asks for one: neither empty nor `none`.
bool set(const std::string& password) { return !password.empty() && password != "none"; }

std::string info_text(const Q2NetServer& s) {
  char line[96];
  std::snprintf(line, sizeof line, "%16s %8s %2d/%2d\n", s.cfg.hostname.c_str(),
                (*s.rules)->map_name.c_str(), q2_net_clients(s), q2_public_slots(s));
  return line;
}

std::string status_text(const Q2NetServer& s) {
  std::string info;
  info_set(info, "hostname", s.cfg.hostname);
  info_set(info, "mapname", (*s.rules)->map_name);
  info_set(info, "maxclients", std::to_string(q2_public_slots(s)));
  info_set(info, "maxspectators", std::to_string(s.cfg.max_spectators));
  if (!s.cfg.ruleset.empty()) info_set(info, "ruleset", s.cfg.ruleset);
  // `needpass`: 1 a player's password asked, 2 a spectator's.
  info_set(info, "needpass",
           std::to_string((set(s.cfg.password) ? 1 : 0) | (set(s.cfg.spectator_password) ? 2 : 0)));
  q2_status_keys(s, info);
  info_set(info, "protocol", std::to_string(q2_status_protocol(ext_of(s))));
  if (s.cfg.status.uptime > 0)
    info_set(info, "uptime", ge::server::uptime_text(s.now - s.started, s.cfg.status.uptime));
  std::string t = info + "\n";
  if (s.cfg.status.show < 2) return t;  // sv_status 1: the info alone
  for (int i = 0; i < q2_net_slots(s); i++) {
    const Q2NetClient& c = s.clients[i];
    if (!q2_held(c)) continue;
    const int frags = static_cast<int>((*s.rules)->world.clients[i].numbers.frags);
    t += std::to_string(frags) + " " + std::to_string(c.ping_ms) + " \"" + c.name + "\"\n";
  }
  return t;
}

void connect(Q2NetServer& s, const gn::Address& from, const std::vector<std::string>& w) {
  if (w.size() < 2) return;
  Q2ConnectAsk ask;
  std::string unsupported;
  if (!q2_connect_ask(w, ask, unsupported)) return refuse(s, from, unsupported);
  if (w.size() < 5) return;
  if (const std::string old = q2_extended_refusal(ext_of(s), ask); !old.empty())
    return refuse(s, from, old);
  const auto qport = static_cast<uint16_t>(std::atoi(w[2].c_str()));
  // By the host and the number (classic/protocol/challenges.hpp): a port that moved still finds it.
  const int32_t challenge = std::atoi(w[3].c_str());
  switch (ge::classic::challenge_check(s.challenges, from, challenge)) {
    case ge::classic::ChallengeCheck::none:
      return refuse(s, from, "No challenge for address.");
    case ge::classic::ChallengeCheck::bad:
      return refuse(s, from, "Bad challenge.");
    case ge::classic::ChallengeCheck::ok:
      break;
  }
  if (ge::server::ban_refuses(s.bans, s.cfg.ban_mode, from,
                              s.cfg.unix_time ? s.cfg.unix_time() : 0))
    return refuse(s, from, "Banned.");
  std::string userinfo = w[4];
  // A connect from a held slot's address and port: a repeat while the slot waits for its first
  // `new` (told again it is connected), else the client starting over: the old one dropped
  // ("reconnected") and the slot taken again. A zombie's settings for the forced reconnect carry.
  std::string carried_var, carried_value;
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2NetClient& c = s.clients[i];
    if (c.stage == Q2Slot::free || c.stage == Q2Slot::dummy || !gn::same_host(c.address, from) ||
        c.address.port != from.port)
      continue;
    if (c.stage == Q2Slot::zombie) {
      carried_var = c.reconnect_var;
      carried_value = c.reconnect_value;
      c = Q2NetClient{};
      continue;
    }
    if (c.stage == Q2Slot::signon && !c.newed) return send_oob(s, from, "client_connect");
    drop_told(s, i, "reconnected", Told::dropped);
    c = Q2NetClient{};
  }
  if (s.cfg.locked) {  // sv_locked: only a client taking back the slot its address still holds
    bool back = false;
    for (const Q2NetClient& c : s.clients)
      back = back || (c.stage == Q2Slot::zombie && gn::same_host(c.address, from) &&
                      (c.qport == qport || c.address.port == from.port));
    if (!back) return refuse(s, from, "Server is locked.");
  }
  if (s.cfg.ip_limit > 0) {  // one address's slots held to sv_iplimit, a zombie's a half
    std::vector<ge::server::HeldSeat> held;
    for (const Q2NetClient& o : s.clients)
      if (o.stage != Q2Slot::free && o.stage != Q2Slot::dummy)
        held.push_back({o.address, o.stage == Q2Slot::zombie});
    const std::string refusal = ge::server::admit_refusal(s.cfg.ip_limit, from, held);
    if (!refusal.empty()) return refuse(s, from, refusal);
  }
  // THE USER INFO (the strict rule, classic/protocol/info.hpp), then a name with something to
  // show in its first 15 characters.
  if (userinfo.empty()) return refuse(s, from, "Empty userinfo string.");
  if (!info_strict(userinfo)) return refuse(s, from, "Malformed userinfo string.");
  if (!shown_name(info_get(userinfo, "name")))
    return refuse(s, from, "Please set your name before connecting.");
  const std::string asked = info_get(userinfo, "spectator");
  const bool spectator = !asked.empty() && asked != "0";
  // A wrong password past sv_passwordrate gets no answer (server/limit.hpp).
  auto wrong = [&](const char* why) {
    if (ge::server::limit_take(s.password_tries, s.now, s.cfg.password_rate)) refuse(s, from, why);
  };
  if (spectator && set(s.cfg.spectator_password) && asked != s.cfg.spectator_password)
    return wrong("Spectator password required or incorrect.\nConnection refused.");
  const std::string given = info_get(userinfo, "password");
  bool vip = false;
  for (const std::string& word : s.cfg.vip_passwords) vip = vip || (set(word) && given == word);
  if (!vip && !spectator && set(s.cfg.password) && given != s.cfg.password)
    return wrong("Password required or incorrect.\nConnection refused.");
  vip = vip || (set(s.cfg.password) && given == s.cfg.password);
  // A competition server's ruleset (sv_ruleset, server/admission.hpp): a player keeps to it.
  if (const std::string why = ge::server::ruleset_refusal(
          s.cfg.ruleset, info_get(userinfo, std::string(ge::classic::ruleset_info_key(true))),
          spectator);
      !why.empty())
    return refuse(s, from, why);
  // What the rules never see: the recorder's mark and the password (when one is asked); the
  // address the server saw, always.
  info_remove(userinfo, "mvdspec");
  if (set(s.cfg.password) || !s.cfg.vip_passwords.empty()) info_remove(userinfo, "password");
  if (!info_set(userinfo, "ip", gn::to_string(from)) || userinfo.size() >= 512)
    return refuse(s, from, "Oversize userinfo string.");
  {  // A user info ban that kicks refuses the connect (server/settingban.hpp).
    std::vector<std::pair<std::string, std::string>> pairs;
    ge::classic::protocol::info_pairs(userinfo, pairs);
    for (const auto& [k, v] : pairs)
      for (const ge::server::SettingBan* b : ge::server::setting_ban_hits(s.userinfo_bans, k, v))
        if (b->action == ge::server::FilterDo::kick)
          return refuse(s, from,
                        (b->text.empty() ? "Userinfo banned." : b->text) + "\nConnection refused.");
  }
  if (const std::string_view to = ge::server::redirect_to(s.cfg.redirect, false, vip); !to.empty())
    return redirect(s, from, qport, to, false);  // sv_redirect "!": everyone but a VIP
  // A newcomer for whom no seat is free: sent on (sv_redirect), else refused.
  auto full = [&](const char* why) {
    if (const std::string_view to = ge::server::redirect_to(s.cfg.redirect, true, vip); !to.empty())
      return redirect(s, from, qport, to, true);
    refuse(s, from, why);
  };
  int players = 0, watching = 0, slot = -1;
  for (int i = 0; i < q2_net_slots(s); i++) {
    const Q2NetClient& o = s.clients[i];
    const sv::ClientInfo& seat = (**s.rules).clients[i];
    if (o.stage == Q2Slot::free && seat.active && seat.bot) {
      players++;  // a bot the server seated (sv_bots): a player, with no connection
    } else if (o.stage == Q2Slot::free) {
      if (slot < 0 && !seat.active) slot = i;
    } else if (o.stage != Q2Slot::dummy) {
      (o.spectator ? watching : players)++;
    }
  }
  // Spectators have their own slots when `max_spectators` gives them some, else take players'.
  const bool own = s.cfg.max_spectators > 0;
  if (spectator && own && watching >= s.cfg.max_spectators)
    return full("Server spectator limit is full.\nConnection refused.");
  const int taken = players + (own ? 0 : watching);
  if ((!spectator || !own) && taken >= s.cfg.max_clients) slot = -1;
  if (slot < 0) return full("Server is full.");
  if ((!spectator || !own) &&
      ge::server::vip_slots_refuse(s.cfg.max_clients, taken, s.cfg.vip_slots, vip))
    return full("Server and reserved slots are full.");
  ge::classic::challenge_used(s.challenges, from, challenge);  // used once
  Q2NetClient& c = s.clients[slot];
  c = Q2NetClient{};
  c.stage = Q2Slot::signon;
  c.address = from;
  c.qport = qport;
  c.userinfo = userinfo;
  c.name = cut_name(info_get(userinfo, "name"));
  c.reconnect_var = carried_var;
  c.reconnect_value = carried_value;
  const int ext = std::atoi(info_get(userinfo, "*ge_ext").c_str());
  c.told_origins = (ext & kGeDownloadOrigin) != 0;
  c.told_moves = (ext & kGeOtherMoves) != 0;
  c.told_share = (ext & kGeSpeedJumpShare) != 0;
  c.told_tag = (ext & kGeAccountKey) != 0;
  c.told_feel = (ext & kGeMoveFeel) != 0;
  c.told_back = (ext & kGeItemBack) != 0;
  c.told_arsenal = (ext & kGeArsenal) != 0;
  c.told_voice = s.cfg.voice && (ext & kGeVoice) != 0;
  c.told_frames = (ext & kGeEveryFrame) != 0;
  c.told_hits = (ext & kGeHitReports) != 0;
  c.userid = s.next_userid++;
  c.spectator = spectator;
  c.rate = rate_of(s, userinfo, from);
  if (s.name_rule != nullptr) s.name_rule(s.hook_ctx, slot, c.name, true);
  c.heard = s.now;
  channel_setup(c.ch, false, qport, s.now);
  channel_set_rate(c.ch, 0);
  c.protocol = ask.protocol;
  c.minor = ask.minor;
  c.zlib = ask.zlib;
  const bool local = from.family == gn::Family::ipv4 && from.bytes[0] == 127;
  c.msglen = q2_msglen(ask, s.cfg.net_maxmsglen, local);
  if (c.protocol != kQ2NetProtocol) {  // a byte of qport, none when it is 0
    c.ch.qport_bytes = qport != 0 ? 1 : 0;
    c.ch.fragments = ask.chan == 1;
    c.ch.packet_len = static_cast<size_t>(c.msglen);
  }
  // The answer: the channel kind a 36 client asked for, the web address files come from, the map.
  std::string answer = "client_connect";
  if (c.protocol == kQ2Protocol36) answer += " nc=" + std::to_string(ask.chan);
  // A GitHub repository named as its raw files' address, which a stock client fetches as it is.
  if (!s.cfg.download_server.empty()) {
    answer += " dlserver=" + ge::classic::content::web_base(s.cfg.download_server);
  } else if (!s.cfg.web_url.empty()) {  // the server's own web files, and how to trust them
    answer += " dlserver=" + s.cfg.web_url;
    if (!s.cfg.web_pin.empty()) answer += " dlpin=" + s.cfg.web_pin;
  }
  answer += " map=" + (*s.rules)->map_name;
  send_oob(s, from, answer);
  note(s, c.name + " joins from " + gn::to_string(from));  // the rules say "connected" at its begin
}

void read_oob(Q2NetServer& s, const gn::Address& from, const uint8_t* data, size_t size) {
  const std::string text = q2_oob_text(data, size);
  const std::vector<std::string> w = q2_words(text);
  if (w.empty()) return;
  if (s.killed && w[0] != "rcon") return;  // `killserver`: only the operator answered
  if (w[0] == "getchallenge") {
    // 31 bits, never 0; one an address and port, the same one asked again, the oldest of 1024
    // replaced (classic/protocol/challenges.hpp).
    const int32_t number = ge::classic::challenge_issue(s.challenges, from, [&s] {
      s.next_challenge = s.next_challenge * 1103515245u + 12345u;
      const uint32_t high = s.next_challenge >> 16;
      s.next_challenge = s.next_challenge * 1103515245u + 12345u;
      return static_cast<int32_t>(((high << 16) | (s.next_challenge >> 16)) & 0x7FFFFFFFu) | 1;
    });
    send_oob(s, from, "challenge " + std::to_string(number) + " p=34,35,36");
  } else if (w[0] == "connect") {
    connect(s, from, w);
  } else if (w[0] == "info") {
    const int asked = w.size() > 1 ? std::atoi(w[1].c_str()) : kQ2NetProtocol;
    if (asked < kQ2NetProtocol || asked > kQ2Protocol36)
      send_oob(s, from, "info\n" + s.cfg.hostname + ": wrong version\n");
    else
      send_oob(s, from, "info\n" + info_text(s));
  } else if (w[0] == "status") {  // sv_status 0 or past sv_statusrate: no answer
    if (s.cfg.status.show > 0 && ge::server::limit_take(s.status_tries, s.now, s.cfg.status.rate))
      send_oob(s, from, "print\n" + status_text(s));
  } else if (w[0] == "ping") {
    send_oob(s, from, "ack");
  } else if (w[0] == "ack") {  // a master heard the heartbeat (`listmasters`)
    s.acks.push_back(from);
  } else if (w[0] == "rcon") {
    q2_rcon(s, from, text);
  }
}

// An entry's action on client `slot` (server/cmdfilter.hpp, settingban.hpp): `said` the log's line
// (log and kick), `text` what print shows, stuff runs and kick tells (print and stuff do nothing
// without one). True when the client was kicked.
bool act(Q2NetServer& s, int slot, ge::server::FilterDo action, const std::string& text,
         const std::string& said) {
  Q2NetClient& c = s.clients[slot];
  switch (action) {
    case ge::server::FilterDo::ignore:
      return false;
    case ge::server::FilterDo::log:
      note(s, said);
      return false;
    case ge::server::FilterDo::print:
      if (!text.empty()) q2_net_print(s, slot, text + "\n");
      return false;
    case ge::server::FilterDo::stuff:
      if (!text.empty()) stuff(c, text + "\n");
      return false;
    case ge::server::FilterDo::kick:
      note(s, said);
      if (!text.empty()) q2_net_print(s, slot, text + "\n");
      drop_told(s, slot, "was kicked", Told::plain);
      return true;
  }
  return false;
}

// A client's setting `name` = `value` judged by `bans` (server/settingban.hpp): every entry that
// matches acts; true when one kicked it.
bool judge(Q2NetServer& s, int slot, const ge::server::SettingBans& bans, const std::string& name,
           const std::string& value) {
  for (const ge::server::SettingBan* b : ge::server::setting_ban_hits(bans, name, value)) {
    s.setting_banned++;
    const Q2NetClient& c = s.clients[slot];
    const std::string said =
        ge::server::setting_ban_log_line(c.name, gn::to_string(c.address), b->name, value);
    if (act(s, slot, b->action, b->text, said)) return true;
  }
  return false;
}

// Every key of the client's user info judged by the user info bans; true when it was kicked.
bool judge_userinfo(Q2NetServer& s, int slot) {
  if (s.userinfo_bans.entries.empty()) return false;
  std::vector<std::pair<std::string, std::string>> pairs;
  ge::classic::protocol::info_pairs(s.clients[slot].userinfo, pairs);
  for (const auto& [k, v] : pairs)
    if (judge(s, slot, s.userinfo_bans, k, v)) return true;
  return false;
}

// The client asked for each setting a setting ban names (its console answers, settingban.hpp).
void ask_settings(Q2NetServer& s, Q2NetClient& c) {
  for (const ge::server::SettingBan& b : s.cvar_bans.entries)
    stuff(c, ge::server::setting_ban_query(b.name));
}

void start_signon(Q2NetServer& s, int slot) {
  Q2NetClient& c = s.clients[slot];
  if (c.stage == Q2Slot::active) {
    s.rules->drop(slot);
    c.stage = Q2Slot::signon;
  }
  sv::Q2WireOptions opt = s.cfg.wire;
  opt.attract_loop = 0;
  opt.protocol = c.protocol;
  opt.minor = c.minor;
  opt.hit_reports = c.told_hits;  // our own client draws the hit marker from it
  if (s.native != nullptr) {      // an extended game's level (sv::Q2Raw::ext) goes on the wire
    opt.ext = s.native->raw.ext;
    sv::q2_signon_raw(c.wire, s.native->raw, slot, opt, c.signon);
  } else
    sv::q2_signon_parts(c.wire, **s.rules, slot, opt, c.signon);
  ask_settings(s, c);  // setting bans and user info bans at every `new` (server/settingban.hpp)
  if (judge_userinfo(s, slot)) return;
  reliable(c, c.signon.server_data);
  c.signon_queue.clear();
  if (c.protocol == kQ2NetProtocol && s.cfg.wire.show.empty())  // a show: the server data alone
    stuff(c, "cmd configstrings " + std::to_string(s.count) + " 0\n");
}

// A message compressed for a client that takes it (zpacket: its id, the compressed and the plain
// length as words, a raw deflate stream), when that is smaller; else the message as it is.
std::vector<uint8_t> packed(const Q2NetClient& c, std::vector<uint8_t> msg) {
  if (!c.zlib || msg.size() > sv::kQ2BigMessage) return msg;
  const std::vector<uint8_t> z = ge::core::deflate_raw(msg);
  if (z.size() + 5 >= msg.size()) return msg;
  std::vector<uint8_t> out = {
      kSvcZPacket, static_cast<uint8_t>(z.size() & 0xFF), static_cast<uint8_t>(z.size() >> 8),
      static_cast<uint8_t>(msg.size() & 0xFF), static_cast<uint8_t>(msg.size() >> 8)};
  out.insert(out.end(), z.begin(), z.end());
  return out;
}

// A later protocol's signon after the server data, all at once and in order: on the newer
// channel the game state (minor below 1024) or the configstring then baseline streams; on the old
// channel the configstrings then the baselines in reliable messages that fit a packet; each
// compressed when the client takes it, then `precache`. Queued, and fed to the channel as it has
// room (`feed_signon`).
void queue_signon(Q2NetServer& s, Q2NetClient& c) {
  if (c.ch.fragments) {
    std::vector<std::vector<uint8_t>> streams;
    sv::q2_signon_streams(c.signon, c.minor, streams);
    for (std::vector<uint8_t>& m : streams) c.signon_queue.push_back(packed(c, std::move(m)));
  } else {
    const size_t budget = std::min<size_t>(static_cast<size_t>(c.msglen), kMaxMsgLen - 16) - 64;
    std::vector<uint8_t> batch;
    const auto add = [&](const std::vector<uint8_t>& one) {
      if (!batch.empty() && batch.size() + one.size() > budget) {
        c.signon_queue.push_back(packed(c, std::move(batch)));
        batch.clear();
      }
      batch.insert(batch.end(), one.begin(), one.end());
    };
    for (const auto& [i, one] : c.signon.config_strings) add(one);
    for (const auto& [n, one] : c.signon.baselines) add(one);
    if (!batch.empty()) c.signon_queue.push_back(packed(c, std::move(batch)));
  }
  const std::string line = "precache " + std::to_string(s.count) + "\n";
  std::vector<uint8_t> pre = {kSvcStuffText};
  pre.insert(pre.end(), line.begin(), line.end());
  pre.push_back(0);
  c.signon_queue.push_back(std::move(pre));
}

// The queued signon into the channel while it has room: the newer channel's one buffer, or the
// old channel's waiting messages less one.
void feed_signon(Q2NetClient& c) {
  size_t taken = 0;
  for (; taken < c.signon_queue.size(); taken++) {
    const std::vector<uint8_t>& m = c.signon_queue[taken];
    const bool room = c.ch.fragments ? c.ch.big_pending.size() + m.size() <= sp::kBigMessage
                                     : c.ch.pending_count < sp::kBackBuffers - 1;
    if (!room) break;
    reliable(c, m);
  }
  c.signon_queue.erase(c.signon_queue.begin(), c.signon_queue.begin() + static_cast<long>(taken));
}

// The signon's parts from `from` on, about 700 bytes of them, and the next prompt.
template <typename Parts>
void signon_share(Q2NetClient& c, const Parts& parts, int from, const std::string& more,
                  const std::string& done) {
  std::vector<uint8_t> b;
  size_t i = 0;
  while (i < parts.size() && parts[i].first < from) i++;
  for (; i < parts.size() && b.size() < kSignonShare; i++)
    b.insert(b.end(), parts[i].second.begin(), parts[i].second.end());
  reliable(c, b);
  stuff(c, i < parts.size() ? more + std::to_string(parts[i].first) + "\n" : done);
}

// Whether the players in `a` and `b` are on one team: the rules' own teams (Quake II's by skin
// or model), else the same `team` key.
bool same_team(const Q2NetServer& s, int a, int b) {
  if (s.game != nullptr) return sv::fight_teammates(*s.game, a, b);
  return info_get(s.clients[a].userinfo, "team") == info_get(s.clients[b].userinfo, "team");
}

bool hears(const Q2NetServer& s, int talker, int listener, VoiceTo to, int one) {
  TalkPair p;
  p.talker_spectator = s.clients[talker].spectator;
  p.listener_spectator = s.clients[listener].spectator;
  p.same_team = same_team(s, talker, listener);
  return talk_route(s.rules != nullptr ? &**s.rules : nullptr, s.cfg.earshot, talker, listener, to,
                    one, p);
}

// A client's voice to those who hear it (THE VOICE, q2_net_server.hpp), sent on the next tick.
void relay_voice(Q2NetServer& s, int slot, int gen, int seq, const std::vector<uint8_t>& frames) {
  Q2NetClient& c = s.clients[slot];
  if (!c.told_voice || c.stage != Q2Slot::active || frames.empty()) return;
  c.voice_in++;
  for (int j = 0; j < sh::kMaxClients; j++) {
    Q2NetClient& d = s.clients[j];
    if (d.stage != Q2Slot::active || !d.told_voice || talk_mutes(d.talk, slot)) continue;
    if (j == slot ? !s.cfg.voice_echo : !hears(s, slot, j, c.talk.to, c.talk.one)) continue;
    if (d.voice_out.size() + frames.size() + 6 > kVoiceTickBytes) continue;  // a tick's room
    d.voice_out.push_back(kSvcGeVoice);
    write_voice_down(d.voice_out, slot, gen, seq, frames);
    d.voice_heard++;
  }
}

// A client's talk word (voicetarg, unmuteall, muteall, vignore, say_near): true when it was one.
bool talk_command(Q2NetServer& s, int slot, const std::string& line) {
  Q2NetClient& c = s.clients[slot];
  pg::Words w;
  pg::tokenize_words(line.c_str(), w);
  if (talk_word(c.talk, w, sh::kMaxClients)) return true;
  if (std::string(pg::word(w, 0)) != "say_near") return false;
  const std::string text = talk_said(w);
  if (text.empty()) return true;
  if (s.cfg.earshot <= 0) {
    q2_net_print(s, slot, "This server has no nearby chat.\n");
    return true;
  }
  const std::string said = talk_near_line(c.name, text);
  for (int j = 0; j < sh::kMaxClients; j++)
    if (s.clients[j].stage == Q2Slot::active && hears(s, slot, j, VoiceTo::near, -1)) {
      std::vector<uint8_t> b = {kSvcPrint, 3};  // the chat level
      b.insert(b.end(), said.begin(), said.end());
      b.push_back(0);
      reliable(s.clients[j], b);
    }
  return true;
}

// A client's line a command filter refuses: its entry's action done, true.
bool filtered(Q2NetServer& s, int slot, const std::string& line) {
  const ge::server::CmdFilter* f = ge::server::cmdfilter_match(s.cmd_filters, line);
  if (f == nullptr) return false;
  s.filtered++;
  const Q2NetClient& c = s.clients[slot];
  act(s, slot, f->action, f->text,
      ge::server::cmdfilter_log_line(c.name, gn::to_string(c.address), line));
  return true;
}

// A client's chunked answer: high-level prints of at most 1384 bytes each.
void answer(Q2NetServer& s, int slot, const std::string& text) {
  for (size_t at = 0; at < text.size(); at += 1384) q2_net_print(s, slot, text.substr(at, 1384));
}

// THE PLAYER'S QUERIES the server answers itself, in any stage (q2_admin.hpp): `info`, `sinfo`,
// `lag [player]`, `packetdup [n]`, `stopdl`, `nogamedata`, `download http`. True when it was one.
bool player_query(Q2NetServer& s, int slot, const std::vector<std::string>& w,
                  const std::string& line) {
  Q2NetClient& c = s.clients[slot];
  const std::string& cmd = w[0];
  if (cmd == "info") {
    answer(s, slot, q2_info_lines(s));
  } else if (cmd == "sinfo") {
    answer(s, slot, q2_misc_lines(s, slot));
  } else if (cmd == "lag") {
    int who = slot;
    if (w.size() > 1) {
      std::vector<ge::server::PickSlot> slots;
      slots.reserve(static_cast<size_t>(q2_net_slots(s)));
      for (int i = 0; i < q2_net_slots(s); i++)
        slots.push_back({s.clients[i].name, q2_held(s.clients[i])});
      std::string error;
      who = ge::server::pick_player(slots, ge::server::line_rest(line, 1), true, error);
      if (who < 0) {
        answer(s, slot, error);
        return true;
      }
    }
    q2_net_print(s, slot, q2_lag_lines(s, who));
  } else if (cmd == "packetdup") {
    if (w.size() > 1) {
      const int n = std::atoi(w[1].c_str());
      if (n < 0 || n > s.cfg.packetdup_most) {
        q2_net_print(s, slot,
                     "Packetdup of " + std::to_string(n) + " is not allowed on this server.\n");
        return true;
      }
      c.packet_dup = n;
    }
    q2_net_print(s, slot,
                 "Server is sending " + std::to_string(c.packet_dup) + " duplicate packet" +
                     (c.packet_dup == 1 ? "" : "s") + " to you.\n");
  } else if (cmd == "stopdl") {
    if (!c.downloading) return true;
    const int percent =
        c.download.empty() ? 0 : static_cast<int>(c.download_at * 100 / c.download.size());
    const uint8_t stop[4] = {kSvcDownload, 0xff, 0xff, static_cast<uint8_t>(percent)};
    reliable(c, std::vector<uint8_t>(stop, stop + 4));
    c.downloading = false;
    c.download.clear();
    note(s, c.name + " stopped a download");
  } else if (cmd == "nogamedata") {
    c.no_game_data = !c.no_game_data;
    if (!c.no_game_data) c.acked = -1;  // the frames start whole again
  } else if (cmd == "download" && w.size() > 1 && w[1] == "http") {
    c.http_download = true;
  } else {
    return false;
  }
  return true;
}

// THE FIRST `new` of a connection: the version probe, the `connect` stuff list, then the forced
// reconnect: a newcomer is sent its random settings and the address and dropped silently (true);
// one that came back is asked for its setting.
bool first_new(Q2NetServer& s, int slot) {
  Q2NetClient& c = s.clients[slot];
  c.newed = true;
  c.new_at = s.now;
  const bool local = c.address.family == gn::Family::ipv4 && c.address.bytes[0] == 127;
  if (!s.force_reconnect.empty() && c.reconnect_var.empty() && !local) {
    // Eight random strings A..H of 15: A a setting `set` is put in, B one `connect` is, C the
    // setting brought back and D its value, E the address, F a decoy and G its value.
    static constexpr char kAbc[] =
        "!#%&()*+,-./:<=>?@[]^_{}0123456789abcdefghijklmnopqrstuvwxyz"
        "\x60\x60\x60\x60\x60\x60\x60\x60\x60\x60~~~~~~~~~~";  // ten grave accents, ten tildes
    std::string r[8];
    for (std::string& x : r)
      for (int k = 0; k < 15; k++) {
        s.junk = s.junk * 1103515245u + 12345u;
        x += kAbc[(s.junk >> 16) % (sizeof kAbc - 1)];
      }
    c.reconnect_var = r[2];
    c.reconnect_value = r[3];
    const std::string a = "$" + r[0] + " ";
    stuff(c, "set " + r[0] + " set\n");
    stuff(c, a + r[1] + " connect\n");
    const std::string mine = a + r[2] + " " + r[3] + "\n";
    const std::string to = a + r[4] + " " + s.force_reconnect + "\n" + a + r[5] + " " + r[6] + "\n";
    s.junk = s.junk * 1103515245u + 12345u;
    if (((s.junk >> 16) & 1) != 0) {
      stuff(c, mine);
      stuff(c, to);
    } else {
      stuff(c, to);
      stuff(c, mine);
    }
    stuff(c, a + r[0] + " \"\"\n");
    stuff(c, "$" + r[1] + " $" + r[4] + "\n");
    // Dropped with no word to anyone; its zombie keeps the setting for the connect that follows.
    drop(s, slot, "", true);
    return true;
  }
  return false;
}

// After the first `new`'s server data (so a recording still opens with it): the version probe,
// the `connect` stuff list, and a client back from the forced reconnect asked for its setting.
void probe(Q2NetServer& s, int slot) {
  Q2NetClient& c = s.clients[slot];
  stuff(c, "cmd " + std::string(ge::server::kSettingAnswer) + " version $version\n");
  for (const std::string& text : s.stuff_lists.connect) stuff(c, text + "\n");
  const bool local = c.address.family == gn::Family::ipv4 && c.address.bytes[0] == 127;
  if (!c.reconnect_var.empty() && !local)
    stuff(c,
          "cmd " + std::string(ge::server::kSettingAnswer) + " connect $" + c.reconnect_var + "\n");
}

void string_command(Q2NetServer& s, int slot, const std::string& line) {
  Q2NetClient& c = s.clients[slot];
  const std::vector<std::string> w = q2_words(line);
  if (w.empty()) return;
  const std::string& cmd = w[0];
  if (cmd == ge::server::kSettingAnswer) {  // an answer the server asked for: `<DEL>c <what> ...`
    if (w.size() < 2) return;
    const size_t at = line.find(w[1], line.find(cmd) + cmd.size()) + w[1].size();
    std::string value = at < line.size() ? line.substr(at + 1) : std::string();
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
    if (w[1] == "version") {  // the version probe's answer: the first one kept
      if (c.version_heard) return;
      c.version_heard = true;
      c.version = value;
      note(s, c.name + "[" + gn::to_string(c.address) + "]: " + value);
      return;
    }
    if (w[1] == "console") {  // a `stuffcvar` answer: `console <name> <value>`
      if (c.cvars_asked <= 0 || w.size() < 3) return;
      c.cvars_asked--;
      const size_t vat = line.find(w[2], at) + w[2].size();
      std::string v = vat < line.size() ? line.substr(vat + 1) : std::string();
      while (!v.empty() && (v.back() == '\n' || v.back() == '\r')) v.pop_back();
      note(s, c.name + "[" + gn::to_string(c.address) + "]: \"" + w[2] + "\" is \"" + v + "\"");
      return;
    }
    if (w[1] == "connect") {  // the forced reconnect's setting brought back
      if (!c.reconnect_var.empty() && value == c.reconnect_value) c.reconnected = true;
      return;
    }
    judge(s, slot, s.cvar_bans, w[1], value);
    return;
  }
  if (player_query(s, slot, w, line)) return;
  const bool signon = cmd == "configstrings" || cmd == "baselines" || cmd == "begin";
  const bool protocol = signon || cmd == "new" || cmd == "disconnect" || cmd == "download" ||
                        cmd == "nextdl" || cmd == "nextserver";
  if (!protocol && filtered(s, slot, line)) return;
  if (!protocol && s.line_hook != nullptr && s.line_hook(s.hook_ctx, slot, line)) return;
  if (!protocol && c.stage == Q2Slot::active && talk_command(s, slot, line)) return;
  if (signon && (w.size() < 2 || std::atoi(w[1].c_str()) != s.count)) {
    note(s, c.name + " from a different level: the signon starts again");
    return start_signon(s, slot);
  }
  const int at = w.size() > 2 ? std::atoi(w[2].c_str()) : 0;
  const std::string count = std::to_string(s.count);
  if (cmd == "new") {
    const bool first = c.stage == Q2Slot::signon && !c.newed;
    if (first && first_new(s, slot)) return;
    if (c.stage == Q2Slot::signon) start_signon(s, slot);
    if (first) probe(s, slot);
    if (c.stage == Q2Slot::signon && c.protocol != kQ2NetProtocol && s.cfg.wire.show.empty()) {
      queue_signon(s, c);
      feed_signon(c);
    }
  } else if (cmd == "configstrings") {
    if (c.stage == Q2Slot::signon && c.protocol == kQ2NetProtocol)
      signon_share(c, c.signon.config_strings, at, "cmd configstrings " + count + " ",
                   "cmd baselines " + count + " 0\n");
  } else if (cmd == "baselines") {
    if (c.stage == Q2Slot::signon && c.protocol == kQ2NetProtocol)
      signon_share(c, c.signon.baselines, at, "cmd baselines " + count + " ",
                   "precache " + count + "\n");
  } else if (cmd == "nextserver") {  // a show watched to its end (q2_net_server.hpp)
    if (s.nextserver) s.nextserver(w.size() > 1 ? std::atoi(w[1].c_str()) : -1);
  } else if (cmd == "begin") {
    if (c.stage != Q2Slot::signon || !s.cfg.wire.show.empty()) return;
    if (!c.version_heard) return drop_told(s, slot, "failed version probe", Told::silent);
    if (!c.reconnect_var.empty() && !c.reconnected)
      return drop_told(s, slot, "failed to reconnect", Told::silent);
    if (s.native != nullptr) {  // a game library reads the client's whole user info
      auto& info = (**s.rules).clients[slot].userinfo;
      info.clear();
      ge::classic::protocol::info_pairs(c.userinfo, info);
    }
    s.rules->connect(slot, c.name, c.spectator, false);
    // A stock client's protocol, for the moves it predicts (a spectator's flight); ours is told.
    (**s.rules).clients[slot].protocol = c.told_feel ? 0 : c.protocol;
    if (s.native == nullptr) {  // the rest of its user info (its skin: the teams by skin)
      std::vector<std::pair<std::string, std::string>> pairs;
      ge::classic::protocol::info_pairs(c.userinfo, pairs);
      for (const auto& [k, v] : pairs)
        if (k != "name" && k != "*ge_ext") s.rules->user_info(slot, k, v);
    }
    if (s.native == nullptr && c.told_feel)  // our rules run his move on the feel he is told
      s.rules->user_info(slot, "*ge_ext", info_get(c.userinfo, "*ge_ext"));
    c.stage = Q2Slot::active;
    ge::server::idle_start(c.idle, s.now);
    c.acked = -1;
    sv::q2_resync(c.wire);
    if (c.told_voice) {  // voice taken here: the empty message the client waits for
      std::vector<uint8_t> on = {kSvcGeVoice};
      write_voice_down(on, 255, 0, 0, {});
      reliable(c, on);
    }
    note(s, c.name + " entered the game");
    c.http_download = false;
    for (const std::string& text : s.stuff_lists.begin) stuff(c, text + "\n");
    rate_check(s, slot);
  } else if (cmd == "download") {
    const std::string refused = q2_start_download(s, c, w.size() > 1 ? w[1] : std::string(), at);
    if (!refused.empty()) note(s, refused);
    q2_download_piece(s, c);
  } else if (cmd == "nextdl") {
    c.download_owed = c.downloading;
    q2_download_piece(s, c);
  } else if (cmd == "disconnect") {
    drop(s, slot, "left");
  } else if (c.stage == Q2Slot::active) {
    if (cmd == "say" || cmd == "say_team") ge::server::idle_chat(c.idle, s.now);
    s.rules->client_command(slot, line);
  }
}

// A move's round trip `trip` (s) kept, and the ping it makes by sv_pingmethod told to the rules.
void take_trip(Q2NetServer& s, int slot, double trip) {
  Q2NetClient& c = s.clients[slot];
  c.trips[c.trip_count++ % 16] = trip;
  const int n = c.trip_count < 16 ? c.trip_count : 16;
  double sum = 0, least = c.trips[0];
  for (int i = 0; i < n; i++) {
    sum += c.trips[i];
    least = c.trips[i] < least ? c.trips[i] : least;
  }
  const double shown = s.cfg.ping_method == 0 ? 0 : s.cfg.ping_method == 1 ? sum / n : least;
  c.ping_ms = static_cast<int>(ge::core::num::round(shown * 1000));
  (**s.rules).clients[slot].ping_ms = c.ping_ms;
}

// One packet's messages from client `slot`; its moves onto `packets`.
// A client's new user info (whole, or 36's change applied): the strict rule, the name, the bans,
// what the rules are told. False when the client was dropped.
bool take_userinfo(Q2NetServer& s, int slot, std::string info) {
  Q2NetClient& c = s.clients[slot];
  const auto dropped = [&](const char* why) {
    drop_told(s, slot, why, Told::dropped);
    return false;
  };
  const std::string was = info_get(c.userinfo, "rate");
  const std::string before = c.userinfo;
  c.userinfo = std::move(info);
  // The strict rule (classic/protocol/info.hpp); a name with nothing to show keeps the one
  // the server has.
  if (c.userinfo.size() >= 512) return dropped("oversize userinfo");
  if (c.userinfo.empty()) return dropped("empty userinfo");
  if (!info_strict(c.userinfo)) return dropped("malformed userinfo");
  if (!shown_name(info_get(c.userinfo, "name"))) {
    if (c.name.empty()) return dropped("malformed name");
    if (!info_set(c.userinfo, "name", c.name) || c.userinfo.size() >= 512)
      return dropped("oversize userinfo");
    q2_net_print(s, slot, "You can't have an empty name.\n");
    stuff(c, "set name \"" + c.name + "\"\n");
  }
  if (judge_userinfo(s, slot)) return false;  // a user info ban that kicks (server/settingban.hpp)
  const bool rewound = antilag_wanted(info_get(c.userinfo, "cl_antilag"));
  if (s.cfg.antilag > 0 && c.stage == Q2Slot::active &&
      rewound != antilag_wanted(info_get(before, "cl_antilag")))
    q2_net_print(s, slot, antilag_told(rewound));          // his own shots rewound or not
  if (c.stage == Q2Slot::active && s.native == nullptr) {  // what changed, to the rules
    std::vector<std::pair<std::string, std::string>> pairs;
    ge::classic::protocol::info_pairs(c.userinfo, pairs);
    for (const auto& [k, v] : pairs)
      if (k != "name" && k != "*ge_ext" && info_get(before, k) != v) s.rules->user_info(slot, k, v);
  }
  c.rate = rate_of(s, c.userinfo, c.address);
  if (c.stage == Q2Slot::active && info_get(c.userinfo, "rate") != was) rate_check(s, slot);
  std::string name = cut_name(info_get(c.userinfo, "name"));
  // A player in the game renamed past sv_namelimit keeps the old name (server/names.hpp).
  const bool renamed = !name.empty() && name != c.name && c.stage == Q2Slot::active;
  if (renamed && !ge::server::rename_allowed(c.renames, s.now, s.cfg.names)) {
    info_set(c.userinfo, "name", c.name);
    q2_net_print(s, slot, ge::server::kRenameRefusal);
    name = c.name;
  }
  if (!name.empty() && s.name_rule != nullptr)
    s.name_rule(s.hook_ctx, slot, name, c.stage != Q2Slot::active);
  if (!name.empty() && name != c.name) {
    const std::string old = c.name;
    c.name = name;
    if (c.stage == Q2Slot::active) {
      s.rules->user_info(slot, "name", name);
      std::string line = old;
      line += " [" + gn::to_string(c.address) + "] renamed ";
      note(s, line + name);
      if (ge::server::rename_told(s.cfg.names, !c.spectator))
        for (int k = 0; k < sh::kMaxClients; k++)
          if (s.clients[k].stage == Q2Slot::active)
            q2_net_print(s, k, ge::server::rename_notice(old, name));
    }
  }
  return true;
}

// The frame a later protocol's move says the client read: the lag's numbers, the ping, a whole
// frame asked for too often (sv_nodelta 0). False when the client was dropped.
bool took_frame(Q2NetServer& s, int slot, int32_t frame) {
  Q2NetClient& c = s.clients[slot];
  const bool sent = frame > 0 && c.sent_frame[frame & 63] == frame;
  if (frame > c.acked && sent) c.frames_acked++;
  c.acked = frame;
  if (sent) {
    c.ping_s = s.now - c.sent_at[frame & 63];
    take_trip(s, slot, c.ping_s);
  }
  if (frame <= 0 && ++c.nodelta > kNoDeltaMost && !s.cfg.allow_nodelta) {
    drop(s, slot, "too many nodelta frames");
    return false;
  }
  c.moves++;
  return true;
}

// One command of a later protocol's client handed to the rules as a packet of its own (its own
// sequence), so a batch of any length runs whole and in order.
void run_command(Q2NetServer& s, int slot, const Q2Cmd& cmd, std::vector<sh::Packet>& packets) {
  Q2NetClient& c = s.clients[slot];
  sh::Packet p;
  p.client = slot;
  p.sequence = ++c.run_sequence;
  sh::MoveCmd m = move_cmd(cmd);
  if (s.move_hook != nullptr) s.move_hook(s.move_ctx, slot, m);
  for (sh::MoveCmd& k : p.cmds) k = m;
  packets.push_back(p);
  c.last_run = cmd;
  c.last_cmd = ge::classic::q2_command(m);
  c.last_cmd_at = s.now;
  c.cmds_run++;
  c.scale_ms += cmd.msec;
  const int msec[3] = {0, 0, cmd.msec};
  ge::server::skew_packet(c.skew, s.now, p.sequence, msec);
  ge::server::idle_move(c.idle, s.now, c.last_cmd.buttons, c.last_cmd.forward, c.last_cmd.side,
                        c.last_cmd.up);
}

// 36's user info change (clc 12, its id read): a key and a value, again while the next byte is 12,
// each set (an empty value takes the key out), then the whole user info judged as a new one.
bool userinfo_delta(Q2NetServer& s, int slot, gn::Reader& r) {
  Q2NetClient& c = s.clients[slot];
  std::string info = c.userinfo;
  for (bool first = true; first || (gn::remaining(r) > 0 && r.data[r.at] == kClcUserInfoDelta);
       first = false) {
    if (!first) read_byte(r);
    const std::string key = read_string(r), value = read_string(r);
    if (r.bad) {
      drop_told(s, slot, "read past end of message", Told::dropped);
      return false;
    }
    const char* refused = key.size() >= 64     ? "oversize userinfo key"
                          : value.size() >= 64 ? "oversize userinfo value"
                                               : nullptr;
    if (refused == nullptr && value.empty()) {
      info_remove(info, key);
    } else if (refused == nullptr && !info_set(info, key, value)) {
      refused = "malformed userinfo";
    }
    if (refused != nullptr) {
      drop_told(s, slot, refused, Told::dropped);
      return false;
    }
  }
  return take_userinfo(s, slot, std::move(info));
}

void read_messages(Q2NetServer& s, int slot, const uint8_t* data, size_t size,
                   std::vector<sh::Packet>& packets) {
  Q2NetClient& c = s.clients[slot];
  gn::Reader r = gn::make_reader(data, size);
  int commands = 0;
  bool moved = false;
  while (gn::remaining(r) > 0 && q2_held(c)) {
    const int id = read_byte(r);
    if (id == kClcNop) continue;
    const bool later = c.protocol != kQ2NetProtocol;
    const int low = id & 0x1F;
    if (later && id == kClcSetting) {  // a client setting: kept as sent, an unknown index ignored
      const int index = read_short(r), value = read_short(r);
      if (index >= 0 && index < kQ2Settings) c.asked[index] = static_cast<int16_t>(value);
      continue;
    }
    if (c.protocol == kQ2Protocol36 && id == kClcUserInfoDelta) {
      if (!userinfo_delta(s, slot, r)) return;
      continue;
    }
    if (c.protocol == kQ2Protocol36 && (low == kClcMoveNoDelta || low == kClcMoveBatched)) {
      if (moved) return drop_told(s, slot, "multiple clc_move commands in packet", Told::dropped);
      moved = true;
      Q2Batch b;
      std::string reason;
      if (!read_q2_batch(r, static_cast<uint8_t>(id), b, reason))
        return drop_told(s, slot, reason, Told::dropped);
      if (c.stage != Q2Slot::active) {
        c.acked = -1;
        continue;
      }
      if (!took_frame(s, slot, b.frame)) return;
      // The frames a loss covers (all their commands, oldest first), the last command again for
      // each packet lost past them, then the newest frame.
      const int extra = static_cast<int>(b.frames.size()) - 1;
      const int lost = static_cast<int>(c.ch.last_drop);
      if (lost > extra) c.frame_flags |= 4;
      if (lost < 20) {
        for (int k = extra; k < lost; k++) run_command(s, slot, c.last_run, packets);
        for (int f = std::max(0, extra - lost); f < extra; f++)
          for (const Q2Cmd& cmd : b.frames[size_t(f)]) run_command(s, slot, cmd, packets);
      }
      for (const Q2Cmd& cmd : b.frames.back()) run_command(s, slot, cmd, packets);
      continue;
    }
    if (id == kClcMove) {
      if (moved) {
        if (later) return drop_told(s, slot, "multiple clc_move commands in packet", Told::dropped);
        return;  // one move a packet: the rest is ignored
      }
      moved = true;
      Q2Move m;
      const bool read = later ? read_q2_move_ext(r, c.protocol, c.minor, m)
                              : read_q2_move(r, c.ch.incoming_sequence, m);
      if (!read) return drop(s, slot, "a cut move");
      if (c.stage != Q2Slot::active) {
        c.acked = -1;
        continue;
      }
      if (later) {  // the commands one by one: the last again past two lost, then those held
        if (!took_frame(s, slot, m.frame)) return;
        const int lost = static_cast<int>(c.ch.last_drop);
        if (lost > 2) c.frame_flags |= 4;
        if (lost < 20) {
          for (int k = 2; k < lost; k++) run_command(s, slot, c.last_run, packets);
          for (int k = 2 - std::min(lost, 2); k < 2; k++) run_command(s, slot, m.cmds[k], packets);
        }
        run_command(s, slot, m.cmds[2], packets);
        continue;
      }
      if (m.frame > c.acked && m.frame > 0 && c.sent_frame[m.frame & 63] == m.frame)
        c.frames_acked++;  // the lag's loss toward the client
      c.acked = m.frame;
      if (m.frame > 0 && c.sent_frame[m.frame & 63] == m.frame) {
        c.ping_s = s.now - c.sent_at[m.frame & 63];
        take_trip(s, slot, c.ping_s);
      }
      if (m.frame <= 0 && ++c.nodelta > kNoDeltaMost && !s.cfg.allow_nodelta)
        return drop(s, slot, "too many nodelta frames");
      if (s.cfg.verify_checksum && !m.checksum_ok) {
        c.bad_checksums++;
        return;
      }
      c.moves++;
      // The moves run: the newest, and one a lost packet held for each lost (two at most).
      c.cmds_run += 1 + std::min<uint32_t>(c.ch.last_drop, 2);
      c.scale_ms += m.cmds[2].msec;
      sh::Packet p;
      p.client = slot;
      p.sequence = c.ch.incoming_sequence;
      for (int k = 0; k < 3; k++) {
        p.cmds[k] = move_cmd(m.cmds[k]);
        if (s.move_hook != nullptr) s.move_hook(s.move_ctx, slot, p.cmds[k]);
      }
      packets.push_back(p);
      c.last_cmd = ge::classic::q2_command(p.cmds[2]);  // as the rules run it
      c.last_cmd_at = s.now;
      const int msec[3] = {m.cmds[0].msec, m.cmds[1].msec, m.cmds[2].msec};
      ge::server::skew_packet(c.skew, s.now, c.ch.incoming_sequence, msec);
      ge::server::idle_move(c.idle, s.now, c.last_cmd.buttons, c.last_cmd.forward, c.last_cmd.side,
                            c.last_cmd.up);
    } else if (id == kClcUserInfo) {
      if (!take_userinfo(s, slot, read_string(r))) return;
    } else if (id == kClcStringCmd) {
      const std::string line = read_string(r);
      if (line.size() >= 1024) return drop_told(s, slot, "oversize stringcmd", Told::dropped);
      if (++commands <= kStringCmdsPerPacket) string_command(s, slot, line);
    } else if (id == kClcGeVoice) {
      int gen = 0, seq = 0;
      std::vector<uint8_t> frames;
      if (!read_voice_up(r, gen, seq, frames)) return drop(s, slot, "a voice message cut short");
      relay_voice(s, slot, gen, seq, frames);
    } else if (later) {
      return drop_told(s, slot, "unknown command byte", Told::dropped);
    } else {
      return drop(s, slot, "a message " + std::to_string(id) + " it does not know");
    }
    if (r.bad) {
      if (later) return drop_told(s, slot, "read past end of message", Told::dropped);
      return drop(s, slot, "a message cut short");
    }
  }
}

// THE LAG's numbers, once a host tick: the round trip sampled and the move rate made every 10 s,
// the time scale every 16 s.
void lag_tick(const Q2NetServer& s, Q2NetClient& c) {
  if (c.stage != Q2Slot::active) return;
  if (c.ping_ms > 0) {
    c.rtt_min = std::min(c.rtt_min, c.ping_ms);
    c.rtt_max = std::max(c.rtt_max, c.ping_ms);
  }
  if (c.rtt_next < 0) c.rtt_next = s.now + 10;
  if (s.now >= c.rtt_next) {
    c.rtt_sum += c.ping_ms;
    c.rtt_samples++;
    c.move_fps = static_cast<int>((c.cmds_run - c.cmds_counted) / 10);
    c.cmds_counted = c.cmds_run;
    c.rtt_next += 10;
  }
  if (c.scale_from < 0) {
    c.scale_from = s.now;
    c.scale_ms = 0;
  } else if (s.now - c.scale_from >= 16) {
    c.scale = static_cast<double>(c.scale_ms) / ((s.now - c.scale_from) * 1000.0);
    c.scale_from = s.now;
    c.scale_ms = 0;
  }
}

// The slot a packet is from: a held one first, then a zombie (whose packets are read for their
// acknowledgements alone), so a client that connects again from the same port is its new slot.
// Protocol 34's qport is a word after the header; 35's and 36's a byte, or none when it is 0 (the
// client then known by its port).
int find_client(const Q2NetServer& s, const gn::Address& from, const uint8_t* data, size_t size) {
  int zombie = -1;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const Q2NetClient& c = s.clients[i];
    if (c.stage == Q2Slot::free || c.stage == Q2Slot::dummy || !gn::same_host(c.address, from))
      continue;
    bool match = false;
    if (c.protocol == kQ2NetProtocol) {
      uint16_t qport = 0;
      match = peek_qport(data, size, qport) && qport == c.qport;
    } else if (c.ch.qport_bytes == 1) {
      match = size > 8 && data[8] == (c.qport & 0xFFu);
    } else {
      match = c.address.port == from.port;
    }
    if (!match) continue;
    if (q2_held(c)) return i;
    if (zombie < 0) zombie = i;
  }
  return zombie;
}

int64_t frame_of(const Q2NetServer& s) {
  return s.native != nullptr ? s.native->frame_number : s.game->frame_number;
}

// Every item taken and coming back (the rules' world's `back_in`) onto `rel` for a client that
// agreed it: once when taken and again whenever its time moves (svc 119: entity, model, skin as
// shorts and a byte, the origin in eighths of a unit, the yaw as a short angle, the milliseconds
// until it shows as a long).
void write_items_back(const Q2NetServer& s, Q2NetClient& c, std::vector<uint8_t>& rel) {
  if (s.rules == nullptr) return;
  const sv::RulesCommon& g = **s.rules;
  const sv::RulesWorld& w = g.world;
  const int n = std::min(w.entity_count, static_cast<int>(w.entities.size()));
  if (c.back_told.size() < static_cast<size_t>(n)) c.back_told.resize(static_cast<size_t>(n), -1);
  const auto put16 = [&](int v) {
    rel.push_back(static_cast<uint8_t>(v & 255));
    rel.push_back(static_cast<uint8_t>((v >> 8) & 255));
  };
  const auto eighths = [](float v) {
    const double q = ge::core::num::round(static_cast<double>(v) * 8);
    return static_cast<int>(std::clamp(q, -32768.0, 32767.0));
  };
  for (int e = sh::kMaxClients + 1; e < n; e++) {
    const sv::WorldEntity& r = w.entities[static_cast<size_t>(e)];
    double& told = c.back_told[static_cast<size_t>(e)];
    if (!(r.back_in > 0) || r.back_model <= 0) {
      told = -1;
      continue;
    }
    const double due = w.time + static_cast<double>(r.back_in);
    if (told >= 0 && std::abs(told - due) < 0.05) continue;
    told = due;
    rel.push_back(kSvcGeBack);
    put16(e);
    put16(r.back_model);
    rel.push_back(static_cast<uint8_t>(r.skin & 255));
    put16(eighths(r.origin.x));
    put16(eighths(r.origin.y));
    put16(eighths(r.origin.z));
    put16(static_cast<int>(ge::core::num::round(static_cast<double>(r.angles.y) * 65536 / 360)));
    const double ms = std::min(static_cast<double>(r.back_in) * 1000 + 0.5, 2.0e9);
    const auto v = static_cast<uint32_t>(ms);
    for (int k = 0; k < 4; k++) rel.push_back(static_cast<uint8_t>((v >> (8 * k)) & 255));
  }
}

}  // namespace

std::vector<uint8_t> q2_arsenal_message(const sv::SqBody& body) {
  std::vector<uint8_t> out;
  uint32_t held = 0;
  for (int w = 0; w < sv::kSqWeaponCount; w++)
    if (sv::sq_holds(body, w)) held |= 1u << w;
  out.push_back(static_cast<uint8_t>(held & 255));
  out.push_back(static_cast<uint8_t>(held >> 8));
  for (int k = 1; k < sv::kSqBaseAmmoKinds; k++) {  // a pack's own kinds stay off the wire
    const int n = std::clamp(body.ammo[k], 0, 32767);
    out.push_back(static_cast<uint8_t>(n & 255));
    out.push_back(static_cast<uint8_t>(n >> 8));
  }
  return out;
}

namespace {

// Every rules frame, counted from the level's start (a hosted library's are its tenths).
int64_t fast_of(const Q2NetServer& s) {
  return s.native != nullptr ? s.native->frame_number : s.game->fast_number;
}

// The frames a second client `c` is sent: every rules frame when it agreed to them, else ten.
int rate_slots(const Q2NetServer& s, const Q2NetClient& c) {
  if (!c.told_frames || s.native != nullptr) return kRateFrames;
  return std::clamp(s.game->fps, kRateFrames, kRateSlots);
}

// The other players' moves for client `i` (svc 124, q2_net_server.hpp) onto its frame.
void write_moves(const Q2NetServer& s, int i, std::vector<uint8_t>& b) {
  const Q2NetClient& c = s.clients[i];
  const size_t count_at = b.size() + 1;
  b.push_back(kSvcGeMoves);
  b.push_back(0);
  const auto u8 = [&b](int v) { b.push_back(static_cast<uint8_t>(v)); };
  const auto i16 = [&b](int v) {
    b.push_back(static_cast<uint8_t>(v & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  };
  int n = 0;
  for (int j = 0; j < sh::kMaxClients && n < 30; j++) {
    const Q2NetClient& o = s.clients[j];
    if (j == i || o.stage != Q2Slot::active || o.last_cmd_at < 0) continue;
    const auto e = static_cast<size_t>(j) + 1;
    if (e >= c.wire.last.size() || !c.wire.last[e].present) continue;  // not in this frame
    const ge::classic::Q2State& m = s.game->players[j].move;
    if (m.type != ge::classic::Q2Type::normal) continue;
    if (b.size() + 38 > kMovesBlockMost) break;
    const double ms = (s.now - o.last_cmd_at) * 1000.0;
    u8(j);
    u8(static_cast<int>(std::clamp(ms + 0.5, 0.0, 255.0)));
    u8(static_cast<int>(m.type));
    for (int32_t v : m.origin) i16(v);
    for (int32_t v : m.velocity) i16(v);
    u8(m.flags);
    u8(m.time);
    i16(m.gravity);
    for (int16_t v : m.delta_angles) i16(v);
    const ge::classic::Q2Cmd& k = o.last_cmd;
    u8(k.buttons);
    for (int16_t v : k.angles) i16(v);
    i16(k.forward);
    i16(k.side);
    i16(k.up);
    n++;
  }
  b[count_at] = static_cast<uint8_t>(n);
}

// Some other player than `i` moved after `since`.
// The bytes sent client `c` over the last second (THE RATE).
int sent_lately(const Q2NetClient& c) {
  int sum = 0;
  for (int b : c.frame_sizes) sum += b;
  return sum;
}

bool moved_since(const Q2NetServer& s, int i, double since) {
  for (int j = 0; j < sh::kMaxClients; j++)
    if (j != i && s.clients[j].stage == Q2Slot::active && s.clients[j].last_cmd_at > since)
      return true;
  return false;
}

void send_frames(Q2NetServer& s) {
  const int64_t tenth = frame_of(s), fast = fast_of(s);
  const bool fresh_tenth = tenth != s.written_frame, fresh_fast = fast != s.written_fast;
  s.written_frame = tenth;
  s.written_fast = fast;
  // The cut's links, once a frame: a hosted library's are taken with its view.
  const sv::Q2Sight* sight = nullptr;
  if (fresh_fast && s.cfg.cut_to_view) {
    if (s.native != nullptr) {
      sight = &s.native->raw.sight;
    } else {
      sv::q2_sight_reset(s.sight, s.game->map.get());
      sv::q2_sight_link_rules(s.sight, **s.rules);
      const sv::Q2Server* q = sv::q2_of(*s.rules);
      s.sight.groups = q != nullptr ? sv::map_area_groups(*q) : std::vector<int32_t>{};
      sight = &s.sight;
    }
  }
  const bool paused = *s.rules && (*s.rules)->paused;
  std::vector<uint8_t> block, rel;
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2NetClient& c = s.clients[i];
    if (c.stage == Q2Slot::free || c.stage == Q2Slot::dummy) continue;
    if (c.stage == Q2Slot::zombie) {  // the disconnect resent until the client has it
      if (channel_reliable_backlog(c.ch) > 0 && s.now - c.sent >= 0.1) transmit(s, c, nullptr, 0);
      continue;
    }
    if (c.ch.overflow) {
      drop(s, i, "overflowed");
      continue;
    }
    if (!c.signon_queue.empty()) feed_signon(c);
    // Reliable data alone: in the signon, after `nogamedata`, and while a file comes down.
    if (c.stage == Q2Slot::signon || c.no_game_data || c.downloading) {
      q2_download_piece(s, c);
      if (channel_reliable_backlog(c.ch) > 0 || s.now - c.sent >= 1.0) transmit(s, c, nullptr, 0);
      continue;
    }
    // A stock client is sent the tenths; one that agreed every frame, each rules frame.
    const bool fresh = c.told_frames ? fresh_fast : fresh_tenth;
    const int64_t frame = c.told_frames ? fast : tenth;
    const int slots = rate_slots(s, c);
    if (!fresh) {
      // Paused (a timeout): no frame comes, so the words said meanwhile and the acknowledgement of
      // what the client sends go alone, or a second command would wait on the first until play.
      if (paused && (channel_reliable_backlog(c.ch) > 0 || s.now - c.sent >= 1.0)) {
        transmit(s, c, nullptr, 0);
        continue;
      }
      if (s.cfg.moves_between && c.told_moves && s.native == nullptr && (s.ticks & 1) == 0 &&
          moved_since(s, i, c.moves_sent) && sent_lately(c) <= c.rate) {
        block.clear();
        write_moves(s, i, block);
        if (block[1] == 0) continue;
        c.frame_sizes[frame % slots] += static_cast<int>(block.size());  // the frame's second
        transmit(s, c, block.data(), block.size());
        c.moves_sent = s.now;
      }
      continue;
    }
    // The newer channel still sending a datagram's pieces: the next piece in place of this frame.
    if (channel_sending_pieces(c.ch)) {
      transmit(s, c, nullptr, 0);
      if (c.wire.suppress < 255) c.wire.suppress++;
      continue;
    }
    // THE RATE: past it over the last second, this frame is held back.
    int& size = c.frame_sizes[frame % slots];
    if (sent_lately(c) > c.rate) {
      size = 0;
      if (c.wire.suppress < 255) c.wire.suppress++;
      c.held++;
      continue;
    }
    if (c.acked <= 0) {
      sv::q2_resync(c.wire);
      c.full_frames++;
    } else if (c.acked != c.wire.last_frame) {  // from the newest frame it holds, if still kept
      const Q2NetClient::SentFrame& base = c.sent_frames[c.acked & 15];
      if (base.number == c.acked) {
        c.wire.last.assign(base.entities.begin(), base.entities.end());
        c.wire.last_player = base.player;
        c.wire.last_frame = c.acked;
      } else {
        sv::q2_resync(c.wire);
        c.full_frames++;
      }
    }
    rel.clear();
    c.wire.sight = sight;
    if (c.protocol != kQ2NetProtocol) {  // what the client asked to be left out, the frame's flags
      const bool on = c.asked[kQ2SetRecording] != 0;
      c.wire.asked = {.no_gun = c.asked[kQ2SetNoGun],
                      .no_blend = c.asked[kQ2SetNoBlend] != 0,
                      .recording = on,
                      .no_gibs = c.asked[kQ2SetNoGibs] != 0,
                      .no_footsteps = c.asked[kQ2SetNoFootsteps] != 0,
                      .no_predict = c.asked[kQ2SetNoPredict] != 0,
                      .no_flares = c.asked[kQ2SetNoFlares] != 0};
      c.wire.frame_flags = c.frame_flags;
      c.frame_flags = 0;
    }
    // The frame sized to the packet's room after the reliable data it carries, so a long
    // reliable message costs the frame its last entities, effects and sounds, not the frame.
    // A Quake II client reads at most 1400 bytes after the header, whatever the channel allows;
    // a newer channel cuts a long packet into pieces, so its frame keeps the block's own cap.
    const size_t room = channel_unreliable_room(c.ch, &c.wire.room_grows);
    const size_t over = kMaxPacket - kServerHeader - sv::kQ2MaxBlock;
    c.wire.room = c.ch.fragments ? 0 : std::max<size_t>(1, room > over ? room - over : 0);
    if (s.native != nullptr)
      sv::q2_frame_raw(c.wire, s.native->raw, static_cast<int32_t>(frame), block, rel);
    else
      sv::q2_frame(c.wire, **s.rules, static_cast<int32_t>(frame), &s.game->players[i].move, block,
                   &rel);
    Q2NetClient::SentFrame& kept = c.sent_frames[c.wire.last_frame & 15];
    kept.number = c.wire.last_frame;
    kept.entities.assign(c.wire.last.begin(), c.wire.last.end());  // no allocation once grown
    kept.player = c.wire.last_player;
    if (c.told_moves && s.native == nullptr) {
      write_moves(s, i, block);
      c.moves_sent = s.now;
    }
    if (c.told_share && s.game != nullptr) {  // the easy jump's share, when it changed
      const double share = std::clamp(static_cast<double>(s.game->sv.set.move.speedjump), 0.0, 1.0);
      const int milli = static_cast<int>(ge::core::num::round(share * 1000));
      if (milli != c.share_sent) {
        rel.push_back(kSvcGeShare);
        rel.push_back(static_cast<uint8_t>(milli & 255));
        rel.push_back(static_cast<uint8_t>(milli >> 8));
        c.share_sent = milli;
      }
    }
    if (c.told_back) write_items_back(s, c, rel);
    if (c.told_arsenal && s.game != nullptr) {  // the weapons held and the ammunition, when changed
      const std::vector<uint8_t> a = q2_arsenal_message(s.game->players[i].body);
      if (a != c.arsenal_sent) {
        rel.push_back(kSvcGeArsenal);
        rel.insert(rel.end(), a.begin(), a.end());
        c.arsenal_sent = a;
      }
    }
    if (c.told_tag && !c.tag_sent && !s.account_tag.empty()) {  // the logins' key, once known
      rel.push_back(kSvcGeAccountKey);
      rel.insert(rel.end(), s.account_tag.begin(), s.account_tag.end());
      rel.push_back(0);
      c.tag_sent = true;
    }
    if (c.told_feel && s.game != nullptr) {  // the move's feel, when it changed
      const std::vector<uint8_t> feel = q2_feel_message(sv::q2_move_feel(*s.game));
      if (feel != c.feel_sent) {
        rel.push_back(kSvcGeFeel);
        rel.insert(rel.end(), feel.begin(), feel.end());
        c.feel_sent = feel;
      }
    }
    reliable(c, rel);
    size = static_cast<int>(block.size());
    transmit(s, c, block.data(), block.size());
    for (int k = 0; k < c.packet_dup; k++) {  // `packetdup`: the same datagram again
      send_to(s, c.address, c.last_out.data(), c.last_out.size());
      size += static_cast<int>(c.last_out.size());
    }
    c.sent_frame[frame & 63] = frame;
    c.sent_at[frame & 63] = s.now;
    c.frames++;
  }
}

// Where `target`'s player was as `shooter`'s screen showed him, and his box then (our rules'
// `lag_place`); false when the shooter is not rewound (a bot, or `cl_antilag 0`).
bool lag_place(void* ctx, int shooter, int target, sh::Vec3f& at, sh::Vec3f& mins,
               sh::Vec3f& maxs) {
  const Q2NetServer& s = *static_cast<const Q2NetServer*>(ctx);
  if (shooter < 0 || shooter >= sh::kMaxClients || target < 0 || target >= sh::kMaxClients ||
      shooter == target)
    return false;
  const Q2NetClient& sc = s.clients[shooter];
  const Q2NetClient& tc = s.clients[target];
  // A bot aims at where players are; a bot is never moved.
  if (sc.stage != Q2Slot::active || tc.stage != Q2Slot::active || tc.spectator) return false;
  if (!antilag_wanted(info_get(sc.userinfo, "cl_antilag"))) return false;
  const bool known = sc.acked > 0 && sc.sent_frame[sc.acked & 63] == sc.acked;
  const double acked = known ? sc.sent_at[sc.acked & 63] : s.now;
  const double when =
      antilag_target(acked, sc.ping_s, 77, s.now, s.cfg.antilag_exact,
                     s.cfg.antilag_max_ms / 1000.0, s.cfg.antilag_interp ? s.game->step : 0.0);
  const int32_t* v = s.game->players[target].move.velocity;
  const sh::Vec3f velocity{float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
  AntilagPlace p;
  if (!antilag_place(s.antilag.players[target], when, velocity, p)) return false;
  at = p.origin;
  mins = p.mins;
  maxs = p.maxs;
  return true;
}

}  // namespace

std::string q2_status_text(const Q2NetServer& s) { return status_text(s); }

int q2_net_slots(const Q2NetServer& s) {
  return std::min(sh::kMaxClients, s.cfg.max_clients + std::max(0, s.cfg.max_spectators));
}

int q2_net_clients(const Q2NetServer& s) {
  int n = 0;
  for (const Q2NetClient& c : s.clients) n += q2_held(c) ? 1 : 0;
  return n;
}

namespace {

// The multiview recorder's turn (q2_mvd_server.hpp): its viewers each tick, a level's game state
// and a block each 10 Hz rules frame (our rules only: a hosted library's world is its own).
void record_multiview(Q2NetServer& s) {
  if (s.mvd.cfg.enable <= 0) return;
  q2_mvd_pump(s.mvd, s.now);
  if (s.game == nullptr) return;
  sv::Mvd2Capture cap;
  cap.no_blend = s.mvd.cfg.noblend;
  cap.no_gun = s.mvd.cfg.nogun;
  for (int i = 0; i < sh::kMaxClients; i++) cap.moves[i] = &s.game->players[i].move;
  const int64_t frame = frame_of(s);
  const bool level = s.mvd_level;
  if (!level && frame == s.mvd_frame) return;
  s.mvd_level = false;
  s.mvd_frame = frame;
  q2_mvd_dummy_before(s, level, cap);  // the recorder's spectator first (q2_mvd_dummy.hpp)
  if (level)
    q2_mvd_level(s.mvd, **s.rules, s.cfg.wire, cap, s.now);
  else
    q2_mvd_frame(s.mvd, **s.rules, s.cfg.wire, cap, s.now);
  q2_mvd_dummy_after(s);
}

}  // namespace

bool q2_net_start(Q2NetServer& s, const sv::RulesRunner& rules, gn::Transport& t,
                  const Q2NetConfig& cfg, std::string& error) {
  s.cfg = cfg;
  s.rules = &rules;
  s.game = sv::q2_of(rules);
  s.native = sv::native_q2_of(rules);
  s.transport = &t;
  if (s.game == nullptr && s.native == nullptr) {
    error = "the runner is not Quake II's: Quake II's net server serves only its rules";
    return false;
  }
  s.count = 0x10000 + cfg.wire.server_count;
  s.force_reconnect = cfg.force_reconnect;
  s.mvd.cfg = cfg.mvd;
  s.mvd.unix_time = cfg.unix_time;
  s.written_frame = frame_of(s);
  if (s.game != nullptr && cfg.antilag > 0) {
    s.game->lag_place = &lag_place;
    s.game->lag_ctx = &s;
    s.game->lag_missiles = cfg.antilag >= 2 && cfg.antilag_projectiles;
  }
  return true;
}

void q2_net_frame(Q2NetServer& s, int64_t now_us) {
  s.now = static_cast<double>(now_us) / 1e6;
  if (s.started < 0) s.started = s.now;
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
    if (!s.blackholes.entries.empty() &&
        ge::server::blackholed(s.blackholes, from, s.cfg.unix_time ? s.cfg.unix_time() : 0)) {
      s.blackholed++;
      continue;
    }
    if (is_out_of_band(buf, size)) {
      read_oob(s, from, buf, size);
      continue;
    }
    if (s.killed) continue;  // `killserver`: no player's packets until the next map
    const int slot = find_client(s, from, buf, size);
    if (slot < 0) continue;
    Q2NetClient& c = s.clients[slot];
    c.address = from;  // a router may have moved its port
    std::span<const uint8_t> payload;
    if (!channel_read(c.ch, buf, size, s.now, payload)) continue;
    c.heard = s.now;
    c.heard_once = true;
    if (c.ch.last_drop > 0) c.frame_flags |= 2;  // 36's frame flag: the client's packets lost
    if (q2_held(c)) read_messages(s, slot, payload.data(), payload.size(), packets);
  }
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2NetClient& c = s.clients[i];
    if (c.stage == Q2Slot::signon && !c.heard_once && s.cfg.ghost_time > 0 &&
        s.now - c.heard > s.cfg.ghost_time) {  // sv_ghosttime: a slot taken and never used
      note(s, c.name + " [" + gn::to_string(c.address) + "] never came: its slot is free again");
      c = Q2NetClient{};
      continue;
    }
    if (q2_held(c) && c.ch.outgoing_sequence >= 0x7FFFFF00u)
      drop_told(s, i, "outgoing sequence too big", Told::dropped);
    if (q2_held(c) && s.now - c.heard > s.cfg.timeout) {  // freed at once: no zombie
      drop_told(s, i, "timed out", Told::plain);
      c = Q2NetClient{};
      continue;
    }
    lag_tick(s, c);
    double ratio = 0;
    const ge::server::Skew skew = c.stage == Q2Slot::active
                                      ? ge::server::skew_check(c.skew, s.now, s.cfg.skew, ratio)
                                      : ge::server::Skew::fine;
    if (skew != ge::server::Skew::fine)
      note(s, c.name + " [" + gn::to_string(c.address) + "]: time skew " +
                  ge::core::num::to_fixed(ratio, 3));
    if (skew == ge::server::Skew::kick) {
      for (int k = 0; k < sh::kMaxClients; k++)
        if (k != i && s.clients[k].stage == Q2Slot::active)
          q2_net_print(s, k, c.name + " was dropped for a fast clock.\n");
      drop(s, i, "time skew too high");
    } else if (c.stage == Q2Slot::active && ge::server::idle_over(c.idle, s.now, s.cfg.idle_time)) {
      for (int k = 0; k < sh::kMaxClients; k++)
        if (k != i && s.clients[k].stage == Q2Slot::active)
          q2_net_print(s, k, c.name + " was dropped for idling.\n");
      q2_net_print(s, i, "You were dropped for idling.\n");
      drop(s, i, "idling");
    }
    if (c.stage == Q2Slot::zombie && s.now - c.heard > s.cfg.zombie_time) c = Q2NetClient{};
  }
  if (s.killed) return;   // `killserver`: no game runs until the next map
  if (s.game != nullptr)  // what the rules check a switch to or from watching against
    s.game->gate = {s.cfg.password, s.cfg.spectator_password,
                    s.cfg.max_spectators > 0 ? s.cfg.max_spectators : sh::kMaxClients};
  packets.insert(packets.end(), s.local_moves.begin(), s.local_moves.end());  // the bots'
  s.local_moves.clear();
  s.rules->frame(packets.data(), static_cast<int>(packets.size()));
  if (s.game != nullptr)  // a seat watches as the rules say: a switch mid-match lands here
    for (int i = 0; i < sh::kMaxClients; i++)
      if (s.clients[i].stage == Q2Slot::active)
        s.clients[i].spectator = s.game->clients[i].spectator;
  if (s.game != nullptr && s.cfg.antilag)  // where each player stands now, for the rewinds
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (s.clients[i].stage != Q2Slot::active) {
        antilag_clear(s.antilag, i);
        continue;
      }
      const sv::Q2Player& p = s.game->players[i];
      const int32_t* o = p.move.origin;
      antilag_record(s.antilag, i, s.now,
                     {float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f}, p.mins,
                     p.maxs);
    }
  send_frames(s);
  record_multiview(s);
  for (Q2NetClient& c : s.clients) {  // the voices relayed this tick: a frame late is lost
    if (c.voice_out.empty()) continue;
    if (c.stage == Q2Slot::active) transmit(s, c, c.voice_out.data(), c.voice_out.size());
    c.voice_out.clear();
  }
  s.ticks++;
}

void q2_net_level(Q2NetServer& s, const sv::Q2WireOptions& wire) {
  s.killed = false;
  s.force_reconnect = s.cfg.force_reconnect;          // latched: from this map on
  const int32_t count = s.cfg.wire.server_count + 1;  // the server data carries it too
  s.cfg.wire = wire;
  s.cfg.wire.server_count = count;
  s.count = 0x10000 + count;
  s.written_frame = frame_of(s);
  s.written_fast = fast_of(s);
  s.mvd_level = true;
  for (Q2NetClient& c : s.clients) {
    std::fill(std::begin(c.frame_sizes), std::end(c.frame_sizes), 0);  // the numbers start again
    for (Q2NetClient::SentFrame& f : c.sent_frames) f.number = -1;
    if (!q2_held(c)) continue;
    c.stage = Q2Slot::signon;  // the rules keep the slot until `begin` seats it again
    c.acked = -1;
    stuff(c, "changing\n");
    stuff(c, "reconnect\n");
  }
  note(s, "the level changed to " + (**s.rules).map_name);
}

void q2_net_print(Q2NetServer& s, int slot, const std::string& text) {
  if (slot < 0 || slot >= sh::kMaxClients || !q2_held(s.clients[slot])) return;
  std::vector<uint8_t> b = {kSvcPrint, 2};
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
  reliable(s.clients[slot], b);
}

void q2_net_rename(Q2NetServer& s, int slot, const std::string& name) {
  if (slot < 0 || slot >= sh::kMaxClients) return;
  Q2NetClient& c = s.clients[slot];
  if (!q2_held(c) || c.name == name) return;
  c.name = name;
  if (c.stage == Q2Slot::active) s.rules->user_info(slot, "name", name);
}

void q2_net_ask_settings(Q2NetServer& s, int slot) { ask_settings(s, s.clients[slot]); }

void q2_net_drop(Q2NetServer& s, int slot, const std::string& why) {
  if (slot < 0 || slot >= sh::kMaxClients) return;
  drop(s, slot, why);
}

void q2_net_stop(Q2NetServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    drop(s, i, "the server stopped");
    s.clients[i] = Q2NetClient{};  // no zombies past the end
  }
}

void q2_net_oob(Q2NetServer& s, const gn::Address& to, const std::string& text) {
  send_oob(s, to, text);
}

}  // namespace shooters_edge::net

namespace shooters_edge::net {

namespace {

// `status t`'s and the misc block's connection time: `mm.ss`, `h:mm.ss`, `d+h:mm.ss`.
std::string span_text(double seconds) {
  const auto t = static_cast<int64_t>(seconds < 0 ? 0 : seconds);
  const int64_t sec = t % 60, min = t / 60 % 60, hour = t / 3600 % 24, day = t / 86400;
  char b[48];
  if (t < 3600)
    std::snprintf(b, sizeof b, "%02d.%02d", int(min), int(sec));
  else if (t < 86400)
    std::snprintf(b, sizeof b, "%d:%02d.%02d", int(hour), int(min), int(sec));
  else
    std::snprintf(b, sizeof b, "%d+%d:%02d.%02d", int(day), int(hour), int(min), int(sec));
  return b;
}

std::string padded(const char* label, const std::string& value) {
  char b[32];
  std::snprintf(b, sizeof b, "%-21s", label);
  return b + value + "\n";
}

}  // namespace

std::string q2_span_text(double seconds) { return span_text(seconds); }

double q2_loss_down(const Q2NetClient& c) {
  return c.frames == 0
             ? 0.0
             : 100.0 * (1.0 - static_cast<double>(c.frames_acked) / static_cast<double>(c.frames));
}

double q2_loss_up(const Q2NetClient& c) {
  return c.ch.accepted == 0
             ? 0.0
             : 100.0 * static_cast<double>(c.ch.dropped) / static_cast<double>(c.ch.accepted);
}

int q2_rtt_avg(const Q2NetClient& c) {
  return c.rtt_samples == 0 ? c.ping_ms : static_cast<int>(c.rtt_sum / c.rtt_samples);
}

std::string q2_misc_lines(const Q2NetServer& s, int slot) {
  const Q2NetClient& c = s.clients[slot];
  char b[96];
  std::string t;
  t += padded("version", c.version_heard ? c.version : "-");
  t += padded("protocol (maj/min)", std::to_string(c.protocol) + "/" + std::to_string(c.minor));
  t += padded("maxmsglen", std::to_string(c.msglen));
  t += padded("zlib support", c.zlib ? "yes" : "no");
  t += padded("netchan type", c.ch.fragments ? "new" : "old");
  t += padded("ping", std::to_string(c.ping_ms));
  t += padded("movement fps", std::to_string(c.move_fps));
  std::snprintf(b, sizeof b, "%d/%d/%d ms", c.rtt_min, q2_rtt_avg(c), c.rtt_max);
  t += padded("RTT (min/avg/max)", b);
  std::snprintf(b, sizeof b, "%.2f%% (approx)", q2_loss_down(c));
  t += padded("PL server to client", b);
  std::snprintf(b, sizeof b, "%.2f%%", q2_loss_up(c));
  t += padded("PL client to server", b);
  t += padded("packetdup", std::to_string(c.packet_dup));
  std::snprintf(b, sizeof b, "%.3f", c.scale);
  t += padded("timescale", b);
  t += padded("connection time", span_text(c.new_at < 0 ? 0 : s.now - c.new_at));
  return t;
}

std::string q2_lag_lines(const Q2NetServer& s, int slot) {
  const Q2NetClient& c = s.clients[slot];
  char b[160];
  std::string t = "Lag stats for:       " + c.name + "\n";
  std::snprintf(b, sizeof b, "RTT (min/avg/max):   %d/%d/%d ms\n", c.rtt_min, q2_rtt_avg(c),
                c.rtt_max);
  t += b;
  std::snprintf(b, sizeof b, "Server to client PL: %.2f%% (approx)\n", q2_loss_down(c));
  t += b;
  std::snprintf(b, sizeof b, "Client to server PL: %.2f%%\n", q2_loss_up(c));
  t += b;
  std::snprintf(b, sizeof b, "Timescale          : %.3f\n", c.scale);
  return t + b;
}

std::string q2_info_lines(const Q2NetServer& s) {
  std::vector<std::pair<std::string, std::string>> pairs;
  info_pairs(status_text(s).substr(0, status_text(s).find('\n')), pairs);
  for (auto& [k, v] : pairs)
    if (k == "maxclients") v = std::to_string(s.cfg.max_clients);  // whole here
  std::sort(pairs.begin(), pairs.end());
  std::string t;
  char b[32];
  for (const auto& [k, v] : pairs) {
    if (v.empty()) continue;
    std::snprintf(b, sizeof b, "%-20s ", k.c_str());
    t += b + v + "\n";
  }
  return t;
}

int q2_public_slots(const Q2NetServer& s) {
  return std::max(0, s.cfg.max_clients - std::clamp(s.cfg.vip_slots, 0, s.cfg.max_clients - 1));
}

void q2_net_stuff(Q2NetServer& s, int slot, const std::string& text) {
  if (slot < 0 || slot >= sh::kMaxClients || !q2_held(s.clients[slot])) return;
  stuff(s.clients[slot], text);
}

void q2_net_kick(Q2NetServer& s, int slot, bool logged) {
  drop_told(s, slot, "was kicked", Told::plain, logged);
}

void q2_net_ask_setting(Q2NetServer& s, int slot, const std::string& name) {
  if (slot < 0 || slot >= sh::kMaxClients || !q2_held(s.clients[slot])) return;
  Q2NetClient& c = s.clients[slot];
  c.cvars_asked++;
  stuff(c,
        "cmd " + std::string(ge::server::kSettingAnswer) + " console " + name + " $" + name + "\n");
}

void q2_net_shutdown(Q2NetServer& s, const std::string& text, bool reconnect) {
  q2_mvd_shutdown(s.mvd, reconnect);
  std::vector<uint8_t> b = {kSvcPrint, 2};
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
  b.push_back(reconnect ? kSvcReconnect : kSvcDisconnect);
  for (int pass = 0; pass < 2; pass++)
    for (Q2NetClient& c : s.clients)
      if (q2_held(c)) transmit(s, c, b.data(), b.size());
  for (int i = 0; i < sh::kMaxClients; i++) {
    if ((**s.rules).clients[i].active && q2_held(s.clients[i])) s.rules->drop(i);
    s.clients[i] = Q2NetClient{};
  }
}

}  // namespace shooters_edge::net
