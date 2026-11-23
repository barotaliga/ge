// Our Quake II server's match (q2_match.hpp states it).
//
// ge:summary Our Quake II server's match: seats, the lobby's words, timeouts, overtime, the hold
// ge:keywords q2_match_frame q2_match_open q2-lobby q2-timeout q2-overtime prewar lock
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_match_test.cpp
#include "shooters_edge/net/q2_match.hpp"

#include <vector>

#include "server/overtime.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_spectate.hpp"

namespace shooters_edge::net {

namespace gs = ge::server;

namespace {

constexpr int kSlots = 32;

double rules_time(const Q2Match& m) { return (*m.rules)->sv.time; }

// A client on the map as a player: in the game, and not watching.
bool playing(const Q2Match& m, int i) {
  const Q2NetClient& c = m.net->clients[i];
  const sv::ClientInfo& r = (*m.rules)->clients[i];
  return c.stage == Q2Slot::active && r.active && !r.spectator;
}

void say(Q2Match& m, int to, const std::string& line) {
  const std::string text = line.ends_with('\n') ? line : line + "\n";
  for (int i = 0; i < kSlots && i < sh::kMaxClients; i++)
    if ((to < 0 || to == i) && m.net->clients[i].stage == Q2Slot::active)
      q2_net_print(*m.net, i, text);
}

double number(const Q2Match& m, const char* name) {
  const ge::console::Cvar* c = (*m.rules)->cvars.find(name);
  return c != nullptr ? c->number : 0;
}

bool in_match(const Q2Match& m) { return !m.on || m.lobby.phase == gs::LobbyPhase::match; }

// The lobby's words as the rules' events would carry them, printed: a print or a centre print to
// the slots it names (bit 0 the first slot).
void announce_turn(Q2Match& m) {
  sv::MatchLook look;
  look.deathmatch = static_cast<int>(number(m, "deathmatch"));
  look.timelimit = static_cast<int>(number(m, "timelimit"));
  look.fraglimit = static_cast<int>(number(m, "fraglimit"));
  if (m.hold.held) {  // the warmup holds them at 0: the match's are the kept ones
    look = sv::match_look(look, {"timelimit " + m.hold.timelimit, "fraglimit " + m.hold.fraglimit});
  }
  sv::RulesCommon& now = **m.rules;
  sv::announce(m.announcer, m.lobby, now.localinfo, look, now.sv.time);
  sv::announce_phase(now.localinfo, m.lobby.phase);
  for (const sv::NetEvent& e : m.announcer.pending) {
    if (e.kind != sv::EventKind::print && e.kind != sv::EventKind::centerprint) continue;
    for (int i = 0; i < kSlots && i < sh::kMaxClients; i++)
      if (((e.to >> i) & 1u) != 0 && m.net->clients[i].stage == Q2Slot::active)
        q2_net_print(*m.net, i, e.text.ends_with('\n') ? e.text : e.text + "\n");
  }
  m.announcer.pending.clear();
}

// Every slot's place in the lobby kept to the clients: a player seated, one gone freed, a newcomer
// to a match closed to him held on the sidelines.
void seats_turn(Q2Match& m) {
  sv::Q2Server* game = sv::q2_of(*m.rules);
  const double t = rules_time(m);
  for (int i = 0; i < kSlots && i < sh::kMaxClients && i < gs::kLobbySeats; i++) {
    const auto k = static_cast<size_t>(i);
    const Q2NetClient& c = m.net->clients[i];
    gs::LobbySeat& seat = m.lobby.seats[k];
    const bool here = c.stage == Q2Slot::active;
    if (seat.used && (!here || m.seat_id[k] != c.userid || !playing(m, i))) {
      gs::lobby_leave(m.lobby, i, t);
      m.seat_id[k] = 0;
      m.deaths_left += m.deaths[k];
      m.deaths[k] = 0;
    }
    const uint32_t bit = 1u << i;
    if (!here && (m.held_out & bit) != 0) m.held_out &= ~bit;
    if (!here || seat.used || !playing(m, i)) continue;
    if (m.on) {
      const std::string why = sv::lock_refusal(m.lobby, c.name);
      if (!why.empty()) {
        if ((m.held_out & bit) == 0) say(m, i, why);
        m.held_out |= bit;
        if (game != nullptr) sv::spectate_hold(*game, i, true);
        continue;
      }
    }
    gs::lobby_join(m.lobby, i, c.name, false, t);
    m.seat_id[k] = c.userid;
    if (game != nullptr) m.died_seen[k] = game->players[i].died_at;
  }
  // The rules' own flags follow the match's (a new map's rules start with none).
  if (game != nullptr && game->held_out != m.held_out)
    for (int i = 0; i < kSlots && i < sh::kMaxClients; i++) {
      const bool want = ((m.held_out >> i) & 1u) != 0;
      if (want != (((game->held_out >> i) & 1u) != 0)) sv::spectate_hold(*game, i, want);
    }
}

// The deaths the rules scored since the last frame, and the score a two-player match keeps.
gs::MatchScore score_turn(Q2Match& m, int64_t& deaths) {
  deaths = m.deaths_left;
  std::vector<gs::ScoredPlayer> players;
  sv::Q2Server* game = sv::q2_of(*m.rules);
  if (game == nullptr) return {};
  for (int i = 0; i < kSlots && i < sh::kMaxClients && i < gs::kLobbySeats; i++) {
    const auto k = static_cast<size_t>(i);
    if (!m.lobby.seats[k].used) continue;
    const sv::Q2Player& p = game->players[i];
    if (p.died_at != m.died_seen[k]) {
      m.died_seen[k] = p.died_at;
      m.deaths[k]++;
    }
    deaths += m.deaths[k];
    players.push_back({m.lobby.seats[k].name, static_cast<int64_t>(p.frags), true});
  }
  return gs::match_score(players, false);
}

void timeout_turn(Q2Match& m) {
  gs::TimeoutBook& b = m.timeout;
  for (size_t i = 0; i < b.said.size(); i++) say(m, b.to[i], b.said[i]);
  b.said.clear();
  b.to.clear();
  sv::timeout_hold(b, *m.rules, m.timeout_held);
}

// Quake II's match words for a timeout: one word calls it and, from the side that called it, ends
// it.
std::string timeout_word(const Q2Match& m, int slot, const std::string& line) {
  if (line != "time" && line != "calltime" && line != "ctime" && line != "hold") return line;
  return m.timeout.phase != gs::TimeoutPhase::none && m.timeout.caller == slot ? "timein"
                                                                               : "timeout";
}

bool hook(void* ctx, int slot, const std::string& line) {
  Q2Match& m = *static_cast<Q2Match*>(ctx);
  if (m.rules != nullptr && slot >= 0 && slot < kSlots && slot < sh::kMaxClients) {
    const Q2NetClient& c = m.net->clients[slot];
    if (line == "pause" && gs::timeout_paused(m.timeout)) {
      say(m, slot, "The game is in a timeout. Type timein to end yours early.");
      return true;
    }
    gs::TimeoutCaller who;
    who.slot = slot;
    who.name = c.name;
    who.side = c.name;
    who.player = playing(m, slot);
    if (gs::timeout_command(m.timeout, who, in_match(m), timeout_word(m, slot, line), m.net->now)) {
      timeout_turn(m);
      return true;
    }
    if (m.on && c.stage == Q2Slot::active) {
      const std::string word =
          line == "unready" || line == "noready" ? std::string("notready") : line;
      if (gs::lobby_command(m.lobby, slot, word, rules_time(m))) {
        announce_turn(m);
        return true;
      }
    }
  }
  return m.prior != nullptr && m.prior(m.prior_ctx, slot, line);
}

// Prewar on a move: fire held, and the jump (the up move) for those the lobby holds.
void move(void* ctx, int slot, sh::MoveCmd& cmd) {
  Q2Match& m = *static_cast<Q2Match*>(ctx);
  if (!m.on || m.rules == nullptr || slot < 0 || slot >= kSlots || slot >= gs::kLobbySeats) return;
  std::string tell;
  cmd.buttons = sv::prewar_buttons(m.lobby, slot, cmd.buttons, rules_time(m),
                                   m.held_told[static_cast<size_t>(slot)], tell, false);
  if (sv::prewar_jump_held(m.lobby, slot) && cmd.up > 0) cmd.up = 0;
  if (!tell.empty()) say(m, slot, tell);
}

}  // namespace

void q2_match_open(Q2Match& m, Q2NetServer& net, sv::RulesRunner& rules,
                   const gs::ServerConfig& c) {
  m.net = &net;
  m.rules = &rules;
  m.on = c.lobby != 0;
  m.lobby.settings.countdown_s = c.countdown_s;
  m.lobby.settings.intermission_s = 0;  // the rules' own scoreboard is the intermission
  m.lobby.settings.humans = 1;
  m.lobby.settings.min_players = 2;
  sv::lobby_rules(m.lobby.settings, c, false);
  m.lobby.settings.idle_kick_s = 0;  // Quake II's sv_idlekick is the idle drop, not the lobby's
  if (!m.on) m.lobby.phase = gs::LobbyPhase::match;
  m.timeout.settings = gs::timeout_settings(c);
  if (rules && rules->cvars.find("sv_overtime") == nullptr) gs::add_overtime_settings(rules->cvars);
  m.prior = net.line_hook;
  m.prior_ctx = net.hook_ctx;
  net.line_hook = hook;
  net.hook_ctx = &m;
  net.move_hook = move;
  net.move_ctx = &m;
}

void q2_match_frame(Q2Match& m, const std::string& map, double now) {
  if (m.net == nullptr || m.rules == nullptr || !*m.rules) return;
  m.map = map;
  gs::timeout_tick(m.timeout, now);
  timeout_turn(m);
  sv::RulesCommon& rules = **m.rules;
  if (m.on) {
    seats_turn(m);
    if (gs::lobby_due(m.lobby, rules.sv.time) && rules.pending_map.empty()) {
      rules.pending_map = map;  // the match: the map again, every score new
      rules.pending_change = sv::MapChange::restart;
    }
    gs::lobby_tick(m.lobby, rules.sv.time);
    std::vector<int> kicked;
    for (const gs::LobbyNote& n : m.lobby.notes)
      if (n.kind == gs::LobbyNoteKind::kicked) kicked.push_back(n.slot);
    announce_turn(m);
    for (int slot : kicked) q2_net_drop(*m.net, slot, "was kicked for not getting ready");
    sv::warmup_hold(m.hold, *m.rules, m.lobby.phase == gs::LobbyPhase::match);
  }
  int64_t deaths = 0;
  const gs::MatchScore score = score_turn(m, deaths);
  std::vector<std::string> said;
  sv::overtime_step(m.overtime, *m.rules, score, deaths,
                    m.on && m.lobby.phase == gs::LobbyPhase::match && number(m, "deathmatch") != 0,
                    said);
  for (const std::string& line : said) say(m, -1, line);
}

void q2_match_map_change(Q2Match& m, bool restart, double now) {
  (void)now;
  const bool match_over = !restart;
  if (match_over) {
    gs::timeout_map_over(m.timeout);
  } else {
    m.timeout.phase = gs::TimeoutPhase::none;
    m.timeout.caller = -1;
  }
  m.timeout_held = false;
  m.deaths.fill(0);
  m.deaths_left = 0;
  m.died_seen.fill(0);  // a new map's rules: deaths counted from there
  if (!m.on || !match_over || m.lobby.phase != gs::LobbyPhase::match) return;
  const double t = (*m.rules)->sv.time;
  gs::lobby_end(m.lobby, t);
  if (gs::lobby_next_due(m.lobby, t)) gs::lobby_back(m.lobby);
  m.held_out = 0;  // the sidelines open with the warmup
}

std::string q2_match_status(const Q2Match& m) {
  return m.on ? sv::phase_status(m.lobby.phase) : std::string();
}

}  // namespace shooters_edge::net
