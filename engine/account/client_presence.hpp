// A PLAYER'S OWN CLIENT KEEPING ITS PRESENCE AND READING ITS INVITES (docs/accounts.md § Friends):
// with the ticket the account file holds (client_login.hpp loads it), the client tells the master
// where its player is (online, in a lobby, playing; the game, the server's address, the game's
// words) whenever that changes and again every kPresenceEvery_us, so a crashed client lapses to
// offline on the master's clock, and asks for the invites waiting every kInvitesEvery_us. One ask
// is in flight at a time, presence first; an ask the master does not answer is dropped and tried
// again at its next turn. Invites read are handed to the caller once (`got`), oldest first.
//
// Nothing here blocks or reads a clock: the caller passes the time, the socket and the fresh
// secrets (each ask's exchange secret).
//
//   client_presence_start(p, login, fresh, ctx)          a loaded ClientLogin's master and ticket
//   client_presence_set(p, state, game, where, words)    what friends see (sent at the next update)
//   client_presence_update(p, transport, now_us)         each frame; `got` holds new invites
//   client_presence_leave(p, transport, now_us)          offline, sent once, on leaving
//
// ge:summary Our own client's presence kept fresh on the master and its invites read in turn
// ge:keywords client-presence presence-refresh invites-poll friends invite offline-on-leave
// ge:skill net
// ge:test tests/account_client_presence_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "account/client_login.hpp"
#include "account/friends.hpp"
#include "account/friends_service.hpp"
#include "account/service.hpp"

namespace ge::account {

constexpr int64_t kPresenceEvery_us = 60000000;  // well inside kPresenceLife_s
constexpr int64_t kInvitesEvery_us = 15000000;

enum class PresenceAsk : uint8_t { none, presence, invites };

struct ClientPresence {
  net::Address master;
  net::Key key{};
  std::vector<uint8_t> ticket;
  Presence want, sent;  // what the player is; what the master was last told (set_s unused)
  bool told = false;    // `sent` reached the master at least once
  int64_t presence_due_us = 0, invites_due_us = 0;
  Ask ask;
  PresenceAsk asking = PresenceAsk::none;
  FreshFn fresh = nullptr;
  void* fresh_ctx = nullptr;
  std::vector<InviteRow> got;  // invites read since the caller last cleared it
  std::string words;           // the master's last refusal, in a player's words ("" none)
  bool started = false;
};

// Bound to a loaded login's master, key and ticket; `fresh` (nullptr: the OS's) makes each ask's
// exchange secret.
void client_presence_start(ClientPresence& p, const ClientLogin& login, FreshFn fresh,
                           void* fresh_ctx);
// What friends see; a change is sent at the next update.
void client_presence_set(ClientPresence& p, PresenceState state, std::string game,
                         std::string where, std::string words);
// Sends what is due and reads what came on `t`. True when an invite was added to `got`.
bool client_presence_update(ClientPresence& p, net::Transport& t, int64_t now_us);
// Offline, sent once and not waited for (the master's lapse covers a lost one).
void client_presence_leave(ClientPresence& p, net::Transport& t, int64_t now_us);

}  // namespace ge::account
