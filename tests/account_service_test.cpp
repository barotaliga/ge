// Tests: the account service (engine/account/service.hpp) on the simulated network: the key
// asked for, a sign-up, a sign-in's ticket checked offline, a code made and a game server's login
// ask spending it once, an ask lost and sent again answered the same (the account made once), a
// replayed ask useless to anyone else, an ask sealed to another key refused, no answer bigger
// than its ask.
//
// ge:summary Tests: the account service's sealed asks and answers on the simulated network
// ge:keywords test account service ask answer ticket code login replay resend sealed key
// ge:skill net testing
#include "account/service.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "net/sim_network.hpp"

namespace ga = ge::account;
namespace gn = ge::net;

namespace {

constexpr int64_t kT0 = 1794500000ll * 1000000;
const gn::Address kServiceAt = gn::sim_address(1, ga::kAccountPort);
const gn::Address kClientAt = gn::sim_address(2, 5000);
const gn::Address kServerAt = gn::sim_address(3, 27500);

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

gn::Key key_of(uint8_t b) {
  gn::Key k{};
  for (size_t i = 0; i < k.size(); i++) k[i] = static_cast<uint8_t>(size_t{b} * 7 + i);
  return k;
}

struct World {
  gn::SimNetwork net = gn::make_sim_network(5, gn::LinkConditions{});
  gn::Transport service_port, client, server;
  ga::Service svc;
  uint64_t fresh_n = 1;
  int64_t now = kT0;
  size_t biggest_answer = 0;

  World() {
    gn::open_sim_transport(service_port, net, kServiceAt);
    gn::open_sim_transport(client, net, kClientAt);
    gn::open_sim_transport(server, net, kServerAt);
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::ServiceConfig c;
    c.seed = key_of(1);
    ga::service_init(svc, c, std::move(store), service_port, counter_fresh, &fresh_n);
  }

  // Run an ask from `port` until it is done.
  ga::Ask ask(gn::Transport& port, ga::Op op, const std::vector<std::string>& fields,
              uint8_t fresh) {
    ga::Ask a;
    ga::ask_start(a, op, kServiceAt, ga::service_key(svc), fields, key_of(fresh), now);
    for (int i = 0; i < 100 && !a.done; i++) {
      ga::ask_update(a, port, now);
      now += 20000;
      gn::sim_advance(net, now);
      ga::service_update(svc, now);
      gn::sim_advance(net, now);
      uint8_t buf[gn::kMaxDatagram];
      gn::Address from;
      int n = 0;
      while ((n = gn::transport_receive(port, buf, sizeof buf, from, now)) >= 0) {
        if (static_cast<size_t>(n) > biggest_answer) biggest_answer = static_cast<size_t>(n);
        ga::ask_read(a, buf, static_cast<size_t>(n), from);
      }
    }
    return a;
  }
};

}  // namespace

TEST(service_key_is_asked_and_matches) {
  World w;
  ga::KeyAsk k;
  ga::key_ask_start(k, kServiceAt, w.now);
  for (int i = 0; i < 20 && !k.done; i++) {
    ga::key_ask_update(k, w.client, w.now);
    w.now += 20000;
    gn::sim_advance(w.net, w.now);
    ga::service_update(w.svc, w.now);
    gn::sim_advance(w.net, w.now);
    uint8_t buf[gn::kMaxDatagram];
    gn::Address from;
    int n = 0;
    while ((n = gn::transport_receive(w.client, buf, sizeof buf, from, w.now)) >= 0) {
      ga::key_ask_read(k, buf, static_cast<size_t>(n), from);
    }
  }
  CHECK(k.answered && k.key == ga::service_key(w.svc));
}

TEST(service_signs_up_signs_in_and_logs_a_classic_client_in_by_code) {
  World w;
  const ga::Ask made = w.ask(w.client, ga::Op::create, {"Niclas", "correct horse"}, 10);
  CHECK(made.answered && made.reply.refusal == ga::Refusal::none && made.reply.account == 1);
  CHECK(made.reply.code.size() == 19);  // the recovery code
  CHECK(w.ask(w.client, ga::Op::create, {"n1clas", "another one"}, 11).reply.refusal ==
        ga::Refusal::name_taken);

  const ga::Ask in = w.ask(w.client, ga::Op::sign_in, {"niclas", "correct horse"}, 12);
  CHECK(in.answered && in.reply.refusal == ga::Refusal::none && !in.reply.ticket.empty());
  ga::Ticket t;
  CHECK(ga::ticket_read(in.reply.ticket.data(), in.reply.ticket.size(), ga::service_key(w.svc),
                        w.now / 1000000, t) == ga::TicketCheck::ok);
  CHECK(t.account == 1 && t.name == "Niclas");

  const ga::Ask code = w.ask(w.client, ga::Op::code, {"Niclas", "correct horse"}, 13);
  CHECK(code.reply.refusal == ga::Refusal::none && code.reply.code.size() == 9);
  const ga::Ask login =
      w.ask(w.server, ga::Op::login, {"Niclas", code.reply.code, "10.0.0.7:27001"}, 14);
  CHECK(login.reply.refusal == ga::Refusal::none && login.reply.account == 1);
  CHECK(ga::ticket_read(login.reply.ticket.data(), login.reply.ticket.size(),
                        ga::service_key(w.svc), w.now / 1000000, t) == ga::TicketCheck::ok);
  // The code is spent: the same line again (a new ask) logs nobody in.
  const ga::Ask again =
      w.ask(w.server, ga::Op::login, {"Niclas", code.reply.code, "10.0.0.9:27001"}, 15);
  CHECK(again.reply.refusal == ga::Refusal::wrong && again.reply.ticket.empty());
  // The password is never a classic login.
  CHECK(w.ask(w.server, ga::Op::login, {"Niclas", "correct horse", "10.0.0.8:27001"}, 16)
            .reply.refusal != ga::Refusal::none);
  CHECK(w.biggest_answer <= ga::kAccountAskSize);
}

