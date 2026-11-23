// Our Quake II server's match (q2_match.hpp states it).
//
// ge:summary Our Quake II server's match: seats, the lobby's words, timeouts, overtime, the hold,
// ge:summary teams and captains, the referee, the teams' scores and the stats words
// ge:keywords q2_match_frame q2_match_open q2-lobby q2-timeout q2-overtime prewar lock
// ge:keywords q2-teams q2-referee q2-stats team-skins team-score
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_match_test.cpp
#include "shooters_edge/net/q2_match.hpp"

#include <algorithm>
#include <vector>

#include "core/num_text.hpp"
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

std::string text(const Q2Match& m, const char* name) {
  const ge::console::Cvar* c = (*m.rules)->cvars.find(name);
  return c != nullptr ? c->value : std::string();
}

bool in_match(const Q2Match& m) { return !m.on || m.lobby.phase == gs::LobbyPhase::match; }

size_t team_index(gs::Team t) { return t == gs::Team::blue ? 1 : 0; }

gs::Team seat_team(const Q2Match& m, int i) {
  const gs::LobbySeat& s = m.lobby.seats[static_cast<size_t>(i)];
  return s.used ? s.team : gs::Team::none;
}

// Every slot's name ("" a free one), for the words that name a player.
std::array<std::string, gs::kLobbySeats> names(const Q2Match& m) {
  std::array<std::string, gs::kLobbySeats> out{};
  for (int i = 0; i < kSlots && i < sh::kMaxClients; i++)
    if (m.net->clients[i].stage == Q2Slot::active)
      out[static_cast<size_t>(i)] = m.net->clients[i].name;
  return out;
}

void log_line(Q2Match& m, const std::string& line) {
  if (m.net->log.size() < 512) m.net->log.push_back(line);
}

// Lines a book left for the game: -1 everyone, -2 the server's log, else that slot.
void say_book(Q2Match& m, std::vector<std::string>& said, std::vector<int>& to) {
  for (size_t i = 0; i < said.size(); i++) {
    if (to[i] == -2)
      log_line(m, said[i]);
    else
      say(m, to[i], said[i]);
  }
  said.clear();
  to.clear();
}

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
      if (seat.team != gs::Team::none) m.team_left[team_index(seat.team)] += m.frags_seen[k];
      m.frags_seen[k] = 0;
      gs::lobby_leave(m.lobby, i, t);
      m.seat_id[k] = 0;
      m.deaths_left += m.deaths[k];
      m.deaths[k] = 0;
    }
    const uint32_t bit = 1u << i;
    if (!here && (m.held_out & bit) != 0) m.held_out &= ~bit;
    if (!here) {
      m.joining[k] = 0;
      gs::referee_left(m.referee, i);
    }
    if (!here || seat.used || !playing(m, i)) continue;
    if (m.on && m.joining[k] == 0) {  // an invite or a pick lets a player past the lock
      const std::string why = sv::lock_refusal(m.lobby, c.name);
      if (!why.empty()) {
        if ((m.held_out & bit) == 0) say(m, i, why);
        m.held_out |= bit;
        if (game != nullptr) sv::spectate_hold(*game, i, true);
        continue;
      }
    }
    gs::lobby_join(m.lobby, i, c.name, false, t);
    if (m.joining[k] != 0) gs::teams_seat(m.teams, m.lobby, i, static_cast<gs::Team>(m.joining[k]));
    m.joining[k] = 0;
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
    m.frags_seen[k] = static_cast<int64_t>(p.frags);
    const gs::Team team = m.lobby.seats[k].team;
    players.push_back({m.teams_on ? gs::team_name(team) : m.lobby.seats[k].name,
                       static_cast<int64_t>(p.frags), true});
  }
  if (m.teams_on)  // the teams' frags of those who left still count
    for (const gs::Team team : {gs::Team::red, gs::Team::blue})
      players.push_back({gs::team_name(team), m.team_left[team_index(team)], false});
  return gs::match_score(players, m.teams_on);
}

// Each team's score: its players' frags, those who left included.
std::array<int64_t, 2> team_scores(const Q2Match& m) {
  std::array<int64_t, 2> out = m.team_left;
  for (int i = 0; i < kSlots && i < gs::kLobbySeats; i++)
    if (const gs::Team team = seat_team(m, i); team != gs::Team::none)
      out[team_index(team)] += m.frags_seen[static_cast<size_t>(i)];
  return out;
}

// The match's end in words, for a team match: who won, by how much.
void say_result(Q2Match& m) {
  if (!m.teams_on) return;
  const std::array<int64_t, 2> s = team_scores(m);
  say(m, -1,
      sv::team_result(gs::teams_name(m.teams, gs::Team::red), s[0],
                      gs::teams_name(m.teams, gs::Team::blue), s[1]));
}

