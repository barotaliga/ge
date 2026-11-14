// FRIENDS, PRESENCE AND INVITES (docs/accounts.md § Friends): what the account service keeps
// between accounts, beside the store. Every game shares one list, since an account is every
// game's.
//
// FRIENDS are made the way duos are: both ask. A asks B; while B has not asked A back, the ask
// waits (B sees it among the asks to them); when B asks A, they are friends. Either drops the
// other at any time. A BLOCK is one-sided and silent: B blocks A, and A's asks to B, invites to B
// and look at B's presence all answer as if B simply had not answered (A is never told), and a
// friendship or an ask between them is dropped. Lines of the friends' log (`friend ask a b`,
// `friend drop a b`, `friend block a b`, `friend unblock a b`) are the record, read back in order
// by `friends_line`, so the state is the log's.
//
// PRESENCE is where a player is right now: online, in a lobby, in a game, with the game's id, the
// server's address and the game's own words (the map, the mode). The client sets it each time it
// changes and again every minute; one not set again for kPresenceLife_s lapses to offline, so a
// client that crashed goes offline on its own. It is no record: never logged.
//
// AN INVITE is a friend's ask to join them where they are: the game, the server's address and a
// note. It waits for the friend kInviteLife_s, at most kInvitesKept to one player (the oldest
// dropped), and goes when the friend reads or answers it. Never logged.
//
//   friend_ask(f, a, b) / friend_drop(f, a, b) / friend_block(f, a, b) / friend_unblock(f, a, b)
//   friends_of(f, a), asks_to(f, a), blocked(f, a, b)
//   presence_set(f, a, p, now_s), presence_of(f, a, now_s)
//   invite_send(f, from, to, invite, now_s), invites_take(f, to, now_s)
//
// ge:summary Friends between accounts: asks both ways, silent blocks, presence and invites
// ge:keywords friends friend-ask block presence invite online lobby in-game lapse friends_line
// ge:keywords friend_ask friend_drop friend_block presence_set invite_send invites_take
// ge:skill net
// ge:test tests/account_friends_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::account {

constexpr int64_t kPresenceLife_s = 150;  // a presence not set again for this long: offline
constexpr int64_t kInviteLife_s = 600;
constexpr size_t kInvitesKept = 8;     // to one player
constexpr size_t kFriendsKept = 200;   // an account's friends, and the asks waiting on one
constexpr size_t kPresenceWords = 64;  // a presence's words and an invite's note, cut there

enum class FriendAnswer : uint8_t {
  asked,        // the ask waits for the other's ask (also what a blocked asker is told)
  made,         // both asked: friends
  already,      // already friends
  dropped,      // the friendship, or the ask, is gone
  not_friends,  // nothing to drop
  blocked,      // the block is set (the friendship and asks between them dropped)
  unblocked,
  self,  // an account is not its own friend
  full,  // past kFriendsKept
};
const char* friend_answer_name(FriendAnswer a);
// What the player reads ("You are now friends with Ann."): `other` is the friend's name.
std::string friend_answer_words(FriendAnswer a, std::string_view other);

enum class PresenceState : uint8_t { offline = 0, online = 1, lobby = 2, playing = 3 };
const char* presence_state_name(PresenceState s);

struct Presence {
  PresenceState state = PresenceState::offline;
  std::string game;   // the game's id ("shooters_edge"): "" none
  std::string where;  // the server's address, "" when not on one
  std::string words;  // the game's own (the map and the mode), at most kPresenceWords
  int64_t set_s = 0;  // when it was last set
};

struct Invite {
  uint64_t from = 0;
  std::string game, where, note;  // note at most kPresenceWords
  int64_t sent_s = 0;
};

struct Friends {
  std::set<std::pair<uint64_t, uint64_t>> pairs;   // friends, (smaller id, larger)
  std::set<std::pair<uint64_t, uint64_t>> asks;    // (asker, asked), waiting
  std::set<std::pair<uint64_t, uint64_t>> blocks;  // (blocker, blocked)
  std::map<uint64_t, Presence> presence;
  std::map<uint64_t, std::vector<Invite>> invites;  // by the one invited, oldest first
  std::string log;                                  // the lines since the master last wrote them
};

FriendAnswer friend_ask(Friends& f, uint64_t a, uint64_t b);
FriendAnswer friend_drop(Friends& f, uint64_t a, uint64_t b);
FriendAnswer friend_block(Friends& f, uint64_t a, uint64_t b);
FriendAnswer friend_unblock(Friends& f, uint64_t a, uint64_t b);
bool are_friends(const Friends& f, uint64_t a, uint64_t b);
// Whether `a` blocks `b`.
bool blocks(const Friends& f, uint64_t a, uint64_t b);
std::vector<uint64_t> friends_of(const Friends& f, uint64_t a);
// The accounts whose asks wait on `a` (none from one `a` blocks).
std::vector<uint64_t> asks_to(const Friends& f, uint64_t a);

// A's presence set at `now_s` (its words cut to kPresenceWords).
void presence_set(Friends& f, uint64_t a, Presence p, int64_t now_s);
// A's presence as `viewer` sees it: offline when it lapsed, when A has none, or when A blocks the
// viewer. `viewer` 0: the service's own look.
Presence presence_of(const Friends& f, uint64_t a, int64_t now_s, uint64_t viewer = 0);

// An invite from `from` to `to`: false (nothing kept) unless they are friends and `to` does not
// block `from`; the sender is told the same either way, so a block stays silent.
bool invite_send(Friends& f, uint64_t from, uint64_t to, Invite invite, int64_t now_s);
// `to`'s invites still waiting at `now_s`, oldest first, taken (they are gone after).
std::vector<Invite> invites_take(Friends& f, uint64_t to, int64_t now_s);

// A line of the friends' log read back (`friend ask|drop|block|unblock a b`); false with `error`.
// A line not starting `friend ` is not one: false with `error` empty.
bool friends_line(Friends& f, std::string_view line, std::string& error);

}  // namespace ge::account
