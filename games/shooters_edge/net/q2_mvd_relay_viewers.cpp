// Quake II's multiview relay, the viewers' half (q2_mvd_relay.hpp): the port's datagrams routed to
// the faces, the Waiting Room and its Channel Chooser, each viewer's view of a channel, and the
// viewers' words (chase, autofollow, the menu, the lists, chat and its flood rule, admin, fwd).
//
// ge:summary Quake II relay's viewers: the Waiting Room, views per viewer, the viewers' words
// ge:keywords waiting-room channel-chooser follow chase autofollow observe inven invnext invuse
// ge:keywords mvdadmin fwd say flood_msgs mvd_part_filter set-uf layout relay-chat
// ge:skill net shooter replay
// ge:test tests/shooters_edge_q2_mvd_relay_test.cpp
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "shooters_edge/client/demo_mvd2.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/q2_mvd_relay.hpp"
#include "shooters_edge/net/q2_mvd_server.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/server/q2_mvd2.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;
namespace sv = shooters_edge::server;

namespace {

constexpr uint8_t kSvcLayout = 4, kSvcPrint = 10, kSvcStuffText = 11, kSvcServerData = 12,
                  kSvcConfigString = 13, kSvcFrame = 20, kSvcPacketEntities = 18;
constexpr int kPrintLow = 0, kPrintMedium = 1, kPrintHigh = 2;
constexpr int kStatLayouts = 13, kStatFrags = 14;
constexpr int kCsPlayerSkins = 1312;
constexpr uint8_t kPmFreeze = 4, kPmfNoPrediction = 64;
constexpr uint32_t kEfQuad = 0x8000, kEfPent = 0x10000, kEfFlag1 = 0x40000, kEfFlag2 = 0x80000;
constexpr int kUfOwnFov = 4, kUfMutePlayers = 8, kUfMuteObservers = 16, kUfMuteMisc = 32,
              kUfPlayerFov = 64;

uint64_t key_of(gn::Address a, uint16_t qport) {
  a.port = 0;
  return gn::address_hash(a) * 65537u + qport;
}

std::string hi(std::string s) {
  for (char& ch : s) ch = static_cast<char>(static_cast<uint8_t>(ch) | 0x80);
  return s;
}

void print(Q2Viewer& q, int level, const std::string& text) {
  std::vector<uint8_t> m = {kSvcPrint, static_cast<uint8_t>(level)};
  m.insert(m.end(), text.begin(), text.end());
  m.push_back(0);
  q2_tv_send(q, m);
}

void mvd(Q2Viewer& q, const std::string& text, int level = kPrintHigh) {
  print(q, level, "[MVD] " + text + "\n");
}

void stuff(Q2Viewer& q, const std::string& text) {
  std::vector<uint8_t> m = {kSvcStuffText};
  m.insert(m.end(), text.begin(), text.end());
  m.push_back(0);
  q2_tv_send(q, m);
}

void send_layout(Q2Viewer& q, const std::string& text) {
  std::vector<uint8_t> m = {kSvcLayout};
  m.insert(m.end(), text.begin(), text.end());
  m.push_back(0);
  q2_tv_send(q, m);
}

// --- the faces ---

bool pipe_send(void* ctx, const gn::Address& to, const uint8_t* data, size_t size) {
  auto* v = static_cast<Q2RelayView*>(ctx);
  return gn::transport_send(*v->relay->port, to, data, size,
                            static_cast<int64_t>(v->relay->now * 1e6));
}

int pipe_receive(void* ctx, uint8_t* buffer, size_t capacity, gn::Address& from) {
  auto* v = static_cast<Q2RelayView*>(ctx);
  if (v->inbox.empty()) return -1;
  auto [addr, bytes] = std::move(v->inbox.front());
  v->inbox.pop_front();
  from = addr;
  const size_t n = std::min(capacity, bytes.size());
  std::memcpy(buffer, bytes.data(), n);
  return static_cast<int>(n);
}

void command(Q2Relay& r, Q2RelayView& view, Q2Viewer& q, const std::string& line);
void begun(Q2Relay& r, Q2RelayView& view, Q2Viewer& q);

std::unique_ptr<Q2RelayView> make_view(Q2Relay& r, int max_viewers) {
  auto v = std::make_unique<Q2RelayView>();
  v->relay = &r;
  gn::DatagramPipe p;
  p.send = pipe_send;
  p.receive = pipe_receive;
  p.ctx = v.get();
  gn::open_pipe_transport(v->port, p);
  Q2TvConfig fc;
  fc.name = r.cfg.hostname;
  fc.max_viewers = max_viewers;
  fc.password = r.cfg.password;
  fc.timeout = r.cfg.viewer_timeout;
  fc.attract_loop = 0;
  q2_tv_start(v->face, v->port, fc);
  Q2RelayView* raw = v.get();
  v->face.on_command = [&r, raw](Q2Viewer& q, const std::string& line) {
    command(r, *raw, q, line);
  };
  v->face.on_begin = [&r, raw](Q2Viewer& q) { begun(r, *raw, q); };
  return v;
}

// The blocks a conversion gave (each its length, then its bytes) to the face.
void to_face(Q2RelayView& v, const std::vector<uint8_t>& out) {
  size_t at = 0;
  while (at + 4 <= out.size()) {
    int32_t len = 0;
    std::memcpy(&len, out.data() + at, 4);
    at += 4;
    if (len < 0 || at + size_t(len) > out.size()) break;
    q2_tv_block(v.face, std::span<const uint8_t>(out.data() + at, size_t(len)));
    at += size_t(len);
  }
}

Q2RelayViewer* record_of(Q2Relay& r, const Q2Viewer& q) {
  const auto it = r.viewers.find(key_of(q.address, q.qport));
  return it == r.viewers.end() ? nullptr : &it->second;
}

Q2TvFace& face_of(Q2Relay& r, Q2RelayViewer& rec) {
  return rec.view ? rec.view->face : r.room->face;
}

Q2Viewer* viewer_of(Q2Relay& r, Q2RelayViewer& rec) {
  return q2_tv_find(face_of(r, rec), rec.addr, rec.qport);
}

// The level the channel's reader stands at (its players and their names).
bool level_of(const Q2Channel& c, sv::Mvd2Level& level) {
  return client::mvd2_reader_level(c.base, level);
}

std::string player_name(const sv::Mvd2Level& level, int slot) {
  const size_t cs = size_t(kCsPlayerSkins) + size_t(slot);
  if (cs >= level.config_strings.size()) return "";
  std::string s = level.config_strings[cs];
  if (const size_t bs = s.find('\\'); bs != std::string::npos) s.resize(bs);
  return s;
}

bool playing(const sv::Mvd2Level& level, int slot) {
  return slot >= 0 && size_t(slot) < level.base.players.size() &&
         level.base.players[size_t(slot)].in;
}

int players_in(const sv::Mvd2Level& level) {
  int n = 0;
  for (const sv::Mvd2Player& p : level.base.players) n += p.in ? 1 : 0;
  return n;
}

std::vector<const Q2Channel*> listed_channels(const Q2Relay& r) {
  std::vector<const Q2Channel*> out;
  for (const auto& c : r.channels)
    if (c->have_state) out.push_back(c.get());
  return out;
}

int spectators_on(const Q2Relay& r, int chan) {
  int n = 0;
  for (const auto& [key, rec] : r.viewers) n += rec.chan == chan ? 1 : 0;
  return n;
}

// The player most viewers of `chan` follow; -1 when none follows anyone.
int most_followed(const Q2Relay& r, int chan) {
  std::map<int, int> counts;
  for (const auto& [key, rec] : r.viewers)
    if (rec.chan == chan && rec.target >= 0) counts[rec.target]++;
  int best = -1, most = 0;
  for (const auto& [slot, n] : counts)
    if (n > most) {
      best = slot;
      most = n;
    }
  return best;
}

// --- layouts ---

std::string chooser(const Q2Relay& r, const Q2RelayViewer& rec) {
  std::string t =
      "xv 32 yv 8 picn inventory xv 240 yv 172 string2 \"" + r.cfg.version +
      "\" xv 0 yv 32 cstring \"[ Channel Chooser ]\" xv 64 yv 48 string2 \"Name         "
      "Map     S/P\" yv 56 string \"------------ ------- ---\" ";
  const std::vector<const Q2Channel*> list = listed_channels(r);
  if (list.empty()) {
    if (r.active)
      t += "yv 72 string \" No active channels.\" yv 80 string \" Please wait until players\" "
           "yv 88 string \" connect.\"";
    else
      t += "yv 72 string \" Traffic saving mode.\" yv 80 string \" Press any key to wake\" "
           "yv 88 string \" this server up.\"";
    return t;
  }
  int y = 64;
  for (size_t i = 0; i < list.size() && i < 13; i++, y += 8) {
    const Q2Channel& c = *list[i];
    sv::Mvd2Level level;
    const int plr = level_of(c, level) ? players_in(level) : 0;
    char row[96];
    std::snprintf(row, sizeof row, "%-12.12s %-7.7s %d/%d", c.name.c_str(), c.map.c_str(),
                  spectators_on(r, c.id), plr);
    if (static_cast<int>(i) == rec.cursor)
      t += "xv 56 yv " + std::to_string(y) + " string \"\x0d\" ";
    t += "xv 64 yv " + std::to_string(y) + (c.id == rec.chan ? " string2 \"" : " string \"") + row +
         "\" ";
  }
  return t;
}

const char* yes_no(bool b) { return b ? "Yes" : "No"; }

std::string menu(const Q2Relay& r, const Q2RelayViewer& rec) {
  const Q2Channel* c = nullptr;
  for (const auto& ch : r.channels)
    if (ch->id == rec.chan) c = ch.get();
  const int fov = (rec.uf & kUfOwnFov) ? 1 : (rec.uf & kUfPlayerFov) ? 2 : 0;
  static const char* const kFov[] = {"Default", "Yes", "No"};
  const std::vector<std::string> rows = {
      rec.target >= 0 ? "Leave in-eyes mode" : "Enter in-eyes mode",
      "Show scoreboard",
      "Show spectators (" + std::to_string(spectators_on(r, rec.chan)) + ")",
      "Show channels (" + std::to_string(listed_channels(r).size()) + ")",
      "Leave this channel",
      "Ignore spectator chat: " + hi(yes_no(rec.uf & kUfMuteObservers)),
      "Ignore connect msgs:   " + hi(yes_no(rec.uf & kUfMuteMisc)),
      "Ignore player chat:    " + hi(yes_no(rec.uf & kUfMutePlayers)),
      "Ignore player FOV: " + hi(kFov[fov]),
      " (use 'set uf " + std::to_string(rec.uf) + " u' in cfg)",
      "Exit menu",
  };
  std::string t = "xv 32 yv 8 picn inventory xv 0 yv 32 cstring \"[ Main Menu ]\" ";
  int y = 48, item = 0;
  for (size_t i = 0; i < rows.size(); i++, y += 8) {
    const bool hint = i == 9;
    if (!hint && item++ == rec.cursor) t += "xv 48 yv " + std::to_string(y) + " string \"\x0d\" ";
    t += "xv 56 yv " + std::to_string(y) + " string \"" + rows[i] + "\" ";
  }
  if (c != nullptr && c->state == Q2ChanState::wait) t += "xv 0 yv 160 cstring \"[BUFFERING]\" ";
  t += "xv 240 yv 172 string2 \"" + r.cfg.version + "\"";
  return t;
}

std::string clients(Q2Relay& r, const Q2RelayViewer& rec) {
  std::string t = "xv 0 yv 0 string \"    Name            RTT Status\" ";
  const Q2Channel* c = relay::channel(r, rec.chan);
  sv::Mvd2Level level;
  const bool have = c != nullptr && level_of(*c, level);
  int y = 16, n = 0;
  for (const auto& [key, other] : r.viewers) {
    if (other.chan != rec.chan) continue;
    if (n >= rec.cursor * 16 && n < rec.cursor * 16 + 16) {
      std::string status = "observing";
      if (other.target >= 0 && have) status = "-> " + player_name(level, other.target);
      char row[96];
      std::snprintf(row, sizeof row, "%3d %-15.15s %3d %s", n, other.name.c_str(), 0,
                    status.c_str());
      t += "yv " + std::to_string(y) + " string \"" + row + "\" ";
      y += 8;
    }
    n++;
  }
  return t;
}

// The channel's players by their frags, as a scoreboard.
std::string scoreboard(const Q2Channel& c) {
  sv::Mvd2Level level;
  if (!level_of(c, level)) return "xv 0 yv 32 cstring \"<no scoreboard>\"";
  std::vector<std::pair<int, std::string>> rows;
  for (size_t i = 0; i < level.base.players.size(); i++)
    if (level.base.players[i].in)
      rows.emplace_back(level.base.players[i].s.stats[kStatFrags],
                        player_name(level, static_cast<int>(i)));
  if (rows.empty()) return "xv 0 yv 32 cstring \"<no scoreboard>\"";
  std::stable_sort(rows.begin(), rows.end(),
                   [](const auto& a, const auto& b) { return a.first > b.first; });
  std::string t = "xv 32 yv 16 string2 \"Frags Name\" ";
  int y = 32;
  for (const auto& [frags, name] : rows) {
    char row[64];
    std::snprintf(row, sizeof row, "%5d %s", frags, name.c_str());
    t += "yv " + std::to_string(y) + " string \"" + row + "\" ";
    y += 8;
  }
  return t;
}

void draw(Q2Relay& r, Q2RelayViewer& rec) {
  Q2Viewer* q = viewer_of(r, rec);
  if (q == nullptr) return;
  std::string t;
  switch (rec.layout) {
    case Q2Layout::none:
      break;
    case Q2Layout::chooser:
      t = chooser(r, rec);
      break;
    case Q2Layout::menu:
      t = menu(r, rec);
      break;
    case Q2Layout::clients:
      t = clients(r, rec);
      break;
    case Q2Layout::score:
      if (const Q2Channel* c = relay::channel(r, rec.chan)) t = scoreboard(*c);
      break;
    case Q2Layout::oldscore:
      if (const Q2Channel* c = relay::channel(r, rec.chan))
        t = c->oldscore.empty() ? "xv 0 yv 32 cstring \"<no scoreboard>\"" : c->oldscore;
      break;
  }
  if (rec.layout == Q2Layout::none && rec.chan >= 0 && rec.target >= 0) {
    const Q2Channel* c = relay::channel(r, rec.chan);
    sv::Mvd2Level level;
    if (c != nullptr && level_of(*c, level))
      t = r.cfg.chase_prefix + " string \"[" + c->name + "] Chasing " +
          (playing(level, rec.target) ? player_name(level, rec.target) : "<no target>") + "\"";
  }
  send_layout(*q, t);
}

// --- views ---

void touch(const Q2Relay& r, uint64_t key, int chan, uint8_t& type, uint8_t& flags,
           int16_t* stats) {
  type = kPmFreeze;  // the view is the player's: the viewer's client must not move it
  flags |= kPmfNoPrediction;
  const auto it = r.viewers.find(key);
  if (it == r.viewers.end()) return;
  const Q2RelayViewer& rec = it->second;
  if (rec.layout != Q2Layout::none || rec.target >= 0) stats[kStatLayouts] |= 1;
  (void)chan;
}

// A view of `c` for the viewer `key`, following `target` (-1: the view's own choice).
std::unique_ptr<Q2RelayView> view_of(Q2Relay& r, Q2Channel& c, uint64_t key, int target) {
  auto v = make_view(r, 1);
  v->pov = target;
  client::convert_fork(c.base, v->conv);
  v->conv.options.touch_player = [&r, key, id = c.id](int, uint8_t& type, uint8_t& flags,
                                                      int16_t* stats) {
    touch(r, key, id, type, flags, stats);
  };
  client::convert_set_pov(v->conv, target);
  return v;
}

void announce(Q2Relay& r, const Q2RelayViewer& who, int chan, const std::string& text) {
  if (r.cfg.part_filter < 0) return;
  if (r.cfg.part_filter > 0 && !who.admin && r.now - who.talked > r.cfg.part_filter) return;
  for (auto& [key, rec] : r.viewers) {
    if (rec.chan != chan || &rec == &who || (rec.uf & kUfMuteMisc)) continue;
    if (Q2Viewer* q = viewer_of(r, rec)) mvd(*q, text);
  }
}

// The viewer moved: into channel `chan` following `target`, or to the Waiting Room.
void enter(Q2Relay& r, uint64_t key, int chan, int target) {
  const auto it = r.viewers.find(key);
  if (it == r.viewers.end()) return;
  Q2RelayViewer& rec = it->second;
  Q2Viewer* q = viewer_of(r, rec);
  if (q == nullptr) return;
  Q2TvFace& from = face_of(r, rec);
  std::unique_ptr<Q2RelayView> old = std::move(rec.view);
  if (rec.chan != chan) announce(r, rec, rec.chan, rec.name + " left the channel");
  if (chan < 0) {
    if (!q2_tv_move(from, r.room->face, *q)) {
      rec.view = std::move(old);
      return;
    }
    rec.chan = -1;
    rec.target = -1;
    rec.layout = Q2Layout::chooser;
  } else {
    Q2Channel* c = relay::channel(r, chan);
    if (c == nullptr) {
      rec.view = std::move(old);
      return;
    }
    if (rec.chan != chan) {
      rec.marks.clear();
      rec.autochase = false;
      rec.powerup = 0;
      rec.layout = Q2Layout::none;
    }
    rec.chan = chan;
    rec.target = target;
    rec.view = view_of(r, *c, key, target);
    q2_tv_move(from, rec.view->face, *q);
  }
  rec.cursor = 0;
  rec.entered = r.now;
}

// The viewer's view follows `target` now (the same channel): its conversion starts the view
// again, and the face sends the signon.
void retarget(Q2RelayViewer& rec, int target) {
  if (!rec.view || rec.chan < 0) return;
  rec.prev = rec.target;
  rec.target = target;
  rec.view->pov = target;
  client::convert_set_pov(rec.view->conv, target);
}

void ask_move(Q2Relay& r, uint64_t key, int chan, int target, bool same) {
  Q2Relay::Move m;
  m.key = key;
  m.chan = chan;
  m.target = target;
  m.same = same;
  r.moves.push_back(m);
}

void chase(Q2Relay& r, Q2RelayViewer& rec, Q2Viewer& q, int slot, const sv::Mvd2Level& level) {
  ask_move(r, key_of(rec.addr, rec.qport), rec.chan, slot, true);
  mvd(q, "Chasing " + player_name(level, slot) + ".", kPrintLow);
}

// The next or previous player in the game after the one followed.
int step_player(const sv::Mvd2Level& level, int from, int dir, const std::vector<int>* only) {
  const int n = static_cast<int>(level.base.players.size());
  for (int k = 1; k <= n; k++) {
    const int s = ((from < 0 ? (dir > 0 ? -1 : 0) : from) + dir * k + n * 2) % n;
    if (!playing(level, s)) continue;
    if (only != nullptr && std::find(only->begin(), only->end(), s) == only->end()) continue;
    return s;
  }
  return -1;
}

// `follow`'s argument as a player: a slot, a name, a powerup target.
void follow(Q2Relay& r, Q2RelayViewer& rec, Q2Viewer& q, const std::vector<std::string>& w) {
  if (rec.chan < 0) return mvd(q, "Please enter a channel first.");
  Q2Channel* c = relay::channel(r, rec.chan);
  sv::Mvd2Level level;
  if (c == nullptr || !level_of(*c, level)) return;
  if (w.size() < 2) {
    // As `observe`.
    rec.powerup = 0;
    rec.autochase = false;
    if (rec.target >= 0) return ask_move(r, key_of(rec.addr, rec.qport), rec.chan, -1, true);
    const int s = playing(level, rec.prev) ? rec.prev : step_player(level, -1, 1, nullptr);
    if (s < 0) return mvd(q, "No players to chase.", kPrintMedium);
    return chase(r, rec, q, s, level);
  }
  const std::string& a = w[1];
  if (!a.empty() && a.find_first_not_of("0123456789") == std::string::npos) {
    const int s = std::atoi(a.c_str());
    if (s >= static_cast<int>(level.base.players.size()))
      return mvd(q, "Player slot number " + a + " is invalid.");
    if (!playing(level, s)) return mvd(q, "Player slot number " + a + " is not active.");
    return chase(r, rec, q, s, level);
  }
  std::string name = a;
  if (a.size() >= 2 && a[0] == '!' && a[1] == '!') {
    name = a.substr(1);
  } else if (a[0] == '!') {
    const char k =
        a.size() > 1 ? static_cast<char>(std::tolower(static_cast<unsigned char>(a[1]))) : 0;
    if (k == 'q' || k == 'i' || k == 'r' || k == 'b') {
      rec.powerup = k;
      rec.autochase = false;
      return mvd(q, "Chasing players with '" + a.substr(1) + "' powerup.", kPrintMedium);
    }
    if (k == 'p') {
      if (rec.prev < 0) return mvd(q, "You have no previous chase target.");
      if (!playing(level, rec.prev)) return mvd(q, "Previous chase target is not active.");
      return chase(r, rec, q, rec.prev, level);
    }
    return mvd(q, "Unknown chase target '" + a.substr(1) +
                      "'. Valid targets are: q[uad]/i[nvulner]/r[ed_flag]/b[lue_flag]/"
                      "p[revious_target].");
  }
  const auto lower = [](std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
  };
  const std::string want = lower(name);
  int exact = -1, found = -1, matches = 0;
  for (size_t i = 0; i < level.base.players.size(); i++) {
    if (!level.base.players[i].in) continue;
    const std::string n = lower(player_name(level, static_cast<int>(i)));
    if (n == want) exact = static_cast<int>(i);
    if (n.find(want) != std::string::npos) {
      found = static_cast<int>(i);
      matches++;
    }
  }
  if (exact >= 0) return chase(r, rec, q, exact, level);
  if (matches == 0) return mvd(q, "No players matching '" + name + "' found.");
  if (matches > 1) return mvd(q, "'" + name + "' matches multiple players.");
  chase(r, rec, q, found, level);
}

bool wildcard(const std::string& pat, const std::string& s, size_t p = 0, size_t i = 0) {
  while (p < pat.size()) {
    if (pat[p] == '*') {
      for (size_t k = i; k <= s.size(); k++)
        if (wildcard(pat, s, p + 1, k)) return true;
      return false;
    }
    if (i >= s.size()) return false;
    const auto a = std::tolower(static_cast<unsigned char>(pat[p]));
    const auto b = std::tolower(static_cast<unsigned char>(s[i]));
    if (pat[p] != '?' && a != b) return false;
    p++;
    i++;
  }
  return i == s.size();
}

void autofollow(Q2Relay& r, Q2RelayViewer& rec, Q2Viewer& q, const std::vector<std::string>& w) {
  if (rec.chan < 0) return mvd(q, "Please enter a channel first.");
  Q2Channel* c = relay::channel(r, rec.chan);
  sv::Mvd2Level level;
  if (c == nullptr || !level_of(*c, level)) return;
  const auto count = [&rec]() {
    return std::to_string(rec.marks.size()) + (rec.marks.size() == 1 ? " target" : " targets");
  };
  if (w.size() < 2)
    return mvd(q, std::string("Auto chasing is ") + (rec.autochase ? "ON" : "OFF") + " (" +
                      count() + ").");
  const std::string& sub = w[1];
  if (sub == "add" || sub == "rm" || sub == "del") {
    if (w.size() < 3)
      return print(q, kPrintHigh, "Usage: " + w[0] + " " + sub + " <wildcard> [...]\n");
    for (size_t i = 2; i < w.size(); i++)
      for (size_t s = 0; s < level.base.players.size(); s++) {
        if (!wildcard(w[i], player_name(level, static_cast<int>(s)))) continue;
        const auto at = std::find(rec.marks.begin(), rec.marks.end(), static_cast<int>(s));
        if (sub == "add" && at == rec.marks.end()) rec.marks.push_back(static_cast<int>(s));
        if (sub != "add" && at != rec.marks.end()) rec.marks.erase(at);
      }
    if (rec.marks.empty()) rec.autochase = false;
    if (sub == "add" && rec.autochase) {
      const int s = step_player(level, rec.target, 1, &rec.marks);
      if (s >= 0 && s != rec.target) chase(r, rec, q, s, level);
    }
  } else if (sub == "set") {
    rec.marks.clear();
    for (size_t i = 2; i < w.size(); i++) rec.marks.push_back(std::atoi(w[i].c_str()));
  } else if (sub == "list") {
    if (rec.marks.empty()) return mvd(q, "Auto chase list is empty.");
    std::string names;
    for (const int s : rec.marks) names += (names.empty() ? "" : ", ") + player_name(level, s);
    mvd(q, "Auto chasing " + names);
  } else if (sub == "on") {
    if (rec.marks.empty()) return mvd(q, "Please add auto chase targets first.");
    rec.autochase = true;
    rec.powerup = 0;
  } else if (sub == "off") {
    rec.autochase = false;
  } else {
    mvd(q, "Usage: " + w[0] + " <add|rm|set|list|on|off> [...]");
  }
}

// The viewer's line as chat: the mute, the ignore mode, the flood rule, then to the channel.
void talk(Q2Relay& r, Q2RelayViewer& rec, Q2Viewer& q, std::string text) {
  if (r.cfg.flood_mute && !rec.admin) return mvd(q, "Spectators may not talk on this server.");
  if (rec.uf & kUfMuteObservers) return mvd(q, "Please turn off ignore mode first.");
  if (r.now < rec.mute_until)
    return mvd(q, "You can't talk for " + std::to_string(static_cast<int>(rec.mute_until - r.now)) +
                      " more seconds.");
  const int msgs = std::clamp(r.cfg.flood_msgs, 0, 15);
  const double back = rec.said[size_t((rec.said_at - msgs - 1 + 32) & 15)];
  if (r.now - back < r.cfg.flood_persecond) {
    rec.mute_until = r.now + r.cfg.flood_waitdelay;
    return mvd(q, "You can't talk for " + std::to_string(static_cast<int>(r.cfg.flood_waitdelay)) +
                      " seconds.");
  }
  rec.said[size_t(rec.said_at & 15)] = r.now;
  rec.said_at++;
  rec.talked = r.now;
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
    text = text.substr(1, text.size() - 2);
  std::string line = "[MVD] " + rec.name + ": " + text;
  if (line.size() > 149) line.resize(149);
  for (auto& [key, other] : r.viewers) {
    if (other.chan != rec.chan || ((other.uf & kUfMuteObservers) && !rec.admin)) continue;
    if (Q2Viewer* o = viewer_of(r, other); o != nullptr && o->stage == Q2ViewerStage::active)
      print(*o, kPrintHigh, hi(line) + "\n");
  }
}

void fwd(Q2Relay& r, Q2RelayViewer& rec, Q2Viewer& q, const std::string& line) {
  if (!rec.admin) return mvd(q, "You don't have admin status.");
  Q2Channel* c = relay::channel(r, rec.chan);
  if (c == nullptr || c->conn == -1)
    return mvd(q, "This channel does not support command forwarding.");
  Q2GtvConn* g = nullptr;
  for (Q2GtvConn& x : r.conns)
    if (x.id == c->conn) g = &x;
  if (g == nullptr || !g->hello) return mvd(q, "Not connected to the game server.");
  if ((g->flags & kGtfStringCmds) == 0)
    return mvd(q, "Game server does not allow command forwarding.");
  if (g->stream.out.size() > g->stream.out_head)
    return mvd(q, "Send buffer not empty, please wait.");
  std::string rest = line.size() > 4 ? line.substr(4) : "";
  if (rest.size() > 150) rest.resize(150);
  std::vector<uint8_t> m;
  m.push_back(static_cast<uint8_t>(rest.size() + 2));
  m.push_back(static_cast<uint8_t>((rest.size() + 2) >> 8));
  m.push_back(kGtvCmd);
  m.insert(m.end(), rest.begin(), rest.end());
  m.push_back(0);
  gn::tcp_send_bytes(g->stream, m.data(), m.size());
  g->sent = r.now;
}

void set_uf(Q2RelayViewer& rec, Q2Viewer& q) {
  stuff(q, "set uf " + std::to_string(rec.uf) + " u\n");
}

void cursor(Q2Relay& r, Q2RelayViewer& rec, int dir) {
  int rows = 0;
  if (rec.layout == Q2Layout::menu) rows = 10;
  if (rec.layout == Q2Layout::chooser) rows = static_cast<int>(listed_channels(r).size());
  if (rec.layout == Q2Layout::clients) rows = (spectators_on(r, rec.chan) + 15) / 16;
  if (rows <= 0) return;
  rec.cursor = (rec.cursor + dir + rows) % rows;
  draw(r, rec);
}

void join(Q2Relay& r, Q2RelayViewer& rec, Q2Viewer& q, int chan) {
  const uint64_t key = key_of(rec.addr, rec.qport);
  if (chan == rec.chan)
    return mvd(
        q, chan < 0 ? "You are already in the Waiting Room." : "You are already on this channel.");
  if (r.now - rec.entered < 2) return mvd(q, "You may not switch channels too soon.");
  int target = -1;
  if (chan >= 0) target = most_followed(r, chan);
  ask_move(r, key, chan, target, false);
}

void use(Q2Relay& r, Q2RelayViewer& rec, Q2Viewer& q) {
  if (rec.layout == Q2Layout::chooser) {
    const std::vector<const Q2Channel*> list = listed_channels(r);
    if (rec.cursor < static_cast<int>(list.size())) join(r, rec, q, list[size_t(rec.cursor)]->id);
    return;
  }
  if (rec.layout != Q2Layout::menu) return;
  switch (rec.cursor) {
    case 0:
      rec.layout = Q2Layout::none;
      follow(r, rec, q, {"observe"});
      break;
    case 1:
      rec.layout = Q2Layout::score;
      break;
    case 2:
      rec.layout = Q2Layout::clients;
      rec.cursor = 0;
      break;
    case 3:
      rec.layout = Q2Layout::chooser;
      rec.cursor = 0;
      break;
    case 4:
      join(r, rec, q, -1);
      return;
    case 5:
      rec.uf ^= kUfMuteObservers;
      set_uf(rec, q);
      break;
    case 6:
      rec.uf ^= kUfMuteMisc;
      set_uf(rec, q);
      break;
    case 7:
      rec.uf ^= kUfMutePlayers;
      set_uf(rec, q);
      break;
    case 8: {
      // Default, then Yes (the viewer's own), then No (the player's), then back.
      const bool own = rec.uf & kUfOwnFov, theirs = rec.uf & kUfPlayerFov;
      rec.uf &= ~(kUfOwnFov | kUfPlayerFov);
      if (!own && !theirs) rec.uf |= kUfOwnFov;
      if (own) rec.uf |= kUfPlayerFov;
      set_uf(rec, q);
      break;
    }
    default:
      rec.layout = Q2Layout::none;
      break;
  }
  draw(r, rec);
}

void command(Q2Relay& r, Q2RelayView& view, Q2Viewer& q, const std::string& line) {
  (void)view;
  r.activity = r.now;
  Q2RelayViewer* rp = record_of(r, q);
  if (rp == nullptr) return;
  Q2RelayViewer& rec = *rp;
  const std::vector<std::string> w = q2_words(line);
  if (w.empty()) return;
  const std::string& c = w[0];
  if (c == "say" || c == "say_team") {
    const size_t sp = line.find(' ');
    talk(r, rec, q, sp == std::string::npos ? "" : line.substr(sp + 1));
  } else if (c == "!mvdadmin") {
    if (rec.admin) {
      rec.admin = false;
      return mvd(q, "Lost admin status.");
    }
    if (w.size() < 2) return print(q, kPrintHigh, "Usage: !mvdadmin <password>\n");
    if (r.cfg.admin_password.empty() || w[1] != r.cfg.admin_password)
      return mvd(q, "Invalid password.");
    rec.admin = true;
    mvd(q, "Granted admin status.");
  } else if (c == "fwd") {
    fwd(r, rec, q, line);
  } else if (c == "follow" || c == "chase") {
    if (rec.chan < 0) return mvd(q, "Please enter a channel first.");
    follow(r, rec, q, w);
  } else if (c == "autofollow" || c == "autochase") {
    autofollow(r, rec, q, w);
  } else if (c == "observe" || c == "spectate" || c == "observer" || c == "spectator" ||
             c == "obs" || c == "spec") {
    if (rec.chan < 0) return mvd(q, "Please enter a channel first.");
    follow(r, rec, q, {"observe"});
  } else if (c == "inven" || c == "menu") {
    rec.layout = rec.layout == Q2Layout::menu ? Q2Layout::none : Q2Layout::menu;
    rec.cursor = 0;
    draw(r, rec);
  } else if (c == "invnext" || c == "invprev") {
    const int dir = c == "invnext" ? 1 : -1;
    if (rec.layout == Q2Layout::menu || rec.layout == Q2Layout::chooser ||
        rec.layout == Q2Layout::clients)
      return cursor(r, rec, dir);
    Q2Channel* ch = relay::channel(r, rec.chan);
    sv::Mvd2Level level;
    if (ch == nullptr || !level_of(*ch, level)) return;
    rec.powerup = 0;
    const int s = step_player(level, rec.target, dir, rec.autochase ? &rec.marks : nullptr);
    if (s >= 0 && s != rec.target) chase(r, rec, q, s, level);
  } else if (c == "invuse") {
    use(r, rec, q);
  } else if (c == "help" || c == "score") {
    rec.layout = rec.layout == Q2Layout::score ? Q2Layout::none : Q2Layout::score;
    draw(r, rec);
  } else if (c == "oldscore" || c == "oldscores" || c == "lastscore" || c == "lastscores") {
    rec.layout = rec.layout == Q2Layout::oldscore ? Q2Layout::none : Q2Layout::oldscore;
    draw(r, rec);
  } else if (c == "putaway") {
    rec.layout = rec.chan < 0 ? Q2Layout::chooser : Q2Layout::none;
    draw(r, rec);
  } else if (c == "channels") {
    std::string t =
        "id name         map      spc plr who is playing\n"
        "-- ------------ -------- --- --- --------------\n";
    for (const Q2Channel* ch : listed_channels(r)) {
      sv::Mvd2Level level;
      std::string names;
      int plr = 0;
      if (level_of(*ch, level))
        for (size_t i = 0; i < level.base.players.size(); i++)
          if (level.base.players[i].in) {
            names += (names.empty() ? "" : ", ") + player_name(level, static_cast<int>(i));
            plr++;
          }
      char row[128];
      std::snprintf(row, sizeof row, "%2d %-12.12s %-8.8s %3d %3d ", ch->id, ch->name.c_str(),
                    ch->map.c_str(), spectators_on(r, ch->id), plr);
      t += row + names + "\n";
    }
    print(q, kPrintHigh, t);
  } else if (c == "clients" || c == "players") {
    rec.layout = Q2Layout::clients;
    rec.cursor = 0;
    draw(r, rec);
  } else if (c == "commands") {
    print(q, kPrintHigh,
          "chase [player_id]      toggle chasecam mode\n"
          "autochase <...>        control automatic chasecam\n"
          "observe                toggle observer mode\n"
          "menu                   show main menu\n"
          "score                  show scoreboard\n"
          "oldscore               show previous scoreboard\n"
          "channels               list active channels\n"
          "join [channel_id]      join specified channel\n"
          "leave                  go to the Waiting Room\n");
  } else if (c == "join") {
    const std::vector<const Q2Channel*> list = listed_channels(r);
    const Q2Channel* pick = nullptr;
    if (list.empty()) return print(q, kPrintHigh, "No active channels.\n");
    if (w.size() < 2) {
      if (list.size() != 1) return print(q, kPrintHigh, "Please specify an exact channel ID.\n");
      pick = list[0];
    } else {
      const bool number = w[1].find_first_not_of("0123456789") == std::string::npos;
      for (const Q2Channel* ch : list)
        if ((number && ch->id == std::atoi(w[1].c_str())) || (!number && ch->name == w[1]))
          pick = ch;
      if (pick == nullptr) return print(q, kPrintHigh, "No such channel ID: " + w[1] + "\n");
    }
    join(r, rec, q, pick->id);
  } else if (c == "leave") {
    join(r, rec, q, -1);
  } else {
    talk(r, rec, q, line);  // a word it does not know is chat
  }
}

void begun(Q2Relay& r, Q2RelayView& view, Q2Viewer& q) {
  r.activity = r.now;
  const uint64_t key = key_of(q.address, q.qport);
  auto it = r.viewers.find(key);
  const bool fresh = it == r.viewers.end();
  if (fresh) {
    Q2RelayViewer rec;
    rec.addr = q.address;
    rec.qport = q.qport;
    rec.name = q.name;
    rec.layout = Q2Layout::chooser;
    rec.said.fill(-1e18);
    it = r.viewers.emplace(key, std::move(rec)).first;
  }
  Q2RelayViewer& rec = it->second;
  rec.said.fill(-1e18);
  rec.said_at = 0;
  if (&view == r.room.get()) {
    rec.chan = -1;
    rec.layout = Q2Layout::chooser;
    draw(r, rec);
    const std::vector<const Q2Channel*> list = listed_channels(r);
    // A viewer just come to the relay goes straight to the only channel.
    if (fresh && list.size() == 1)
      ask_move(r, key, list[0]->id, most_followed(r, list[0]->id), false);
    return;
  }
  announce(r, rec, rec.chan, rec.name + " entered the channel");
  if (Q2Channel* c = relay::channel(r, rec.chan); c != nullptr && c->state == Q2ChanState::wait)
    mvd(q, "Buffering data, please wait...");
  draw(r, rec);
}

// --- the Waiting Room's stream ---

void room_start(Q2Relay& r) {
  r.room_frame = 0;
  std::vector<uint8_t> b;
  const auto u8 = [&b](uint32_t v) { b.push_back(static_cast<uint8_t>(v)); };
  const auto i16 = [&](int32_t v) {
    u8(uint32_t(v) & 0xFF);
    u8((uint32_t(v) >> 8) & 0xFF);
  };
  const auto i32 = [&](int32_t v) {
    i16(v & 0xFFFF);
    i16(static_cast<int32_t>(uint32_t(v) >> 16));
  };
  const auto str = [&b](const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
  };
  u8(kSvcServerData);
  i32(34);
  i32(1);
  u8(0);
  str("");
  i16(0);
  str("Waiting Room");
  const std::pair<int, std::string> strings[] = {{0, "Waiting Room"},
                                                 {2, "unit1_"},
                                                 {30, "8"},
                                                 {31, std::to_string(r.room_checksum)},
                                                 {33, "maps/" + r.room_map + ".bsp"}};
  for (const auto& [i, s] : strings) {
    u8(kSvcConfigString);
    i16(i);
    str(s);
  }
  q2_tv_block(r.room->face, b);
}

void room_tick(Q2Relay& r) {
  if (r.now < r.room_next) return;
  r.room_next = std::max(r.room_next + 0.1, r.now - 0.1);
  std::vector<uint8_t> b = {kSvcFrame};
  const int32_t n = ++r.room_frame;
  for (int k = 0; k < 4; k++) b.push_back(static_cast<uint8_t>(uint32_t(n) >> (8 * k)));
  for (int k = 0; k < 4; k++) b.push_back(0xFF);  // whole: from no frame
  b.push_back(0);
  b.push_back(32);
  for (int k = 0; k < 32; k++) b.push_back(0xFF);
  sv::Q2PlayerState p;
  p.type = kPmFreeze;
  p.flags = kPmfNoPrediction;
  for (int k = 0; k < 3; k++) {
    p.origin[k] = static_cast<int16_t>(r.room_origin[k] * 8);
    p.view_angles[k] = static_cast<int16_t>(static_cast<int32_t>(r.room_angles[k] * 65536 / 360));
  }
  p.fov = 90;
  p.stats[kStatLayouts] = 1;
  sv::q2_write_player(b, sv::Q2PlayerState{}, p);
  b.push_back(kSvcPacketEntities);
  b.push_back(0);
  b.push_back(0);
  q2_tv_block(r.room->face, b);
}

}  // namespace

