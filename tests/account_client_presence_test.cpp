// Tests: our own client's presence and invites (engine/account/client_presence.hpp): the saved
// ticket's presence reaches the master and a friend sees it, a change is sent at once, the
// refresh keeps it from lapsing, a friend's invite is read once, leaving says offline, and a
// ticket the master refuses stops the asks.
//
// ge:summary Tests: our client's presence kept on the master, refreshed, invites read, leave
// ge:keywords test client-presence presence-refresh invites-poll
// ge:skill net testing
#include "account/client_presence.hpp"

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
  ga::ClientPresence p;

  World() {
    gn::open_sim_transport(service_port, net, kServiceAt);
    gn::open_sim_transport(client, net, kClientAt);
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::create(store, "Niclas", "correct horse", now, fresh_of(1));
    ga::create(store, "Ann", "battery staple", now, fresh_of(2));
    ga::ServiceConfig c;
    c.seed = key_of(1);
    ga::service_init(svc, c, std::move(store), service_port, counter_fresh, &fresh_n);
    ga::friend_ask(svc.friends, 1, 2);
    ga::friend_ask(svc.friends, 2, 1);
    const fs::path dir = fs::temp_directory_path() / "ge_account_client_presence_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    path = (dir / "account.txt").string();
  }

  // Niclas signed in on this computer (the file `account signin` leaves), presence started.
  void start(int64_t life_s = ga::kTicketLife_s) {
    const ga::Ticket t = ga::ticket_issue(svc.keys, 1, "Niclas", now / 1000000, life_s);
    uint8_t bytes[ga::kTicketMax];
    const size_t n = ga::ticket_write(t, bytes, sizeof bytes);
    ga::AccountFile f;
    f.master = gn::to_string(kServiceAt);
    f.key = gn::key_to_hex(ga::service_key(svc));
    f.name = "Niclas";
    f.ticket = ga::bytes_to_hex(std::vector<uint8_t>(bytes, bytes + n));
    ga::account_file_write(path, f);
    ga::ClientLogin c;
    std::string why;
    CHECK_MSG(ga::client_login_load(c, path, now / 1000000, why), why);
    ga::client_presence_start(p, c, counter_fresh, &client_n);
  }

  void run(int64_t us) {
    for (const int64_t end = now + us; now < end;) {
      now += 20000;
      gn::sim_advance(net, now);
      ga::client_presence_update(p, client, now);
      gn::sim_advance(net, now);
      ga::service_update(svc, now);
    }
  }

  ga::Presence seen_by_ann() const { return ga::presence_of(svc.friends, 1, now / 1000000, 2); }
};

}  // namespace

TEST(client_presence_reaches_a_friend_and_changes_at_once) {
  World w;
  w.start();
  ga::client_presence_set(w.p, ga::PresenceState::playing, "shooters_edge", "10.0.0.7:27500",
                          "dm4 duel");
  w.run(1000000);
  ga::Presence seen = w.seen_by_ann();
  CHECK(seen.state == ga::PresenceState::playing);
  CHECK(seen.where == "10.0.0.7:27500");
  CHECK(seen.words == "dm4 duel");
  ga::client_presence_set(w.p, ga::PresenceState::lobby, "shooters_edge", "", "");
  w.run(1000000);
  CHECK(w.seen_by_ann().state == ga::PresenceState::lobby);
}

TEST(client_presence_refreshes_so_it_never_lapses_and_leave_says_offline) {
  World w;
  w.start();
  ga::client_presence_set(w.p, ga::PresenceState::playing, "shooters_edge", "10.0.0.7:27500", "");
  w.run((ga::kPresenceLife_s + 60) * 1000000);
  CHECK(w.seen_by_ann().state == ga::PresenceState::playing);
  ga::client_presence_leave(w.p, w.client, w.now);
  for (int i = 0; i < 10; i++) {
    w.now += 20000;
    gn::sim_advance(w.net, w.now);
    ga::service_update(w.svc, w.now);
  }
  CHECK(w.seen_by_ann().state == ga::PresenceState::offline);
}

TEST(client_presence_reads_a_friends_invite_once) {
  World w;
  w.start();
  w.run(1000000);
  CHECK(ga::invite_send(w.svc.friends, 2, 1, {0, "shooters_edge", "10.0.0.9:27500", "2on2?", 0},
                        w.now / 1000000));
  w.run(ga::kInvitesEvery_us + 1000000);
  CHECK(w.p.got.size() == 1);
  if (w.p.got.empty()) return;
  CHECK(w.p.got[0].from == "Ann");
  CHECK(w.p.got[0].where == "10.0.0.9:27500");
  CHECK(w.p.got[0].note == "2on2?");
  w.p.got.clear();
  w.run(ga::kInvitesEvery_us + 1000000);
  CHECK(w.p.got.empty());
}

TEST(client_presence_stops_asking_once_the_master_refuses_the_ticket) {
  World w;
  w.start(5);
  w.run(1000000);
  CHECK(w.p.words.empty());
  w.now += int64_t{10} * 1000000;  // the ticket runs out
  w.run(ga::kPresenceEvery_us + 1000000);
  CHECK(!w.p.words.empty());
  CHECK(w.p.words.find("next:") == std::string::npos);
  CHECK(w.p.asking == ga::PresenceAsk::none);
}
