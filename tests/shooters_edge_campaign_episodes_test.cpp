// Tests: the episodes in order (games/shooters_edge/server/campaign_episodes.hpp): which episodes
// are open for what is done; the start map's bars kept shut in front of an episode not open (the
// full-game trigger made a message trigger, its targets gone, every entity kept in its place) and
// left alone on the shareware packs, on another map and for an open or done episode; and the host
// keeping what is done: a rune carried out of a map added to `sv_episodes_done`, and the start
// map's `serverflags` given every done episode's bit, a new game too.
//
// ge:summary Tests: episodes in order: open episodes, the start map's bars shut, done kept
// ge:keywords test episodes episode-order sv_episodes_done start-map bars serverflags runes
// ge:skill shooter progs testing
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "check.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/campaign_episodes.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

// A start map: the world, the ways into episodes 1 and 2 (models 1, 2), episode 2's bars' trigger
// beside its way in (model 3), a shareware sign's remover beside episode 1's (model 4), and the
// bars themselves (model 5), an unnamed bar 2 units beside them (model 6) and an unnamed door
// far away (model 7).
const char* const kStart = R"({
"classname" "worldspawn"
}
{
"classname" "trigger_changelevel"
"map" "e1m1"
"model" "*1"
}
{
"classname" "trigger_changelevel"
"map" "e2m1"
"model" "*2"
}
{
"classname" "trigger_onlyregistered"
"target" "t2"
"wait" "-1"
"message" "full game only"
"model" "*3"
}
{
"classname" "trigger_onlyregistered"
"killtarget" "t9"
"model" "*4"
}
{
"classname" "func_door"
"targetname" "t2"
"model" "*5"
}
{
"classname" "func_door"
"model" "*6"
}
{
"classname" "func_door"
"model" "*7"
}
)";

sh::BrushModel box(float x, float y) {
  sh::BrushModel m;
  m.mins = {x - 8, y - 8, 0};
  m.maxs = {x + 8, y + 8, 64};
  return m;
}

sv::MapSpec start_map() {
  sv::MapSpec m;
  m.name = "start";
  m.entities = kStart;
  m.models = {box(0, 0),  box(0, 0),    box(1000, 0), box(900, 0),
              box(40, 0), box(900, 20), box(900, 38), box(0, 500)};
  return m;
}

const sv::EpisodeOrder kOrder = {.start_map = "start",
                                 .first_maps = {"e1m1", "e2m1", "e3m1", "e4m1"}};

std::vector<ge::brush::Entity> parsed(const sv::MapSpec& m) {
  std::vector<ge::brush::Entity> out;
  std::string error;
  CHECK_MSG(ge::brush::parse_entities(m.entities, &out, &error), error);
  return out;
}

}  // namespace

TEST(each_episode_opens_once_the_one_before_is_done) {
  CHECK(sv::episodes_open(0, 4) == 1);      // the first only
  CHECK(sv::episodes_open(1, 4) == 2);      // the first done: the second, the first shut
  CHECK(sv::episodes_open(3, 4) == 4);      // two done: the third
  CHECK(sv::episodes_open(7, 4) == 8);      // three done: the fourth
  CHECK(sv::episodes_open(15, 4) == 0);     // all done: none (the last map's way is the gate's)
  CHECK(sv::episodes_open(2, 4) == 1 + 4);  // the second alone done: the first and the third
}

