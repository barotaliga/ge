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
#include <utility>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "server/admission.hpp"
#include "server/rate.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/q2_admin.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/server/q2_fight.hpp"

namespace shooters_edge::net {

namespace {

namespace pg = ge::progs;

constexpr size_t kSignonShare = 700;  // bytes of the signon one answer adds before its prompt
constexpr int kStringCmdsPerPacket = 7;
constexpr uint8_t kSvcDisconnect = 7, kSvcPrint = 10, kSvcStuffText = 11, kSvcDownload = 16;
constexpr uint8_t kSvcGeOrigin = 123;      // the game's own: a download's archive (qw_svc.hpp)
constexpr uint8_t kSvcGeMoves = 124;       // the game's own: the other players' moves
constexpr uint8_t kSvcGeShare = 126;       // the game's own: the easy jump's share
constexpr uint8_t kSvcGeAccountKey = 127;  // the game's own: the logins' key tag
constexpr uint8_t kSvcGeFeel = 122;        // the game's own: the move's feel
constexpr uint8_t kSvcGeBack = 119;        // the game's own: an item taken and when it comes back
constexpr uint8_t kSvcGeVoice = 121;       // the game's own: a voice (qw_voice_route.hpp)
constexpr size_t kVoiceTickBytes = 1200;   // the voices one client is sent a tick, at most
constexpr size_t kMovesBlockMost = 1200;   // the frame with the moves, at most (under a datagram)
constexpr size_t kLogKept = 512;
// Quake II's reading of a client's `rate`: none 5000 (held to no limit), 0 the least, and 100 up
// to the engine's most (sv_minrate and sv_maxrate, 15000 and 60000 by default, hold it nearer).
constexpr ge::server::RateBounds kRateBounds{
    .none = 5000, .zero = 100, .low = 100, .none_held = false};
constexpr int kRateFrames = 10;  // the frames whose sizes are summed (a second at 10 Hz)
constexpr int kRateSlots = 60;   // the most: a second at `sv_fps` 60

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

uint64_t host_key(gn::Address a) {
  a.port = 0;
  return gn::address_hash(a);
}

bool same_host(gn::Address a, gn::Address b) {
  a.port = b.port = 0;
  return a == b;
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

// THE DOWNLOADS (Q2NetConfig): the next piece of the file being sent, up to 1024 bytes: size,
// percent, the bytes; percent 100 ends it.
void download_piece(Q2NetClient& c) {
  const size_t n = std::min<size_t>(1024, c.download.size() - c.download_at);
  c.download_at += n;
  const int percent =
      c.download.empty() ? 100 : static_cast<int>(c.download_at * 100 / c.download.size());
  std::vector<uint8_t> b = {kSvcDownload, static_cast<uint8_t>(n & 0xff),
                            static_cast<uint8_t>(n >> 8), static_cast<uint8_t>(percent)};
  b.insert(b.end(), c.download.begin() + static_cast<std::ptrdiff_t>(c.download_at - n),
           c.download.begin() + static_cast<std::ptrdiff_t>(c.download_at));
  reliable(c, b);
  if (c.download_at >= c.download.size()) {
    c.downloading = false;
    c.downloads++;
    c.download.clear();
  }
}

void start_download(Q2NetServer& s, Q2NetClient& c, std::string path, int offset) {
  for (char& ch : path) {
    if (ch == '\\') ch = '/';
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  }
  c.downloading = false;
  c.download.clear();
  const auto under = [&path](const char* dir) { return path.rfind(dir, 0) == 0; };
  std::string why;
  ge::vfs::ArchiveOrigin origin;
  bool packed = false;
  if (!s.cfg.allow_download || !s.cfg.read_file) {
    why = "downloads are off";
  } else if (path.empty() || path[0] == '.' || path[0] == '/' ||
             path.find("..") != std::string::npos || path.find('/') == std::string::npos) {
    why = "not a file the server offers";
  } else if ((under("players/") && !s.cfg.allow_players) ||
             (under("models/") && !s.cfg.allow_models) ||
             (under("sound/") && !s.cfg.allow_sounds) || (under("maps/") && !s.cfg.allow_maps)) {
    why = "that kind of file is not offered";
  } else if (!s.cfg.read_file(path, &c.download) || c.download.empty()) {
    why = "not found";
  } else {
    packed = s.cfg.file_origin && s.cfg.file_origin(path, &origin);
    if (packed && under("maps/") && !c.told_origins) why = "the map is in an archive";
  }
  if (!why.empty() || offset < 0 || static_cast<size_t>(offset) > c.download.size()) {
    c.download.clear();
    const uint8_t refused[4] = {kSvcDownload, 0xff, 0xff, 0};
    reliable(c, std::vector<uint8_t>(refused, refused + 4));
    note(s, c.name + " asked for " + path + ": refused, " + (why.empty() ? "a bad offset" : why));
    return;
  }
  if (packed && c.told_origins) {  // the archive the file comes out of, before its first piece
    std::vector<uint8_t> b = {kSvcGeOrigin};
    ge::vfs::write_origin(path, origin, &b);
    reliable(c, b);
  }
  c.downloading = true;
  c.download_at = static_cast<size_t>(offset);
  download_piece(c);
}

void transmit(Q2NetServer& s, Q2NetClient& c, const uint8_t* data, size_t size) {
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(c.ch, data, size, s.now, out);
  send_to(s, c.address, out, n);
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

void drop(Q2NetServer& s, int slot, const std::string& why) {
  Q2NetClient& c = s.clients[slot];
  if (!q2_held(c)) return;
  note(s, c.name + " dropped: " + why);
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

// A password setting that asks for one: neither empty nor `none`.
bool set(const std::string& password) { return !password.empty() && password != "none"; }

std::string info_text(const Q2NetServer& s) {
  char line[96];
  std::snprintf(line, sizeof line, "%16s %8s %2d/%2d\n", s.cfg.hostname.c_str(),
                (*s.rules)->map_name.c_str(), q2_net_clients(s), s.cfg.max_clients);
  return line;
}

std::string status_text(const Q2NetServer& s) {
  std::string info;
  info_set(info, "hostname", s.cfg.hostname);
  info_set(info, "mapname", (*s.rules)->map_name);
  info_set(info, "maxclients", std::to_string(s.cfg.max_clients));
  info_set(info, "maxspectators", std::to_string(s.cfg.max_spectators));
  // `needpass`: 1 a player's password asked, 2 a spectator's.
  info_set(info, "needpass",
           std::to_string((set(s.cfg.password) ? 1 : 0) | (set(s.cfg.spectator_password) ? 2 : 0)));
  info_set(info, "deathmatch", "1");
  info_set(info, "protocol", std::to_string(kQ2NetProtocol));
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
  if (w.size() < 5) return;
  if (std::atoi(w[1].c_str()) != kQ2NetProtocol)
    return refuse(s, from, "Server is protocol version 34.");
  const auto qport = static_cast<uint16_t>(std::atoi(w[2].c_str()));
  const auto at = s.challenges.find(host_key(from));
  if (at == s.challenges.end()) return refuse(s, from, "No challenge for address.");
  if (at->second != std::atoi(w[3].c_str())) return refuse(s, from, "Bad challenge.");
  if (ge::server::ban_refuses(s.bans, s.cfg.ban_mode, from,
                              s.cfg.unix_time ? s.cfg.unix_time() : 0))
    return refuse(s, from, "Banned.");
  const std::string& userinfo = w[4];
  for (const Q2NetClient& c : s.clients)
    if (q2_held(c) && same_host(c.address, from) &&
        (c.qport == qport || c.address.port == from.port))
      return;          // already in: a repeat
  if (s.cfg.locked) {  // sv_locked: only a client taking back the slot its address still holds
    bool back = false;
    for (const Q2NetClient& c : s.clients)
      back = back || (c.stage == Q2Slot::zombie && same_host(c.address, from) &&
                      (c.qport == qport || c.address.port == from.port));
    if (!back) return refuse(s, from, "Server is locked.\nConnection refused.");
  }
  if (s.cfg.ip_limit > 0) {  // one address's slots held to sv_iplimit, a zombie's a half
    std::vector<ge::server::HeldSeat> held;
    for (const Q2NetClient& o : s.clients)
      if (o.stage != Q2Slot::free) held.push_back({o.address, o.stage == Q2Slot::zombie});
    const std::string refusal = ge::server::admit_refusal(s.cfg.ip_limit, from, held);
    if (!refusal.empty()) return refuse(s, from, refusal + "\nConnection refused.");
  }
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
    if (o.stage == Q2Slot::free) {
      if (slot < 0) slot = i;
    } else {
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
  Q2NetClient& c = s.clients[slot];
  c = Q2NetClient{};
  c.stage = Q2Slot::signon;
  c.address = from;
  c.qport = qport;
  c.userinfo = userinfo;
  c.name = info_get(userinfo, "name");
  if (c.name.empty()) c.name = "unnamed";
  const int ext = std::atoi(info_get(userinfo, "*ge_ext").c_str());
  c.told_origins = (ext & kGeDownloadOrigin) != 0;
  c.told_moves = (ext & kGeOtherMoves) != 0;
  c.told_share = (ext & kGeSpeedJumpShare) != 0;
  c.told_tag = (ext & kGeAccountKey) != 0;
  c.told_feel = (ext & kGeMoveFeel) != 0;
  c.told_back = (ext & kGeItemBack) != 0;
  c.told_voice = s.cfg.voice && (ext & kGeVoice) != 0;
  c.told_frames = (ext & kGeEveryFrame) != 0;
  c.userid = s.next_userid++;
  c.spectator = spectator;
  c.rate = rate_of(s, userinfo, from);
  if (s.name_rule != nullptr) s.name_rule(s.hook_ctx, slot, c.name, true);
  c.heard = s.now;
  channel_setup(c.ch, false, qport, s.now);
  channel_set_rate(c.ch, 0);
  send_oob(s, from, "client_connect");
  note(s, c.name + " connected");
}

void read_oob(Q2NetServer& s, const gn::Address& from, const uint8_t* data, size_t size) {
  const std::string text = q2_oob_text(data, size);
  const std::vector<std::string> w = q2_words(text);
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
    if (w.size() > 1 && std::atoi(w[1].c_str()) != kQ2NetProtocol)
      send_oob(s, from, "info\n" + s.cfg.hostname + ": wrong version\n");
    else
      send_oob(s, from, "info\n" + info_text(s));
  } else if (w[0] == "status") {  // sv_status 0 or past sv_statusrate: no answer
    if (s.cfg.status.show > 0 && ge::server::limit_take(s.status_tries, s.now, s.cfg.status.rate))
      send_oob(s, from, "print\n" + status_text(s));
  } else if (w[0] == "ping") {
    send_oob(s, from, "ack");
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
      q2_net_print(s, slot, "You were kicked from the game\n");
      drop(s, slot, "kicked");
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
  if (s.native != nullptr)
    sv::q2_signon_raw(c.wire, s.native->raw, slot, opt, c.signon);
  else
    sv::q2_signon_parts(c.wire, **s.rules, slot, opt, c.signon);
  ask_settings(s, c);  // setting bans and user info bans at every `new` (server/settingban.hpp)
  if (judge_userinfo(s, slot)) return;
  reliable(c, c.signon.server_data);
  stuff(c, "cmd configstrings " + std::to_string(s.count) + " 0\n");
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

void string_command(Q2NetServer& s, int slot, const std::string& line) {
  Q2NetClient& c = s.clients[slot];
  const std::vector<std::string> w = q2_words(line);
  if (w.empty()) return;
  const std::string& cmd = w[0];
  if (cmd == ge::server::kSettingAnswer) {  // a setting ban's answer: `<DEL>c <name> <value>`
    if (w.size() < 2) return;
    const size_t at = line.find(w[1], line.find(cmd) + cmd.size()) + w[1].size();
    std::string value = at < line.size() ? line.substr(at + 1) : std::string();
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) value.pop_back();
    judge(s, slot, s.cvar_bans, w[1], value);
    return;
  }
  const bool signon = cmd == "configstrings" || cmd == "baselines" || cmd == "begin";
  const bool protocol =
      signon || cmd == "new" || cmd == "disconnect" || cmd == "download" || cmd == "nextdl";
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
    if (c.stage == Q2Slot::signon) start_signon(s, slot);
  } else if (cmd == "configstrings") {
    if (c.stage == Q2Slot::signon)
      signon_share(c, c.signon.config_strings, at, "cmd configstrings " + count + " ",
                   "cmd baselines " + count + " 0\n");
  } else if (cmd == "baselines") {
    if (c.stage == Q2Slot::signon)
      signon_share(c, c.signon.baselines, at, "cmd baselines " + count + " ",
                   "precache " + count + "\n");
  } else if (cmd == "begin") {
    if (c.stage != Q2Slot::signon) return;
    if (s.native != nullptr) {  // a game library reads the client's whole user info
      auto& info = (**s.rules).clients[slot].userinfo;
      info.clear();
      ge::classic::protocol::info_pairs(c.userinfo, info);
    }
    s.rules->connect(slot, c.name, c.spectator, false);
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
    rate_check(s, slot);
  } else if (cmd == "download") {
    start_download(s, c, w.size() > 1 ? w[1] : std::string(), at);
  } else if (cmd == "nextdl") {
    if (c.downloading) download_piece(c);
  } else if (cmd == "disconnect") {
    drop(s, slot, "left");
  } else if (c.stage == Q2Slot::active) {
    if (cmd == "say" || cmd == "say_team") ge::server::idle_chat(c.idle, s.now);
    s.rules->client_command(slot, line);
  }
}

// sv_nodelta 0: the whole frames a client may ask for before it is dropped.
constexpr int kNoDeltaMost = 64;

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
void read_messages(Q2NetServer& s, int slot, const uint8_t* data, size_t size,
                   std::vector<sh::Packet>& packets) {
  Q2NetClient& c = s.clients[slot];
  gn::Reader r = gn::make_reader(data, size);
  int commands = 0;
  bool moved = false;
  while (gn::remaining(r) > 0 && q2_held(c)) {
    const int id = read_byte(r);
    if (id == kClcNop) continue;
    if (id == kClcMove) {
      if (moved) return;  // one move a packet: the rest is ignored
      moved = true;
      Q2Move m;
      if (!read_q2_move(r, c.ch.incoming_sequence, m)) return drop(s, slot, "a cut move");
      if (c.stage != Q2Slot::active) continue;
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
      sh::Packet p;
      p.client = slot;
      p.sequence = c.ch.incoming_sequence;
      for (int k = 0; k < 3; k++) p.cmds[k] = move_cmd(m.cmds[k]);
      packets.push_back(p);
      c.last_cmd = ge::classic::q2_command(p.cmds[2]);  // as the rules run it
      c.last_cmd_at = s.now;
      const int msec[3] = {m.cmds[0].msec, m.cmds[1].msec, m.cmds[2].msec};
      ge::server::skew_packet(c.skew, s.now, c.ch.incoming_sequence, msec);
      ge::server::idle_move(c.idle, s.now, c.last_cmd.buttons, c.last_cmd.forward, c.last_cmd.side,
                            c.last_cmd.up);
    } else if (id == kClcUserInfo) {
      const std::string was = info_get(c.userinfo, "rate");
      const std::string before = c.userinfo;
      c.userinfo = read_string(r);
      if (judge_userinfo(s, slot)) return;  // a user info ban that kicks (server/settingban.hpp)
      const bool rewound = antilag_wanted(info_get(c.userinfo, "cl_antilag"));
      if (s.cfg.antilag > 0 && c.stage == Q2Slot::active &&
          rewound != antilag_wanted(info_get(before, "cl_antilag")))
        q2_net_print(s, slot, antilag_told(rewound));          // his own shots rewound or not
      if (c.stage == Q2Slot::active && s.native == nullptr) {  // what changed, to the rules
        std::vector<std::pair<std::string, std::string>> pairs;
        ge::classic::protocol::info_pairs(c.userinfo, pairs);
        for (const auto& [k, v] : pairs)
          if (k != "name" && k != "*ge_ext" && info_get(before, k) != v)
            s.rules->user_info(slot, k, v);
      }
      c.rate = rate_of(s, c.userinfo, c.address);
      if (c.stage == Q2Slot::active && info_get(c.userinfo, "rate") != was) rate_check(s, slot);
      std::string name = info_get(c.userinfo, "name");
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
    } else if (id == kClcStringCmd) {
      const std::string line = read_string(r);
      if (++commands <= kStringCmdsPerPacket) string_command(s, slot, line);
    } else if (id == kClcGeVoice) {
      int gen = 0, seq = 0;
      std::vector<uint8_t> frames;
      if (!read_voice_up(r, gen, seq, frames)) return drop(s, slot, "a voice message cut short");
      relay_voice(s, slot, gen, seq, frames);
    } else {
      return drop(s, slot, "a message " + std::to_string(id) + " it does not know");
    }
    if (r.bad) return drop(s, slot, "a message cut short");
  }
}

// The slot a packet is from: a held one first, then a zombie (whose packets are read for their
// acknowledgements alone), so a client that connects again from the same port is its new slot.
int find_client(const Q2NetServer& s, const gn::Address& from, uint16_t qport) {
  int zombie = -1;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const Q2NetClient& c = s.clients[i];
    if (c.stage == Q2Slot::free || c.qport != qport || !same_host(c.address, from)) continue;
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
    for (int16_t v : m.origin) i16(v);
    for (int16_t v : m.velocity) i16(v);
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
      sight = &s.sight;
    }
  }
  std::vector<uint8_t> block, rel;
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2NetClient& c = s.clients[i];
    if (c.stage == Q2Slot::free) continue;
    if (c.stage == Q2Slot::zombie) {  // the disconnect resent until the client has it
      if (channel_reliable_backlog(c.ch) > 0 && s.now - c.sent >= 0.1) transmit(s, c, nullptr, 0);
      continue;
    }
    if (c.ch.overflow) {
      drop(s, i, "overflowed");
      continue;
    }
    if (c.stage == Q2Slot::signon) {
      if (channel_reliable_backlog(c.ch) > 0 || s.now - c.sent >= 1.0) transmit(s, c, nullptr, 0);
      continue;
    }
    // A stock client is sent the tenths; one that agreed every frame, each rules frame.
    const bool fresh = c.told_frames ? fresh_fast : fresh_tenth;
    const int64_t frame = c.told_frames ? fast : tenth;
    const int slots = rate_slots(s, c);
    if (!fresh) {
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
  const int16_t* v = s.game->players[target].move.velocity;
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
    uint16_t qport = 0;
    if (!peek_qport(buf, size, qport)) continue;
    const int slot = find_client(s, from, qport);
    if (slot < 0) continue;
    Q2NetClient& c = s.clients[slot];
    c.address = from;  // a router may have moved its port
    size_t at = 0;
    if (!channel_process(c.ch, buf, size, s.now, at)) continue;
    c.heard = s.now;
    c.heard_once = true;
    if (q2_held(c)) read_messages(s, slot, buf + at, size - at, packets);
  }
  for (int i = 0; i < sh::kMaxClients; i++) {
    Q2NetClient& c = s.clients[i];
    if (c.stage == Q2Slot::signon && !c.heard_once && s.cfg.ghost_time > 0 &&
        s.now - c.heard > s.cfg.ghost_time) {  // sv_ghosttime: a slot taken and never used
      note(s, c.name + " [" + gn::to_string(c.address) + "] never came: its slot is free again");
      c = Q2NetClient{};
      continue;
    }
    if (q2_held(c) && s.now - c.heard > s.cfg.timeout) drop(s, i, "timed out");
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
  s.rules->frame(packets.data(), static_cast<int>(packets.size()));
  if (s.game != nullptr && s.cfg.antilag)  // where each player stands now, for the rewinds
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (s.clients[i].stage != Q2Slot::active) {
        antilag_clear(s.antilag, i);
        continue;
      }
      const sv::Q2Player& p = s.game->players[i];
      const int16_t* o = p.move.origin;
      antilag_record(s.antilag, i, s.now,
                     {float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f}, p.mins,
                     p.maxs);
    }
  send_frames(s);
  for (Q2NetClient& c : s.clients) {  // the voices relayed this tick: a frame late is lost
    if (c.voice_out.empty()) continue;
    if (c.stage == Q2Slot::active) transmit(s, c, c.voice_out.data(), c.voice_out.size());
    c.voice_out.clear();
  }
  s.ticks++;
}

void q2_net_level(Q2NetServer& s, const sv::Q2WireOptions& wire) {
  const int32_t count = s.cfg.wire.server_count + 1;  // the server data carries it too
  s.cfg.wire = wire;
  s.cfg.wire.server_count = count;
  s.count = 0x10000 + count;
  s.written_frame = frame_of(s);
  s.written_fast = fast_of(s);
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
