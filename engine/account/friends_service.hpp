// THE FRIENDS' OPS on the account service (service.hpp; docs/accounts.md § Friends): our client
// asks them with the ticket it holds from signing in, never the password, so a game keeps its
// friends list working for the ticket's life. Each ask's first field is the ticket's bytes; the
// service answers an ask whose ticket it did not sign, or that ran out, with "sign in again".
//
//   friend    ticket, verb (ask, drop, block, unblock), the other's name   -> the words, their id
//   friends   ticket, page (from 0)       -> a page of the list in the answer's bytes
//   presence  ticket, state (offline, online, lobby, playing), game, where, words   -> ok
//   invite    ticket, the friend's name, game, where, note   -> the words
//   invites   ticket                      -> the invites waiting, taken, in the answer's bytes
//
// THE ANSWER'S BYTES (the slot a sign-in's ticket travels in; little endian, a text is a length
// u8 and its bytes):
//   friends page   more u8 (1: ask for the next page), count u8, then per row: kind u8 (1 a
//                  friend, 2 an ask waiting on you, 3 one you sent), name, state u8, game, where,
//                  words
//   invites        count u8, then per invite: from's name, game, where, note, age in seconds u32
// A page holds what fits kFriendsPageBytes, so no answer outgrows its ask (service.hpp): the rows
// are the friends by name, then the asks.
//
//   friends_run(svc, op, fields, now_s, out_bytes)   on the service, for the ops above
//   friends_page_read(bytes, rows, more) / invites_read(bytes, invites)   on the asker's side
//
// ge:summary The account service's friends ops: asks, the list in pages, presence, invites
// ge:keywords friends-ops friend presence invite friends-page ticket friends_run friends_page_read
// ge:skill net
// ge:test tests/account_service_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "account/friends.hpp"
#include "account/store.hpp"

namespace ge::account {

struct Service;
enum class Op : uint8_t;

constexpr size_t kFriendsPageBytes = 320;

enum class FriendRowKind : uint8_t { friend_ = 1, asks_you = 2, you_asked = 3 };

struct FriendRow {
  FriendRowKind kind = FriendRowKind::friend_;
  std::string name;
  Presence presence;  // its set_s unused
};

struct InviteRow {
  std::string from, game, where, note;
  int64_t age_s = 0;
};

// One of the friends' ops answered; the list ops' bytes in `out`.
Answer friends_run(Service& svc, Op op, const std::vector<std::string>& fields, int64_t now_s,
                   std::vector<uint8_t>& out);
// A presence state's name read back; false when it is none of them.
bool presence_state_from(std::string_view name, PresenceState& out);

bool friends_page_read(const std::vector<uint8_t>& bytes, std::vector<FriendRow>& rows, bool& more);
bool invites_read(const std::vector<uint8_t>& bytes, std::vector<InviteRow>& invites);

}  // namespace ge::account
