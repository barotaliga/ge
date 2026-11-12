// PLAYER AIDS: help a server gives one chosen player (a player with a disability, or one on a
// phone's touch controls) and nobody else: the easy jump, the rocket jump helper, the aim help.
// The same for every game on the engine (docs/servers.md § Player aids); the game says which aids
// it has (`AidKind`, a table) and what each one does.
//
// AN AID IS OFF until it is GRANTED to a player, by a vote of the players (`vote <aid> <player>
// on`, the vote's key `<aid>:<user id>` so two votes on one player are one change) or by the
// server's administrator (`<aid> <player> on|off|<value>` on the server's console or over rcon).
// A grant is DISCLOSED: it sits in the player's info under the aid's server-only key (`*aimhelp`),
// everyone is told when it is given or taken back, the players list shows it, and a player who
// joins is told every grant on the server (`aids_join_line`). A vote's grant lasts the map, as
// every vote's change does (vote.hpp).
//
// A GRANT'S VALUE is text: `kind.on` for `on` (`1`, or the easy jump's share `0.8`), and, for an
// aid whose value is a share (`AidKind::share`), any number 0..1 written at most two decimals.
//
// THE PLAYERS LIST (`users_text`, the `users` command of every game's server): one row a seat, the
// user id, the frags and the name, then the seat's aids: each aid allowed to him (granted, or on
// for everyone by a server setting) with how, and whether he uses it. A game fills the rows
// (`UserRow`); the engine writes the table, so every game's reads alike.
//
// ge:summary Player aids: the kinds a game has, grants by vote or admin, the players list, the
// ge:summary join line
// ge:keywords player-aids aids accessibility disabled-player grant easy-jump aim-help aimhelp
// ge:keywords rocket-jump-helper users players-list user-id aid_parse aid_value users_text
// ge:keywords aids_join_line aid_vote_key aidkind userrow touch-controls phone
// ge:skill net
// ge:test tests/server_aids_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::server {

// One aid a game has.
struct AidKind {
  const char* name;   // the word on the console and in a vote: "aimhelp"
  const char* words;  // what a player reads: "aim help"
  const char* key;    // the server-only user-info key a grant is kept in: "*aimhelp"
  const char* on;     // the value `on` grants: "1", or a share "0.8"
  bool share;         // the value may be a share 0..1 (else only on or off)
};

// What a seat has of one aid, as the game reads it.
struct AidSeat {
  const AidKind* kind = nullptr;
  std::string allowed;   // how it is allowed ("granted 0.8", "on for all"); empty: not allowed
  std::string used;      // how it is used ("cl_speedjump 1", "12 shots helped"); empty: not
  bool granted = false;  // granted to him (by a vote or the administrator), not only on for all
};

// One row of the players list.
struct UserRow {
  int userid = 0;
  int frags = 0;
  std::string name;
  bool spectator = false, bot = false;
  std::vector<AidSeat> aids;  // only the aids allowed or used
};

// The aid called `name` in `kinds`, or null.
const AidKind* aid_find(std::span<const AidKind> kinds, std::string_view name);

// `word` (on, off, or a share for a share aid) as a grant's value: "" for off. False with `error`
// (a player's sentence) when it is none of them.
bool aid_value(const AidKind& kind, std::string_view word, std::string& value, std::string& error);

// A share as a grant shows it (`0.8`, `1`): at most two decimals, no trailing zeros.
std::string aid_share_text(float share);
// `text` read whole as a share 0..1, rounded to hundredths; false when it is not one.
bool aid_parse_share(std::string_view text, float& out);

// An order to grant: `<aid> [<id|name> [on|off|<value>]]`. `who` empty: list the aid's grants.
struct AidOrder {
  const AidKind* kind = nullptr;
  std::string who;
  std::string value;  // the grant's value, "" for off
};
// The line as an order (false: its first word is no aid's). `error` set (and true returned) when
// the line is an aid's but wrong: the words are the player's.
bool aid_parse(std::span<const AidKind> kinds, std::string_view line, AidOrder& out,
               std::string& error);

// The vote's key for one aid of one player.
std::string aid_vote_key(const AidKind& kind, int userid);
// A vote's value (`Ann on`, `Ann 0.5`, `Ann off`) split into the player and the grant's value.
bool aid_vote_split(const AidKind& kind, std::string_view text, std::string& who,
                    std::string& value, std::string& error);
// A grant's value in a vote's words: `on`, `off`, or the share.
std::string aid_vote_word(const AidKind& kind, std::string_view value);

// What one seat has, short: `aim help (granted), easy jump 0.8 (granted, cl_speedjump 1)`; "-"
// for none.
std::string aids_column(const UserRow& row);
// The players list: `userid frags name` and each seat's aids, then `<n> total users`.
std::string users_text(std::span<const UserRow> rows);
// The line a player who joins is told: every aid granted on the server, by player, with `detail`
// after each aid's words when the game gives one (`aim help (24 across, 48 up and down)`). Empty
// when nobody has an aid.
std::string aids_join_line(std::span<const UserRow> rows,
                           std::string (*detail)(const AidKind& kind, const void* ctx) = nullptr,
                           const void* ctx = nullptr);

}  // namespace ge::server
