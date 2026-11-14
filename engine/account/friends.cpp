// Friends, presence and invites, defined (friends.hpp).
//
// ge:summary Friends defined: asks, drops, blocks, the log's lines, presence lapsing, invites kept
// ge:keywords friends friend_ask friend_block presence_of invite_send invites_take friends_line
// ge:skill net
// ge:test tests/account_friends_test.cpp
#include "account/friends.hpp"

#include <algorithm>
#include <cstdlib>

namespace ge::account {

namespace {

std::pair<uint64_t, uint64_t> pair_of(uint64_t a, uint64_t b) {
  return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}

size_t count_of(const Friends& f, uint64_t a) {
  size_t n = 0;
  for (const auto& [x, y] : f.pairs) n += (x == a || y == a) ? 1 : 0;
  for (const auto& [asker, asked] : f.asks) n += (asker == a || asked == a) ? 1 : 0;
  return n;
}

void log_line(Friends& f, const char* verb, uint64_t a, uint64_t b) {
  f.log += std::string("friend ") + verb + " " + std::to_string(a) + " " + std::to_string(b) + "\n";
}

std::string cut(std::string s) {
  if (s.size() > kPresenceWords) s.resize(kPresenceWords);
  return s;
}

}  // namespace

const char* friend_answer_name(FriendAnswer a) {
  switch (a) {
    case FriendAnswer::asked:
      return "asked";
    case FriendAnswer::made:
      return "made";
    case FriendAnswer::already:
      return "already";
    case FriendAnswer::dropped:
      return "dropped";
    case FriendAnswer::not_friends:
      return "not_friends";
    case FriendAnswer::blocked:
      return "blocked";
    case FriendAnswer::unblocked:
      return "unblocked";
    case FriendAnswer::self:
      return "self";
    case FriendAnswer::full:
      return "full";
  }
  return "?";
}

std::string friend_answer_words(FriendAnswer a, std::string_view other) {
  const std::string o(other);
  switch (a) {
    case FriendAnswer::asked:
      return "Friend request sent to " + o + ". You will be friends when they add you too.";
    case FriendAnswer::made:
      return "You are now friends with " + o + ".";
    case FriendAnswer::already:
      return "You are already friends with " + o + ".";
    case FriendAnswer::dropped:
      return o + " is no longer on your friends list.";
    case FriendAnswer::not_friends:
      return o + " is not on your friends list.";
    case FriendAnswer::blocked:
      return o + " is blocked. They cannot ask you, invite you or see where you play.";
    case FriendAnswer::unblocked:
      return o + " is no longer blocked.";
    case FriendAnswer::self:
      return "You cannot add yourself as a friend.";
    case FriendAnswer::full:
      return "Your friends list is full. Remove someone first.";
  }
  return "";
}

const char* presence_state_name(PresenceState s) {
  switch (s) {
    case PresenceState::offline:
      return "offline";
    case PresenceState::online:
      return "online";
    case PresenceState::lobby:
      return "lobby";
    case PresenceState::playing:
      return "playing";
  }
  return "?";
}

FriendAnswer friend_ask(Friends& f, uint64_t a, uint64_t b) {
  if (a == b) return FriendAnswer::self;
  if (f.pairs.count(pair_of(a, b))) return FriendAnswer::already;
  if (blocks(f, b, a)) return FriendAnswer::asked;  // silent: nothing kept, nothing told
  if (f.asks.count({b, a})) {
    if (count_of(f, a) >= kFriendsKept) return FriendAnswer::full;
    f.asks.erase({b, a});
    f.pairs.insert(pair_of(a, b));
    f.blocks.erase({a, b});  // asking someone back ends one's own block on them
    log_line(f, "ask", a, b);
    return FriendAnswer::made;
  }
  if (f.asks.count({a, b})) return FriendAnswer::asked;
  if (count_of(f, a) >= kFriendsKept || count_of(f, b) >= kFriendsKept) return FriendAnswer::full;
  f.asks.insert({a, b});
  f.blocks.erase({a, b});
  log_line(f, "ask", a, b);
  return FriendAnswer::asked;
}

FriendAnswer friend_drop(Friends& f, uint64_t a, uint64_t b) {
  const bool was = f.pairs.erase(pair_of(a, b)) + f.asks.erase({a, b}) + f.asks.erase({b, a}) > 0;
  if (!was) return FriendAnswer::not_friends;
  log_line(f, "drop", a, b);
  return FriendAnswer::dropped;
}

FriendAnswer friend_block(Friends& f, uint64_t a, uint64_t b) {
  if (a == b) return FriendAnswer::self;
  f.pairs.erase(pair_of(a, b));
  f.asks.erase({a, b});
  f.asks.erase({b, a});
  f.blocks.insert({a, b});
  auto it = f.invites.find(a);  // what the blocked one sent and is still waiting goes too
  if (it != f.invites.end())
    std::erase_if(it->second, [&](const Invite& i) { return i.from == b; });
  log_line(f, "block", a, b);
  return FriendAnswer::blocked;
}

FriendAnswer friend_unblock(Friends& f, uint64_t a, uint64_t b) {
  if (!f.blocks.erase({a, b})) return FriendAnswer::not_friends;
  log_line(f, "unblock", a, b);
  return FriendAnswer::unblocked;
}

bool are_friends(const Friends& f, uint64_t a, uint64_t b) {
  return f.pairs.count(pair_of(a, b)) > 0;
}

bool blocks(const Friends& f, uint64_t a, uint64_t b) { return f.blocks.count({a, b}) > 0; }

std::vector<uint64_t> friends_of(const Friends& f, uint64_t a) {
  std::vector<uint64_t> v;
  for (const auto& [x, y] : f.pairs) {
    if (x == a) v.push_back(y);
    if (y == a) v.push_back(x);
  }
  return v;
}

std::vector<uint64_t> asks_to(const Friends& f, uint64_t a) {
  std::vector<uint64_t> v;
  for (const auto& [asker, asked] : f.asks)
    if (asked == a && !blocks(f, a, asker)) v.push_back(asker);
  return v;
}

void presence_set(Friends& f, uint64_t a, Presence p, int64_t now_s) {
  p.game = cut(std::move(p.game));
  p.where = cut(std::move(p.where));
  p.words = cut(std::move(p.words));
  p.set_s = now_s;
  if (p.state == PresenceState::offline) {
    f.presence.erase(a);
    return;
  }
  f.presence[a] = std::move(p);
}

Presence presence_of(const Friends& f, uint64_t a, int64_t now_s, uint64_t viewer) {
  if (viewer != 0 && blocks(f, a, viewer)) return {};
  const auto it = f.presence.find(a);
  if (it == f.presence.end() || now_s - it->second.set_s > kPresenceLife_s) return {};
  return it->second;
}

bool invite_send(Friends& f, uint64_t from, uint64_t to, Invite invite, int64_t now_s) {
  if (!are_friends(f, from, to) || blocks(f, to, from)) return false;
  invite.from = from;
  invite.game = cut(std::move(invite.game));
  invite.where = cut(std::move(invite.where));
  invite.note = cut(std::move(invite.note));
  invite.sent_s = now_s;
  std::vector<Invite>& v = f.invites[to];
  std::erase_if(
      v, [&](const Invite& i) { return i.from == from || now_s - i.sent_s > kInviteLife_s; });
  v.push_back(std::move(invite));
  while (v.size() > kInvitesKept) v.erase(v.begin());
  return true;
}

std::vector<Invite> invites_take(Friends& f, uint64_t to, int64_t now_s) {
  std::vector<Invite> out;
  const auto it = f.invites.find(to);
  if (it == f.invites.end()) return out;
  for (Invite& i : it->second)
    if (now_s - i.sent_s <= kInviteLife_s) out.push_back(std::move(i));
  f.invites.erase(it);
  return out;
}

bool friends_line(Friends& f, std::string_view line, std::string& error) {
  error.clear();
  if (line.rfind("friend ", 0) != 0) return false;
  const std::string text(line.substr(7));
  const size_t s1 = text.find(' ');
  const size_t s2 = s1 == std::string::npos ? s1 : text.find(' ', s1 + 1);
  if (s2 == std::string::npos) {
    error = "a friend line needs a verb and two account ids: \"" + std::string(line) + "\"";
    return false;
  }
  const std::string verb = text.substr(0, s1);
  char* end = nullptr;
  const uint64_t a = std::strtoull(text.c_str() + s1 + 1, &end, 10);
  const uint64_t b = std::strtoull(text.c_str() + s2 + 1, &end, 10);
  if (a == 0 || b == 0 || *end != '\0') {
    error = "a friend line's account ids do not read: \"" + std::string(line) + "\"";
    return false;
  }
  const std::string before = f.log;
  if (verb == "ask")
    friend_ask(f, a, b);
  else if (verb == "drop")
    friend_drop(f, a, b);
  else if (verb == "block")
    friend_block(f, a, b);
  else if (verb == "unblock")
    friend_unblock(f, a, b);
  else {
    error =
        "a friend line's verb is not ask, drop, block or unblock: \"" + std::string(line) + "\"";
    return false;
  }
  f.log = before;  // a line read back is not written again
  return true;
}

}  // namespace ge::account
