// engine/net/session.hpp over the simulated network: seats and welcomes, the full lobby, teams,
// the leader and its hand-over, chat to all, to a team and directly with its cap and its rate,
// ready and start and return, settings, a kick and its ban, reconnecting by token, an away seat
// timing out, a listen server's own player, and a session replaying to the bit.
//
// ge:summary Test: sessions: seats, teams, leader, chat scopes and limits, start, kick, reconnect
// ge:keywords test net session lobby team chat rate-limit kick ban reconnect token leader start
// ge:skill net testing
#include "net/session.hpp"

#include <memory>
#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::net;

namespace {

constexpr int64_t kTick = 10000;

NodeConfig node_config(bool host, int max_peers, uint64_t seed) {
  NodeConfig c;
  c.protocol = protocol_id("session-test", 1);
  c.accept = host;
  c.max_peers = max_peers;
  c.channels.count = 1;
  c.channels.delivery[0] = Delivery::reliable;
  c.timeout_us = 1000000;
  c.seed = seed;
  return c;
}

struct Client {
  Transport t;
  Node n;
  SessionClient s;
  std::vector<std::string> log;
};

struct World {
  SimNetwork net;
  Transport host_t;
  Node host_n;
  SessionHost host;
  std::vector<std::unique_ptr<Client>> clients;
  std::vector<std::string> host_log;
  int64_t now = 0;

  World(const SessionConfig& sc, int clients_n, const LinkConditions& link = {}, uint32_t seed = 1)
      : net(make_sim_network(seed, link)) {
    open_sim_transport(host_t, net, sim_address(0, 1));
    node_init(host_n, node_config(true, 32, 7), host_t);
    session_host_init(host, sc, host_n, now);
    for (int i = 0; i < clients_n; i++) {
      auto c = std::make_unique<Client>();
      open_sim_transport(c->t, net, sim_address(static_cast<uint32_t>(i + 1), 1));
      node_init(c->n, node_config(false, 1, 100 + static_cast<uint64_t>(i)), c->t);
      session_client_init(c->s, c->n, 0);
      c->s.reconnect_us = sc.reconnect_us;
      clients.push_back(std::move(c));
    }
  }

  void join(int i, const char* name, uint64_t token = 0) {
    session_client_join(clients[i]->s, sim_address(0, 1), name, token, now);
  }

  static std::string line(const SessionEvent& e, const std::vector<uint8_t>& bytes) {
    std::string s = std::string(session_event_name(e.type)) + " " + std::to_string(e.player);
    if (e.type == SessionEventType::chat) {
      s += std::string(" ") + scope_name(e.scope) + " " + std::to_string(e.other) + " " +
           std::string(session_text(bytes, e), e.size);
    }
    if (e.why != SessionWhy::none) s += std::string(" ") + why_name(e.why);
    if (e.type == SessionEventType::refused) s += std::string(" ") + ask_name(e.ask);
    return s;
  }

  void run(int ms) {
    for (int t = 0; t < ms; t += 10) {
      now += kTick;
      sim_advance(net, now);
      node_update(host_n, now);
      session_host_update(host, now);
      for (const SessionEvent& e : host.events) host_log.push_back(line(e, host.event_bytes));
      for (auto& c : clients) {
        node_update(c->n, now);
        session_client_update(c->s, now);
        for (const SessionEvent& e : c->s.events) c->log.push_back(line(e, c->s.event_bytes));
      }
      node_flush(host_n, now);
      for (auto& c : clients) node_flush(c->n, now);
    }
  }

  int count(const std::vector<std::string>& log, const std::string& prefix) const {
    int n = 0;
    for (const std::string& l : log) n += l.compare(0, prefix.size(), prefix) == 0;
    return n;
  }
};

SessionConfig lobby(int max_players, int teams = 0) {
  SessionConfig c;
  c.max_players = max_players;
  c.team_count = teams;
  c.seed = 42;
  c.reconnect_us = 5000000;
  return c;
}

}  // namespace

