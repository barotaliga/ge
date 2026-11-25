// PICKING TEAMS IN TURN: two captains choose the other players one at a time by the number each
// was given. The same for every team game on the engine (docs/servers.md § Elections and picks);
// the game names the captains and the pool, applies each pick (the player's team) and says the
// lines.
//
// THE START (`draft_start`): every player in the pool gets a number, 1, 2, 3... in slot order,
// and the captain who picks first is drawn from the seed (one in two). THE ORDER: the first
// captain picks once, the second twice, then one each: A, B, B, A, B, A...
// A PICK (`draft_pick`): only the captain whose turn it is, of a number still in the pool. When
// one player is left in the pool he goes to the captain whose turn it is at once, and the draft is
// over. A player who joins gets the lowest free number (`draft_join`); one who leaves is taken
// out (`draft_leave`), and the draft is over when the pool empties. A captain who leaves (or
// resigns) ends it: `aborted`, the numbers cleared.
//
// A pure function of the acts in order and the seed: no clock.
//
// ge:summary Two captains pick the players in turn by number: A, B, B, A; the last one given
// ge:keywords draft pick-order captain-picks team-picking numbered-players draft_pick draft_start
// ge:skill net
// ge:test tests/server_draft_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ge::server {

constexpr int kDraftSeats = 32;

struct Draft {
  bool active = false;
  bool aborted = false;                      // the last one ended because a captain left
  std::array<int, 2> captain{-1, -1};        // the first and the second captain's slots
  std::array<int8_t, kDraftSeats> number{};  // a slot's number in the pool (0: not in it)
  int first = 0;                             // which captain picks first (0 or 1)
  int picks = 0;                             // picks made
};

// One pick made: the slot and the captain (0 or 1) whose team he joins.
struct DraftPick {
  int slot = -1;
  int captain = -1;
};

enum class DraftRefusal { none, not_captain, not_your_turn, no_such_player };

// Starts it with the two captains and the pool (the captains are never in it).
void draft_start(Draft& d, int captain_a, int captain_b, const std::vector<int>& pool,
                 uint64_t seed);
// The captain (0 or 1) whose turn it is (-1: no draft).
int draft_turn(const Draft& d);
// `slot` picks `number`. The picks made (the chosen one, then the last one given) go to `out`.
DraftRefusal draft_pick(Draft& d, int slot, int number, std::vector<DraftPick>& out);
// A slot joins the pool: its number (0: no draft).
int draft_join(Draft& d, int slot);
// A slot leaves: a captain ends the draft (aborted); a player in the pool is taken out.
void draft_leave(Draft& d, int slot);
// How many are still in the pool.
int draft_left(const Draft& d);

}  // namespace ge::server
