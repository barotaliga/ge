// OUR QUAKE II SERVER'S MATCH: the lobby before it (warmup, ready, the countdown), timeouts in it,
// overtime at its end, on the rules every classic server shares (server/match_rules.hpp,
// engine/server/match.hpp, timeout.hpp, overtime.hpp). QuakeWorld's server plays the same match.
//
// THE LOBBY (`sv_lobby 1`): every map warms up first. A player on the map is seated in the lobby
// (a spectator is not); he says `ready` (`notready`, `unready`, `noready` or `break` takes it
// back), and once every seated player is ready, with two at least, the countdown of `sv_countdown`
// runs (the lobby's own lines, "Match Settings" with the match's limits first) and the map restarts
// as the match, scores new. While it warms up the rules' `fraglimit` and `timelimit` are held at 0
// (server/match_rules.hpp, WarmupHold) and the match gets them back. When the match ends the next
// map warms up again.
//
// PREWAR (`sv_prewar`): fire held, and the jump (Quake II's up move) for the unready under 2, by
// the move hook of the net server (q2_net_server.hpp's `move_hook`).
//
// THE LOCK (`sv_lockjoin`): a newcomer to a match closed to him is kept on the sidelines (the
// rules' `held_out`, q2_spectate.hpp) and told why; the next warmup lets him go.
//
// TIMEOUTS (`sv_timeouts`, `sv_timeoutlength`, `sv_timeoutresume`): `timeout` and `timein`, and the
// words Quake II's match players type for them: `time`, `calltime`, `ctime` and `hold` call one or,
// from the side that called it, end it. The rules pause while one is on. A side is the player.
//
// OVERTIME (`sv_overtime`, `sv_overtime_minutes`): a match tied at its time limit plays on.
//
// TEAMS (`dmflags` 64 or 128 with `sv_lobby`): the lobby's two teams (engine/server/captains.hpp),
// named by `g_team_a_name` and `g_team_b_name`, each player wearing his team's look
// (`g_team_a_skin`, `g_team_b_skin`), at most `g_max_players_per_team` a team. `team` and `join`
// move a player (`1`, `a`, `2`, `b` or a team's name); captains lock, rename, invite, pick
// (`g_tdm_allow_pick`) and remove. The teams' scores are their players' frags, those who left
// included; overtime compares the teams, and the match's end says who won by how much.
//
// COACHES (engine/server/coach.hpp): a spectator elected by the players (`coach <team>`, `yes`)
// coaches one team: its team chat both ways and his chase camera kept on its players.
//
// THE REFEREE (engine/server/referee.hpp): `referee` (or `admin`) and `g_admin_password` let a
// player end the match, set the time limit, change the map, kick, ban, mute and shuffle the teams
// without a vote. A muted player's chat is held back.
//
// STATS (server/q2_stats.hpp): counted by the rules while the match is on; `stats [player]` shows
// the table (another team's only once the match is over), `teamstats` both teams' totals, and
// `laststats` (`oldstats`) the last match's.
//
// THE PHASE is in the rules' local info (`ge_phase`) for the rules to read (the warmup's loadout,
// q2_server.hpp) and in the server's `status` text as `match` (Standby, Countdown, In progress).
//
// ge:summary Our Quake II server's match: lobby, ready, countdown, timeouts, overtime, prewar, lock
// ge:keywords q2-match q2_match lobby ready notready unready countdown warmup timeout timein
// ge:keywords calltime hold overtime sudden-death sv_lobby sv_prewar sv_lockjoin match-phase
// ge:keywords tdm team-match captains referee g_admin_password g_team_a_name g_team_a_skin
// ge:keywords g_max_players_per_team g_tdm_allow_pick stats teamstats laststats team-score
// ge:keywords coach q2-coach
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_match_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "server/captains.hpp"
#include "server/coach.hpp"
#include "server/match.hpp"
#include "server/referee.hpp"
#include "server/server.hpp"
#include "server/timeout.hpp"
#include "server/vote.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/match_rules.hpp"
#include "shooters_edge/server/q2_stats.hpp"

namespace shooters_edge::net {

struct Q2Match {  // NOLINT(clang-analyzer-optin.performance.Padding): one a server, by name
  Q2NetServer* net = nullptr;
  sv::RulesRunner* rules = nullptr;
  bool on = false;  // sv_lobby
  ge::server::Lobby lobby;
  sv::Announcer announcer;
  ge::server::TimeoutBook timeout;
  bool timeout_held = false;
  sv::OvertimeRun overtime;
  sv::WarmupHold hold;
  std::array<int, 32> seat_id{};       // the user id seated at a slot (0 none)
  std::array<double, 32> held_told{};  // when a held fire was last told
  std::array<double, 32> died_seen{};  // the death last counted, by the rules' died_at
  std::array<int64_t, 32> deaths{};    // deaths counted this match, a slot
  int64_t deaths_left = 0;             // of players who left the match
  uint32_t held_out = 0;               // the slots kept on the sidelines (sv_lockjoin)
  std::string map;                     // the map the countdown restarts
  ge::server::Teams teams;
  ge::server::Coaches coaches;
  const ge::server::VoteBook* votes = nullptr;  // the players' votes (no coach election beside one)
  ge::server::Referee referee;
  bool teams_on = false;                         // a team match (`dmflags` 64 or 128)
  std::array<std::string, 2> team_names_set;     // the settings' names last read
  std::array<int8_t, 32> joining{};              // the team an invite or a pick seats a slot on
  std::array<int64_t, 32> frags_seen{};          // a seat's frags at the last frame
  std::array<int64_t, 2> team_left{};            // the frags of players who left each team
  std::array<sv::Q2SlotStats, 32> last_stats{};  // the last match's (`laststats`)
  std::array<std::string, 32> last_names{};
  bool has_last = false;
  bool (*prior)(void* ctx, int slot, const std::string& line) = nullptr;
  void* prior_ctx = nullptr;
};

// The match on `net`'s rules, by the server's settings (`sv_lobby` and the rest); its words hooked
// ahead of the ones already there (the votes'), and its moves' hook set.
void q2_match_open(Q2Match& m, Q2NetServer& net, sv::RulesRunner& rules,
                   const ge::server::ServerConfig& c);
// After a rules frame: the seats kept to the clients, the lobby and the timeout ticked and said,
// overtime, the warmup's hold, and the countdown's restart asked of the rules. `map` the map on.
void q2_match_frame(Q2Match& m, const std::string& map, double now);
// A map change about to happen: the match over unless it is a restart.
void q2_match_map_change(Q2Match& m, bool restart, double now);
// The phase as the server's status says it ("" with no lobby).
std::string q2_match_status(const Q2Match& m);

}  // namespace shooters_edge::net