TEST(session_seats_everyone_and_rosters_agree) {
  World w(lobby(8, 2), 6);
  for (int i = 0; i < 6; i++) w.join(i, ("p" + std::to_string(i)).c_str());
  w.run(500);
  CHECK(session_player_count(w.host, true) == 6);
  for (auto& c : w.clients) {
    CHECK(c->s.state == ClientState::seated);
    for (int s = 0; s < kMaxPlayers; s++) {
      CHECK(c->s.players[s].used == w.host.players[s].used);
      CHECK(c->s.players[s].team == w.host.players[s].team);
      CHECK(c->s.players[s].leader == w.host.players[s].leader);
      CHECK(std::string(c->s.players[s].name) == w.host.players[s].name);
    }
  }
  // The first to join leads a dedicated server; teams are filled evenly.
  CHECK(session_leader(w.host) == w.clients[0]->s.me);
  int team0 = 0;
  for (int s = 0; s < 8; s++) team0 += w.host.players[s].used && w.host.players[s].team == 0;
  CHECK(team0 == 3);
}

TEST(session_full_lobby_refuses_the_next) {
  World w(lobby(4), 5);
  for (int i = 0; i < 5; i++) {
    w.join(i, "p");
    w.run(50);
  }
  w.run(500);
  CHECK(session_player_count(w.host, false) == 4);
  CHECK(w.clients[4]->s.state == ClientState::lost);
  CHECK(w.count(w.clients[4]->log, "refused -1 full join") == 1);
  CHECK(w.host.stats.refused_joins == 1);
}

TEST(session_names_are_cleaned_and_capped) {
  World w(lobby(4), 2);
  w.join(0, "  a\tb\x01  ");
  w.join(1, "abcdefghijklmnopqrstuvwxyz0123456789");
  w.run(300);
  CHECK(std::string(w.host.players[w.clients[0]->s.me].name) == "a b");
  CHECK(std::string(w.host.players[w.clients[1]->s.me].name).size() == kMaxName);
}

TEST(session_chat_reaches_its_scope_only) {
  World w(lobby(6, 2), 4);
  for (int i = 0; i < 4; i++) w.join(i, "p");
  w.run(400);
  const int a = w.clients[0]->s.me;
  const int b = w.clients[1]->s.me;
  CHECK(w.host.players[a].team != w.host.players[b].team);
  session_client_chat(w.clients[0]->s, ChatScope::all, kNoPlayer, "hi", 2);
  session_client_chat(w.clients[0]->s, ChatScope::team, kNoPlayer, "team", 4);
  session_client_chat(w.clients[0]->s, ChatScope::direct, b, "psst", 4);
  w.run(300);
  for (size_t i = 0; i < w.clients.size(); i++) {
    const auto& log = w.clients[i]->log;
    const int me = w.clients[i]->s.me;
    CHECK(w.count(log, "chat " + std::to_string(a) + " all") == 1);
    const bool mate = w.host.players[me].team == w.host.players[a].team;
    CHECK(w.count(log, "chat " + std::to_string(a) + " team") == (mate ? 1 : 0));
    const bool direct = me == a || me == b;
    CHECK(w.count(log, "chat " + std::to_string(a) + " direct " + std::to_string(b) + " psst") ==
          (direct ? 1 : 0));
  }
  CHECK(w.host.stats.chat_relayed == 3);
}

TEST(session_chat_is_rate_limited_and_capped) {
  SessionConfig sc = lobby(4);
  sc.chat_max = 10;
  World w(sc, 2);
  w.join(0, "p");
  w.join(1, "q");
  w.run(300);
  for (int i = 0; i < 8; i++) session_client_chat(w.clients[0]->s, ChatScope::all, -1, "x", 1);
  w.run(300);
  CHECK(w.count(w.clients[1]->log, "chat") == sc.chat_burst);
  CHECK(w.count(w.clients[0]->log, "refused " + std::to_string(w.clients[0]->s.me) + " rate") ==
        8 - sc.chat_burst);
  // A second later the bucket holds one line more.
  w.run(1000);
  session_client_chat(w.clients[0]->s, ChatScope::all, -1, "a\nb is longer than ten", 22);
  w.run(300);
  CHECK(w.clients[1]->log.back() ==
        "chat " + std::to_string(w.clients[0]->s.me) + " all -1 a b is lon");
}

