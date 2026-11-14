// A PLAYER'S OWN CLIENT ASKING THE MATCHMAKER FOR A RANKED GAME (docs/accounts.md § The
// matchmaker): with the ticket the account file holds (client_login.hpp loads it), the client
// asks to join the queue for a game and mode, and asks again every kQueueAskEvery_us (each ask
// keeps its place and reads the state) until the matchmaker answers with a game: the server's
// address and the sides' names. A refusal (no such duo, a ticket run out) ends it with the
// master's words; so does leaving.
//
// Nothing here blocks or reads a clock: the caller passes the time, the socket and the fresh
// secrets.
//
//   client_queue_start(q, login, game, mode, duo, fresh, ctx)
//   client_queue_update(q, transport, now_us)   each frame; true once, when the game is found
//   client_queue_leave(q, transport, now_us)    out of the queue, sent once
//
// ge:summary Our own client asking the matchmaker for a ranked game until one is found
// ge:keywords client-queue ranked-queue matchmaker find-game ranked
// ge:skill net
// ge:test tests/account_client_queue_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "account/client_login.hpp"
#include "account/matchmaker_service.hpp"
#include "account/service.hpp"

namespace ge::account {

constexpr int64_t kQueueAskEvery_us = 3000000;

enum class QueueStep : uint8_t { idle, waiting, found, refused, left };

struct ClientQueue {
  net::Address master;
  net::Key key{};
  std::vector<uint8_t> ticket;
  std::string game, mode, duo;
  FreshFn fresh = nullptr;
  void* fresh_ctx = nullptr;
  Ask ask;
  bool asking = false;
  int64_t due_us = 0;
  QueueStep step = QueueStep::idle;
  QueueView view;     // the last state read
  std::string words;  // the master's last words (the wait, the game found, a refusal)
};

void client_queue_start(ClientQueue& q, const ClientLogin& login, std::string game,
                        std::string mode, std::string duo, FreshFn fresh, void* fresh_ctx);
// True once: the frame the game is found (`view.address`, `view.theirs`).
bool client_queue_update(ClientQueue& q, net::Transport& t, int64_t now_us);
void client_queue_leave(ClientQueue& q, net::Transport& t, int64_t now_us);

}  // namespace ge::account
