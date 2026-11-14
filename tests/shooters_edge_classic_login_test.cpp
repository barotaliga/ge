// Tests: logins on Shooter's Edge's classic endpoint (net/classic_login.hpp): a stock client on a
// server that is its own master logs in by a `login` line sent as chat and gets the `*auth` key
// and its account's name, the line reaching no other client; a guest under a registered name
// plays under the default name; a server with sv_login on and no service to ask refuses to start;
// our own client, holding a ticket from the same master, logs itself in on joining, and joins a
// server whose master is another as a guest.
//
// ge:summary Tests: the classic endpoint's logins: *auth, the account's name, a guest renamed
// ge:keywords test classic-login login auth-key reserved-names sv_login
// ge:skill net testing
#include "shooters_edge/net/classic_login.hpp"

#include <string>
#include <vector>

#include <filesystem>

#include "account/service.hpp"
#include "account/ticket.hpp"
#include "check.hpp"
#include "net/crypto.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_login.hpp"
#include "shooters_edge/net/classic_remote.hpp"

namespace ga = ge::account;
namespace gn = ge::net;
namespace gs = ge::server;
namespace se = shooters_edge::net;

namespace {

constexpr int64_t kUnix0 = 1794500000ll * 1000000;
int64_t g_unix = kUnix0;
int64_t unix_now() { return g_unix; }

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

ga::Fresh fresh_of(uint8_t b) {
  ga::Fresh f{};
  for (size_t i = 0; i < f.size(); i++) f[i] = static_cast<uint8_t>(size_t{b} * 13 + i);
  return f;
}

}  // namespace

TEST(classic_login_by_chat_line_sets_auth_and_name_and_renames_a_guest) {
  g_unix = kUnix0;
  gn::SimNetwork net = gn::make_sim_network(3, gn::LinkConditions{});
  gn::Transport own_port, port, ct[3];
  gn::open_sim_transport(own_port, net, gn::sim_address(8, ga::kAccountPort));
  gn::open_sim_transport(port, net, gn::sim_address(0, 27500));
  uint64_t fresh_n = 1;
  ga::Store store;
  ga::store_init(store, ga::HashCost{8, 1});
  ga::create(store, "Niclas", "correct horse", g_unix, fresh_of(1));
  ga::Service own;
  ga::ServiceConfig sc;
  ga::service_init(own, sc, std::move(store), own_port, counter_fresh, &fresh_n);
  const std::string code = ga::make_code(own.store, 1, g_unix, fresh_of(2)).code;

  gs::ServerConfig c;
  c.map = "arena";
  c.maxclients = 4;
  c.login.login = 1;
  c.accounts = &own;
  c.unix_us = unix_now;
  gs::Server s;
  std::string error;
  CHECK_MSG(gs::server_open(s, se::classic_game(), c, &port, nullptr, error),
            "server_open: " + error);
  if (!error.empty()) return;
  se::ClassicClientEnd ends[3];
  const char* names[3] = {"nic", "N1clas", "watcher"};
  bool said = false;
  for (int f = 0; f < 77 * 8; f++) {
    const int64_t now = s.now_us;
    g_unix = kUnix0 + now;
    gn::sim_advance(net, now);
    for (int i = 0; i < 3; i++) {
      if (ends[i].transport == nullptr) {
        gn::open_sim_transport(ct[i], net, gn::sim_address(static_cast<uint32_t>(20 + i), 27001));
        se::ClientConfig cc;
        cc.name = names[i];
        cc.files = {"skins/base.pcx"};
        cc.qport = static_cast<uint16_t>(100 + i);
        cc.spectator = i == 2;
        se::client_start(ends[i], ct[i], gn::sim_address(0, 27500), cc, 0);
      }
      if (i == 0 && !said && ends[0].stage == se::ClientStage::active && now > 3000000) {
        se::client_command(ends[0], "say \"login niclas " + code + "\"");
        said = true;
      }
      se::client_tick(ends[i], now);
    }
    gs::server_frame(s);
  }
  CHECK(said);
  const se::ClassicClientEnd& me = ends[0];
  CHECK(me.playernum >= 0);
  if (me.playernum < 0) return;
  const std::string& info = ends[2].userinfos[me.playernum];
  CHECK_MSG(se::info_get(info, "*auth") == "Niclas",
            "*auth: '" + se::info_get(info, "*auth") + "'");
  CHECK_MSG(se::info_get(info, "name") == "Niclas", "name: '" + se::info_get(info, "name") + "'");
  const std::string guest = se::info_get(ends[2].userinfos[ends[1].playernum], "name");
  CHECK_MSG(guest != "N1clas" && guest.find("player") != std::string::npos, "guest: " + guest);
  for (const std::string& line : ends[2].prints) CHECK(line.find(code) == std::string::npos);
  bool told = false;
  for (const std::string& line : ends[2].prints)
    told = told || line.find("Niclas logged in.") != std::string::npos;
  CHECK(told);
}