TEST(session_start_needs_the_leader_and_everyone_ready) {
  SessionConfig sc = lobby(4);
  sc.min_players = 2;
  World w(sc, 3);
  for (int i = 0; i < 3; i++) {
    w.join(i, "p");
    w.run(50);
  }
  w.run(300);
  SessionClient& lead = w.clients[0]->s;
  SessionClient& other = w.clients[1]->s;
  CHECK(lead.players[lead.me].leader);
  session_client_start(other);
  session_client_start(lead);
  w.run(200);
  CHECK(w.count(w.clients[1]->log, "refused " + std::to_string(other.me) + " not_leader start") ==
        1);
  CHECK(w.count(w.clients[0]->log, "refused " + std::to_string(lead.me) + " not_ready start") == 1);
  for (auto& c : w.clients) session_client_ready(c->s, true);
  w.run(200);
  session_client_start(lead);
  w.run(200);
  CHECK(w.host.phase == SessionPhase::playing);
  for (auto& c : w.clients) CHECK(c->s.phase == SessionPhase::playing);
  // Back in the lobby nobody is ready.
  session_client_return(lead);
  w.run(200);
  CHECK(w.host.phase == SessionPhase::lobby);
  for (int s = 0; s < 4; s++) CHECK(!w.host.players[s].ready);
}

TEST(session_join_in_progress_is_refused_unless_allowed) {
  SessionConfig sc = lobby(4);
  sc.need_ready = false;
  World w(sc, 2);
  w.join(0, "p");
  w.run(300);
  session_client_start(w.clients[0]->s);
  w.run(200);
  w.join(1, "late");
  w.run(400);
  CHECK(w.clients[1]->s.state == ClientState::lost);
  CHECK(w.count(w.clients[1]->log, "refused -1 in_progress join") == 1);
}

TEST(session_settings_from_the_leader_reach_everyone) {
  SessionConfig sc = lobby(4);
  sc.settings_max = 8;
  World w(sc, 3);
  for (int i = 0; i < 2; i++) {
    w.join(i, "p");
    w.run(50);
  }
  w.run(300);
  const uint8_t map[3] = {1, 2, 3};
  session_client_settings(w.clients[1]->s, map, 3);
  session_client_settings(w.clients[0]->s, map, 3);
  const uint8_t big[9] = {};
  session_client_settings(w.clients[0]->s, big, 9);
  w.run(300);
  CHECK(w.host.settings.size() == 3);
  CHECK(w.clients[1]->s.settings == w.host.settings);
  CHECK(w.count(w.clients[1]->log,
                "refused " + std::to_string(w.clients[1]->s.me) + " not_leader settings") == 1);
  CHECK(w.count(w.clients[0]->log,
                "refused " + std::to_string(w.clients[0]->s.me) + " too_big settings") == 1);
  // A player who joins later is sent them.
  w.join(2, "late");
  w.run(300);
  CHECK(w.clients[2]->s.settings == w.host.settings);
}

TEST(session_kick_removes_and_bans) {
  World w(lobby(4), 3);
  for (int i = 0; i < 3; i++) {
    w.join(i, "p");
    w.run(50);
  }
  w.run(300);
  const int victim = w.clients[2]->s.me;
  session_client_kick(w.clients[0]->s, victim);
  w.run(600);
  CHECK(w.count(w.clients[2]->log, "kicked " + std::to_string(victim) + " kicked") == 1);
  CHECK(w.clients[2]->s.state == ClientState::lost);
  CHECK(w.count(w.clients[1]->log, "left " + std::to_string(victim) + " kicked") == 1);
  CHECK(!w.host.players[victim].used);
  CHECK(w.host.stats.kicks == 1);
  // Coming back from the same address is refused.
  w.run(1500);
  w.join(2, "again");
  w.run(500);
  CHECK(w.count(w.clients[2]->log, "refused -1 banned join") == 1);
  CHECK(session_player_count(w.host, false) == 2);
}

TEST(session_reconnects_by_token_to_the_same_seat) {
  World w(lobby(4, 2), 2);
  w.join(0, "p");
  w.join(1, "q");
  w.run(400);
  const int seat = w.clients[1]->s.me;
  const int team = w.host.players[seat].team;
  LinkConditions cut;
  cut.loss = 1;
  sim_set_path(w.net, 0, 2, cut);
  w.run(2000);
  CHECK(!w.host.players[seat].present);
  CHECK(w.host.players[seat].used);
  CHECK(w.count(w.clients[0]->log, "away " + std::to_string(seat)) == 1);
  CHECK(w.clients[1]->s.state == ClientState::reconnecting);
  sim_set_path(w.net, 0, 2, LinkConditions{});
  w.run(2000);
  CHECK(w.clients[1]->s.state == ClientState::seated);
  CHECK(w.clients[1]->s.me == seat);
  CHECK(w.host.players[seat].present);
  CHECK(w.host.players[seat].team == team);
  CHECK(w.count(w.clients[0]->log, "rejoined " + std::to_string(seat)) == 1);
  CHECK(w.host.stats.rejoins == 1);
}