TEST(service_answers_an_ask_sent_again_the_same_and_makes_the_account_once) {
  World w;
  gn::LinkConditions drop;
  drop.loss = 1.0;
  gn::sim_set_link(w.net, 1, 2, drop);  // every answer to the client lost
  ga::Ask a;
  ga::ask_start(a, ga::Op::create, kServiceAt, ga::service_key(w.svc), {"Rocket", "long enough"},
                key_of(20), w.now);
  for (int i = 0; i < 3; i++) {
    ga::ask_update(a, w.client, w.now);
    w.now += a.ask_us;
    gn::sim_advance(w.net, w.now);
    ga::service_update(w.svc, w.now);
  }
  CHECK(w.svc.stats.asks == 1 && w.svc.stats.resent == 2);
  CHECK(w.svc.store.accounts.size() == 1);
  // Someone who caught the ask sends it from elsewhere: the same answer, which only the asker
  // opens; no second account.
  ga::Ask thief;
  ga::ask_start(thief, ga::Op::create, kServiceAt, ga::service_key(w.svc),
                {"Rocket", "long enough"}, key_of(21), w.now);
  thief.datagram = a.datagram;
  w.ask(w.server, ga::Op::sign_in, {"x", "y"}, 22);  // let the clock run
  ga::ask_update(thief, w.server, w.now);
  w.now += 20000;
  gn::sim_advance(w.net, w.now);
  ga::service_update(w.svc, w.now);
  gn::sim_advance(w.net, w.now);
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  int n = gn::transport_receive(w.server, buf, sizeof buf, from, w.now);
  CHECK(n > 0);
  CHECK(!ga::ask_read(thief, buf, static_cast<size_t>(n), from) || !thief.answered);
  CHECK(w.svc.store.accounts.size() == 1);
}

TEST(service_refuses_an_ask_sealed_to_another_key) {
  World w;
  const gn::Key wrong = gn::sign_keys(key_of(99)).pub;
  ga::Ask a;
  ga::ask_start(a, ga::Op::create, kServiceAt, wrong, {"Rocket", "long enough"}, key_of(30), w.now);
  a.max_tries = 2;
  for (int i = 0; i < 40 && !a.done; i++) {
    ga::ask_update(a, w.client, w.now);
    w.now += 50000;
    gn::sim_advance(w.net, w.now);
    ga::service_update(w.svc, w.now);
  }
  CHECK(a.done && !a.answered);
  CHECK(w.svc.stats.refused == 2 && w.svc.store.accounts.empty());
}

TEST(service_trades_its_own_ticket_for_a_login_code) {
  World w;
  CHECK(w.ask(w.client, ga::Op::create, {"Niclas", "correct horse"}, 10).reply.refusal ==
        ga::Refusal::none);
  const ga::Ask in = w.ask(w.client, ga::Op::sign_in, {"niclas", "correct horse"}, 11);
  CHECK(!in.reply.ticket.empty());
  const std::string ticket(in.reply.ticket.begin(), in.reply.ticket.end());
  const ga::Ask code = w.ask(w.client, ga::Op::ticket_code, {ticket}, 12);
  CHECK(code.reply.refusal == ga::Refusal::none && code.reply.account == 1);
  CHECK(code.reply.code.size() == 9);
  // The code logs a classic client in once, as any login code does.
  const ga::Ask login =
      w.ask(w.server, ga::Op::login, {"Niclas", code.reply.code, "10.0.0.7:27001"}, 13);
  CHECK(login.reply.refusal == ga::Refusal::none && login.reply.account == 1);
  // A ticket changed by one byte, or signed by another key, gets nothing.
  std::string bent = ticket;
  bent[bent.size() - 1] = static_cast<char>(bent[bent.size() - 1] ^ 1);
  CHECK(w.ask(w.client, ga::Op::ticket_code, {bent}, 14).reply.refusal == ga::Refusal::wrong);
  ga::Ticket other = ga::ticket_issue(gn::sign_keys(key_of(9)), 1, "Niclas", w.now / 1000000);
  uint8_t buf[ga::kTicketMax];
  const size_t n = ga::ticket_write(other, buf, sizeof buf);
  CHECK(w.ask(w.client, ga::Op::ticket_code, {std::string(reinterpret_cast<char*>(buf), n)}, 15)
            .reply.refusal == ga::Refusal::wrong);
}
