// Our own client's presence and invites (account/client_presence.hpp).
//
// ge:summary Our own client's presence kept fresh on the master and its invites read in turn
// ge:keywords client-presence presence-refresh invites-poll
// ge:skill net
// ge:test tests/account_client_presence_test.cpp
#include "account/client_presence.hpp"

#include "net/token.hpp"

namespace ge::account {

namespace {

bool same(const Presence& a, const Presence& b) {
  return a.state == b.state && a.game == b.game && a.where == b.where && a.words == b.words;
}

net::Key fresh_key(const ClientPresence& p) {
  net::Key k{};
  if (p.fresh != nullptr)
    p.fresh(p.fresh_ctx, k.data(), k.size());
  else
    net::os_random_bytes(k.data(), k.size());
  return k;
}

std::string ticket_text(const ClientPresence& p) {
  return std::string(p.ticket.begin(), p.ticket.end());
}

void ask_presence(ClientPresence& p, int64_t now_us) {
  ask_start(
      p.ask, Op::presence, p.master, p.key,
      {ticket_text(p), presence_state_name(p.want.state), p.want.game, p.want.where, p.want.words},
      fresh_key(p), now_us);
  p.asking = PresenceAsk::presence;
}

}  // namespace

void client_presence_start(ClientPresence& p, const ClientLogin& login, FreshFn fresh,
                           void* fresh_ctx) {
  p = ClientPresence{};
  p.master = login.master;
  p.key = login.key;
  p.ticket = login.ticket;
  p.fresh = fresh;
  p.fresh_ctx = fresh_ctx;
  p.want.state = PresenceState::online;
  p.started = true;
}

void client_presence_set(ClientPresence& p, PresenceState state, std::string game,
                         std::string where, std::string words) {
  p.want.state = state;
  p.want.game = std::move(game);
  p.want.where = std::move(where);
  p.want.words = std::move(words);
}

bool client_presence_update(ClientPresence& p, net::Transport& t, int64_t now_us) {
  if (!p.started) return false;
  bool added = false;
  if (p.asking != PresenceAsk::none) {
    ask_update(p.ask, t, now_us);
    uint8_t buffer[net::kMaxDatagram];
    net::Address from;
    for (;;) {
      const int n = net::transport_receive(t, buffer, sizeof buffer, from, now_us);
      if (n < 0) break;
      if (n > 0) ask_read(p.ask, buffer, static_cast<size_t>(n), from);
    }
    if (!p.ask.done) return false;
    const Reply& r = p.ask.reply;
    const bool ok = p.ask.answered && r.refusal == Refusal::none;
    if (p.ask.answered && r.refusal != Refusal::none) p.words = r.words;
    if (p.asking == PresenceAsk::presence) {
      if (ok) {
        p.sent = p.want;
        p.told = true;
      }
      p.presence_due_us = now_us + (ok ? kPresenceEvery_us : kInvitesEvery_us);
    } else {
      std::vector<InviteRow> rows;
      if (ok && invites_read(r.ticket, rows)) {
        for (InviteRow& i : rows) p.got.push_back(std::move(i));
        added = !rows.empty();
      }
      p.invites_due_us = now_us + kInvitesEvery_us;
    }
    p.asking = PresenceAsk::none;
  }
  if (!p.words.empty()) return added;  // refused (the ticket ran out): asked no more
  // Presence first: when it changed since it was told, or when its refresh (or retry) is due.
  if (now_us >= p.presence_due_us || (p.told && !same(p.want, p.sent))) {
    ask_presence(p, now_us);
    ask_update(p.ask, t, now_us);
    return added;
  }
  if (now_us >= p.invites_due_us) {
    ask_start(p.ask, Op::invites, p.master, p.key, {ticket_text(p)}, fresh_key(p), now_us);
    p.asking = PresenceAsk::invites;
    ask_update(p.ask, t, now_us);
  }
  return added;
}

void client_presence_leave(ClientPresence& p, net::Transport& t, int64_t now_us) {
  if (!p.started || !p.told) return;
  p.want = Presence{};
  ask_presence(p, now_us);
  ask_update(p.ask, t, now_us);
  p.asking = PresenceAsk::none;
  p.started = false;
}

}  // namespace ge::account
