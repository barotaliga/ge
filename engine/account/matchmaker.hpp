// THE MATCHMAKER (docs/accounts.md § The matchmaker): players who want a ranked game ask the
// master to find them one, and it pairs them by their ladder rating (ladder.hpp) on a server that
// has offered its seats. Every game it makes is a rated one: a duel between two accounts, or a
// two-on-two between two registered duos, on the ladder of the game and mode asked for.
//
// THE QUEUE holds one entry per account: a player alone for a duel, or a duo (either member asks,
// naming the duo, and both are queued) for two-on-two. An entry's rating is its all-time rating on
// that ladder (kRatingStart when it has none). It waits at most kQueueLife_s; asking again keeps
// its place, so a client asks every few seconds and a crashed one drops out on its own.
//
// THE WINDOW: two entries are paired only when their ratings are within both their windows. A
// window starts at kWindowStart and widens by kWindowStep every kWindowEvery_s waited, up to
// kWindowMost, so a strong player waits for a fair game first and still gets one. The oldest entry
// is paired first, with the nearest rating in its window (the older on a tie).
//
// SERVERS offer their seats for a game and a mode (`matchmaker_offer`, from trusted servers only:
// a matched game is rated, so it must be played where reports count). An offer lasts
// kOfferLife_s and is made again by the server while it has room. A pair is made only when an
// offer has the seats; the seats it takes are held until the server offers again. With no server
// free the pair stays queued.
//
// A MATCH made is kept kMatchKept_s for its players to read: the server's address, the mode and
// the other side's names (the caller names them). Leaving the queue drops a waiting entry; a match
// already made is the player's to join or not.
//
//   matchmaker_join(mm, ladder, account, game, mode, duo, now_s) / matchmaker_leave(mm, account)
//   matchmaker_offer(mm, server, game, mode, address, seats, now_s)
//   matchmaker_pair(mm, now_s)       pairs what it can; the new matches' ids returned
//   matchmaker_look(mm, account, now_s)   waiting (with the window), matched, or not queued
//
// ge:summary The matchmaker: a queue paired by ladder rating in a widening window on offered seats
// ge:keywords matchmaker matchmaking queue rating-window ranked-queue offer seats duel two-on-two
// ge:keywords matchmaker_join matchmaker_pair matchmaker_offer matchmaker_look
// ge:skill net
// ge:test tests/account_matchmaker_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "account/ladder.hpp"
#include "net/crypto.hpp"

namespace ge::account {

constexpr int64_t kQueueLife_s = 30;    // an entry not asked for again this long is dropped
constexpr int64_t kWindowStart = 1000;  // tenths of a point: 100
constexpr int64_t kWindowStep = 500;
constexpr int64_t kWindowEvery_s = 10;
constexpr int64_t kWindowMost = 6000;
constexpr int64_t kOfferLife_s = 60;
constexpr int64_t kMatchKept_s = 120;
constexpr size_t kQueueMost = 4096;

enum class QueueAnswer : uint8_t {
  queued,    // waiting (or still waiting)
  no_duo,    // a two-on-two asked for with a duo the asker is not in
  bad_mode,  // a mode that is neither a duel nor a two-on-two
  full       // past kQueueMost
};
const char* queue_answer_words(QueueAnswer a);

struct QueueEntry {
  uint64_t account = 0;  // who asked
  uint64_t partner = 0;  // the duo's other member (two-on-two), 0 for a duel
  uint64_t duo = 0;      // the duo's id (two-on-two)
  std::string game, mode;
  int64_t rating = kRatingStart;
  int64_t since_s = 0, asked_s = 0;
};

struct Offer {
  net::Key server{};
  std::string game, mode, address;
  int seats = 0;  // free, less what the matchmaker has taken since the offer
  int64_t made_s = 0;
};

struct MadeMatch {
  uint64_t id = 0;
  std::string game, mode, address;
  std::vector<uint64_t> side_a, side_b;  // accounts
  int64_t made_s = 0;
  int64_t rating_a = 0, rating_b = 0;  // the sides' ratings when paired
};

struct Matchmaker {
  std::vector<QueueEntry> queue;  // oldest first
  std::vector<Offer> offers;
  std::vector<MadeMatch> made;
  uint64_t next_id = 1;
  uint64_t paired = 0;  // matches ever made
};

// Whether a mode is played two-on-two ("2on2", "2v2", "tdm2") rather than one-on-one.
bool mode_is_pairs(std::string_view mode);
// The window an entry waiting since `since_s` has at `now_s`, in tenths.
int64_t queue_window(int64_t since_s, int64_t now_s);

// `account` queued (or kept queued) for `game` and `mode`; `duo` names its duo for two-on-two.
QueueAnswer matchmaker_join(Matchmaker& mm, const Ladder& ladder, uint64_t account,
                            std::string_view game, std::string_view mode, std::string_view duo,
                            int64_t now_s);
// Out of the queue (both of a duo); false when it was not queued.
bool matchmaker_leave(Matchmaker& mm, uint64_t account);
// A server's free seats for a game and mode (its last offer replaced).
void matchmaker_offer(Matchmaker& mm, const net::Key& server, std::string_view game,
                      std::string_view mode, std::string_view address, int seats, int64_t now_s);
// Pairs what it can at `now_s` (and drops what lapsed); the new matches, in the order made.
std::vector<uint64_t> matchmaker_pair(Matchmaker& mm, int64_t now_s);

enum class LookState : uint8_t { none, waiting, matched };
struct Look {
  LookState state = LookState::none;
  int64_t waited_s = 0, window = 0;  // waiting
  const MadeMatch* match = nullptr;  // matched
};
Look matchmaker_look(const Matchmaker& mm, uint64_t account, int64_t now_s);

}  // namespace ge::account