// The stats as a match's report (the shared lines read one): a player for each slot counted, his
// score his kills less his suicides and team kills, his team while the teams are on.
ge::net::MatchReport stats_report(const Q2Match& m, const std::array<sv::Q2SlotStats, 32>& table,
                                  const std::array<std::string, 32>& who) {
  const sv::StatsGame& g = sv::q2_stats_game();
  ge::net::MatchReport r;
  for (int k = 0; k < sv::stats_columns(g); k++) r.columns.push_back(sv::stats_column_name(g, k));
  for (int i = 0; i < kSlots; i++) {
    const sv::Q2SlotStats& t = table[static_cast<size_t>(i)];
    if (!t.counted || who[static_cast<size_t>(i)].empty()) continue;
    ge::net::ReportPlayer p;
    p.name = who[static_cast<size_t>(i)];
    if (m.teams_on && seat_team(m, i) != gs::Team::none)
      p.team = gs::teams_name(m.teams, seat_team(m, i));
    p.score = p.frags = t.kills - t.suicides - t.team_kills;
    p.deaths = t.deaths;
    p.suicides = t.suicides;
    p.team_kills = t.team_kills;
    p.columns = sv::stats_row(g, t.tally);
    r.players.push_back(std::move(p));
  }
  return r;
}

// The match's stats kept as the last match's, with the names they were counted under.
bool keep_stats(Q2Match& m) {
  const sv::Q2Server* game = sv::q2_of(*m.rules);
  if (game == nullptr) return false;
  m.last_stats = game->stats.slot;
  for (int i = 0; i < kSlots && i < sh::kMaxClients; i++)
    m.last_names[static_cast<size_t>(i)] = m.net->clients[i].name;
  m.has_last = true;
  return true;
}

// The teams as the server's settings and the dmflags say: on or off, names, sizes, looks worn.
void teams_turn(Q2Match& m) {
  const auto flags = static_cast<int64_t>(number(m, "dmflags"));
  m.teams_on = m.on && (flags & (64 | 128)) != 0;
  m.lobby.settings.teams = m.teams_on;
  gs::TeamsSettings& ts = m.teams.settings;
  for (size_t i = 0; i < 2; i++) {  // a setting changed wins over a captain's name
    const std::string set = text(m, i == 0 ? "g_team_a_name" : "g_team_b_name").substr(0, 15);
    if (set != m.team_names_set[i]) {
      m.team_names_set[i] = set;
      if (!set.empty()) ts.names[i] = set;
    }
  }
  ts.max_per_team = static_cast<int>(number(m, "g_max_players_per_team"));
  ts.allow_pick = number(m, "g_tdm_allow_pick") != 0;
  ts.names_locked = number(m, "g_locked_names") != 0;
  if (!m.teams_on) return;
  gs::teams_tick(m.teams, m.lobby);
  say_book(m, m.teams.said, m.teams.to);
  const std::string skins[2] = {text(m, "g_team_a_skin"), text(m, "g_team_b_skin")};
  for (int i = 0; i < kSlots && i < sh::kMaxClients; i++) {
    const gs::Team team = seat_team(m, i);
    if (team == gs::Team::none) continue;
    const std::string& want = skins[team_index(team)];
    if (want.empty()) continue;
    std::string now;
    for (const auto& [k, v] : (*m.rules)->clients[i].userinfo)
      if (k == "skin") now = v;
    if (now != want) m.rules->user_info(i, "skin", want);
  }
}

