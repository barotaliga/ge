// Tests: logins on Shooter's Edge's classic endpoint (net/classic_login.hpp): a stock client on a
// server that is its own master logs in by a `login` line sent as chat and gets the `*auth` key
// and its account's name, the line reaching no other client; a guest under a registered name
// plays under the default name; a server with sv_login on and no service to ask refuses to start.
//
// ge:summary Tests: the classic endpoint's logins: *auth, the account's name, a guest renamed
// ge:keywords test classic-login login auth-key reserved-names sv_login
// ge:skill net testing
#include "shooters_edge/net/classic_login.hpp"

#include <string>
#include <vector>

#include "account/service.hpp"
#include "check.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_info.hpp"

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