TEST(classic_login_on_without_a_service_refuses_to_start) {
  gn::SimNetwork net = gn::make_sim_network(3, gn::LinkConditions{});
  gn::Transport port;
  gn::open_sim_transport(port, net, gn::sim_address(0, 27500));
  gs::ServerConfig c;
  c.map = "arena";
  c.login.login = 1;
  gs::Server s;
  std::string error;
  CHECK(!gs::server_open(s, se::classic_game(), c, &port, nullptr, error));
  CHECK(error.find("sv_accountmaster") != std::string::npos);
}

namespace {

// Our own client joining a server that is its own master, over the simulated network; the master
// answers on a network of its own run on the wall clock (the client asks it on the wall clock).
struct OwnClient {
  gn::SimNetwork net = gn::make_sim_network(4, gn::LinkConditions{});
  gn::SimNetwork mnet = gn::make_sim_network(5, gn::LinkConditions{});
  gn::Transport port, ct, master_port, login_port, other_port;
  uint64_t fresh_n = 1;
  ga::Service own;
  gs::Server s;
  se::Remote remote;
  std::vector<std::string> said;
  std::string path;

  bool start(bool same_master) {
    g_unix = kUnix0;
    gn::open_sim_transport(port, net, gn::sim_address(0, 27500));
    gn::open_sim_transport(ct, net, gn::sim_address(20, 27001));
    gn::open_sim_transport(master_port, mnet, gn::sim_address(8, ga::kAccountPort));
    gn::open_sim_transport(login_port, mnet, gn::sim_address(20, 5000));
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::create(store, "Niclas", "correct horse", g_unix, fresh_of(1));
    ga::create(store, "Ann", "battery staple", g_unix, fresh_of(2));
    ga::ServiceConfig sc;
    ga::service_init(own, sc, std::move(store), master_port, counter_fresh, &fresh_n);
    // The file `account signin` leaves, its ticket signed by this master or another.
    ga::Service other;
    if (!same_master) {
      ga::Store o;
      ga::store_init(o, ga::HashCost{8, 1});
      ga::ServiceConfig oc;
      oc.seed[0] = 99;
      gn::open_sim_transport(other_port, mnet, gn::sim_address(9, ga::kAccountPort));
      ga::service_init(other, oc, std::move(o), other_port, counter_fresh, &fresh_n);
    }
    const ga::Service& signer = same_master ? own : other;
    const ga::Ticket t = ga::ticket_issue(signer.keys, 1, "Niclas", g_unix / 1000000);
    uint8_t bytes[ga::kTicketMax];
    const size_t n = ga::ticket_write(t, bytes, sizeof bytes);
    ga::AccountFile f;
    f.master = gn::to_string(gn::sim_address(8, ga::kAccountPort));
    f.key = gn::key_to_hex(ga::service_key(signer));
    f.name = "Niclas";
    f.ticket = ga::bytes_to_hex(std::vector<uint8_t>(bytes, bytes + n));
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "ge_se_classic_login_test";
    path = (dir / (same_master ? "same.txt" : "other.txt")).string();
    ga::account_file_write(path, f);

    gs::ServerConfig c;
    c.map = "arena";
    c.maxclients = 4;
    c.login.login = 1;
    c.accounts = &own;
    c.unix_us = unix_now;
    std::string error;
    if (!gs::server_open(s, se::classic_game(), c, &port, nullptr, error)) return false;
    se::ClientConfig cc;
    cc.name = "nic";
    cc.files = {"skins/base.pcx"};
    se::remote_start(remote, ct, gn::sim_address(0, 27500), cc);
    se::remote_login_from(remote, path, unix_now, &login_port);
    remote.login->fresh = counter_fresh;
    remote.login->fresh_ctx = &fresh_n;
    return true;
  }