// What the teams' book asked: a spectator put on a team, a player taken off his.
void team_moves(Q2Match& m) {
  for (const gs::TeamMove& mv : m.teams.moves) {
    if (mv.slot < 0 || mv.slot >= kSlots) continue;
    const uint32_t bit = 1u << mv.slot;
    sv::Q2Server* game = sv::q2_of(*m.rules);
    if (mv.team == gs::Team::none) {
      m.rules->user_info(mv.slot, "spectator", "1");
      continue;
    }
    m.joining[static_cast<size_t>(mv.slot)] = static_cast<int8_t>(mv.team);
    m.held_out &= ~bit;
    if (game != nullptr) sv::spectate_hold(*game, mv.slot, false);
    m.rules->user_info(mv.slot, "spectator", "0");
  }
  m.teams.moves.clear();
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

// What the referee asked, done; his words said.
void referee_turn(Q2Match& m) {
  say_book(m, m.referee.said, m.referee.to);
  sv::RulesCommon& rules = **m.rules;
  for (const gs::RefereeAct& a : m.referee.acts) {
    switch (a.kind) {
      case gs::RefereeActKind::end_match:
        m.overtime.over = true;  // no overtime after this
        m.rules->console("timelimit " + ge::core::num::to_string(rules.sv.time / 60));
        break;
      case gs::RefereeActKind::time_limit:
        if (m.hold.held)  // the warmup holds the limit: the match gets this one
          m.hold.timelimit = ge::core::num::to_string(a.value);
        else
          m.rules->console("timelimit " + ge::core::num::to_string(a.value));
        break;
      case gs::RefereeActKind::change_map:
        rules.pending_map = a.text;
        rules.pending_change = sv::MapChange::change_level;
        break;
      case gs::RefereeActKind::ban: {
        const ge::net::Address& at = m.net->clients[a.slot].address;
        uint8_t ip[4] = {};
        if (at.family == ge::net::Family::sim)
          ge::net::sim_to_ipv4(at, ip);
        else if (at.family == ge::net::Family::ipv4)
          std::copy(at.bytes, at.bytes + 4, ip);
        if (at.family == ge::net::Family::sim || at.family == ge::net::Family::ipv4) {
          const int64_t now = m.net->cfg.unix_time ? m.net->cfg.unix_time() : 0;
          std::string out;
          gs::ban_command(m.net->bans,
                          "addip " + std::to_string(ip[0]) + "." + std::to_string(ip[1]) + "." +
                              std::to_string(ip[2]) + "." + std::to_string(ip[3]) + " ban +" +
                              std::to_string(static_cast<int64_t>(a.value * 60)),
                          now, out);
        }
        q2_net_drop(*m.net, a.slot, "was banned by the referee");
        break;
      }
      case gs::RefereeActKind::kick:
        q2_net_drop(*m.net, a.slot, "was kicked by the referee");
        break;
      case gs::RefereeActKind::shuffle:
        if (!m.teams_on || m.lobby.phase != gs::LobbyPhase::warmup) {
          say(m, -1, "The teams are shuffled only in a team match's warmup.");
          break;
        }
        gs::teams_shuffle(m.teams, m.lobby,
                          static_cast<uint64_t>(rules.sv.time * 1000) + uint64_t(m.lobby.matches));
        say_book(m, m.teams.said, m.teams.to);
        break;
      case gs::RefereeActKind::ready_all:
        for (int i = 0; i < kSlots && i < gs::kLobbySeats; i++)
          if (m.lobby.seats[static_cast<size_t>(i)].used)
            gs::lobby_ready(m.lobby, i, a.value != 0, rules.sv.time);
        announce_turn(m);
        break;
    }
  }
  m.referee.acts.clear();
}

// The stats words: `stats [player]`, `teamstats`, `laststats [player]` (and `oldstats`).
bool stats_word(Q2Match& m, int slot, const std::string& line) {
  const size_t sp = line.find(' ');
  const std::string word = line.substr(0, sp);
  const std::string rest = sp == std::string::npos ? std::string() : line.substr(sp + 1);
  const bool last = word == "laststats" || word == "oldstats" || word == "lastscores";
  if (word != "stats" && word != "teamstats" && !last) return false;
  const sv::Q2Server* game = sv::q2_of(*m.rules);
  // The warmup shows the last match; so does `laststats`.
  const bool old = last || (m.on && m.lobby.phase != gs::LobbyPhase::match);
  if (old && !m.has_last) {
    say(m, slot, "There are no stats yet: no match has been played.");
    return true;
  }
  if (!old && game == nullptr) return false;
  const auto& table = old ? m.last_stats : game->stats.slot;
  const auto now_names = names(m);
  const auto& who_names = old ? m.last_names : now_names;
  if (word == "teamstats") {
    for (const gs::Team team : {gs::Team::red, gs::Team::blue}) {
      const sv::StatsGame& g = sv::q2_stats_game();
      int64_t kills = 0, deaths = 0, dealt = 0, received = 0;
      for (int i = 0; i < kSlots; i++) {
        if (seat_team(m, i) != team) continue;
        const sv::Q2SlotStats& p = table[static_cast<size_t>(i)];
        kills += p.kills;
        deaths += p.deaths;
        dealt += p.tally.n[static_cast<size_t>(sv::stat_rest(g, sv::kStatDmgGiven))];
        received += p.tally.n[static_cast<size_t>(sv::stat_rest(g, sv::kStatDmgTaken))];
      }
      say(m, slot,
          "Team '" + gs::teams_name(m.teams, team) + "': kills " + std::to_string(kills) +
              ", deaths " + std::to_string(deaths) + ", damage dealt " + std::to_string(dealt) +
              ", received " + std::to_string(received));
    }
    return true;
  }
  int of = slot;
  if (!rest.empty()) {
    of = -1;
    for (int i = 0; i < kSlots; i++)
      if (!who_names[static_cast<size_t>(i)].empty() && who_names[static_cast<size_t>(i)] == rest)
        of = i;
    if (of < 0) {
      say(m, slot, "There is no player " + rest + " in those stats.");
      return true;
    }
  }
  if (!old && m.teams_on && of != slot && seat_team(m, slot) != gs::Team::none &&
      seat_team(m, of) != seat_team(m, slot)) {
    say(m, slot, "The other team's stats are shown once the match is over.");
    return true;
  }
  std::string out;
  const ge::net::MatchReport r = stats_report(m, table, who_names);
  for (const ge::net::ReportPlayer& p : r.players) {
    if (p.name != who_names[static_cast<size_t>(of)]) continue;
    for (const std::string& said : sv::stats_player_lines(sv::q2_stats_game(), r, p))
      out += said + "\n";
    break;
  }
  say(m, slot,
      out.empty() ? "Nothing counted yet for " + who_names[static_cast<size_t>(of)] + "." : out);
  return true;
}

bool hook(void* ctx, int slot, const std::string& line) {
  Q2Match& m = *static_cast<Q2Match*>(ctx);
  if (m.rules != nullptr && slot >= 0 && slot < kSlots && slot < sh::kMaxClients) {
    const Q2NetClient& c = m.net->clients[slot];
    const double t = m.net->now;
    if ((line.starts_with("say ") || line.starts_with("say_team ")) &&
        gs::referee_muted(m.referee, slot, t)) {
      say_book(m, m.referee.said, m.referee.to);
      return true;
    }
    const auto all = names(m);
    const std::string password = text(m, "g_admin_password");
    gs::RefereeCaller ref;
    ref.slot = slot;
    ref.password = password;
    ref.in_match = m.on && m.lobby.phase == gs::LobbyPhase::match;
    ref.names = &all;
    if (gs::referee_command(m.referee, ref, line, t)) {
      referee_turn(m);
      return true;
    }
    if (stats_word(m, slot, line)) return true;
    if (line == "pause" && gs::timeout_paused(m.timeout)) {
      say(m, slot, "The game is in a timeout. Type timein to end yours early.");
      return true;
    }
    gs::TimeoutCaller who;
    who.slot = slot;
    who.name = c.name;
    const gs::Team side = seat_team(m, slot);
    const std::string side_name =
        m.teams_on && side != gs::Team::none ? std::string(gs::team_name(side)) : c.name;
    who.side = side_name;
    who.player = playing(m, slot);
    if (gs::timeout_command(m.timeout, who, in_match(m), timeout_word(m, slot, line), m.net->now)) {
      timeout_turn(m);
      return true;
    }
    if (m.teams_on && c.stage == Q2Slot::active) {
      const gs::TeamsCaller tc{slot, gs::referee_is(m.referee, slot), &all};
      if (gs::teams_command(m.teams, m.lobby, tc, line, rules_time(m))) {
        say_book(m, m.teams.said, m.teams.to);
        announce_turn(m);
        team_moves(m);
        return true;
      }
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
    teams_turn(m);
    if (gs::lobby_due(m.lobby, rules.sv.time) && rules.pending_map.empty()) {
      rules.pending_map = map;  // the match: the map again, every score new
      rules.pending_change = sv::MapChange::restart;
      gs::teams_match_began(m.teams);
    }
    gs::lobby_tick(m.lobby, rules.sv.time);
    std::vector<int> kicked;
    for (const gs::LobbyNote& n : m.lobby.notes)
      if (n.kind == gs::LobbyNoteKind::kicked) kicked.push_back(n.slot);
    announce_turn(m);
    for (int slot : kicked) q2_net_drop(*m.net, slot, "was kicked for not getting ready");
    sv::warmup_hold(m.hold, *m.rules, m.lobby.phase == gs::LobbyPhase::match);
  }
  if (sv::Q2Server* game = sv::q2_of(*m.rules); game != nullptr) {  // the stats count in a match
    game->stats.on = m.on ? m.lobby.phase == gs::LobbyPhase::match : number(m, "deathmatch") != 0;
    sv::stats_tick(*game);
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
  const bool ended = m.on && match_over && m.lobby.phase == gs::LobbyPhase::match;
  if (ended || (!m.on && match_over)) {
    say_result(m);
    if (keep_stats(m)) {  // every player's stats, as each classic game shows them
      const ge::net::MatchReport r = stats_report(m, m.last_stats, m.last_names);
      if (!r.players.empty())
        for (const std::string& line : sv::stats_lines(sv::q2_stats_game(), r)) say(m, -1, line);
    }
  }
  m.frags_seen.fill(0);
  m.team_left.fill(0);
  if (!ended) return;
  gs::teams_match_ended(m.teams);
  const double t = (*m.rules)->sv.time;
  gs::lobby_end(m.lobby, t);
  if (gs::lobby_next_due(m.lobby, t)) gs::lobby_back(m.lobby);
  m.held_out = 0;  // the sidelines open with the warmup
}

std::string q2_match_status(const Q2Match& m) {
  return m.on ? sv::phase_status(m.lobby.phase) : std::string();
}

}  // namespace shooters_edge::net
