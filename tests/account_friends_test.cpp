// engine/account/friends.hpp: friends made when both ask, dropped by either; a block silent to the
// one blocked (their asks, invites and look at the presence answered as if unanswered); the log's
// lines read back to the same state; presence lapsing; invites only between friends, kept a while
// and taken once; the player's words read like a game's.
//
// ge:summary Test: friends between accounts: asks, blocks, the log, presence and invites
// ge:keywords test friends friend-ask block presence invite friends_line
// ge:skill net testing
#include "account/friends.hpp"

#include <string>

#include "check.hpp"

namespace ga = ge::account;

namespace {

constexpr int64_t kNow = 1800000000;

}  // namespace

TEST(friends_are_made_when_both_ask_and_dropped_by_either) {
  ga::Friends f;
  CHECK(ga::friend_ask(f, 1, 1) == ga::FriendAnswer::self);
  CHECK(ga::friend_ask(f, 1, 2) == ga::FriendAnswer::asked);
  CHECK(ga::friend_ask(f, 1, 2) == ga::FriendAnswer::asked);  // asked again: still waiting
  CHECK(ga::asks_to(f, 2) == std::vector<uint64_t>{1} && ga::asks_to(f, 1).empty());
  CHECK(!ga::are_friends(f, 1, 2));
  CHECK(ga::friend_ask(f, 2, 1) == ga::FriendAnswer::made);
  CHECK(ga::are_friends(f, 1, 2) && ga::asks_to(f, 2).empty());
  CHECK(ga::friend_ask(f, 1, 2) == ga::FriendAnswer::already);
  CHECK(ga::friends_of(f, 2) == std::vector<uint64_t>{1});
  CHECK(ga::friend_drop(f, 2, 1) == ga::FriendAnswer::dropped && !ga::are_friends(f, 1, 2));
  CHECK(ga::friend_drop(f, 2, 1) == ga::FriendAnswer::not_friends);
}

TEST(friends_a_block_is_silent_to_the_one_blocked) {
  ga::Friends f;
  ga::friend_ask(f, 1, 2);
  ga::friend_ask(f, 2, 1);
  ga::presence_set(
      f, 2, {ga::PresenceState::playing, "shooters_edge", "203.0.113.4:27500", "dm4 duel", 0},
      kNow);
  CHECK(ga::presence_of(f, 2, kNow, 1).state == ga::PresenceState::playing);
  CHECK(ga::friend_block(f, 2, 1) == ga::FriendAnswer::blocked);
  CHECK(!ga::are_friends(f, 1, 2) && ga::blocks(f, 2, 1));
  // The one blocked is told what anyone unanswered is told, and nothing is kept.
  CHECK(ga::friend_ask(f, 1, 2) == ga::FriendAnswer::asked && ga::asks_to(f, 2).empty());
  CHECK(ga::presence_of(f, 2, kNow, 1).state == ga::PresenceState::offline);
  CHECK(ga::presence_of(f, 2, kNow, 3).state == ga::PresenceState::playing);  // others still see
  CHECK(!ga::invite_send(f, 1, 2, {0, "shooters_edge", "203.0.113.9:27500", "come", 0}, kNow));
  CHECK(ga::friend_unblock(f, 2, 1) == ga::FriendAnswer::unblocked);
  CHECK(ga::friend_unblock(f, 2, 1) == ga::FriendAnswer::not_friends);
  CHECK(ga::friend_ask(f, 1, 2) == ga::FriendAnswer::asked && ga::asks_to(f, 2).size() == 1);
}

TEST(friends_the_log_reads_back_to_the_same_state) {
  ga::Friends f;
  ga::friend_ask(f, 1, 2);
  ga::friend_ask(f, 2, 1);
  ga::friend_ask(f, 3, 1);
  ga::friend_ask(f, 4, 1);
  ga::friend_block(f, 1, 4);
  ga::friend_drop(f, 9, 8);  // nothing to drop: no line
  ga::Friends back;
  std::string error;
  size_t at = 0;
  int lines = 0;
  while (at < f.log.size()) {
    const size_t end = f.log.find('\n', at);
    CHECK_MSG(ga::friends_line(back, f.log.substr(at, end - at), error), error);
    at = end + 1;
    lines++;
  }
  CHECK(lines == 5);
  CHECK(back.pairs == f.pairs && back.asks == f.asks && back.blocks == f.blocks);
  CHECK(back.log.empty());
  CHECK(!ga::friends_line(back, "friend wave 1 2", error) && !error.empty());
  CHECK(!ga::friends_line(back, "friend ask 1", error) && !error.empty());
  CHECK(!ga::friends_line(back, "duo 1 2 6e", error) && error.empty());  // not a friend line
}

TEST(friends_presence_lapses_and_invites_are_taken_once) {
  ga::Friends f;
  ga::presence_set(
      f, 1, {ga::PresenceState::lobby, "fall_line", "198.51.100.2:27600", std::string(100, 'x'), 0},
      kNow);
  CHECK(ga::presence_of(f, 1, kNow + 60).words.size() == ga::kPresenceWords);
  CHECK(ga::presence_of(f, 1, kNow + ga::kPresenceLife_s + 1).state == ga::PresenceState::offline);
  ga::presence_set(f, 1, {ga::PresenceState::offline, "", "", "", 0}, kNow);
  CHECK(f.presence.empty());
  // Invites: only between friends, one per sender, kept a while, taken once.
  CHECK(!ga::invite_send(f, 1, 2, {0, "fall_line", "198.51.100.2:27600", "ride?", 0}, kNow));
  ga::friend_ask(f, 1, 2);
  ga::friend_ask(f, 2, 1);
  CHECK(ga::invite_send(f, 1, 2, {0, "fall_line", "198.51.100.2:27600", "ride?", 0}, kNow));
  CHECK(ga::invite_send(f, 1, 2, {0, "fall_line", "198.51.100.2:27600", "now?", 0}, kNow + 5));
  std::vector<ga::Invite> got = ga::invites_take(f, 2, kNow + 10);
  CHECK(got.size() == 1 && got[0].from == 1 && got[0].note == "now?");
  CHECK(ga::invites_take(f, 2, kNow + 10).empty());
  ga::invite_send(f, 1, 2, {0, "fall_line", "x", "late", 0}, kNow);
  CHECK(ga::invites_take(f, 2, kNow + ga::kInviteLife_s + 1).empty());
}

TEST(friends_words_read_like_a_game) {
  for (int i = 0; i <= static_cast<int>(ga::FriendAnswer::full); i++) {
    const std::string w = ga::friend_answer_words(static_cast<ga::FriendAnswer>(i), "Ann");
    CHECK_MSG(!w.empty() && w.find("next:") == std::string::npos &&
                  w.find('`') == std::string::npos && w.back() == '.',
              w);
  }
  CHECK(ga::friend_answer_words(ga::FriendAnswer::made, "Ann") == "You are now friends with Ann.");
}
