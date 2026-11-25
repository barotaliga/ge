// QUAKEWORLD'S ADMIN IN THE GAME: the engine's referee (engine/server/referee.hpp) in QuakeWorld's
// own words, and the few acts QuakeWorld's admin has that the engine's book does not.
//
// BECOMING ONE: `admin <code>` with the server's `k_admincode` (an exact match; empty or `none`:
// nobody can); `k_admins 0`: "NO admins on this server!". A wrong code: "Access denied..." to him
// alone (and the server's log), then "Wait N second(s)!" for 5 seconds. The right one: "<name>
// gains admins status!" to everyone. `admin` again gives it up ("<name> is no longer an admin").
// It lasts across map changes and goes when the player leaves (a reconnect logs in again). A
// player who is not one reads "You are not an admin". `admin` alone: "Use numbers or impulses to
// enter code", the code typed on the number keys (six digits, "N more to go" after each of the
// first five, `admin` again: "admin code canceled"). ELECTED: `elect` opens the server's one
// election for him (`k_allowvoteadmin`, unset 1; `k_vp_admin` percent, 51; "<name> has requested
// admin rights!", the others' `yes`), and a pass makes him an admin as the code does.
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
// - `mkick <id> [id ...] [reason]`: each user id kicked as `kick` does; the first word that is
//   not a number starts the reason; an id nobody has: "mkick: client <id> not found".
// - `kick` alone: KICK MODE ("Kicking process started"): every other player and spectator in
//   turn, "Kick player <name>?"; `y` kicks him, `n` goes on; `kick` again (with or without a name)
//   leaves it ("Kicking process terminated"), as do the end of the list and giving up admin; 60
//   seconds: "Your kick mode has timed out"; nobody there: "Can't find anybody to kick".
// - The toggles, told to everyone before a match and to the admin alone in one: `lockmap` ("<name>
//   locks map" / "<name> unlocks map"; a player's `vote map` then reads "MAP IS LOCKED!");
//   `exclusive`, outside a match, turns sv_lockjoin between 0 and 1 (a match's own players alone
//   join it once it began; with the lobby only). Each holds across maps.
// - `acommands`: the words.
// The engine's other words (`tl`, `ban`, `mute`, `shuffle`, `readyall`) are not QuakeWorld's and
// are left to the mod. A program of the server's own runs these; a mod's program (a `.dat`, a
// module, a game library) keeps its own admin, and these words go to it untouched.
//
// ge:summary QuakeWorld's admin in the game on the engine's referee: k_admincode, forcebreak,
// ge:summary forcemap, kick with a reason, forcestart and the server lock; captain mode beside it
// ge:keywords qw-referee qw-admin k_admins k_admincode admin-code forcestart forcebreak forcemap
// ge:keywords kick-reason server-lock lock qw_referee_line qw_referee_frame qw_referee_lock
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_referee_test.cpp
#pragma once

#include <array>
#include <string>
#include <string_view>

#include "server/election.hpp"
#include "server/referee.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/qw_captains.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/match_rules.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

struct QwServer;

// What one call needs of the game. `lobby` and `announcer` null: no lobby (sv_lobby 0).
struct QwRefereeGame {
  QwServer* qw = nullptr;
  shooters_edge::server::RulesRunner* rules = nullptr;
  ge::server::Lobby* lobby = nullptr;
  shooters_edge::server::Announcer* announcer = nullptr;
  shooters_edge::server::OvertimeRun* overtime = nullptr;
  ge::server::ServerConfig* settings = nullptr;  // k_admins (unset: 1), k_admincode; toggled
  bool in_match = false;
  double now = 0;  // the server's clock, s
  bool (*map_known)(const void* ctx, std::string_view map) = nullptr;
  const void* map_ctx = nullptr;
};

struct QwReferee {
  ge::server::Referee book;
  std::array<int, ge::server::kLobbySeats> ids{};  // each referee's user id when he became one
  double force_ends = 0;                           // forcestart's count runs out (0: none)
  int force_by = -1;                               // the admin who gave it
  int force_shown = -1;                            // the whole seconds last shown
  double locked_until = 0;                         // `lock`: joins refused until then (0: open)
  ge::server::Election election;  // the server's one election, whatever role (election.hpp)
  QwCaptains captains;            // captain mode (qw_captains.hpp)
  QwRefereeGame last;             // the game as the last frame saw it (the hooks' view)
  uint32_t typing = 0;            // a bit a slot typing the code on the number keys
  std::array<std::string, ge::server::kLobbySeats> digits{};  // the digits typed so far
  int kick_by = -1;                                           // the admin in kick mode (-1: none)
  int kick_at = -1;                                           // the slot he is asked about
  double kick_ends = 0;                                       // kick mode times out
  bool map_locked = false;  // `lockmap`: only an admin\'s map vote is taken
};

// QuakeWorld's words for the engine's book.
const ge::server::RefereeWords& qw_referee_words();
// A client's line: true when it was the admin's (said and done).
bool qw_referee_line(QwReferee& r, const QwRefereeGame& g, int slot, const std::string& line);
// Once a frame: a referee who left forgotten, forcestart's count, the lock's end.
void qw_referee_frame(QwReferee& r, const QwRefereeGame& g);
// The line a joining player reads while `lock` holds ("": open).
std::string qw_referee_lock(const QwReferee& r, double now);
// Captain mode's say over a move's buttons (qw_captains.hpp): no fire while picking.
uint8_t qw_referee_buttons(QwReferee& r, const QwRefereeGame& g, int slot, uint8_t buttons);
// The server's `impulse` and `info_rule` hooks given to the referee, on the game it last saw.
void qw_referee_hooks(QwServer& s, QwReferee& r);

}  // namespace shooters_edge::net
