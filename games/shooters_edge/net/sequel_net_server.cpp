// The sequel's net server (sequel_net_server.hpp): out of band, the signon by the client's
// commands, the moves into the rules, the frames out.
//
// ge:summary The sequel's net server's steps: challenges, connects, the signon, moves and frames
// ge:keywords sequel-server sequel_net_frame signon configstrings baselines begin move frame
// ge:keywords rate suppress-count rate-hold sv_maxrate sv_minrate
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_net_server.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "server/admission.hpp"
#include "server/rate.hpp"
#include "shooters_edge/net/classic_ext.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/sequel_admin.hpp"
#include "shooters_edge/server/sequel_fight.hpp"

namespace shooters_edge::net {

namespace {

namespace pg = ge::progs;

constexpr size_t kSignonShare = 700;  // bytes of the signon one answer adds before its prompt
constexpr int kStringCmdsPerPacket = 7;
constexpr uint8_t kSvcDisconnect = 7, kSvcPrint = 10, kSvcStuffText = 11, kSvcDownload = 16;
constexpr uint8_t kSvcGeOrigin = 123;      // the game's own: a download's archive (classic_svc.hpp)
constexpr uint8_t kSvcGeMoves = 124;       // the game's own: the other players' moves
constexpr uint8_t kSvcGeShare = 126;       // the game's own: the easy jump's share
constexpr uint8_t kSvcGeAccountKey = 127;  // the game's own: the logins' key tag
constexpr uint8_t kSvcGeFeel = 122;        // the game's own: the move's feel
constexpr uint8_t kSvcGeVoice = 121;       // the game's own: a voice (classic_voice_route.hpp)
constexpr size_t kVoiceTickBytes = 1200;   // the voices one client is sent a tick, at most
constexpr size_t kMovesBlockMost = 1200;   // the frame with the moves, at most (under a datagram)
constexpr size_t kLogKept = 512;
// The sequel's reading of a client's `rate`: none 5000, and 100..15000 (0 the least).
constexpr ge::server::RateBounds kRateBounds{.none = 5000, .zero = 100, .low = 100, .high = 15000};
constexpr int kRateFrames = 10;  // the frames whose sizes are summed (a second at 10 Hz)

// The move's feel as svc 122 carries it: a count of 32-bit floats (little-endian) and the floats
// (the top speed, the friction, the air acceleration, the swimmer's share, the water's friction,
// the water jump's speed), then a byte of flags (1 the landing pause, 2 the step climbed in the
// air, 4 no surfacing lift). A knob added later goes after the floats so far (and the count grows)
// or in a spare flag: a client reads the floats it knows and passes over the rest.
std::vector<uint8_t> sequel_feel_message(const ge::classic::SequelFeel& f) {
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

int rate_of(const SequelNetServer& s, const std::string& info) {
  return ge::server::rate_in_force(info_get(info, "rate"), {s.cfg.min_rate, s.cfg.max_rate},
                                   kRateBounds);
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

// A client whose `rate` is past sv_maxrate or under sv_minrate told so and set back.
void rate_check(SequelNetServer& s, int slot) {
  SequelNetClient& c = s.clients[slot];
  const ge::server::RateVerdict v =
      ge::server::rate_verdict(info_get(c.userinfo, "rate"), {s.cfg.min_rate, s.cfg.max_rate});
  if (!v.out) return;
  sequel_net_print(s, slot, v.said);
  stuff(c, "rate " + std::to_string(v.rate) + "\n");
}

// THE DOWNLOADS (SequelNetConfig): the next piece of the file being sent, up to 1024 bytes: size,
// percent, the bytes; percent 100 ends it.
void download_piece(SequelNetClient& c) {
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

void start_download(SequelNetServer& s, SequelNetClient& c, std::string path, int offset) {
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

void transmit(SequelNetServer& s, SequelNetClient& c, const uint8_t* data, size_t size) {
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(c.ch, data, size, s.now, out);
  send_to(s, c.address, out, n);
  c.sent = s.now;
}

void drop(SequelNetServer& s, int slot, const std::string& why) {
  SequelNetClient& c = s.clients[slot];
  if (!sequel_held(c)) return;
  note(s, c.name + " dropped: " + why);
  // A client signing on again after a level change still holds its slot in the rules.
  if ((**s.rules).clients[slot].active) s.rules->drop(slot);
  // The disconnect, once, with whatever reliable data is still owed.
  const uint8_t bye[] = {kSvcDisconnect};
  channel_reliable(c.ch, bye, sizeof bye);
  transmit(s, c, nullptr, 0);
  // A zombie until `zombie_time` after its last packet: the disconnect resent, nothing read.
  c.stage = SequelSlot::zombie;
  c.downloading = false;
  c.download.clear();
}

// A password setting that asks for one: neither empty nor `none`.
bool set(const std::string& password) { return !password.empty() && password != "none"; }

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
  info_set(info, "maxspectators", std::to_string(s.cfg.max_spectators));
  // `needpass`: 1 a player's password asked, 2 a spectator's.
  info_set(info, "needpass",
           std::to_string((set(s.cfg.password) ? 1 : 0) | (set(s.cfg.spectator_password) ? 2 : 0)));
  info_set(info, "deathmatch", "1");
  info_set(info, "protocol", std::to_string(kSequelNetProtocol));
  std::string t = info + "\n";
  for (int i = 0; i < sequel_net_slots(s); i++) {
    const SequelNetClient& c = s.clients[i];
    if (!sequel_held(c)) continue;
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
  if (ge::server::ban_refuses(s.bans, s.cfg.ban_mode, from,
                              s.cfg.unix_time ? s.cfg.unix_time() : 0))
    return refuse(s, from, "Banned.");
  const std::string& userinfo = w[4];
  for (const SequelNetClient& c : s.clients)
    if (sequel_held(c) && same_host(c.address, from) &&
        (c.qport == qport || c.address.port == from.port))
      return;                // already in: a repeat
  if (s.cfg.ip_limit > 0) {  // one address's slots held to sv_iplimit, a zombie's a half
    std::vector<ge::server::HeldSeat> held;
    for (const SequelNetClient& o : s.clients)
      if (o.stage != SequelSlot::free) held.push_back({o.address, o.stage == SequelSlot::zombie});
    const std::string refusal = ge::server::admit_refusal(s.cfg.ip_limit, from, held);
    if (!refusal.empty()) return refuse(s, from, refusal + "\nConnection refused.");
  }
  const std::string asked = info_get(userinfo, "spectator");
  const bool spectator = !asked.empty() && asked != "0";
  if (spectator && set(s.cfg.spectator_password) && asked != s.cfg.spectator_password)
    return refuse(s, from, "Spectator password required or incorrect.\nConnection refused.");
  const std::string given = info_get(userinfo, "password");
  bool vip = false;
  for (const std::string& word : s.cfg.vip_passwords) vip = vip || (set(word) && given == word);
  if (!vip && !spectator && set(s.cfg.password) && given != s.cfg.password)
    return refuse(s, from, "Password required or incorrect.\nConnection refused.");
  vip = vip || (set(s.cfg.password) && given == s.cfg.password);
  int players = 0, watching = 0, slot = -1;
  for (int i = 0; i < sequel_net_slots(s); i++) {
    const SequelNetClient& o = s.clients[i];
    if (o.stage == SequelSlot::free) {
      if (slot < 0) slot = i;
    } else {
      (o.spectator ? watching : players)++;
    }
  }
  // Spectators have their own slots when `max_spectators` gives them some, else take players'.
  const bool own = s.cfg.max_spectators > 0;
  if (spectator && own && watching >= s.cfg.max_spectators)
    return refuse(s, from, "Server spectator limit is full.\nConnection refused.");
  const int taken = players + (own ? 0 : watching);
  if ((!spectator || !own) && taken >= s.cfg.max_clients) slot = -1;
  if (slot < 0) return refuse(s, from, "Server is full.");
  if ((!spectator || !own) &&
      ge::server::vip_slots_refuse(s.cfg.max_clients, taken, s.cfg.vip_slots, vip))
    return refuse(s, from, "Server and reserved slots are full.");
  SequelNetClient& c = s.clients[slot];
  c = SequelNetClient{};
  c.stage = SequelSlot::signon;
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
  c.told_voice = s.cfg.voice && (ext & kGeVoice) != 0;
  c.userid = s.next_userid++;
  c.spectator = spectator;
  c.rate = rate_of(s, userinfo);
  if (s.name_rule != nullptr) s.name_rule(s.hook_ctx, slot, c.name, true);
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
  } else if (w[0] == "rcon") {
    sequel_rcon(s, from, text);
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
  if (s.native != nullptr)
    sv::sequel_signon_raw(c.wire, s.native->raw, slot, opt, c.signon);
  else
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

// Whether the players in `a` and `b` are on one team: the rules' own teams (the sequel's by skin
// or model), else the same `team` key.
bool same_team(const SequelNetServer& s, int a, int b) {
  if (s.game != nullptr) return sv::fight_teammates(*s.game, a, b);
  return info_get(s.clients[a].userinfo, "team") == info_get(s.clients[b].userinfo, "team");
}

bool hears(const SequelNetServer& s, int talker, int listener, VoiceTo to, int one) {
  TalkPair p;
  p.talker_spectator = s.clients[talker].spectator;
  p.listener_spectator = s.clients[listener].spectator;
  p.same_team = same_team(s, talker, listener);
  return talk_route(s.rules != nullptr ? &**s.rules : nullptr, s.cfg.earshot, talker, listener, to,
                    one, p);
}

// A client's voice to those who hear it (THE VOICE, sequel_net_server.hpp), sent on the next tick.
void relay_voice(SequelNetServer& s, int slot, int gen, int seq,
                 const std::vector<uint8_t>& frames) {
  SequelNetClient& c = s.clients[slot];
  if (!c.told_voice || c.stage != SequelSlot::active || frames.empty()) return;
  c.voice_in++;
  for (int j = 0; j < sh::kMaxClients; j++) {
    SequelNetClient& d = s.clients[j];
    if (d.stage != SequelSlot::active || !d.told_voice || talk_mutes(d.talk, slot)) continue;
    if (j == slot ? !s.cfg.voice_echo : !hears(s, slot, j, c.talk.to, c.talk.one)) continue;
    if (d.voice_out.size() + frames.size() + 6 > kVoiceTickBytes) continue;  // a tick's room
    d.voice_out.push_back(kSvcGeVoice);
    write_voice_down(d.voice_out, slot, gen, seq, frames);
    d.voice_heard++;
  }
}

// A client's talk word (voicetarg, unmuteall, muteall, vignore, say_near): true when it was one.
bool talk_command(SequelNetServer& s, int slot, const std::string& line) {
  SequelNetClient& c = s.clients[slot];
  pg::Words w;
  pg::tokenize_words(line.c_str(), w);
  if (talk_word(c.talk, w, sh::kMaxClients)) return true;
  if (std::string(pg::word(w, 0)) != "say_near") return false;
  const std::string text = talk_said(w);
  if (text.empty()) return true;
  if (s.cfg.earshot <= 0) {
    sequel_net_print(s, slot, "This server has no nearby chat.\n");
    return true;
  }
  const std::string said = talk_near_line(c.name, text);
  for (int j = 0; j < sh::kMaxClients; j++)
    if (s.clients[j].stage == SequelSlot::active && hears(s, slot, j, VoiceTo::near, -1)) {
      std::vector<uint8_t> b = {kSvcPrint, 3};  // the chat level
      b.insert(b.end(), said.begin(), said.end());
      b.push_back(0);
      reliable(s.clients[j], b);
    }
  return true;
}

void string_command(SequelNetServer& s, int slot, const std::string& line) {
  SequelNetClient& c = s.clients[slot];
  const std::vector<std::string> w = sequel_words(line);
  if (w.empty()) return;
  const std::string& cmd = w[0];
  const bool signon = cmd == "configstrings" || cmd == "baselines" || cmd == "begin";
  const bool protocol =
      signon || cmd == "new" || cmd == "disconnect" || cmd == "download" || cmd == "nextdl";
  if (!protocol && s.line_hook != nullptr && s.line_hook(s.hook_ctx, slot, line)) return;
  if (!protocol && c.stage == SequelSlot::active && talk_command(s, slot, line)) return;
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
    c.stage = SequelSlot::active;
    ge::server::idle_start(c.idle, s.now);
    c.acked = -1;
    sv::sequel_resync(c.wire);
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
  } else if (c.stage == SequelSlot::active) {
    if (cmd == "say" || cmd == "say_team") ge::server::idle_chat(c.idle, s.now);
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
  while (gn::remaining(r) > 0 && sequel_held(c)) {
    const int id = read_byte(r);
    if (id == kClcNop) continue;
    if (id == kClcMove) {
      if (moved) return;  // one move a packet: the rest is ignored
      moved = true;
      SequelMove m;
      if (!read_sequel_move(r, c.ch.incoming_sequence, m)) return drop(s, slot, "a cut move");
      if (c.stage != SequelSlot::active) continue;
      c.acked = m.frame;
      if (m.frame > 0 && c.sent_frame[m.frame & 63] == m.frame)
        c.ping_s = s.now - c.sent_at[m.frame & 63];
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
      c.last_cmd = ge::classic::sequel_command(p.cmds[2]);  // as the rules run it
      c.last_cmd_at = s.now;
      const int msec[3] = {m.cmds[0].msec, m.cmds[1].msec, m.cmds[2].msec};
      ge::server::skew_packet(c.skew, s.now, c.ch.incoming_sequence, msec);
      ge::server::idle_move(c.idle, s.now, c.last_cmd.buttons, c.last_cmd.forward, c.last_cmd.side,
                            c.last_cmd.up);
    } else if (id == kClcUserInfo) {
      const std::string was = info_get(c.userinfo, "rate");
      const std::string before = c.userinfo;
      c.userinfo = read_string(r);
      if (c.stage == SequelSlot::active && s.native == nullptr) {  // what changed, to the rules
        std::vector<std::pair<std::string, std::string>> pairs;
        ge::classic::protocol::info_pairs(c.userinfo, pairs);
        for (const auto& [k, v] : pairs)
          if (k != "name" && k != "*ge_ext" && info_get(before, k) != v)
            s.rules->user_info(slot, k, v);
      }
      c.rate = rate_of(s, c.userinfo);
      if (c.stage == SequelSlot::active && info_get(c.userinfo, "rate") != was) rate_check(s, slot);
      std::string name = info_get(c.userinfo, "name");
      // A player in the game renamed past sv_namelimit keeps the old name (server/names.hpp).
      const bool renamed = !name.empty() && name != c.name && c.stage == SequelSlot::active;
      if (renamed && !ge::server::rename_allowed(c.renames, s.now, s.cfg.names)) {
        info_set(c.userinfo, "name", c.name);
        sequel_net_print(s, slot, ge::server::kRenameRefusal);
        name = c.name;
      }
      if (!name.empty() && s.name_rule != nullptr)
        s.name_rule(s.hook_ctx, slot, name, c.stage != SequelSlot::active);
      if (!name.empty() && name != c.name) {
        const std::string old = c.name;
        c.name = name;
        if (c.stage == SequelSlot::active) {
          s.rules->user_info(slot, "name", name);
          std::string line = old;
          line += " [" + gn::to_string(c.address) + "] renamed ";
          note(s, line + name);
          if (ge::server::rename_told(s.cfg.names, !c.spectator))
            for (int k = 0; k < sh::kMaxClients; k++)
              if (s.clients[k].stage == SequelSlot::active)
                sequel_net_print(s, k, ge::server::rename_notice(old, name));
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
int find_client(const SequelNetServer& s, const gn::Address& from, uint16_t qport) {
  int zombie = -1;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const SequelNetClient& c = s.clients[i];
    if (c.stage == SequelSlot::free || c.qport != qport || !same_host(c.address, from)) continue;
    if (sequel_held(c)) return i;
    if (zombie < 0) zombie = i;
  }
  return zombie;
}

int64_t frame_of(const SequelNetServer& s) {
  return s.native != nullptr ? s.native->frame_number : s.game->frame_number;
}

// The other players' moves for client `i` (svc 124, sequel_net_server.hpp) onto its frame.
void write_moves(const SequelNetServer& s, int i, std::vector<uint8_t>& b) {
  const SequelNetClient& c = s.clients[i];
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
    const SequelNetClient& o = s.clients[j];
    if (j == i || o.stage != SequelSlot::active || o.last_cmd_at < 0) continue;
    const auto e = static_cast<size_t>(j) + 1;
    if (e >= c.wire.last.size() || !c.wire.last[e].present) continue;  // not in this frame
    const ge::classic::SequelState& m = s.game->players[j].move;
    if (m.type != ge::classic::SequelType::normal) continue;
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
    const ge::classic::SequelCmd& k = o.last_cmd;
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
int sent_lately(const SequelNetClient& c) {
  int sum = 0;
  for (int b : c.frame_sizes) sum += b;
  return sum;
}

bool moved_since(const SequelNetServer& s, int i, double since) {
  for (int j = 0; j < sh::kMaxClients; j++)
    if (j != i && s.clients[j].stage == SequelSlot::active && s.clients[j].last_cmd_at > since)
      return true;
  return false;
}

void send_frames(SequelNetServer& s) {
  const int64_t frame = frame_of(s);
  const bool fresh = frame != s.written_frame;
  s.written_frame = frame;
  // The cut's links, once a frame: a hosted library's are taken with its view.
  const sv::SequelSight* sight = nullptr;
  if (fresh && s.cfg.cut_to_view) {
    if (s.native != nullptr) {
      sight = &s.native->raw.sight;
    } else {
      sv::sequel_sight_reset(s.sight, s.game->map.get());
      sv::sequel_sight_link_rules(s.sight, **s.rules);
      sight = &s.sight;
    }
  }
  std::vector<uint8_t> block, rel;
  for (int i = 0; i < sh::kMaxClients; i++) {
    SequelNetClient& c = s.clients[i];
    if (c.stage == SequelSlot::free) continue;
    if (c.stage == SequelSlot::zombie) {  // the disconnect resent until the client has it
      if (channel_reliable_backlog(c.ch) > 0 && s.now - c.sent >= 0.1) transmit(s, c, nullptr, 0);
      continue;
    }
    if (c.ch.overflow) {
      drop(s, i, "overflowed");
      continue;
    }
    if (c.stage == SequelSlot::signon) {
      if (channel_reliable_backlog(c.ch) > 0 || s.now - c.sent >= 1.0) transmit(s, c, nullptr, 0);
      continue;
    }
    if (!fresh) {
      if (s.cfg.moves_between && c.told_moves && s.native == nullptr && (s.ticks & 1) == 0 &&
          moved_since(s, i, c.moves_sent) && sent_lately(c) <= c.rate) {
        block.clear();
        write_moves(s, i, block);
        if (block[1] == 0) continue;
        c.frame_sizes[frame % kRateFrames] += static_cast<int>(block.size());  // the frame's second
        transmit(s, c, block.data(), block.size());
        c.moves_sent = s.now;
      }
      continue;
    }
    // THE RATE: past it over the last second, this frame is held back.
    int& size = c.frame_sizes[frame % kRateFrames];
    if (sent_lately(c) > c.rate) {
      size = 0;
      if (c.wire.suppress < 255) c.wire.suppress++;
      c.held++;
      continue;
    }
    if (c.acked != c.wire.last_frame || c.acked <= 0) {
      sv::sequel_resync(c.wire);
      c.full_frames++;
    }
    rel.clear();
    c.wire.sight = sight;
    if (s.native != nullptr)
      sv::sequel_frame_raw(c.wire, s.native->raw, static_cast<int32_t>(frame), block, rel);
    else
      sv::sequel_frame(c.wire, **s.rules, static_cast<int32_t>(frame), &s.game->players[i].move,
                       block, &rel);
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
    if (c.told_tag && !c.tag_sent && !s.account_tag.empty()) {  // the logins' key, once known
      rel.push_back(kSvcGeAccountKey);
      rel.insert(rel.end(), s.account_tag.begin(), s.account_tag.end());
      rel.push_back(0);
      c.tag_sent = true;
    }
    if (c.told_feel && s.game != nullptr) {  // the move's feel, when it changed
      const std::vector<uint8_t> feel = sequel_feel_message(sv::sequel_move_feel(*s.game));
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

// Where `target`'s player was as `shooter`'s screen showed him (our rules' `lag_place`).
bool lag_place(void* ctx, int shooter, int target, sh::Vec3f& at) {
  const SequelNetServer& s = *static_cast<const SequelNetServer*>(ctx);
  if (shooter < 0 || shooter >= sh::kMaxClients || target < 0 || target >= sh::kMaxClients ||
      shooter == target)
    return false;
  const SequelNetClient& sc = s.clients[shooter];
  const SequelNetClient& tc = s.clients[target];
  // A bot aims at where players are; a bot is never moved.
  if (sc.stage != SequelSlot::active || tc.stage != SequelSlot::active || tc.spectator)
    return false;
  const bool known = sc.acked > 0 && sc.sent_frame[sc.acked & 63] == sc.acked;
  const double acked = known ? sc.sent_at[sc.acked & 63] : s.now;
  const double when = antilag_target(acked, sc.ping_s, 77, s.now, s.cfg.antilag_exact);
  const int16_t* v = s.game->players[target].move.velocity;
  const sh::Vec3f velocity{float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
  return antilag_position(s.antilag.players[target], when, velocity, at);
}

}  // namespace

int sequel_net_slots(const SequelNetServer& s) {
  return std::min(sh::kMaxClients, s.cfg.max_clients + std::max(0, s.cfg.max_spectators));
}

int sequel_net_clients(const SequelNetServer& s) {
  int n = 0;
  for (const SequelNetClient& c : s.clients) n += sequel_held(c) ? 1 : 0;
  return n;
}

bool sequel_net_start(SequelNetServer& s, const sv::RulesRunner& rules, gn::Transport& t,
                      const SequelNetConfig& cfg, std::string& error) {
  s.cfg = cfg;
  s.rules = &rules;
  s.game = sv::sequel_of(rules);
  s.native = sv::native_sequel_of(rules);
  s.transport = &t;
  if (s.game == nullptr && s.native == nullptr) {
    error = "the runner is not the sequel's: the sequel's net server serves only its rules";
    return false;
  }
  s.count = 0x10000 + cfg.wire.server_count;
  s.written_frame = frame_of(s);
  if (s.game != nullptr && cfg.antilag) {
    s.game->lag_place = &lag_place;
    s.game->lag_ctx = &s;
  }
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
    if (sequel_held(c)) read_messages(s, slot, buf + at, size - at, packets);
  }
  for (int i = 0; i < sh::kMaxClients; i++) {
    SequelNetClient& c = s.clients[i];
    if (sequel_held(c) && s.now - c.heard > s.cfg.timeout) drop(s, i, "timed out");
    double ratio = 0;
    const ge::server::Skew skew = c.stage == SequelSlot::active
                                      ? ge::server::skew_check(c.skew, s.now, s.cfg.skew, ratio)
                                      : ge::server::Skew::fine;
    if (skew != ge::server::Skew::fine)
      note(s, c.name + " [" + gn::to_string(c.address) + "]: time skew " +
                  ge::core::num::to_fixed(ratio, 3));
    if (skew == ge::server::Skew::kick) {
      for (int k = 0; k < sh::kMaxClients; k++)
        if (k != i && s.clients[k].stage == SequelSlot::active)
          sequel_net_print(s, k, c.name + " was dropped for a fast clock.\n");
      drop(s, i, "time skew too high");
    } else if (c.stage == SequelSlot::active &&
               ge::server::idle_over(c.idle, s.now, s.cfg.idle_time)) {
      for (int k = 0; k < sh::kMaxClients; k++)
        if (k != i && s.clients[k].stage == SequelSlot::active)
          sequel_net_print(s, k, c.name + " was dropped for idling.\n");
      sequel_net_print(s, i, "You were dropped for idling.\n");
      drop(s, i, "idling");
    }
    if (c.stage == SequelSlot::zombie && s.now - c.heard > s.cfg.zombie_time) c = SequelNetClient{};
  }
  s.rules->frame(packets.data(), static_cast<int>(packets.size()));
  if (s.game != nullptr && s.cfg.antilag)  // where each player stands now, for the rewinds
    for (int i = 0; i < sh::kMaxClients; i++) {
      if (s.clients[i].stage != SequelSlot::active) {
        antilag_clear(s.antilag, i);
        continue;
      }
      const int16_t* o = s.game->players[i].move.origin;
      antilag_record(s.antilag, i, s.now,
                     {float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f});
    }
  send_frames(s);
  for (SequelNetClient& c : s.clients) {  // the voices relayed this tick: a frame late is lost
    if (c.voice_out.empty()) continue;
    if (c.stage == SequelSlot::active) transmit(s, c, c.voice_out.data(), c.voice_out.size());
    c.voice_out.clear();
  }
  s.ticks++;
}

void sequel_net_level(SequelNetServer& s, const sv::SequelWireOptions& wire) {
  const int32_t count = s.cfg.wire.server_count + 1;  // the server data carries it too
  s.cfg.wire = wire;
  s.cfg.wire.server_count = count;
  s.count = 0x10000 + count;
  s.written_frame = frame_of(s);
  for (SequelNetClient& c : s.clients) {
    if (!sequel_held(c)) continue;
    c.stage = SequelSlot::signon;  // the rules keep the slot until `begin` seats it again
    c.acked = -1;
    stuff(c, "changing\n");
    stuff(c, "reconnect\n");
  }
  note(s, "the level changed to " + (**s.rules).map_name);
}

void sequel_net_print(SequelNetServer& s, int slot, const std::string& text) {
  if (slot < 0 || slot >= sh::kMaxClients || !sequel_held(s.clients[slot])) return;
  std::vector<uint8_t> b = {kSvcPrint, 2};
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
  reliable(s.clients[slot], b);
}

void sequel_net_rename(SequelNetServer& s, int slot, const std::string& name) {
  if (slot < 0 || slot >= sh::kMaxClients) return;
  SequelNetClient& c = s.clients[slot];
  if (!sequel_held(c) || c.name == name) return;
  c.name = name;
  if (c.stage == SequelSlot::active) s.rules->user_info(slot, "name", name);
}

void sequel_net_drop(SequelNetServer& s, int slot, const std::string& why) {
  if (slot < 0 || slot >= sh::kMaxClients) return;
  drop(s, slot, why);
}

void sequel_net_stop(SequelNetServer& s) {
  for (int i = 0; i < sh::kMaxClients; i++) {
    drop(s, i, "the server stopped");
    s.clients[i] = SequelNetClient{};  // no zombies past the end
  }
}

void sequel_net_oob(SequelNetServer& s, const gn::Address& to, const std::string& text) {
  send_oob(s, to, text);
}

}  // namespace shooters_edge::net
