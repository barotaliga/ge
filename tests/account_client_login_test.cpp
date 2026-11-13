// Tests: our own client logging itself in (engine/account/client_login.hpp): the account file's
// ticket traded with the master for a one-time code over the simulated network, the code spent
// by a server's login; a server whose tag is another master's not fitted; a ticket run out or a
// missing file refused in a player's words; a master that is not there gives up.
//
// ge:summary Tests: our client's login: the saved ticket traded for a code, fit, expiry, silence
// ge:keywords test client-login autologin ticket-code accountkey
// ge:skill net testing
#include "account/client_login.hpp"

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
  uint64_t fresh_n = 1;
  int64_t now = kT0;
  std::string path;

  World() {
    gn::open_sim_transport(service_port, net, kServiceAt);
    gn::open_sim_transport(client, net, kClientAt);
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::create(store, "Niclas", "correct horse", now, fresh_of(1));
    ga::ServiceConfig c;
    c.seed = key_of(1);
    ga::service_init(svc, c, std::move(store), service_port, counter_fresh, &fresh_n);
    const fs::path dir = fs::temp_directory_path() / "ge_account_client_login_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    path = (dir / "account.txt").string();
  }

  // The file the game CLI's `account signin` leaves: a ticket for Niclas `life_s` long.
  void signed_in(int64_t life_s = ga::kTicketLife_s) {
    const ga::Ticket t = ga::ticket_issue(svc.keys, 1, "Niclas", now / 1000000, life_s);
    uint8_t bytes[ga::kTicketMax];
    const size_t n = ga::ticket_write(t, bytes, sizeof bytes);
    ga::AccountFile f;
    f.master = gn::to_string(kServiceAt);
    f.key = gn::key_to_hex(ga::service_key(svc));
    f.name = "Niclas";
    f.ticket = ga::bytes_to_hex(std::vector<uint8_t>(bytes, bytes + n));
    ga::account_file_write(path, f);
  }

  void run(ga::ClientLogin& c, bool serve = true) {
    for (int i = 0; i < 400 && !ga::client_login_update(c, client, now); i++) {
      now += 20000;
      gn::sim_advance(net, now);
      if (serve) ga::service_update(svc, now);
      gn::sim_advance(net, now);
    }
  }
};

}  // namespace

TEST(client_login_trades_the_saved_ticket_for_a_code_a_server_spends) {
  World w;
  w.signed_in();
  ga::ClientLogin c;
  std::string why;
  CHECK_MSG(ga::client_login_load(c, w.path, w.now / 1000000, why), why);
  CHECK(c.file.name == "Niclas");
  CHECK(ga::client_login_fits(c, gn::key_to_hex(ga::service_key(w.svc)).substr(0, 16)));
  CHECK(!ga::client_login_fits(c, gn::key_to_hex(key_of(9)).substr(0, 16)));
  CHECK(!ga::client_login_fits(c, ""));
  ga::client_login_ask(c, key_of(30), w.now);
  w.run(c);
  CHECK(c.step == ga::ClientLoginStep::code);
  CHECK(!c.code.empty());
  // The code is the account's: a server's login spends it, once.
  ga::Ticket t;
  CHECK(ga::service_login(w.svc, "niclas", c.code, "10.0.0.7:27001", w.now, t).refusal ==
        ga::Refusal::none);
  CHECK(t.account == 1);
  CHECK(ga::service_login(w.svc, "niclas", c.code, "10.0.0.7:27001", w.now, t).refusal !=
        ga::Refusal::none);
}

TEST(client_login_refuses_no_file_and_a_ticket_run_out_in_a_players_words) {
  World w;
  ga::ClientLogin c;
  std::string why;
  CHECK(!ga::client_login_load(c, w.path, w.now / 1000000, why));
  CHECK(why.find("signed in") != std::string::npos);
  w.signed_in(60);
  CHECK(!ga::client_login_load(c, w.path, w.now / 1000000 + 120, why));
  CHECK(why == "Your sign-in has run out. Sign in again.");
  CHECK(why.find("next:") == std::string::npos && why.find('`') == std::string::npos);
}

TEST(client_login_gives_up_on_a_master_that_does_not_answer) {
  World w;
  w.signed_in();
  ga::ClientLogin c;
  std::string why;
  CHECK(ga::client_login_load(c, w.path, w.now / 1000000, why));
  ga::client_login_ask(c, key_of(31), w.now);
  w.run(c, false);
  CHECK(c.step == ga::ClientLoginStep::silent);
}
