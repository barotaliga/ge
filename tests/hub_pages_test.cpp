// engine/hub/pages.hpp: the hub's HTML pages. Text from a server is escaped, the routes answer the
// API, the pages, the stylesheet and a 404 page; the servers page puts the most interesting match
// first, filters by mode and lists the empty servers after; the players page puts people before
// bots; the overlay shows one server's scoreboard.
//
// ge:summary Test: the hub's pages (routing, escaping, card order, players, scoreboard, 404)
// ge:keywords test hub pages html escape route card order mode-filter players scoreboard css 404
// ge:skill net testing
#include "hub/pages.hpp"

#include <string>

#include "check.hpp"

using namespace ge::net;
using namespace ge::hub;

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
             int bots_on) {
  HubNode n;
  n.address = sim_address(at, 27500);
  n.kind = NodeKind::game;
  n.listed = n.have = true;
  n.changes = 1;
  n.status.name = name;
  n.status.mode = mode;
  n.status.map = "tower";
  n.status.phase = StatusPhase::playing;
  n.status.time_left_s = 125;
  n.status.max_players = 8;
  for (int i = 0; i < humans_on + bots_on; i++) {
    StatusPlayer p;
    p.name = (i < humans_on ? "human" : "botty") + std::to_string(i);
    p.score = i;
    p.flags = i < humans_on ? 0 : kPlayerBot;
    n.status.players.push_back(p);
  }
  return n;
}

Poller sample() {
  Poller p;
  p.nodes.push_back(node(10, "Bot Pit", "ffa", 0, 6));
  p.nodes.push_back(node(11, "Big <Four>", "4on4", 8, 0));
  p.nodes.push_back(node(12, "Quiet Duel", "ffa", 2, 0));
  p.nodes.push_back(node(13, "Empty Hall", "4on4", 0, 0));
  HubNode tv;
  tv.address = sim_address(20, 28000);
  tv.kind = NodeKind::tv;
  tv.listed = tv.have = true;
  p.nodes.push_back(tv);
  StatusStream st;
  st.node = tv.address;
  st.stream = 3;
  st.viewers = 5;
  p.nodes[1].status.streams.push_back(st);
  return p;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}
size_t at(const std::string& s, const std::string& part) { return s.find(part); }

}  // namespace

TEST(escape_covers_markup_and_quotes) {
  CHECK(html_escape("a<b>&\"c'") == "a&lt;b&gt;&amp;&quot;c&#39;");
  CHECK(html_escape("plain") == "plain");
}

TEST(routes_answer_pages_api_css_and_404) {
  const Poller p = sample();
  const HubSpec s = spec();
  Answer a = hub_answer(p, s, "/", 0);
  CHECK(a.code == 200 && a.type == "text/html" && a.max_age_s == kPageRefreshS);
  CHECK(has(a.body, "<!doctype html>") && has(a.body, "http-equiv=\"refresh\""));
  CHECK(hub_answer(p, s, "/v1/servers", 0).type == "application/json");
  CHECK(hub_answer(p, s, "/players", 0).code == 200);
  Answer css = hub_answer(p, s, "/style.css", 0);
  CHECK(css.code == 200 && css.type == "text/css" && css.max_age_s == 3600);
  CHECK(has(css.body, "prefers-color-scheme:light"));
  Answer missing = hub_answer(p, s, "/nowhere<x>", 0);
  CHECK(missing.code == 404 && missing.type == "text/html");
  CHECK(has(missing.body, "/nowhere&lt;x&gt;") && !has(missing.body, "<x>"));
  CHECK(!has(missing.body, "http-equiv=\"refresh\""));
  CHECK(hub_answer(p, s, "/scoreboard/nonsense", 0).code == 404);
  CHECK(hub_answer(p, s, "/players/", 0).code == 200);  // a trailing slash is the same page
}

TEST(servers_page_orders_by_interest_and_escapes) {
  const Poller p = sample();
  const std::string h = servers_page(p, spec(), "", 0);
  CHECK(has(h, "Big &lt;Four&gt;") && !has(h, "Big <Four>"));
  CHECK(at(h, "Big &lt;Four&gt;") < at(h, "Quiet Duel"));  // full and human first
  CHECK(at(h, "Quiet Duel") < at(h, "Bot Pit"));           // bots last among the live
  CHECK(at(h, "Empty servers") < at(h, "Empty Hall"));     // the empty ones after
  CHECK(at(h, "Bot Pit") < at(h, "Empty servers"));
  CHECK(has(h, "test_game://join/") && has(h, "test_game://spectate/"));
  CHECK(has(h, "test_game://tv/") && has(h, "/3\">Watch</a>"));
  CHECK(has(h, "2:05"));  // the time left
  CHECK(has(h, "4 servers") && has(h, "10 playing") && has(h, "1 broadcasts"));
  CHECK(has(h, "<th>frags</th>"));
}

TEST(servers_page_filters_by_mode) {
  const Poller p = sample();
  const std::string h = servers_page(p, spec(), "ffa", 0);
  CHECK(has(h, "Quiet Duel") && has(h, "Bot Pit"));
  CHECK(!has(h, "Big &lt;Four&gt;") && !has(h, "Empty Hall"));
  CHECK(has(h, "href=\"/?mode=ffa\" class=\"on\""));
  const std::string routed = hub_answer(p, spec(), "/?mode=4on4", 0).body;
  CHECK(has(routed, "Big &lt;Four&gt;") && !has(routed, "Quiet Duel"));
}

TEST(players_page_puts_people_before_bots) {
  const Poller p = sample();
  const std::string h = players_page(p, spec(), 0);
  CHECK(has(h, "16 on 4 servers"));
  CHECK(at(h, "human0") < at(h, "botty0"));
  CHECK(has(h, "class=\"bot\">bot</td>"));
}

TEST(scoreboard_overlay_shows_one_server_ranked) {
  const Poller p = sample();
  const HubNode& n = p.nodes[2];
  const std::string addr = to_string(n.address);
  Answer a = hub_answer(p, spec(), "/scoreboard/" + addr, 0);
  CHECK(a.code == 200);
  CHECK(has(a.body, "Quiet Duel") && !has(a.body, "Bot Pit"));
  CHECK(at(a.body, "human1") < at(a.body, "human0"));  // the higher score first
  HubSpec low = spec();
  low.score_low_first = true;
  const std::string b = scoreboard_page(low, n);
  CHECK(at(b, "human0") < at(b, "human1"));
}

TEST(no_live_server_says_so) {
  Poller p;
  CHECK(has(servers_page(p, spec(), "", 0), "Nobody is playing right now."));
  CHECK(has(players_page(p, spec(), 0), "Nobody is on a server right now."));
}
