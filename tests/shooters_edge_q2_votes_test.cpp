// games/shooters_edge/net/q2_votes: our Quake II server's votes (a player alone passes one, the
// owner's settings refused, the next map puts a change back) and its settings sheet (offered as a
// window that asked the bit enters, sent when it asks, sent again when a vote changes a setting;
// a client without the bit is offered nothing).
//
// ge:summary Test: our Quake II server's votes and settings sheet, end to end with the window
// ge:keywords test q2-votes vote settings-sheet ge_sheet svc-120 serversheet
// ge:skill net shooter testing
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/sim_network.hpp"
#include "server/sheet.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/net/q2_votes.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/qw_remote_sheet.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace gs = ge::server;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  br::Entity spawn;
  spawn.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "0"}};
  w.entities.push_back(spawn);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Rig {
  std::shared_ptr<const br::Map> map;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct;
  cn::Q2NetServer server;
  cn::Q2Votes votes;
  cn::Remote remote;
  std::vector<std::string> said, heard;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig() {
  auto r = std::make_unique<Rig>();
  r->rules = sv::q2_runner(sv::make_q2_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  r->map = hall();
  map.brushes = r->map;
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  r->net = gn::make_sim_network(7, gn::LinkConditions{});
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, {}, error), error);
  cn::q2_votes_rules(r->votes, r->rules, gs::VoteSettings{});
  r->votes.map = [] { return std::string("hall"); };
  r->votes.has_map = [](const std::string& name) { return name == "hall" || name == "q2dm1"; };
  cn::q2_votes_open(r->votes, r->server);
  return r;
}

void join(Rig& r) {
  gn::open_sim_transport(r.ct, r.net, gn::sim_address(9, 27901));
  r.remote.q2_game = [] { return true; };
  r.remote.read_file = [](const std::string&, const std::string&) {
    return std::vector<uint8_t>{1};
  };
  cn::ClientConfig cfg;
  cfg.name = "eve";
  cn::remote_start(r.remote, r.ct, gn::sim_address(0, cn::kPortQ2), cfg);
}

void steps(Rig& r, int n) {
  for (int i = 0; i < n; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    cn::remote_step(r.remote, 0.0131, r.said);
    for (const auto& p : r.remote.demo.state.prints) r.heard.push_back(p.text);
    cn::q2_net_frame(r.server, r.now);
    cn::q2_votes_frame(r.votes);
  }
}

double now_s(const Rig& r) { return static_cast<double>(r.now) / 1e6; }

}  // namespace

TEST(a_player_alone_passes_a_vote_and_the_next_map_puts_it_back) {
  auto r = rig();
  join(*r);
  steps(*r, 150);
  CHECK_MSG(r->remote.q2 != nullptr && r->remote.q2->stage == cn::ClientStage::active,
            r->remote.q2 ? r->remote.q2->refused : "no client");
  CHECK(cn::remote_rules_word(r->remote, "vote fraglimit 25"));
  steps(*r, 40);
  CHECK_MSG(r->rules->cvars.find("fraglimit")->value == "25",
            r->rules->cvars.find("fraglimit")->value);
  // The owner's settings are refused, with why.
  CHECK(cn::remote_rules_word(r->remote, "vote deathmatch 0"));
  steps(*r, 40);
  CHECK(r->rules->cvars.find("deathmatch")->value == "1");
  bool told = false;
  for (const std::string& line : r->heard)
    told = told || line.find("deathmatch") != std::string::npos;
  CHECK(told);
  cn::q2_votes_map_over(r->votes);
  CHECK(r->rules->cvars.find("fraglimit")->value == "0");
}

TEST(the_window_is_offered_the_sheet_asks_and_sees_a_vote_s_new_value) {
  auto r = rig();
  join(*r);
  steps(*r, 150);
  CHECK(cn::q2_demo(*r->remote.q2).sheet_offered);
  cn::remote_sheet_command(r->remote, "open", now_s(*r));
  int waited = 0;
  while (waited < 600 && r->remote.sheet.sheet.rows.empty()) {
    steps(*r, 1);
    waited++;
  }
  CHECK_MSG(waited < 600, "the sheet never came");  // a message a frame, under 1400 bytes a packet
  const gs::SettingsSheet& s = r->remote.sheet.sheet;
  CHECK(r->remote.sheet.source == ge::classic::SheetSource::ours);
  CHECK_MSG(s.whole && s.rows.size() > 5, std::to_string(s.rows.size()));
  CHECK(!s.rows.empty() && s.rows[0].name == "map" && s.rows[0].value == "hall");
  const gs::SheetRow* frags = gs::sheet_find(s, "fraglimit");
  CHECK(frags != nullptr && frags->how == gs::SheetHow::vote && frags->group.empty());
  const gs::SheetRow* dm = gs::sheet_find(s, "deathmatch");
  CHECK(dm != nullptr && dm->how == gs::SheetHow::fixed);  // the owner's, shown not offered
  CHECK(cn::remote_rules_word(r->remote, "vote fraglimit 30"));
  const auto fraglimit_now = [&r] {
    const gs::SheetRow* row = gs::sheet_find(r->remote.sheet.sheet, "fraglimit");
    return row != nullptr ? row->value : std::string("none");
  };
  for (int i = 0; i < 600 && fraglimit_now() != "30"; i++) steps(*r, 1);
  const gs::SheetRow* after = gs::sheet_find(r->remote.sheet.sheet, "fraglimit");
  CHECK_MSG(after != nullptr && after->value == "30", after ? after->value : "none");
  cn::remote_sheet_command(r->remote, "close", now_s(*r));
  steps(*r, 10);
  CHECK(r->votes.watchers == 0);
}

TEST(a_client_without_the_bit_is_offered_nothing) {
  auto r = rig();
  gn::open_sim_transport(r->ct, r->net, gn::sim_address(9, 27901));
  cn::Q2ClientEnd c;
  cn::Q2ClientConfig cfg;
  cfg.name = "stock";
  cn::q2_client_start(c, r->ct, gn::sim_address(0, cn::kPortQ2), cfg, 0);
  for (int i = 0; i < 150; i++) {
    r->now += 13000;
    gn::sim_advance(r->net, r->now);
    cn::q2_client_tick(c, r->now);
    cn::q2_net_frame(r->server, r->now);
    cn::q2_votes_frame(r->votes);
  }
  CHECK(c.stage == cn::ClientStage::active);
  CHECK(!cn::q2_demo(c).sheet_offered && cn::q2_demo(c).sheet.rows.empty());
}
