// The account service's friends ops, defined (friends_service.hpp).
//
// ge:summary The friends ops defined: the ticket checked, the asks run, the pages written and read
// ge:keywords friends-ops friends_run friends_page_read invites_read presence_state_from
// ge:skill net
// ge:test tests/account_service_test.cpp
#include "account/friends_service.hpp"

#include <algorithm>

#include "account/service.hpp"
#include "account/ticket.hpp"
#include "net/wire.hpp"

namespace ge::account {

namespace {

void put_text(std::vector<uint8_t>& out, std::string_view s) {
  const size_t n = std::min<size_t>(s.size(), 255);
  out.push_back(static_cast<uint8_t>(n));
  out.insert(out.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(n));
}

std::string get_text(net::Reader& r) {
  const uint8_t n = net::read_u8(r);
  const uint8_t* p = net::read_view(r, n);
  return p ? std::string(reinterpret_cast<const char*>(p), n) : std::string();
}

Answer refuse(Refusal r, std::string words) {
  Answer a;
  a.refusal = r;
  a.words = std::move(words);
  return a;
}

std::string name_of(const Service& svc, uint64_t id) {
  const Account* a = find(svc.store, id);
  return a && a->removed_s == 0 ? a->name : std::string();
}

void put_row(std::vector<uint8_t>& row, FriendRowKind kind, const std::string& name,
             const Presence& p) {
  row.push_back(static_cast<uint8_t>(kind));
  put_text(row, name);
  row.push_back(static_cast<uint8_t>(p.state));
  put_text(row, p.game);
  put_text(row, p.where);
  put_text(row, p.words);
}

}  // namespace

bool presence_state_from(std::string_view name, PresenceState& out) {
  for (const PresenceState s : {PresenceState::offline, PresenceState::online, PresenceState::lobby,
                                PresenceState::playing}) {
    if (name == presence_state_name(s)) {
      out = s;
      return true;
    }
  }
  return false;
}

Answer friends_run(Service& svc, Op op, const std::vector<std::string>& f, int64_t now_s,
                   std::vector<uint8_t>& out) {
  out.clear();
  const size_t want = op == Op::friend_    ? 3
                      : op == Op::friends  ? 2
                      : op == Op::presence ? 5
                      : op == Op::invite   ? 5
                                           : 1;
  if (f.size() != want) return refuse(Refusal::unknown, refusal_words(Refusal::unknown, 0));
  Ticket t;
  const auto* tb = reinterpret_cast<const uint8_t*>(f[0].data());
  const Account* me = nullptr;
  if (ticket_read(tb, f[0].size(), svc.keys.pub, now_s, t) == TicketCheck::ok)
    me = find(svc.store, t.account);
  if (me == nullptr || me->removed_s != 0)
    return refuse(Refusal::wrong, "That sign-in has run out. Sign in again.");
  if (me->blocked) return refuse(Refusal::blocked, refusal_words(Refusal::blocked, 0));
  Friends& fr = svc.friends;
  Answer a;
  a.id = me->id;
  switch (op) {
    case Op::friend_: {
      const Account* other = find_by_name(svc.store, f[2]);
      if (other == nullptr || other->removed_s != 0)
        return refuse(Refusal::unknown, "There is no player called " + f[2] + ".");
      FriendAnswer r = FriendAnswer::not_friends;
      if (f[1] == "ask")
        r = friend_ask(fr, me->id, other->id);
      else if (f[1] == "drop")
        r = friend_drop(fr, me->id, other->id);
      else if (f[1] == "block")
        r = friend_block(fr, me->id, other->id);
      else if (f[1] == "unblock")
        r = friend_unblock(fr, me->id, other->id);
      else
        return refuse(Refusal::unknown, refusal_words(Refusal::unknown, 0));
      a.id = other->id;
      a.words = friend_answer_words(r, other->name);
      a.code = friend_answer_name(r);
      return a;
    }
    case Op::friends: {
      size_t page = 0;
      for (const char c : f[1])
        page = c >= '0' && c <= '9' ? page * 10 + static_cast<size_t>(c - '0') : page;
      struct Entry {
        FriendRowKind kind;
        std::string name;
        uint64_t id;
      };
      std::vector<Entry> all;
      for (const uint64_t id : friends_of(fr, me->id))
        if (std::string n = name_of(svc, id); !n.empty())
          all.push_back({FriendRowKind::friend_, n, id});
      std::sort(all.begin(), all.end(),
                [](const Entry& x, const Entry& y) { return x.name < y.name; });
      for (const uint64_t id : asks_to(fr, me->id))
        if (std::string n = name_of(svc, id); !n.empty())
          all.push_back({FriendRowKind::asks_you, n, id});
      for (const auto& [asker, asked] : fr.asks)
        if (asker == me->id)
          if (std::string n = name_of(svc, asked); !n.empty())
            all.push_back({FriendRowKind::you_asked, n, asked});
      // Pages by bytes: walk from the start so a page is always the same rows.
      std::vector<std::vector<uint8_t>> pages(1);
      std::vector<uint8_t> counts(1, 0);
      for (const Entry& e : all) {
        std::vector<uint8_t> row;
        const Presence p =
            e.kind == FriendRowKind::friend_ ? presence_of(fr, e.id, now_s, me->id) : Presence{};
        put_row(row, e.kind, e.name, p);
        if (pages.back().size() + row.size() > kFriendsPageBytes - 2 || counts.back() == 255) {
          pages.emplace_back();
          counts.push_back(0);
        }
        pages.back().insert(pages.back().end(), row.begin(), row.end());
        counts.back()++;
      }
      if (page >= pages.size()) page = pages.size() - 1;
      out.push_back(page + 1 < pages.size() ? 1 : 0);
      out.push_back(counts[page]);
      out.insert(out.end(), pages[page].begin(), pages[page].end());
      return a;
    }
    case Op::presence: {
      Presence p;
      if (!presence_state_from(f[1], p.state))
        return refuse(Refusal::unknown, refusal_words(Refusal::unknown, 0));
      p.game = f[2];
      p.where = f[3];
      p.words = f[4];
      presence_set(fr, me->id, std::move(p), now_s);
      return a;
    }
    case Op::invite: {
      const Account* other = find_by_name(svc.store, f[1]);
      if (other == nullptr || other->removed_s != 0)
        return refuse(Refusal::unknown, "There is no player called " + f[1] + ".");
      if (!are_friends(fr, me->id, other->id))
        return refuse(Refusal::unknown, "You can only invite your friends.");
      invite_send(fr, me->id, other->id, {0, f[2], f[3], f[4], 0}, now_s);
      a.id = other->id;
      a.words = "Invite sent to " + other->name + ".";
      return a;
    }
    default: {
      std::vector<Invite> got = invites_take(fr, me->id, now_s);
      std::vector<uint8_t> body;
      uint8_t n = 0;
      for (const Invite& i : got) {
        const std::string from = name_of(svc, i.from);
        if (from.empty()) continue;
        std::vector<uint8_t> row;
        put_text(row, from);
        put_text(row, i.game);
        put_text(row, i.where);
        put_text(row, i.note);
        const auto age = static_cast<uint32_t>(std::max<int64_t>(0, now_s - i.sent_s));
        for (int k = 0; k < 4; k++) row.push_back(static_cast<uint8_t>(age >> (8 * k)));
        if (body.size() + row.size() > kFriendsPageBytes - 1) break;
        body.insert(body.end(), row.begin(), row.end());
        n++;
      }
      out.push_back(n);
      out.insert(out.end(), body.begin(), body.end());
      return a;
    }
  }
}

bool friends_page_read(const std::vector<uint8_t>& bytes, std::vector<FriendRow>& rows,
                       bool& more) {
  rows.clear();
  net::Reader r = net::make_reader(bytes.data(), bytes.size());
  more = net::read_u8(r) != 0;
  const uint8_t n = net::read_u8(r);
  for (uint8_t i = 0; i < n && !r.bad; i++) {
    FriendRow row;
    const uint8_t kind = net::read_u8(r);
    row.kind = static_cast<FriendRowKind>(kind);
    row.name = get_text(r);
    const uint8_t state = net::read_u8(r);
    row.presence.state = static_cast<PresenceState>(state);
    row.presence.game = get_text(r);
    row.presence.where = get_text(r);
    row.presence.words = get_text(r);
    if (kind < 1 || kind > 3 || state > 3) return false;
    rows.push_back(std::move(row));
  }
  return !r.bad && r.at == bytes.size();
}

bool invites_read(const std::vector<uint8_t>& bytes, std::vector<InviteRow>& invites) {
  invites.clear();
  net::Reader r = net::make_reader(bytes.data(), bytes.size());
  const uint8_t n = net::read_u8(r);
  for (uint8_t i = 0; i < n && !r.bad; i++) {
    InviteRow row;
    row.from = get_text(r);
    row.game = get_text(r);
    row.where = get_text(r);
    row.note = get_text(r);
    uint32_t age = 0;
    for (int k = 0; k < 4; k++) age |= static_cast<uint32_t>(net::read_u8(r)) << (8 * k);
    row.age_s = age;
    invites.push_back(std::move(row));
  }
  return !r.bad && r.at == bytes.size();
}

}  // namespace ge::account
