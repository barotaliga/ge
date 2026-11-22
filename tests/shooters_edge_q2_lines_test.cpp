// Quake II's server console lines about the map (games/shooters_edge/server/q2_lines.hpp): the
// position's form, the faults said at load, the counts after, `sv test` and the connect line.
//
// ge:summary Tests Quake II's server console lines: positions, load faults, counts, sv words
// ge:keywords q2 console server-console spawn-warnings inhibited teams picktarget position test
// ge:skill shooter
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_lines.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> faulty() {
  br::BrushWorld w;
  w.models.resize(2);
  const float lo[3] = {-1024, -512, -16}, hi[3] = {1024, 512, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  const float tlo[3] = {100, 100, 0}, thi[3] = {164, 164, 64};
  w.models[1].push_back(br::box_brush(tlo, thi, sh::kSqSolid, "world/train"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "path_corner"}, {"origin", "12.5 -0.3 13.5"}}));
  w.entities.push_back(entity({{"classname", "func_train"}, {"model", "*1"}}));
  w.entities.push_back(entity({{"classname", "target_speaker"}, {"origin", "1 2 3"}}));
  w.entities.push_back(entity({{"classname", "target_lightramp"}, {"origin", "4 5 6"}}));
  w.entities.push_back(
      entity({{"classname", "trigger_key"}, {"origin", "7 8 9"}, {"item", "key_no_such_thing"}}));
  w.entities.push_back(
      entity({{"classname", "item_health"}, {"origin", "50 50 16"}, {"spawnflags", "4"}}));
  w.entities.push_back(
      entity({{"classname", "item_armor_body"}, {"origin", "60 60 16"}, {"spawnflags", "2048"}}));
  w.entities.push_back(
      entity({{"classname", "item_armor_shard"}, {"origin", "70 70 16"}, {"team", "pair"}}));
  w.entities.push_back(
      entity({{"classname", "item_armor_jacket"}, {"origin", "80 80 16"}, {"team", "pair"}}));
  w.entities.push_back(entity({{"classname", "monster_no_such"}, {"origin", "1 1 1"}}));
  w.entities.push_back(
      entity({{"classname", "info_null"}, {"Bogus", "x"}, {"_color", "1 0 0"}, {"ORIGIN", "1 2"}}));
  w.entities.push_back(
      entity({{"classname", "target_laser"}, {"origin", "-20 0 40"}, {"target", "nowhere"}}));
  w.entities.push_back(entity({{"classname", "trigger_elevator"}, {"origin", "0 0 0"}}));
  w.entities.push_back(entity({{"classname", "monster_soldier"},
                               {"origin", "300 0 40"},
                               {"item", "item_no_such"},
                               {"target", "lost"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  CHECK(br::write_brush_world(w, &bytes, &error) && br::read_bsp(bytes, map.get(), &error));
  return map;
}

sv::RulesRunner open(const char* deathmatch) {
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec spec;
  spec.name = "faulty";
  spec.brushes = faulty();
  std::string error;
  CHECK_MSG(r.start(spec, error), error);
  return r;
}

bool said(const sv::Q2Server& s, const std::string& line) {
  return std::find(s.console.begin(), s.console.end(), line) != s.console.end();
}

}  // namespace

TEST(a_position_rounds_halves_to_even_and_keeps_a_negative_zero) {
  CHECK(sv::q2_pos({12.7f, -12.7f, 0.5f}) == "(13 -13 0)");
  CHECK(sv::q2_pos({1.5f, 2.5f, -1.5f}) == "(2 2 -2)");
  CHECK(sv::q2_pos({-0.3f, 0, 10000000}) == "(-0 0 10000000)");
}

TEST(a_map_s_faults_are_said_as_it_loads_and_the_counts_after) {
  sv::RulesRunner r = open("1");
  const sv::Q2Server& s = *sv::q2_of(r);
  CHECK(s.console.front() == "==== InitGame ====");
  CHECK(said(s, "path_corner with no targetname at (12 -0 14)"));
  CHECK(said(s, "func_train without a target at (99 99 -1)"));  // its lower corner, less 1
  CHECK(said(s, "target_speaker with no noise set at (1 2 3)"));
  CHECK(said(s, "target_lightramp has bad ramp ((null)) at (4 5 6)"));
  CHECK(said(s, "item key_no_such_thing not found for trigger_key at (7 8 9)"));
  CHECK(said(s, "trigger_key at (7 8 9) has no target"));
  CHECK(said(s, "item_health at (50 50 16) has invalid spawnflags set"));
  CHECK(said(s, "monster_no_such doesn't have a spawn function"));
  CHECK(said(s, "ED_ParseEdict: Bogus is not a field"));  // `_color` passes unremarked
  CHECK(!said(s, "ED_ParseEdict: _color is not a field"));
  CHECK(said(s, "ED_ParseField: couldn't parse 'ORIGIN'"));
  // The body armour is left out of deathmatch by its flag; the two of team "pair" are one team.
  CHECK(said(s, "1 entities inhibited"));
  CHECK_MSG(said(s, "1 teams with 2 entities"), s.console.back());
  const auto inh = std::find(s.console.begin(), s.console.end(), "1 entities inhibited");
  CHECK(inh != s.console.end() && inh + 1 != s.console.end() &&
        *(inh + 1) == "1 teams with 2 entities");
}

TEST(sv_test_answers_and_an_unknown_word_is_named) {
  sv::RulesRunner r = open("1");
  const sv::Q2Server& s = *sv::q2_of(r);
  r.console("sv test");
  CHECK(s.console.back() == "Svcmd_Test_f()");
  r.console("sv frobnicate");
  CHECK(s.console.back() == "Unknown server command \"frobnicate\"");
}

TEST(a_connect_is_said_on_the_console_when_more_than_one_may_play) {
  sv::RulesRunner r = open("1");
  r.connect(0, "Ana", false, false);
  CHECK(said(*sv::q2_of(r), "Ana connected"));
}

TEST(a_laser_aimed_at_nothing_is_said_when_it_starts) {
  sv::RulesRunner r = open("0");
  r.connect(0, "Ana", false, false);
  for (int i = 0; i < 160; i++) {  // 2 s of 13 ms moves
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = i + 1;
    sh::MoveCmd c;
    c.msec = 13;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    r.frame(&pk, 1);
  }
  const sv::Q2Server& s = *sv::q2_of(r);
  std::string all;
  for (const std::string& l : s.console) all += l + "|";
  CHECK_MSG(said(s, "target_laser at (-20 0 40): nowhere is a bad target"), all);
}

TEST(a_combat_point_leading_nowhere_is_said_once_and_forgotten) {
  sv::RulesRunner r = open("0");
  sv::Q2Server& s = *sv::q2_of(r);
  sv::Q2Entity point;
  point.classname = "point_combat";
  point.origin = {10, 20, 30};
  point.target = "gone";
  s.entities.push_back(point);
  const int at = static_cast<int>(s.entities.size()) - 1;
  sv::q2_combat_point_missing(s, at);
  CHECK(s.console[s.console.size() - 2] == "G_PickTarget: target gone not found");
  CHECK(s.console.back() == "point_combat at (10 20 30) target gone does not exist");
  CHECK(s.entities[size_t(at)].target.empty());
  const size_t lines = s.console.size();
  sv::q2_combat_point_missing(s, at);
  CHECK(s.console.size() == lines);
}

TEST(a_frame_on_the_elevators_and_monsters_say_what_they_lack) {
  sv::RulesRunner r = open("0");
  const sv::Q2Server& s = *sv::q2_of(r);
  CHECK(said(s, "monster_soldier at (300 0 40) has bad item: item_no_such"));
  CHECK(said(s, "trigger_elevator has no target"));
  CHECK(said(s, "G_PickTarget: target lost not found"));
  const auto counted = std::find(s.console.begin(), s.console.end(), "1 teams with 2 entities");
  const auto lost = std::find_if(s.console.begin(), s.console.end(), [](const std::string& l) {
    return l.rfind("monster_soldier can't find target lost at (300 0 ", 0) == 0;
  });
  CHECK(lost != s.console.end() && counted < lost);  // after the load's counts
}

TEST(a_put_off_use_with_nobody_to_hand_on_says_so_when_it_is_put_off) {
  sv::RulesRunner r = open("0");
  sv::Q2Server& s = *sv::q2_of(r);
  sv::Q2Entity relay;
  relay.classname = "trigger_relay";
  relay.delay = 1;
  relay.target = "somewhere";
  s.entities.push_back(relay);
  const int at = static_cast<int>(s.entities.size()) - 1;
  const size_t lines = s.console.size();
  sv::map_use_targets(s, at, -1);  // a use by no player: nothing said
  CHECK(s.console.size() == lines);
  sv::map_use_targets(s, at, sv::kNoActivator);
  CHECK(s.console.back() == "Think_Delay with no activator");
  CHECK(s.delayed.back().activator == -1);
}
