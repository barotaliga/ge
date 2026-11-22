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
