// QUAKEWORLD'S ADMIN IN THE GAME: the engine's referee (engine/server/referee.hpp) in QuakeWorld's
// own words, and the few acts QuakeWorld's admin has that the engine's book does not.
//
// BECOMING ONE: `admin <code>` with the server's `k_admincode` (an exact match; empty or `none`:
// nobody can); `k_admins 0`: "NO admins on this server!". A wrong code: "Access denied..." to him
// alone (and the server's log), then "Wait N second(s)!" for 5 seconds. The right one: "<name>
// gains admins status!" to everyone. `admin` again gives it up ("<name> is no longer an admin").
// It lasts across map changes and goes when the player leaves (a reconnect logs in again). A
// player who is not one reads "You are not an admin".
//
// THE WORDS (each the engine's act; a player named by user id, then by his exact name):
// - `forcebreak`: the match ends now ("<name> forces a break!"; nothing outside a match).
// - `forcemap <map>`: to that map now ("<name> forces a map change to <map>!"); refused during a
//   lobby's match (without sv_lobby there is no time between matches: always taken), and for a
//   map the server does not have.
// - `kick <id|name> [reason]`: "<victim> was kicked by <admin>", the reason after it, the victim
//   told "You were kicked from the server" and his address kept out for 30 seconds. An admin is
//   not kicked.
// - `forcestart` (with sv_lobby, in the warmup): "<name> forces matchstart!", a 10-second count
//   on everyone's screen ("N second(s) left before game starts", "Get ready!" to the unready),
//   then every player still not ready is dropped ("<name> was kicked by admin forcestart"; he
//   reads "Bye bye! Pay attention next time.") and the match's countdown begins. It is called off
//   when the admin who gave it is no longer ready ("<name> interrupts countdown").
// - `lock`: nobody joins as a player for 15 seconds ("<name> locked server for 15 seconds"; a
//   joining player reads "server is temporary locked: N second(s)" and watches instead); `lock`
//   again opens it ("<name> unlocked server"), and so does the time ("server unlocked").
// - `acommands`: the words.
// The engine's other words (`tl`, `ban`, `mute`, `shuffle`, `readyall`) are not QuakeWorld's and
// are left to the mod. A program of the server's own runs these; a mod's program (a `.dat`, a
// module, a game library) keeps its own admin, and these words go to it untouched.
//
// ge:summary QuakeWorld's admin in the game on the engine's referee: k_admincode, forcebreak,
// ge:summary forcemap, kick with a reason, forcestart and the server lock
// ge:keywords qw-referee qw-admin k_admins k_admincode admin-code forcestart forcebreak forcemap
// ge:keywords kick-reason server-lock lock qw_referee_line qw_referee_frame qw_referee_lock
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_referee_test.cpp
#pragma once

#include <array>
#include <string>
#include <string_view>

#include "server/referee.hpp"
#include "server/server.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/match_rules.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

struct QwServer;

struct QwReferee {
  ge::server::Referee book;
  std::array<int, ge::server::kLobbySeats> ids{};  // each referee's user id when he became one
  double force_ends = 0;                           // forcestart's count runs out (0: none)
  int force_by = -1;                               // the admin who gave it
  int force_shown = -1;                            // the whole seconds last shown
  double locked_until = 0;                         // `lock`: joins refused until then (0: open)
};

// What one call needs of the game. `lobby` and `announcer` null: no lobby (sv_lobby 0).
struct QwRefereeGame {
  QwServer* qw = nullptr;
  shooters_edge::server::RulesRunner* rules = nullptr;
  ge::server::Lobby* lobby = nullptr;
  shooters_edge::server::Announcer* announcer = nullptr;
  shooters_edge::server::OvertimeRun* overtime = nullptr;
  const ge::server::ServerConfig* settings = nullptr;  // k_admins (unset: 1) and k_admincode
  bool in_match = false;
  double now = 0;  // the server's clock, s
  bool (*map_known)(const void* ctx, std::string_view map) = nullptr;
  const void* map_ctx = nullptr;
};

// QuakeWorld's words for the engine's book.
const ge::server::RefereeWords& qw_referee_words();
// A client's line: true when it was the admin's (said and done).
bool qw_referee_line(QwReferee& r, const QwRefereeGame& g, int slot, const std::string& line);
// Once a frame: a referee who left forgotten, forcestart's count, the lock's end.
void qw_referee_frame(QwReferee& r, const QwRefereeGame& g);
// The line a joining player reads while `lock` holds ("": open).
std::string qw_referee_lock(const QwReferee& r, double now);

}  // namespace shooters_edge::net
