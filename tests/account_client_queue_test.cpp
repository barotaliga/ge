// Tests: our own client asking the matchmaker (engine/account/client_queue.hpp): two signed-in
// clients queue for a ranked duel, keep their places by asking again, and both learn the game the
// matchmaker found on an offered server; a duo the asker is not in is refused in a player's words;
// leaving takes the client out of the queue.
//
// ge:summary Tests: our client queued for a ranked game until found, refused, left
// ge:keywords test client-queue ranked-queue matchmaker
// ge:skill net testing
#include "account/client_queue.hpp"

#include <filesystem>
#include <string>

#include "account/ticket.hpp"
#include "check.hpp"
#include "net/crypto.hpp"
#include "net/sim_network.hpp"

namespace fs = std::filesystem;
namespace ga = ge::account;
namespace gn = ge::net;

namespace {

constexpr int64_t kT0 = 1794500000ll * 1000000;
const gn::Address kServiceAt = gn::sim_address(1, ga::kAccountPort);
const gn::Address kClientAt = gn::sim_address(2, 5000);

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

ga::Fresh fresh_of(uint8_t b) {
  ga::Fresh f{};
  for (size_t i = 0; i < f.size(); i++) f[i] = static_cast<uint8_t>(size_t{b} * 13 + i);
  return f;
}

struct World {
  gn::SimNetwork net = gn::make_sim_network(5, gn::LinkConditions{});
  gn::Transport service_port, client;
  ga::Service svc;
  uint64_t fresh_n = 1, client_n = 1000;
  int64_t now = kT0;
  std::string path;
  ga::ClientQueue ann, bob;
  gn::Transport bob_port;

  World() {
    gn::open_sim_transport(service_port, net, kServiceAt);
    gn::open_sim_transport(client, net, kClientAt);
    gn::open_sim_transport(bob_port, net, gn::sim_address(3, 5000));
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::create(store, "Niclas", "correct horse", now, fresh_of(1));
    ga::create(store, "Ann", "battery staple", now, fresh_of(2));
    ga::ServiceConfig c;
    c.seed = key_of(1);
    ga::service_init(svc, c, std::move(store), service_port, counter_fresh, &fresh_n);
    const fs::path dir = fs::temp_directory_path() / "ge_account_client_queue_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    path = (dir / "account.txt").string();
  }

  // `account` signed in on this computer (the file `account signin` leaves), loaded.
  ga::ClientLogin signed_in(uint64_t account, const char* name) {
    const ga::Ticket t = ga::ticket_issue(svc.keys, account, name, now / 1000000);
    uint8_t bytes[ga::kTicketMax];
    const size_t n = ga::ticket_write(t, bytes, sizeof bytes);
    ga::AccountFile f;
    f.master = gn::to_string(kServiceAt);
    f.key = gn::key_to_hex(ga::service_key(svc));
    f.name = name;
    f.ticket = ga::bytes_to_hex(std::vector<uint8_t>(bytes, bytes + n));
    ga::account_file_write(path, f);
    ga::ClientLogin c;
    std::string why;
    CHECK_MSG(ga::client_login_load(c, path, now / 1000000, why), why);
    return c;
  }

  // Both clients' frames for `us`; how many found their game.
  int run(int64_t us) {
    int found = 0;
    for (const int64_t end = now + us; now < end;) {
      now += 20000;
      gn::sim_advance(net, now);
      found += ga::client_queue_update(ann, client, now) ? 1 : 0;
      found += ga::client_queue_update(bob, bob_port, now) ? 1 : 0;
      gn::sim_advance(net, now);
      ga::service_update(svc, now);
    }
    return found;
  }
};

}  // namespace

TEST(client_queue_finds_a_ranked_game_for_two_and_keeps_their_places) {
  World w;
  ga::client_queue_start(w.ann, w.signed_in(1, "Niclas"), "id1", "1on1", "", counter_fresh,
                         &w.client_n);
  ga::client_queue_start(w.bob, w.signed_in(2, "Ann"), "id1", "1on1", "", counter_fresh,
                         &w.client_n);
  // No server yet: both wait, asking again past the queue's lapse.
  CHECK(w.run((ga::kQueueLife_s + 10) * 1000000) == 0);
  CHECK(w.ann.step == ga::QueueStep::waiting && w.ann.view.state == ga::LookState::waiting);
  CHECK(w.ann.words.find("Looking for a game") == 0);
  ga::service_offer(w.svc, gn::Key{}, "id1", "1on1", "203.0.113.4:27500", 8, w.now / 1000000);
  CHECK(w.run(5000000) == 2);
  CHECK(w.ann.step == ga::QueueStep::found && w.bob.step == ga::QueueStep::found);
  CHECK(w.ann.view.address == "203.0.113.4:27500");
  CHECK(w.ann.view.theirs.size() == 1 && w.ann.view.theirs[0] == "Ann");
  CHECK(w.ann.words.find("Game found: 1on1 against Ann on 203.0.113.4:27500.") == 0);
}

TEST(client_queue_is_refused_a_duo_not_its_own_and_can_leave) {
  World w;
  ga::client_queue_start(w.ann, w.signed_in(1, "Niclas"), "id1", "2on2", "Reds", counter_fresh,
                         &w.client_n);
  w.run(1000000);
  CHECK(w.ann.step == ga::QueueStep::refused);
  CHECK(w.ann.words.find("not in a duo") != std::string::npos);
  CHECK(w.ann.words.find("next:") == std::string::npos);
  ga::client_queue_start(w.bob, w.signed_in(2, "Ann"), "id1", "1on1", "", counter_fresh,
                         &w.client_n);
  w.run(1000000);
  CHECK(w.svc.matchmaker.queue.size() == 1);
  ga::client_queue_leave(w.bob, w.bob_port, w.now);
  w.run(1000000);
  CHECK(w.svc.matchmaker.queue.empty());
  CHECK(w.bob.step == ga::QueueStep::left);
}