TEST(the_bars_of_an_episode_not_open_stay_shut) {
  sv::MapSpec m = start_map();
  sv::EpisodeBars bars;
  std::string error;
  CHECK_MSG(sv::shut_episode_bars(m, kOrder, 0, true, bars, error), error);
  CHECK(bars.shut.size() == 1 && bars.shut[0] == 3 && bars.episode[0] == 1);
  const std::vector<ge::brush::Entity> e = parsed(m);
  CHECK(e.size() == 8);  // every entity kept, in its place
  CHECK(e[3].get("classname") == "trigger_multiple");
  CHECK(e[3].get("target").empty());
  CHECK(e[3].get("model") == "*3");
  CHECK(e[3].get("wait") == "2");
  CHECK(e[3].get("message") == sv::episode_bars_message(1));
  // The shareware sign's remover and the bars themselves are as they were.
  CHECK(e[4].get("classname") == "trigger_onlyregistered" && e[4].get("killtarget") == "t9");
  CHECK(e[5].get("classname") == "func_door" && e[5].get("targetname") == "t2");
  // The bar beside them waits to be fired too; the far door still opens on approach.
  CHECK(e[6].get("targetname") == "t2" && bars.named.size() == 1 && bars.named[0] == 6);
  CHECK(e[7].get("targetname").empty());
}

TEST(the_bars_open_once_the_episode_before_is_done) {
  sv::MapSpec m = start_map();
  const std::string before = m.entities;
  sv::EpisodeBars bars;
  std::string error;
  CHECK(sv::shut_episode_bars(m, kOrder, 1, true, bars, error));
  CHECK(bars.shut.empty() && bars.left_open.size() == 1 && bars.left_open[0] == 3);
  CHECK(m.entities == before);  // nothing rewritten
}

TEST(the_shareware_packs_and_other_maps_are_left_alone) {
  sv::EpisodeBars bars;
  std::string error;
  sv::MapSpec m = start_map();
  const std::string before = m.entities;
  CHECK(sv::shut_episode_bars(m, kOrder, 0, false, bars, error));  // not the full game
  CHECK(m.entities == before && bars.shut.empty());
  m.name = "e1m1";
  CHECK(sv::shut_episode_bars(m, kOrder, 0, true, bars, error));  // not the start map
  CHECK(m.entities == before && bars.shut.empty());
  m.name = "start";
  CHECK(sv::shut_episode_bars(m, sv::EpisodeOrder{}, 0, true, bars, error));  // no order kept
  CHECK(m.entities == before && bars.shut.empty());
}

namespace {

bool load(void*, const std::string& name, sv::MapSpec& out, std::string&) {
  out.models = sv::arena_models();
  out.entities = sv::kStubEntities;
  out.name = name;
  return true;
}

}  // namespace

// The stub's first map leaves for the second with rune 1 set (the campaign host's test).
TEST(the_host_keeps_what_is_done_and_gives_it_to_the_start_map) {
  pg::Progs mod;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(mod, error), error);
  sv::Campaign c;
  sv::CampaignOptions o;
  o.server.loop.max_edicts = 64;
  o.load = &load;
  o.episodes = kOrder;
  CHECK_MSG(sv::campaign_new_game(c, mod, o, "first", error), error);
  sv::campaign_join(c, 0, "player");
  CHECK(sv::qc_of(c.server)->cvars.find("sv_episodes_done")->integer() == 0);
  const size_t entered = c.history.size();
  for (int i = 0; i < 200 && c.history.size() == entered; i++)
    CHECK_MSG(sv::campaign_frame(c, nullptr, 0, error), error);
  CHECK(c.history.back() == "change second");
  CHECK(sv::qc_of(c.server)->cvars.find("sv_episodes_done")->integer() == 1);
  // A new game on the start map: the classic rules start it with no runes; what is done stays.
  CHECK_MSG(sv::campaign_change(c, sv::MapChange::new_game, "start", error), error);
  CHECK(c.serverflags == 1);
  const sv::QcServer& s = *sv::qc_of(c.server);
  CHECK(pg::as_float(*pg::entity_field(s.q.vm, 0, pg::kSinglePlayerLayout.f.items)) == 1);
  // Another map's new game starts as the classic rules say.
  CHECK_MSG(sv::campaign_change(c, sv::MapChange::new_game, "first", error), error);
  CHECK(c.serverflags == 0);
  CHECK(sv::qc_of(c.server)->cvars.find("sv_episodes_done")->integer() == 1);
}
