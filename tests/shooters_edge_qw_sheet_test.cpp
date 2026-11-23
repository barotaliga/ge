// games/shooters_edge/net/qw_sheet and qw_remote_sheet: a client of our QuakeWorld server that
// agreed the settings-sheet bit asks with `ge_sheet` and is sent the sheet (the map and the match's
// limits first, the votable rules marked, the server's own settings left out); a vote that passes
// sends it again with the new value; the window's view takes a stock match mod's listing lines
// out of the prints while it waits for them and leaves every other line.
//
// ge:summary Test: our server's settings sheet over the wire, and the window's view of a stock one
// ge:keywords test settings-sheet ge_sheet qw_sheet_rows remote_sheet_take_print serversheet
// ge:skill net shooter testing
#include "check.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/qw_remote_sheet.hpp"
#include "shooters_edge/net/qw_sheet.hpp"

using namespace ge::server;
namespace gn = ge::net;
namespace se = shooters_edge::net;

namespace {

struct Session {
  gn::SimNetwork net;
  gn::Transport server_port, client_port;
  Server s;
  se::QwClientEnd client;
  std::string error;
  bool opened = false;
  SettingsSheet at_200;  // the client's sheet at frame 200, before the vote
};

void run(Session& r, int frames, const std::vector<std::pair<int, std::string>>& lines,
         bool ask_bit = true) {
  ServerConfig c;
  c.hostname = "Arena Night";
  c.map = "arena";
  c.maxclients = 8;
  c.seed = 1;
  r.net = gn::make_sim_network(c.seed, gn::LinkConditions{});
  gn::open_sim_transport(r.server_port, r.net, gn::sim_address(0, 27500));
  gn::open_sim_transport(r.client_port, r.net, gn::sim_address(1, 27001));
  r.opened = server_open(r.s, se::qw_game(), c, &r.server_port, nullptr, r.error);
  if (!r.opened) return;
  se::ClientConfig cc;
  cc.name = "visitor";
  cc.files = {"skins/base.pcx"};
  if (!ask_bit) cc.ask[se::ExtFamily::ge] &= ~se::kGeSettings;
  se::client_start(r.client, r.client_port, gn::sim_address(0, 27500), cc, 0);
  for (int f = 0; f < frames; f++) {
    const int64_t now = r.s.now_us;
    gn::sim_advance(r.net, now);
    for (const auto& [at, line] : lines)
      if (f == at) se::client_command(r.client, line);
    se::client_tick(r.client, now);
    server_frame(r.s);
    if (f == 200) r.at_200 = r.client.sheet;
  }
}

}  // namespace

TEST(our_server_sends_its_sheet_to_a_client_that_asks) {
  Session r;
  run(r, 400, {{120, "ge_sheet"}, {220, "vote fraglimit 30"}});
  CHECK_MSG(r.opened, r.error);
  CHECK(r.client.ext.has(se::ExtFamily::ge, se::kGeSettings));
  const SettingsSheet& before = r.at_200;
  std::string names;
  for (const SheetRow& row : before.rows)
    names += row.name + "(" + row.group + "," + sheet_how_name(row.how) + ") ";
  CHECK_MSG(before.whole && before.rows.size() > 5, names);
  CHECK(before.rows[0].name == "map" && before.rows[0].value == "arena");
  CHECK(before.rows[0].how == SheetHow::vote);
  const SheetRow* frags = sheet_find(before, "fraglimit");
  CHECK(frags != nullptr && frags->group.empty() && frags->how == SheetHow::vote);
  CHECK(frags != nullptr && frags->words == "Frag limit" && !frags->values.empty());
  CHECK(sheet_find(before, "hostname") == nullptr);  // the owner's, never voted on
  CHECK(sheet_find(before, "sv_password") == nullptr);
  bool help = false;
  for (const SheetRow& row : before.rows) help = help || row.group == "Player help";
  CHECK(help);
  // The vote passed (one voter): the sheet came again with the new value.
  const SheetRow* after = sheet_find(r.client.sheet, "fraglimit");
  CHECK_MSG(after != nullptr && after->value == "30", after ? after->value : "none");
  CHECK(r.client.sheet.revision > before.revision);
}

TEST(a_client_without_the_bit_is_never_sent_the_sheet) {
  Session r;
  run(r, 300, {{120, "ge_sheet"}}, false);
  CHECK_MSG(r.opened, r.error);
  CHECK(r.client.stage != se::ClientStage::dropped);
  CHECK(r.client.sheet.rows.empty());
}

TEST(the_rules_pages_by_name) {
  CHECK(se::qw_sheet_group("fraglimit").empty() && se::qw_sheet_group("k_midair").empty());
  CHECK(se::qw_sheet_group("sv_rjump") == "Player help");
  CHECK(se::qw_sheet_group("pm_speedjump") == "Player help");
  CHECK(se::qw_sheet_group("sv_gravity") == "Movement");
  CHECK(se::qw_sheet_group("pm_ktjump") == "Movement");
  CHECK(se::qw_sheet_group("sv_spectalk") == "Rules");
}

TEST(the_window_takes_a_listing_s_lines_and_leaves_the_rest) {
  se::RemoteSheet s;
  s.source = ge::classic::SheetSource::qw_match;
  ge::classic::qw_sheet_from_info(s.sheet, "\\ktxver\\1.48\\mode\\2on2\\map\\dm4");
  std::string keep;
  CHECK(
      se::remote_sheet_take_print(s, false, "Drop Quad     off Drop Ring     on\nAnn: hi\n", keep));
  CHECK(keep == "Ann: hi\n");
  const SheetRow* ring = sheet_find(s.sheet, "dropring");
  CHECK(ring != nullptr && ring->value == "on");
  CHECK(!se::remote_sheet_take_print(s, false, "Bob: gg\n", keep) && keep == "Bob: gg\n");
  se::RemoteSheet q;
  q.source = ge::classic::SheetSource::q2_plain;
  CHECK(se::remote_sheet_take_print(q, true, "mapname             q2dm1\nfraglimit           25\n",
                                    keep));
  CHECK(keep.empty() && q.q2_info.size() == 2);
}

TEST(a_joined_server_s_sheet_wins_over_the_window_s_own_game) {
  // The window keeps its own game (the demo behind the menu) while it plays on a stock server:
  // the page reads the joined server, never the window's own game.
  se::Remote r;
  r.sheet.local = [](SettingsSheet& out) {
    SheetRow own;
    own.name = "own";
    out.rows = {own};
    return true;
  };
  r.active = true;
  r.end.serverinfo = "\\map\\dm4\\deathmatch\\1\\timelimit\\10";
  se::remote_sheet_command(r, "open", 0);
  CHECK(r.sheet.source != ge::classic::SheetSource::ours);
  CHECK(sheet_find(r.sheet.sheet, "own") == nullptr);
  CHECK(sheet_find(r.sheet.sheet, "timelimit") != nullptr);
  // Back off the server: the page is the window's own game's again.
  r.active = false;
  se::remote_sheet_command(r, "refresh", 1);
  CHECK(r.sheet.source == ge::classic::SheetSource::ours &&
        sheet_find(r.sheet.sheet, "own") != nullptr);
}