  void run(int frames) {
    for (int f = 0; f < frames; f++) {
      const int64_t before = s.now_us;
      gs::server_frame(s);
      g_unix = kUnix0 + s.now_us;
      gn::sim_advance(net, s.now_us);
      gn::sim_advance(mnet, g_unix);
      ga::service_update(own, g_unix);
      gn::sim_advance(mnet, g_unix);
      se::remote_step(remote, static_cast<double>(s.now_us - before) * 1e-6, said);
    }
  }

  bool told(const std::string& part) const {
    for (const std::string& l : said)
      if (l.find(part) != std::string::npos) return true;
    for (const std::string& l : remote.end.prints)
      if (l.find(part) != std::string::npos) return true;
    return false;
  }
};

}  // namespace

TEST(classic_login_our_client_logs_itself_in_by_its_ticket) {
  OwnClient w;
  CHECK(w.start(true));
  w.run(77 * 6);
  CHECK(w.remote.end.stage == se::ClientStage::active);
  CHECK_MSG(w.told("Logging in as Niclas."), "the client did not try");
  CHECK_MSG(w.told("Niclas logged in."), "the server did not log it in");
  const int me = w.remote.end.playernum;
  CHECK(me >= 0);
  if (me < 0) return;
  CHECK(se::info_get(w.remote.end.userinfos[me], "*auth") == "Niclas");
  CHECK(se::info_get(w.remote.end.serverinfo, se::kAccountKeyInfo) ==
        gn::key_to_hex(ga::service_key(w.own)).substr(0, 16));
}

TEST(classic_login_our_client_shows_friends_where_it_plays_and_says_their_invites) {
  OwnClient w;
  CHECK(w.start(true));
  ga::friend_ask(w.own.friends, 1, 2);
  ga::friend_ask(w.own.friends, 2, 1);
  w.run(77 * 6);
  CHECK(w.remote.end.stage == se::ClientStage::active);
  const ga::Presence seen = ga::presence_of(w.own.friends, 1, g_unix / 1000000, 2);
  CHECK(seen.state == ga::PresenceState::playing);
  CHECK(seen.game == "shooters_edge");
  CHECK(seen.where == w.remote.server_name);
  CHECK(seen.words == "arena");
  CHECK(ga::invite_send(w.own.friends, 2, 1, {0, "shooters_edge", "10.0.0.9:27500", "2on2?", 0},
                        g_unix / 1000000));
  w.run(77 * 20);
  CHECK_MSG(w.told("Ann invites you to join them on 10.0.0.9:27500: 2on2?"), "no invite said");
  CHECK(w.told("Type connect 10.0.0.9:27500 to join."));
  se::remote_disconnect(w.remote);
  for (int i = 0; i < 10; i++) {
    g_unix += 20000;
    gn::sim_advance(w.mnet, g_unix);
    ga::service_update(w.own, g_unix);
  }
  CHECK(ga::presence_of(w.own.friends, 1, g_unix / 1000000, 2).state == ga::PresenceState::offline);
}

