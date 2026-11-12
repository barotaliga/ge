// engine/hub/api.hpp and spec.hpp: the API's paths answer the index as JSON (servers by interest
// and filtered, one server, streams, nodes, masters, 404s that name the next step), and the
// interest rule puts full and human matches first, bots and stale score screens last.
//
// ge:summary Test: the hub's JSON API and the interest rule
// ge:keywords test hub api json servers filter interest streams 404 percent-decode
// ge:skill net testing
#include "hub/api.hpp"

#include <string>

#include "check.hpp"
#include "core/json_read.hpp"

using namespace ge::net;
using namespace ge::hub;
using ge::core::FlatJson;

namespace {

HubSpec spec() {
  HubSpec s;
  s.game = "Test Game";
  s.id = "test_game";
  s.protocol = 42;
  s.modes = {{"4on4", 9}, {"ffa", 3}};
  s.score = "frags";
  return s;
}

HubNode node(uint32_t at, const std::string& name, const std::string& mode, int humans_on,
             int bots_on, int max_players) {
  HubNode n;
  n.address = sim_address(at, 27500);
  n.kind = NodeKind::game;
  n.listed = n.have = true;
  n.heard_us = 0;
  n.phase_us = 0;
  n.changes = 1;
  n.status.name = name;
  n.status.mode = mode;
  n.status.map = "tower";
  n.status.phase = StatusPhase::playing;
  n.status.max_players = max_players;
  for (int i = 0; i < humans_on + bots_on; i++) {
    StatusPlayer p;
    p.name = "p" + std::to_string(i);
    p.score = i;
    p.flags = i < humans_on ? 0 : kPlayerBot;
    n.status.players.push_back(p);
  }
  return n;
}

Poller sample() {
  Poller p;
  p.config.masters = {sim_address(1, 27900)};
  p.listed_us = 0;
  p.nodes.push_back(node(10, "bots", "ffa", 0, 6, 8));
  p.nodes.push_back(node(11, "full four", "4on4", 8, 0, 8));
  p.nodes.push_back(node(12, "duel", "ffa", 2, 0, 8));
  HubNode tv;
  tv.address = sim_address(20, 28000);
  tv.kind = NodeKind::tv;
  tv.listed = true;  // listed, never answered
  p.nodes.push_back(tv);
  StatusStream st;
  st.node = tv.address;
  st.stream = 3;
  st.viewers = 5;
  p.nodes[1].status.streams.push_back(st);
  return p;
}

FlatJson read(const std::string& body) {
  FlatJson j;
  std::string err;
  CHECK(ge::core::read_flat(body, j, err, true));
  return j;
}

}  // namespace

TEST(api_servers_by_interest) {
  const Poller p = sample();
  const Answer a = api_answer(p, spec(), "/v1/servers", 10000000);
  CHECK(a.code == 200 && a.max_age_s > 0);
  FlatJson j = read(a.body);
  CHECK(j["count"].text == "3");
  CHECK(j["servers.0.name"].text == "full four");
  CHECK(j["servers.1.name"].text == "duel");
  CHECK(j["servers.2.name"].text == "bots");
  CHECK(j["servers.0.humans"].text == "8" && j["servers.2.bots"].text == "6");
  CHECK(j["servers.0.players.3.frags"].text == "3");  // the spec names the score column
  CHECK(j["game"].text == "Test Game");
}

TEST(api_filters) {
  const Poller p = sample();
  FlatJson j = read(api_answer(p, spec(), "/v1/servers?mode=ffa", 0).body);
  CHECK(j["count"].text == "2");
  j = read(api_answer(p, spec(), "/v1/servers?has_players=1", 0).body);
  CHECK(j["count"].text == "2");
  j = read(api_answer(p, spec(), "/v1/servers?min_players=7", 0).body);
  CHECK(j["count"].text == "1" && j["servers.0.name"].text == "full four");
  j = read(api_answer(p, spec(), "/v1/servers?kind=tv", 0).body);
  CHECK(j["count"].text == "0");  // the TV node never answered
  CHECK(api_answer(p, spec(), "/v1/servers?kind=radio", 0).code == 400);
}

TEST(api_one_server_streams_nodes_masters) {
  const Poller p = sample();
  Answer a = api_answer(p, spec(), "/v1/servers/sim%3A11%3A27500", 0);
  CHECK(a.code == 200);
  FlatJson j = read(a.body);
  CHECK(j["name"].text == "full four" && j["streams.0.viewers"].text == "5");
  a = api_answer(p, spec(), "/v1/servers/sim:11:27500/events", 0);
  CHECK(a.code == 200 && a.events && a.node == sim_address(11, 27500));
  a = api_answer(p, spec(), "/v1/servers/sim:99:1", 0);
  CHECK(a.code == 404);
  CHECK(read(a.body)["next"].text.find("/v1") != std::string::npos);
  CHECK(api_answer(p, spec(), "/v1/servers/nonsense", 0).code == 404);

  j = read(api_answer(p, spec(), "/v1/streams", 0).body);
  CHECK(j["count"].text == "1" && j["streams.0.server_name"].text == "full four");
  j = read(api_answer(p, spec(), "/v1/nodes", 0).body);
  CHECK(j["count"].text == "4" && !j["nodes.3.answered"].boolean);
  j = read(api_answer(p, spec(), "/v1/masters", 5000000).body);
  CHECK(j["masters.0.address"].text == "sim:1:27900" && j["masters.0.list_age_s"].text == "5");
  j = read(api_answer(p, spec(), "/v1/", 0).body);
  CHECK(j["servers"].text == "3" && j["players"].text == "10" && j["modes.0"].text == "4on4");
  CHECK(api_answer(p, spec(), "/v2/anything", 0).code == 404);
}

TEST(interest_orders_the_cards) {
  const HubSpec s = spec();
  const HubNode full = node(1, "a", "ffa", 8, 0, 8), half = node(2, "b", "ffa", 4, 0, 8);
  const HubNode four = node(3, "c", "4on4", 4, 0, 8), bots = node(4, "d", "4on4", 0, 8, 8);
  CHECK(interest(s, full, 0) > interest(s, four, 0));
  CHECK(interest(s, four, 0) > interest(s, half, 0));  // the mode's weight
  CHECK(interest(s, half, 0) > interest(s, bots, 0));  // bots alone sink, full or not
  HubNode stale = full;
  stale.status.phase = StatusPhase::intermission;
  CHECK(interest(s, stale, kStaleIntermissionUs / 2) > interest(s, half, 0));
  CHECK(interest(s, stale, kStaleIntermissionUs + 1) == 1);
  CHECK(interest(s, HubNode{}, 0) == 0);
}

TEST(api_query_and_path_decoding) {
  CHECK(query_param("/v1/servers?mode=4on4&x=1", "mode") == "4on4");
  CHECK(query_param("/v1/servers?a=b%20c+d", "a") == "b c d");
  CHECK(query_param("/v1/servers", "mode").empty());
  CHECK(target_path("/v1/servers/1.2.3.4%3A5?x") == "/v1/servers/1.2.3.4:5");
}
