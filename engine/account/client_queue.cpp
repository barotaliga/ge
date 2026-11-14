// Our own client asking the matchmaker (account/client_queue.hpp).
//
// ge:summary Our own client asking the matchmaker for a ranked game until one is found
// ge:keywords client-queue ranked-queue
// ge:skill net
// ge:test tests/account_client_queue_test.cpp
#include "account/client_queue.hpp"

#include "net/token.hpp"

namespace ge::account {

namespace {

void ask_queue(ClientQueue& q, const char* verb, int64_t now_us) {
  net::Key k{};
  if (q.fresh != nullptr)
    q.fresh(q.fresh_ctx, k.data(), k.size());
  else
    net::os_random_bytes(k.data(), k.size());
  ask_start(q.ask, Op::queue, q.master, q.key,
            {std::string(q.ticket.begin(), q.ticket.end()), verb, q.game, q.mode, q.duo}, k,
            now_us);
  q.asking = true;
}

}  // namespace

void client_queue_start(ClientQueue& q, const ClientLogin& login, std::string game,
                        std::string mode, std::string duo, FreshFn fresh, void* fresh_ctx) {
  q = ClientQueue{};
  q.master = login.master;
  q.key = login.key;
  q.ticket = login.ticket;
  q.game = std::move(game);
  q.mode = std::move(mode);
  q.duo = std::move(duo);
  q.fresh = fresh;
  q.fresh_ctx = fresh_ctx;
  q.step = QueueStep::waiting;
}

bool client_queue_update(ClientQueue& q, net::Transport& t, int64_t now_us) {
  if (q.step != QueueStep::waiting) return false;
  if (q.asking) {
    ask_update(q.ask, t, now_us);
    uint8_t buffer[net::kMaxDatagram];
    net::Address from;
    for (;;) {
      const int n = net::transport_receive(t, buffer, sizeof buffer, from, now_us);
      if (n < 0) break;
      if (n > 0) ask_read(q.ask, buffer, static_cast<size_t>(n), from);
    }
    if (!q.ask.done) return false;
    q.asking = false;
    q.due_us = now_us + kQueueAskEvery_us;
    if (!q.ask.answered) return false;  // asked again at its turn
    const Reply& r = q.ask.reply;
    q.words = r.words;
    if (r.refusal != Refusal::none) {
      q.step = QueueStep::refused;
      return false;
    }
    if (queue_read(r.ticket, q.view) && q.view.state == LookState::matched) {
      q.step = QueueStep::found;
      return true;
    }
    return false;
  }
  if (now_us >= q.due_us) {
    ask_queue(q, "join", now_us);
    ask_update(q.ask, t, now_us);
  }
  return false;
}

void client_queue_leave(ClientQueue& q, net::Transport& t, int64_t now_us) {
  if (q.step != QueueStep::waiting) return;
  ask_queue(q, "leave", now_us);
  ask_update(q.ask, t, now_us);
  q.asking = false;
  q.step = QueueStep::left;
}

}  // namespace ge::account
