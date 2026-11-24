// Tests: QuakeWorld's admin in the game (games/shooters_edge/net/qw_referee.hpp) on the dedicated
// server, two classic clients over the simulated network. `k_admins 0` has no admins; a wrong code
// is refused and holds the next try 5 seconds; the right one makes the admin, told to everyone;
// the admin kicks the other player with a reason and forces a map change, and an unknown map is
// refused; `lock` turns a joining player into a spectator for 15 seconds; `forcestart` in the
// lobby's warmup counts down and drops the player who is not ready, and the match it starts
// refuses `forcemap`; a player who is not the admin
// is told so.
//
// ge:summary Tests: QuakeWorld's admin in the game: the code, kick, forcemap, lock, forcestart
// ge:keywords test qw-referee k_admincode k_admins forcestart forcemap kick lock admin
// ge:skill net shooter testing
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "net/sim_network.hpp"
#include "server/main.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/net/qw_referee.hpp"

using namespace ge::server;
namespace gn = ge::net;
namespace se = shooters_edge::net;

namespace {

constexpr uint16_t kPort = 27500;

ServerConfig arena() {
  ServerConfig c;
  c.hostname = "Referee Night";
  c.map = "arena";
  c.maxclients = 8;
  c.seed = 7;
  c.rules["k_admincode"] = "4321";
  return c;
}

using Lines = std::vector<std::pair<int, std::string>>;

struct Two {
  gn::SimNetwork net;
  gn::Transport server_port, ports[2];
  Server s;
  se::QwClientEnd clients[2];
  std::array<int, 2> key{};  // the number key each client presses on its next move
  std::string error;
  bool opened = false;
};

// The server and two clients ("ana" from the start, "ben" from frame `ben_at`), `frames` frames;
// each client says its lines at their frames.
void run(Two& r, const ServerConfig& c, int frames, const Lines& ana, const Lines& ben,
         int ben_at = 0) {
  gn::LinkConditions lan;
  r.net = gn::make_sim_network(c.seed, lan);
  gn::open_sim_transport(r.server_port, r.net, gn::sim_address(0, kPort));
  r.opened = server_open(r.s, se::qw_game(), c, &r.server_port, nullptr, r.error);
  if (!r.opened) return;
  const char* names[2] = {"ana", "ben"};
  for (int i = 0; i < 2; i++) {
    gn::open_sim_transport(r.ports[i], r.net, gn::sim_address(1 + i, uint16_t(27001 + i)));
  }
  se::ClientConfig cc;
  cc.files = {"skins/base.pcx"};
  for (int f = 0; f < frames; f++) {
    const int64_t now = r.s.now_us;
    gn::sim_advance(r.net, now);
    for (int i = 0; i < 2; i++) {
      if (f == (i == 0 ? 0 : ben_at)) {
        cc.name = names[i];
        int* key = &r.key[static_cast<size_t>(i)];
        cc.brain = [key](int tick) {
          ge::shooter::MoveCmd m = se::default_script(tick, 0);
          m.impulse = static_cast<uint8_t>(*key);
          *key = 0;
          return m;
        };
        se::client_start(r.clients[i], r.ports[i], gn::sim_address(0, kPort), cc,
                         static_cast<double>(now) / 1e6);
      }
      for (const auto& [at, line] : i == 0 ? ana : ben)
        if (f == at && line.starts_with('#'))  // a number key
          r.key[static_cast<size_t>(i)] = line[1] - '0';
        else if (f == at)
          se::client_command(r.clients[i], line);
      if (f >= (i == 0 ? 0 : ben_at)) se::client_tick(r.clients[i], now);
    }
    server_frame(r.s);
  }
}

bool printed(const se::QwClientEnd& c, const std::string& part) {
  for (const std::string& p : c.prints)
    if (p.find(part) != std::string::npos) return true;
  return false;
}

bool centred(const se::QwClientEnd& c, const std::string& part) {
  for (const std::string& p : c.centerprints)
    if (p.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(qw_referee_no_admins_when_k_admins_is_0) {
  ServerConfig c = arena();
  c.rules["k_admins"] = "0";
  Two r;
  run(r, c, 300, {{150, "admin 4321"}, {160, "forcemap dm2"}}, {});
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.clients[0], "NO admins on this server!"));
  CHECK(printed(r.clients[0], "You are not an admin"));
}

TEST(qw_referee_the_code_makes_the_admin_and_a_wrong_one_waits) {
  Two r;
  run(r, arena(), 900,
      {{150, "admin 1111"},
       {160, "admin 4321"},
       {600, "admin 4321"},
       {700, "acommands"},
       {720, "admin"}},
      {});
  CHECK_MSG(r.opened, r.error);
  const se::QwClientEnd& ana = r.clients[0];
  CHECK(printed(ana, "Access denied..."));
  CHECK(printed(ana, "Wait 5 seconds!"));
  CHECK(printed(ana, "ana gains admins status!"));
  CHECK(printed(ana, "Please give up admin rights when you're done"));
  CHECK(printed(ana, "Admin: forcestart, forcebreak, forcemap"));
  CHECK(printed(ana, "ana is no longer an admin"));
  CHECK(printed(r.clients[1], "ana gains admins status!"));
}

TEST(qw_referee_the_admin_kicks_and_forces_the_map) {
  Two r;
  run(r, arena(), 900,
      {{150, "admin 4321"},
       {200, "forcemap nowhere"},
       {220, "kick nobody"},
       {240, "kick ben camping"},
       {400, "forcemap arena"}},
      {{180, "forcemap arena"}});
  CHECK_MSG(r.opened, r.error);
  const se::QwClientEnd& ana = r.clients[0];
  const se::QwClientEnd& ben = r.clients[1];
  CHECK(printed(ben, "You are not an admin"));
  CHECK(printed(ana, "Map nowhere not available on this server."));
  CHECK(printed(ana, "kick: client nobody not found"));
  CHECK(printed(ana, "ben was kicked by ana"));
  CHECK(printed(ana, "[camping]"));
  CHECK(printed(ben, "You were kicked from the server"));
  CHECK(ben.stage != se::ClientStage::active);
  CHECK(printed(ana, "ana forces a map change to arena!"));
  CHECK(ana.stage == se::ClientStage::active);
}

TEST(qw_referee_lock_turns_a_joining_player_into_a_spectator) {
  Two r;
  run(r, arena(), 900, {{150, "admin 4321"}, {200, "lock"}}, {}, 260);
  CHECK_MSG(r.opened, r.error);
  CHECK(printed(r.clients[0], "ana locked server for 15 seconds"));
  CHECK(printed(r.clients[1], "server is temporary locked:"));
}

TEST(qw_referee_forcestart_drops_the_unready_and_starts_the_match) {
  ServerConfig c = arena();
  c.lobby = 1;
  Two r;
  run(r, c, 2400,
      {{150, "admin 4321"}, {200, "forcestart"}, {220, "forcestart"}, {2300, "forcemap arena"}},
      {});
  CHECK_MSG(r.opened, r.error);
  const se::QwClientEnd& ana = r.clients[0];
  const se::QwClientEnd& ben = r.clients[1];
  CHECK(printed(ana, "ana forces matchstart!"));
  CHECK(printed(ana, "forcestart already in progress!"));
  CHECK(centred(ben, "Get ready!"));
  CHECK(centred(ana, "second left before game starts"));
  CHECK(printed(ana, "ben was kicked by admin forcestart"));
  CHECK(printed(ben, "Bye bye! Pay attention next time."));
  CHECK(ben.stage != se::ClientStage::active);
  const std::string status = server_status(r.s);
  CHECK_MSG(status.find("matches=1") != std::string::npos, status);
  CHECK(printed(ana, "Match currently in progress. Use break or forcebreak to terminate."));
}

TEST(qw_referee_an_admin_by_election_and_by_the_number_keys) {
  ServerConfig c = arena();
  c.rules["k_admincode"] = "123456";
  Two r;
  run(r, c, 1200,
      {{150, "elect"},
       {300, "admin"},
       {320, "admin"},
       {340, "admin"},
       {350, "admin"},
       {360, "#1"},
       {380, "#2"},
       {400, "#3"},
       {420, "#4"},
       {440, "#5"},
       {460, "#6"}},
      {{170, "yes"}, {200, "elect"}, {220, "elect"}, {600, "admin"}, {620, "#9"}, {640, "elect"}});
  CHECK_MSG(r.opened, r.error);
  const se::QwClientEnd& ana = r.clients[0];
  const se::QwClientEnd& ben = r.clients[1];
  CHECK(printed(ben, "ana has requested admin rights!"));
  CHECK(printed(ben, "Type yes in console to approve"));
  CHECK(printed(ana, "ana gains admins status!"));
  CHECK(printed(ana, "Please give up admin rights when you're done"));
  CHECK(printed(ben, "Admin election is not allowed on this server") == false);
  // ana gives it up, then types the code on the number keys: canceled once, then all six.
  CHECK(printed(ana, "ana is no longer an admin"));
  CHECK(printed(ana, "Use numbers or impulses to enter code"));
  CHECK(printed(ana, "admin code canceled"));
  CHECK(printed(ana, "5 more to go"));
  CHECK(printed(ana, "1 more to go"));
  CHECK(printed(ben, "ana gains admins status!"));
  CHECK(printed(ben, "ben aborts election!"));
  CHECK(printed(ben, "Finish entering the code first"));
}