TEST(session_away_past_the_window_frees_the_seat) {
  SessionConfig sc = lobby(4);
  sc.reconnect_us = 2000000;
  World w(sc, 2);
  w.join(0, "p");
  w.join(1, "q");
  w.run(400);
  const int seat = w.clients[1]->s.me;
  LinkConditions cut;
  cut.loss = 1;
  sim_set_path(w.net, 0, 2, cut);
  w.run(5000);
  CHECK(!w.host.players[seat].used);
  CHECK(w.count(w.clients[0]->log, "left " + std::to_string(seat) + " timed_out") == 1);
  CHECK(w.clients[1]->s.state == ClientState::lost);
}

TEST(session_leader_passes_when_the_leader_leaves) {
  World w(lobby(4), 3);
  for (int i = 0; i < 3; i++) {
    w.join(i, "p");
    w.run(50);
  }
  w.run(300);
  const int first = w.clients[0]->s.me;
  const int second = w.clients[1]->s.me;
  CHECK(session_leader(w.host) == first);
  session_client_leave(w.clients[0]->s, w.now);
  w.run(300);
  CHECK(session_leader(w.host) == second);
  CHECK(w.clients[2]->s.players[second].leader);
  CHECK(w.count(w.clients[2]->log, "left " + std::to_string(first) + " left") == 1);
}

TEST(session_listen_server_host_is_seat_zero_and_leads) {
  SessionConfig sc = lobby(4, 2);
  sc.host_plays = true;
  sc.host_name = "host";
  World w(sc, 2);
  w.join(0, "p");
  w.join(1, "q");
  w.run(400);
  CHECK(w.host.players[0].used && w.host.players[0].leader);
  CHECK(std::string(w.clients[0]->s.players[0].name) == "host");
  CHECK(w.clients[0]->s.me != 0);
  // The host's own lines reach everyone, its own kick works, and it cannot be kicked.
  CHECK(session_host_chat(w.host, 0, ChatScope::all, -1, "go", 2, w.now) == SessionWhy::none);
  session_client_kick(w.clients[0]->s, 0);
  w.run(300);
  CHECK(w.count(w.clients[1]->log, "chat 0 all -1 go") == 1);
  CHECK(w.count(w.clients[0]->log,
                "refused " + std::to_string(w.clients[0]->s.me) + " not_leader kick") == 1);
  CHECK(session_host_kick(w.host, 0, w.clients[1]->s.me, w.now) == SessionWhy::none);
  w.run(500);
  CHECK(w.clients[1]->s.state == ClientState::lost);
}

TEST(session_teams_refuse_a_full_team) {
  SessionConfig sc = lobby(4, 2);
  World w(sc, 4);
  for (int i = 0; i < 4; i++) {
    w.join(i, "p");
    w.run(50);
  }
  w.run(300);
  SessionClient& c = w.clients[0]->s;
  const int other = 1 - w.host.players[c.me].team;
  session_client_team(c, other);
  session_client_team(c, 5);
  w.run(200);
  CHECK(w.count(w.clients[0]->log, "refused " + std::to_string(c.me) + " team_full team") == 1);
  CHECK(w.count(w.clients[0]->log, "refused " + std::to_string(c.me) + " no_team team") == 1);
}

TEST(session_replays_to_the_bit) {
  auto play = [] {
    LinkConditions bad;
    conditions_preset("bad", bad);
    World w(lobby(8, 2), 6, bad, 9);
    for (int i = 0; i < 6; i++) w.join(i, "p");
    w.run(1500);
    for (int i = 0; i < 6; i++) {
      session_client_ready(w.clients[i]->s, true);
      session_client_chat(w.clients[i]->s, ChatScope::team, -1, "go", 2);
    }
    w.run(2000);
    std::vector<std::string> all = w.host_log;
    for (auto& c : w.clients) all.insert(all.end(), c->log.begin(), c->log.end());
    return all;
  };
  const auto a = play();
  const auto b = play();
  CHECK(a == b);
  CHECK(a.size() > 50);
}