namespace relay {

void tell(Q2Relay& r, int chan, const std::string& text, int level) {
  for (auto& [key, rec] : r.viewers)
    if (rec.chan == chan)
      if (Q2Viewer* q = viewer_of(r, rec)) print(*q, level, "[MVD] " + text + "\n");
}

void feed_views(Q2Relay& r, Q2Channel& c, const std::vector<uint8_t>& bytes) {
  const bool level = bytes.size() > 2 && (bytes[2] & 0x1F) == sv::kMv2ServerData;
  if (level) {
    // The last level's scoreboard kept for `oldscore`, before the reader moves on.
    c.oldscore = scoreboard(c);
  }
  for (auto& [key, rec] : r.viewers) {
    if (rec.chan != c.id || !rec.view) continue;
    Q2RelayView& v = *rec.view;
    client::convert_feed(v.conv, bytes);
    std::vector<uint8_t> out;
    std::string error;
    if (!client::convert_take(v.conv, out, error)) continue;
    to_face(v, out);
  }
}

void refork(Q2Relay& r, Q2Channel& c) {
  for (auto& [key, rec] : r.viewers) {
    if (rec.chan != c.id || !rec.view) continue;
    auto opts = rec.view->conv.options.touch_player;
    client::convert_fork(c.base, rec.view->conv);
    rec.view->conv.options.touch_player = opts;
    client::convert_set_pov(rec.view->conv, rec.target);
  }
}

void evict(Q2Relay& r, Q2Channel& c) {
  for (auto& [key, rec] : r.viewers)
    if (rec.chan == c.id) {
      Q2Viewer* q = viewer_of(r, rec);
      if (q != nullptr && rec.view) q2_tv_move(rec.view->face, r.room->face, *q);
      rec.view.reset();
      rec.chan = -1;
      rec.target = -1;
      rec.layout = Q2Layout::chooser;
      rec.cursor = 0;
    }
}

void listed(Q2Relay& r) {
  for (auto& [key, rec] : r.viewers)
    if (rec.layout == Q2Layout::chooser || rec.layout == Q2Layout::menu) draw(r, rec);
}

void open_room(Q2Relay& r) {
  r.room = make_view(r, std::min(64, std::max(1, r.cfg.max_viewers)));
  std::string error;
  r.room_map = r.cfg.default_map;
  bool ok = r.spot && r.spot(r.room_map, r.room_origin, r.room_angles, r.room_checksum, error);
  if (!ok) {
    if (r.spot) say(r, "Couldn't load maps/" + r.room_map + ".bsp for the Waiting Room: " + error);
    r.cfg.default_map = r.room_map = "q2dm1";
    if (r.spot) r.spot(r.room_map, r.room_origin, r.room_angles, r.room_checksum, error);
  }
  r.room_next = r.now;
  room_start(r);
  room_tick(r);
}

void viewers_frame(Q2Relay& r, int64_t now_us) {
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  for (;;) {
    const int n = gn::transport_receive(*r.port, buf, sizeof buf, from, now_us);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    std::vector<uint8_t> bytes(buf, buf + size);
    if (is_out_of_band(buf, size)) {
      const std::vector<std::string> w = q2_words(q2_oob_text(buf, size));
      if (!w.empty() && (w[0] == "info" || w[0] == "status")) {
        std::string info;
        info_set(info, "hostname", r.cfg.hostname);
        info_set(info, "gamename", "gtv");
        info_set(info, "protocol", "34");
        info_set(info, "mapname", r.room_map);
        info_set(info, "maxclients", std::to_string(r.cfg.max_viewers));
        info_set(info, "mvd_channels", std::to_string(listed_channels(r).size()));
        int players = 0;
        for (const Q2Channel* c : listed_channels(r)) {
          sv::Mvd2Level level;
          if (level_of(*c, level)) players += players_in(level);
        }
        info_set(info, "mvd_players", std::to_string(players));
        std::string text;
        if (w[0] == "status") {
          text = "print\n" + info + "\n";
          for (const auto& [key, rec] : r.viewers) text += "0 0 \"" + rec.name + "\"\n";
        } else {
          char line[96];
          std::snprintf(line, sizeof line, "%16s %8s %2d/%2d\n", r.cfg.hostname.c_str(),
                        r.room_map.c_str(), q2_relay_viewers(r), r.cfg.max_viewers);
          text = std::string("info\n") + line;
        }
        const std::vector<uint8_t> b = q2_oob(text);
        gn::transport_send(*r.port, from, b.data(), b.size(), now_us);
        continue;
      }
      r.room->inbox.emplace_back(from, std::move(bytes));
      continue;
    }
    uint16_t qport = 0;
    if (!peek_qport(buf, size, qport)) continue;
    Q2RelayView* to = r.room.get();
    const auto it = r.viewers.find(key_of(from, qport));
    if (it != r.viewers.end() && it->second.view) to = it->second.view.get();
    to->inbox.emplace_back(from, std::move(bytes));
  }
  // The faces read and answer; the moves their words asked are made after.
  q2_tv_frame(r.room->face, now_us);
  for (auto& [key, rec] : r.viewers)
    if (rec.view) q2_tv_frame(rec.view->face, now_us);
  std::vector<Q2Relay::Move> moves = std::move(r.moves);
  r.moves.clear();
  for (const Q2Relay::Move& m : moves) {
    const auto it = r.viewers.find(m.key);
    if (it == r.viewers.end()) continue;
    if (m.same && it->second.chan == m.chan)
      retarget(it->second, m.target);
    else if (!m.same)
      enter(r, m.key, m.chan, m.target);
  }
  // Powerup chasing and autofollow: the view moves to a player who has it, or the next marked.
  for (auto& [key, rec] : r.viewers) {
    if (rec.chan < 0 || (rec.powerup == 0 && !rec.autochase)) continue;
    Q2Channel* c = relay::channel(r, rec.chan);
    sv::Mvd2Level level;
    if (c == nullptr || !level_of(*c, level)) continue;
    if (rec.powerup != 0) {
      const uint32_t bit = rec.powerup == 'q'   ? kEfQuad
                           : rec.powerup == 'i' ? kEfPent
                           : rec.powerup == 'r' ? kEfFlag1
                                                : kEfFlag2;
      const auto has = [&](int s) {
        return playing(level, s) && size_t(s + 1) < level.base.entities.size() &&
               (level.base.entities[size_t(s + 1)].effects & bit) != 0;
      };
      if (!has(rec.target))
        for (int s = 0; s < static_cast<int>(level.base.players.size()); s++)
          if (has(s)) {
            retarget(rec, s);
            break;
          }
    } else if (!playing(level, rec.target)) {
      const int s = step_player(level, rec.target, 1, &rec.marks);
      if (s >= 0) retarget(rec, s);
    }
  }
  // Viewers gone from their faces are forgotten.
  for (auto it = r.viewers.begin(); it != r.viewers.end();) {
    if (viewer_of(r, it->second) == nullptr)
      it = r.viewers.erase(it);
    else
      ++it;
  }
  room_tick(r);
}

void close_viewers(Q2Relay& r) {
  for (auto& [key, rec] : r.viewers)
    if (rec.view) q2_tv_stop(rec.view->face);
  r.viewers.clear();
  if (r.room) q2_tv_stop(r.room->face);
  r.room.reset();
}

}  // namespace relay

void q2_relay_spawn(Q2Relay& r, gn::Transport& port, double now) {
  r.port = &port;
  r.now = std::max(r.now, now);
  r.spawned = true;
  if (r.cfg.suspend_min <= 0) r.activity = r.now;
  relay::open_room(r);
}

}  // namespace shooters_edge::net