TEST(classic_login_our_client_finds_a_ranked_game_and_is_given_its_server) {
  OwnClient w;
  CHECK(w.start(true));
  ga::service_offer(w.own, gn::Key{}, "id1", "1on1", "203.0.113.4:27500", 8, g_unix / 1000000);
  CHECK(ga::matchmaker_join(w.own.matchmaker, w.own.matches.ladder, 2, "id1", "1on1", "",
                            g_unix / 1000000) == ga::QueueAnswer::queued);
  std::string said;
  CHECK_MSG(se::remote_ranked(w.remote, "id1", "1on1", "", said), said);
  CHECK(said == "Looking for a ranked 1on1 game as Niclas.");
  std::string found;
  for (int f = 0; f < 77 * 3 && found.empty(); f++) {
    w.run(1);
    found = se::remote_queue_step(w.remote, w.said);
  }
  CHECK(found == "203.0.113.4:27500");
  CHECK(w.told("Game found: 1on1 against Ann on 203.0.113.4:27500."));
  CHECK(!se::remote_ranked_leave(w.remote));  // found: nothing to leave
}

TEST(classic_login_our_client_with_another_masters_ticket_stays_a_guest) {
  OwnClient w;
  CHECK(w.start(false));
  w.run(77 * 6);
  CHECK(w.remote.end.stage == se::ClientStage::active);
  CHECK(!w.told("Logging in as"));
  CHECK(!w.told("logged in."));
}

TEST(classic_login_on_the_window_s_own_match) {
  namespace cl = shooters_edge::client;
  g_unix = kUnix0;
  ge::progs::Progs progs;
  std::string error;
  CHECK_MSG(cl::baseline_progs(progs, error), error);
  gn::SimNetwork net = gn::make_sim_network(6, gn::LinkConditions{});
  gn::Transport port, client_port, own_port;
  gn::open_sim_transport(port, net, gn::sim_address(0, 27500));
  gn::open_sim_transport(client_port, net, gn::sim_address(1, 27001));
  gn::open_sim_transport(own_port, net, gn::sim_address(8, ga::kAccountPort));
  uint64_t fresh_n = 1;
  ga::Store store;
  ga::store_init(store, ga::HashCost{8, 1});
  ga::create(store, "Niclas", "correct horse", g_unix, fresh_of(1));
  ga::Service own;
  ga::service_init(own, ga::ServiceConfig{}, std::move(store), own_port, counter_fresh, &fresh_n);
  const std::string code = ga::make_code(own.store, 1, g_unix, fresh_of(2)).code;

  cl::Listen l;
  cl::ListenOptions opt = se::listen_options();
  opt.transport = &port;
  opt.player = "host";
  opt.login.login = 1;
  opt.accounts = &own;
  opt.unix_us = unix_now;
  cl::SearchPath none;
  auto map = cl::load_named_map(none, "arena", error);
  CHECK_MSG(map != nullptr, error);
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  if (l.match == nullptr) return;
  se::ClassicClientEnd visitor;
  se::ClientConfig cc;
  cc.name = "visitor";
  cc.files = {"skins/base.pcx"};
  se::client_start(visitor, client_port, gn::sim_address(0, 27500), cc, 0);
  ge::shooter::MoveCmd idle;
  bool said = false;
  for (int f = 0; f < 77 * 6; f++) {
    const int64_t now = l.match->now_us;
    g_unix = kUnix0 + now;
    gn::sim_advance(net, now);
    if (!said && visitor.stage == se::ClientStage::active && now > 3000000) {
      se::client_command(visitor, "login niclas " + code);
      said = true;
    }
    se::client_tick(visitor, now);
    cl::listen_frame(l, idle);
  }
  CHECK(said);
  CHECK(visitor.playernum >= 0);
  if (visitor.playernum < 0) return;
  CHECK(se::info_get(visitor.userinfos[visitor.playernum], "*auth") == "Niclas");
  CHECK(se::info_get(visitor.serverinfo, se::kAccountKeyInfo) ==
        gn::key_to_hex(ga::service_key(own)).substr(0, 16));
  // The window's own player is never asked to log in.
  CHECK(se::info_get(visitor.userinfos[0], "name") == "host");
  cl::listen_stop(l);
}
