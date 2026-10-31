// Tests: the client's record of a campaign (games/shooters_edge/client/campaign_view.hpp): every
// message a single-player program writes for its clients read off a stream (the intermission, the
// finale and the cutscene with their text, the sell screen, the music's track recorded, a kill and
// a secret, the view turned), a message it does not know ending the read, the text typed out at
// eight characters a second, the completed time; and the listen client running the campaign host
// on the campaign stub (games/shooters_edge/server/test_mod.hpp): its clock, the statistics a
// joining client is sent, the messages read after a frame, the next map read ahead and entered
// when the program changes level, and which game the server's lines ask for.
//
// ge:summary Tests: the campaign's client messages, typed text, and the listen client's campaign
// ge:keywords test campaign intermission finale cutscene sellscreen cdtrack killedmonster
// ge:keywords foundsecret setangle typed-text listen_campaign level-change wants_campaign
// ge:skill shooter testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/campaign_view.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
using Bytes = std::vector<uint8_t>;

std::unique_ptr<cl::LoadedMap> stub_map(const std::string& name) {
  auto m = std::make_unique<cl::LoadedMap>();
  m->name = name;
  m->spec.name = name;
  m->spec.models = sv::arena_models();
  m->spec.entities = sv::kStubEntities;
  return m;
}

}  // namespace

TEST(the_level_end_messages_are_read_in_order) {
  cl::CampaignView v;
  cl::read_campaign(v, Bytes{30}, 12.5);
  CHECK(v.end == cl::LevelEnd::intermission);
  CHECK(v.ended_at == 12.5);
  cl::read_campaign(v, Bytes{31, 'O', 'n', '\n', 'w', 0, 32, 4, 1, 33}, 14);
  CHECK(v.end == cl::LevelEnd::finale);
  CHECK(v.text == "On\nw");
  CHECK(v.text_at == 14);
  CHECK(v.cdtrack == 4 && v.cdloop == 1);
  CHECK(v.sellscreen);
  cl::read_campaign(v, Bytes{34, 'x', 0}, 20);
  CHECK(v.end == cl::LevelEnd::cutscene && v.text == "x");
  CHECK(v.unread == 0 && v.unknown.empty());
}

TEST(kills_and_secrets_add_to_the_joining_clients_statistics) {
  cl::CampaignView v;
  cl::join_stats(v, 2, 3, 0, 1);
  cl::read_campaign(v, Bytes{27, 28, 27}, 5);
  CHECK(v.total_secrets == 2 && v.total_monsters == 3);
  CHECK(v.secrets == 1 && v.monsters == 3);
}

TEST(setangle_turns_the_view) {
  cl::CampaignView v;
  cl::read_campaign(v, Bytes{10, 0, 64, 0}, 1);
  CHECK(v.turned);
  CHECK(v.angles.x == 0 && v.angles.y == 90 && v.angles.z == 0);
}

TEST(a_message_it_does_not_know_ends_the_read) {
  cl::CampaignView v;
  cl::read_campaign(v, Bytes{27, 99, 30, 30}, 1);
  CHECK(v.monsters == 1);
  CHECK(v.end == cl::LevelEnd::none);  // the intermission after it is not reached
  CHECK(v.unread == 3);
  CHECK_MSG(v.unknown.find("message 99") != std::string::npos, v.unknown);
}

TEST(the_text_is_typed_eight_characters_a_second) {
  cl::CampaignView v;
  cl::read_campaign(v, Bytes{31, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 0}, 10);
  CHECK(cl::typed_chars(v, 9) == 0);
  CHECK(cl::typed_chars(v, 10) == 0);
  CHECK(cl::typed_chars(v, 10.5) == 4);
  CHECK(cl::typed_chars(v, 11) == 8);
  CHECK(cl::typed_chars(v, 30) == 10);
  CHECK(cl::typed_chars(v, 10.5, 2) == 1);
}

TEST(the_completed_time_is_whole_minutes_and_seconds) {
  cl::CampaignView v;
  v.ended_at = 1.2 + 125.9;
  int m = 0, s = 0;
  cl::completed_time(v, 1.2, m, s);
  CHECK(m == 2 && s == 5);
  cl::completed_time(v, 500, m, s);
  CHECK(m == 0 && s == 0);
}

TEST(the_listen_client_plays_a_single_player_program_on_the_campaign_host) {
  ge::progs::Progs stub;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(stub, error), error);
  CHECK(cl::single_player(stub));
  cl::Listen l;
  cl::ListenOptions opt;
  opt.console = {"deathmatch 0"};
  CHECK_MSG(cl::listen_start(l, stub, stub_map("first"), opt, error), error);
  CHECK(l.campaign_host != nullptr && l.match == nullptr);
  CHECK(l.server == l.campaign_host->server.get());
  CHECK(cl::listen_tic(l) == sv::kCampaignStep);
  // The stub counts 3 monsters, one killed, and 2 secrets: what a joining client is sent.
  CHECK(l.campaign.total_monsters == 3 && l.campaign.monsters == 1);
  CHECK(l.campaign.total_secrets == 2 && l.campaign.secrets == 0);

  // The program's messages on the all-clients stream are read after the frame and emptied.
  l.server->out.all = {27, 30};
  cl::listen_frame(l, {});
  CHECK(l.campaign.monsters == 2);
  CHECK(l.campaign.end == cl::LevelEnd::intermission);
  CHECK(l.campaign.ended_at == l.server->sv.time);
  CHECK(l.server->out.all.empty());
  CHECK(cl::map_title(l) == "a single-player map");  // the world's message

  // At 1.5 s the stub changes level to `second`: the map read ahead is the one entered.
  l.next = stub_map("second");
  const cl::LoadedMap* ahead = l.next.get();
  int frames = 0;
  while (!l.map_changed && frames < 400) {
    cl::listen_frame(l, {});
    frames++;
  }
  CHECK_MSG(l.map_changed, "no level change in " + std::to_string(frames) + " frames");
  CHECK(l.failed.empty());
  CHECK(l.map.get() == ahead && l.map->name == "second");
  CHECK(l.server == l.campaign_host->server.get());
  CHECK(l.campaign.end == cl::LevelEnd::none);  // a new map's record
  CHECK(l.entered_at == l.server->sv.time);
  CHECK(l.campaign_host->history.back() == "change second");
  l.map_changed = false;
  cl::listen_frame(l, {});
  CHECK(!l.map_changed && l.failed.empty());
}

TEST(a_campaigns_next_map_with_nowhere_to_read_it_from_fails_with_a_reason) {
  ge::progs::Progs stub;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(stub, error), error);
  cl::Listen l;
  CHECK_MSG(cl::listen_start(l, stub, stub_map("first"), {}, error), error);
  for (int i = 0; i < 400 && l.failed.empty(); i++) cl::listen_frame(l, {});
  CHECK_MSG(l.failed.find("no search path") != std::string::npos, l.failed);
  CHECK(l.server == nullptr);
  CHECK(cl::listen_advance(l, 1, {}) == 0);  // nothing more runs
}

TEST(the_server_lines_say_which_game_a_map_starts) {
  cl::ListenOptions o;
  CHECK(!cl::wants_campaign(o));  // deathmatch 1 by default
  o.console.emplace_back("deathmatch 0");
  CHECK(cl::wants_campaign(o));
  o.console.emplace_back("fraglimit 10");
  CHECK(cl::wants_campaign(o));
  o.console.emplace_back("deathmatch 2");
  CHECK(!cl::wants_campaign(o));
}
