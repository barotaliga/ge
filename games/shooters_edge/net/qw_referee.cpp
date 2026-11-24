// QuakeWorld's admin in the game, defined: the rules in qw_referee.hpp's header.
//
// ge:summary QuakeWorld's admin in the game: its words, the acts done, forcestart and the lock
// ge:keywords qw-referee qw_referee_line qw_referee_frame forcestart lock kick ban-address
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_referee_test.cpp
#include "shooters_edge/net/qw_referee.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "core/num_text.hpp"
#include "net/address.hpp"
#include "server/bans.hpp"
#include "shooters_edge/net/qw_server.hpp"

namespace shooters_edge::net {

namespace {

namespace gs = ge::server;
namespace sv = shooters_edge::server;

constexpr double kForceSeconds = 10, kLockSeconds = 15;
constexpr uint32_t kEveryone = 0xffffffffu;

// Whole seconds left, rounded up.
double whole_up(double s) {
  const auto n = static_cast<int64_t>(s);
  return static_cast<double>(n + (static_cast<double>(n) < s ? 1 : 0));
}

gs::RefereeWords make_words() {
  gs::RefereeWords w;
  w.login = {"admin"};
  w.words = {{"forcebreak", "endmatch"},
             {"forcemap", "changemap"},
             {"kick", "kick"},
             {"acommands", "acommands"}};
  w.break_ends = false;  // a player's `break` in a match is his own (the lobby's)
  w.wait_s = 5;
  w.wait_after_wrong = true;
  w.no_password_is_wrong = true;
  w.exact_names = true;
  w.kick_reason = true;
  w.kick_ban_minutes = 0.5;
  w.became = "{name} gains admins status!";
  w.became_me =
      "Please give up admin rights when you're done\nType acommands for the admin's words";
  w.left = "{name} is no longer an admin";
  w.none = "NO admins on this server!";
  w.wrong = "Access denied...";
  w.wrong_log = "{name} gave a wrong admin code.";
  w.wait = "Wait {n} {seconds}!";
  w.not_one = "You are not an admin";
  w.list =
      "Admin: forcestart, forcebreak, forcemap <map>, kick <id or name> [reason], lock; admin "
      "again gives it up.";
  w.map_in_match = "Match currently in progress. Use break or forcebreak to terminate.";
  w.map_usage = "Usage: forcemap <mapname>";
  w.map_unknown = "Map {map} not available on this server.";
  w.new_map = "{name} forces a map change to {map}!";
  w.ended = "{name} forces a break!";
  w.no_player = "kick: client {player} not found";
  w.shielded = "{player} is an admin and can't be kicked";
  w.kicked = "{player} was kicked by {name}";
  w.kicked_you = "You were kicked from the server";
  w.kick_said_reason = "[{reason}]";
  return w;
}

void tell(QwServer& s, int to, const std::string& line) {
  if (to == -1)
    qw_print_all(s, line + "\n");
  else if (to >= 0 && to < ge::shooter::kMaxClients)
    client_print(s, to, 2, line + "\n");
}

void centre(sv::Announcer& a, uint32_t to, std::string text) {
  sv::NetEvent ev;
  ev.kind = sv::EventKind::centerprint;
  ev.to = to;
  ev.reliable = true;
  ev.text = std::move(text);
  a.pending.push_back(std::move(ev));
}

// The players the admin may name: everyone on the server but the bots and a listen server's own
// player (unless he is the one asking).
std::array<std::string, gs::kLobbySeats> names_of(const QwServer& s, int caller,
                                                  std::array<int, gs::kLobbySeats>& ids) {
  std::array<std::string, gs::kLobbySeats> names{};
  for (int i = 0; i < gs::kLobbySeats && i < ge::shooter::kMaxClients; i++) {
    const QwClient& c = s.clients[i];
    ids[static_cast<size_t>(i)] = c.userid;
    if (c.state == ConnState::free || (c.local && i != caller)) continue;
    names[static_cast<size_t>(i)] = c.name.empty() ? "player" : c.name;
  }
  return names;
}

// A kicked player's address kept out for `minutes` (an address the ban list takes).
void ban_address(QwServer& s, int slot, double minutes) {
  const ge::net::Address& at = s.clients[slot].address;
  uint8_t ip[4] = {};
  if (at.family == ge::net::Family::sim)
    ge::net::sim_to_ipv4(at, ip);
  else if (at.family == ge::net::Family::ipv4)
    std::copy(at.bytes, at.bytes + 4, ip);
  else
    return;
  std::string out;
  gs::ban_command(s.bans,
                  "addip " + std::to_string(ip[0]) + "." + std::to_string(ip[1]) + "." +
                      std::to_string(ip[2]) + "." + std::to_string(ip[3]) + " ban +" +
                      std::to_string(static_cast<int64_t>(minutes * 60)),
                  qw_unix_time(s), out);
}

// What the book asked, done; its lines said first, so a kicked player reads his before he goes.
void turn(QwReferee& r, const QwRefereeGame& g) {
  for (size_t i = 0; i < r.book.said.size(); i++) tell(*g.qw, r.book.to[i], r.book.said[i]);
  r.book.said.clear();
  r.book.to.clear();
  sv::RulesCommon& rules = **g.rules;
  for (const gs::RefereeAct& a : r.book.acts) {
    switch (a.kind) {
      case gs::RefereeActKind::end_match:
        if (g.overtime != nullptr) g.overtime->over = true;  // no overtime after this
        g.rules->console("timelimit " + ge::core::num::to_string(rules.sv.time / 60));
        break;
      case gs::RefereeActKind::change_map:
        rules.pending_map = a.text;
        rules.pending_change = sv::MapChange::change_level;
        break;
      case gs::RefereeActKind::kick:
        if (a.slot < 0 || a.slot >= ge::shooter::kMaxClients || g.qw->clients[a.slot].local) break;
        if (a.value > 0) ban_address(*g.qw, a.slot, a.value);
        qw_drop(*g.qw, a.slot, "was kicked");
        break;
      default:  // not among QuakeWorld's words
        break;
    }
  }
  r.book.acts.clear();
}

// Referees whose slot was freed, or taken by someone else since, forgotten.
void forget_gone(QwReferee& r, const QwServer& s) {
  for (int i = 0; i < gs::kLobbySeats && i < ge::shooter::kMaxClients; i++) {
    if (!gs::referee_is(r.book, i)) continue;
    const QwClient& c = s.clients[i];
    if (c.state == ConnState::free || c.userid != r.ids[static_cast<size_t>(i)])
      gs::referee_left(r.book, i);
  }
}

const std::string& name_at(const QwServer& s, int slot) { return s.clients[slot].name; }

bool force_start(QwReferee& r, const QwRefereeGame& g, int slot) {
  QwServer& s = *g.qw;
  if (!gs::referee_is(r.book, slot)) {
    tell(s, slot, qw_referee_words().not_one);
    return true;
  }
  if (g.lobby == nullptr) {
    tell(s, slot, "There is no warmup to start: the server plays without a lobby.");
    return true;
  }
  gs::Lobby& l = *g.lobby;
  if (l.phase != gs::LobbyPhase::warmup) return true;  // a match on or coming: nothing to force
  if (r.force_ends > 0) {
    tell(s, slot, "forcestart already in progress!");
    return true;
  }
  const double t = (*g.rules)->sv.time;
  if (slot < gs::kLobbySeats && l.seats[static_cast<size_t>(slot)].used &&
      !l.seats[static_cast<size_t>(slot)].ready)
    gs::lobby_ready(l, slot, true, t);
  if (gs::lobby_seated(l) == 0) {
    tell(s, slot, "Can't issue! More players needed.");
    return true;
  }
  tell(s, -1, name_at(s, slot) + " forces matchstart!");
  r.force_ends = g.now + kForceSeconds;
  r.force_by = slot;
  r.force_shown = -1;
  return true;
}

void force_frame(QwReferee& r, const QwRefereeGame& g) {
  if (r.force_ends <= 0) return;
  QwServer& s = *g.qw;
  if (g.lobby == nullptr || g.lobby->phase != gs::LobbyPhase::warmup) {
    r.force_ends = 0;  // everyone got ready and the countdown took over (or the lobby went)
    return;
  }
  gs::Lobby& l = *g.lobby;
  const auto by = static_cast<size_t>(r.force_by);
  if (r.force_by >= 0 && l.seats[by].used && !l.seats[by].ready) {
    tell(s, -1, name_at(s, r.force_by) + " interrupts countdown");
    r.force_ends = 0;
    return;
  }
  const double left = r.force_ends - g.now;
  if (left > 0) {
    const auto n = static_cast<int>(std::max(1.0, whole_up(left)));
    if (n == r.force_shown || g.announcer == nullptr) return;
    r.force_shown = n;
    const std::string line =
        std::to_string(n) + (n == 1 ? " second" : " seconds") + " left before game starts";
    uint32_t unready = 0;
    for (int i = 0; i < gs::kLobbySeats; i++)
      if (l.seats[static_cast<size_t>(i)].used && !l.seats[static_cast<size_t>(i)].ready)
        unready |= 1u << i;
    centre(*g.announcer, kEveryone & ~unready, line);
    if (unready != 0) centre(*g.announcer, unready, line + "\n\nGet ready!");
    return;
  }
  r.force_ends = 0;
  const double t = (*g.rules)->sv.time;
  for (int i = 0; i < gs::kLobbySeats && i < ge::shooter::kMaxClients; i++) {
    const gs::LobbySeat& seat = l.seats[static_cast<size_t>(i)];
    if (!seat.used || seat.ready || seat.bot || s.clients[i].local) continue;
    tell(s, -1, name_at(s, i) + " was kicked by admin forcestart");
    tell(s, i, "Bye bye! Pay attention next time.");
    gs::lobby_leave(l, i, t);
    qw_drop(s, i, "was kicked by admin forcestart");
  }
  if (gs::lobby_ready_count(l) == 0) {
    tell(s, -1, "Can't start! More players needed.");
    return;
  }
  gs::lobby_start(l, gs::lobby_leader(l), t);
}

bool lock(QwReferee& r, const QwRefereeGame& g, int slot) {
  QwServer& s = *g.qw;
  if (!gs::referee_is(r.book, slot)) {
    tell(s, slot, qw_referee_words().not_one);
    return true;
  }
  if (r.locked_until > g.now) {
    r.locked_until = 0;
    tell(s, -1, name_at(s, slot) + " unlocked server");
    return true;
  }
  r.locked_until = g.now + kLockSeconds;
  tell(s, -1, name_at(s, slot) + " locked server for 15 seconds");
  return true;
}

// Captain mode's view of the game: the referee's, with the server's one election.
QwCaptainsGame captains_game(QwReferee& r, const QwRefereeGame& g) {
  QwCaptainsGame c;
  c.qw = g.qw;
  c.election = &r.election;
  c.lobby = g.lobby;
  c.settings = g.settings;
  const ge::console::Cvar* teamplay =
      g.rules && g.rules->common ? g.rules->common->cvars.find("teamplay") : nullptr;
  c.team_mode = teamplay != nullptr && teamplay->on();
  c.in_match = g.lobby != nullptr && g.lobby->phase != gs::LobbyPhase::warmup;
  c.now = g.now;
  c.seed = g.settings != nullptr ? g.settings->seed : 0;
  return c;
}

}  // namespace

const gs::RefereeWords& qw_referee_words() {
  static const gs::RefereeWords words = make_words();
  return words;
}

bool qw_referee_line(QwReferee& r, const QwRefereeGame& g, int slot, const std::string& line) {
  if (g.qw == nullptr || g.rules == nullptr || slot < 0 || slot >= gs::kLobbySeats ||
      slot >= ge::shooter::kMaxClients)
    return false;
  forget_gone(r, *g.qw);
  if (qw_captains_line(r.captains, captains_game(r, g), slot, line)) return true;
  if (line == "forcestart") return force_start(r, g, slot);
  if (line == "lock") return lock(r, g, slot);
  std::array<int, gs::kLobbySeats> ids{};
  const auto names = names_of(*g.qw, slot, ids);
  std::string code, admins;
  if (g.settings != nullptr) {
    if (const auto it = g.settings->rules.find("k_admincode"); it != g.settings->rules.end())
      code = it->second == "none" ? std::string() : it->second;
    if (const auto it = g.settings->rules.find("k_admins"); it != g.settings->rules.end())
      admins = it->second;
  }
  gs::RefereeCaller who;
  who.slot = slot;
  who.password = code;
  who.in_match = g.in_match;
  who.names = &names;
  // Without a lobby the server is never between matches: the map is changed whenever asked.
  static const gs::RefereeWords matchless = [] {
    gs::RefereeWords w = qw_referee_words();
    w.map_in_match.clear();
    return w;
  }();
  who.words = g.lobby != nullptr ? &qw_referee_words() : &matchless;
  who.allowed = admins.empty() || std::strtod(admins.c_str(), nullptr) != 0;
  who.ids = &ids;
  who.map_known = g.map_known;
  who.map_ctx = g.map_ctx;
  const bool was = gs::referee_is(r.book, slot);
  if (!gs::referee_command(r.book, who, line, g.now)) return false;
  if (!was && gs::referee_is(r.book, slot))
    r.ids[static_cast<size_t>(slot)] = g.qw->clients[slot].userid;
  turn(r, g);
  return true;
}

void qw_referee_frame(QwReferee& r, const QwRefereeGame& g) {
  if (g.qw == nullptr || g.rules == nullptr) return;
  forget_gone(r, *g.qw);
  r.last = g;
  qw_captains_frame(r.captains, captains_game(r, g));
  force_frame(r, g);
  if (r.locked_until > 0 && g.now >= r.locked_until) {
    r.locked_until = 0;
    tell(*g.qw, -1, "server unlocked");
  }
}

std::string qw_referee_lock(const QwReferee& r, double now) {
  if (r.locked_until <= now) return {};
  const auto n = static_cast<int>(std::max(1.0, whole_up(r.locked_until - now)));
  return "server is temporary locked: " + std::to_string(n) + (n == 1 ? " second" : " seconds");
}

uint8_t qw_referee_buttons(QwReferee& r, const QwRefereeGame& g, int slot, uint8_t buttons) {
  return qw_captains_buttons(r.captains, captains_game(r, g), slot, buttons);
}

void qw_referee_hooks(QwServer& s, QwReferee& r) {
  s.move_ctx = &r;
  s.impulse = [](void* ctx, int slot, uint8_t impulse, bool fresh) {
    QwReferee& me = *static_cast<QwReferee*>(ctx);
    return qw_captains_impulse(me.captains, captains_game(me, me.last), slot, impulse, fresh);
  };
  s.info_rule = [](void* ctx, int slot, const std::string& key, const std::string& value) {
    QwReferee& me = *static_cast<QwReferee*>(ctx);
    return qw_captains_info(me.captains, captains_game(me, me.last), slot, key, value);
  };
}

}  // namespace shooters_edge::net
