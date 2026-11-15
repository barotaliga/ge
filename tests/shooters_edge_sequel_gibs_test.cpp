// Tests: the sequel's gibs (games/shooters_edge/server/sequel_gibs.hpp) on its runner in a floored
// hall: a player thrown apart by a rail leaves 4 small meats flying up, spinning and coming to
// rest on the floor, his row his head, every piece gone within 20 s; a tank's metal bounces before
// it rests; each monster names its set; the pieces' rows carry the gib bit; god mode wears the
// white shell.
//
// ge:summary Tests: the sequel's gibs thrown, spun, landed and gone; god mode's shell
// ge:keywords test sequel gibs meat head skull metal bounce god-mode shell
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_gibs_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_gibs.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -512, -16}, floor_hi[3] = {1024, 512, 0};
  const float roof_lo[3] = {-1024, -512, 1024}, roof_hi[3] = {1024, 512, 1040};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(roof_lo, roof_hi, sh::kSqSolid, "world/roof"));
  br::Entity start;
  start.pairs = {{"classname", "info_player_start"}, {"origin", "-300 0 24"}, {"angle", "0"}};
  w.entities.push_back(start);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Arena {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
};

std::unique_ptr<Arena> arena() {
  auto a = std::make_unique<Arena>();
  a->r = sv::sequel_runner(sv::make_sequel_server({}));
  a->r.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(a->r.start(map, error), error);
  a->r.connect(0, "Ana", false, false);
  a->r.connect(1, "Ben", false, false);
  return a;
}

void run(Arena& a, int ticks) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet packets[2];
    for (int c = 0; c < 2; c++) {
      packets[c].client = c;
      packets[c].sequence = ++a.sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
    }
    a.r.frame(packets, 2);
  }
}

int live(const sv::SequelServer& s) {
  int n = 0;
  for (const sv::SequelGib& g : s.gibs) n += g.live ? 1 : 0;
  return n;
}

TEST(a_player_thrown_apart_leaves_four_meats_and_his_head) {
  auto a = arena();
  run(*a, 80);
  sv::SequelServer& s = a->s();
  sv::SequelPlayer& ben = s.players[1];
  ben.health = -60;
  sv::fight_kill(s, 1, 0, 0);
  CHECK(ben.gibbed);
  CHECK(live(s) == 4);
  for (const sv::SequelGib& g : s.gibs) {
    if (!g.live) continue;
    CHECK(g.model == s.gib_models[0]);  // the small meat
    CHECK(g.velocity.z >= 200 && g.velocity.z <= 500);
    CHECK(g.velocity.x >= -300 && g.velocity.x <= 300);
    CHECK(g.spin.x >= 0 && g.spin.x <= 600 && g.spin.y >= 0 && g.spin.z >= 0);
    CHECK(!g.metal);
  }
  run(*a, 10);
  const sv::WorldEntity& row = a->r->world.entities[2];
  CHECK(row.modelindex == s.gib_models[ben.skull ? 6 : 5]);
  CHECK(row.skin == (ben.skull ? 0 : 1));
  // The pieces' rows carry the gib bit.
  int rows = 0;
  for (const sv::WorldEntity& e : a->r->world.entities)
    if (!e.free && e.effects == 2) rows++;
  CHECK(rows == 4);
  run(*a, 77 * 4);  // 4 s (a host tick is 13 ms): down on the floor, still
  for (const sv::SequelGib& g : s.gibs) {
    if (!g.live) continue;
    CHECK(g.resting);
    CHECK_MSG(g.origin.z >= -0.5f && g.origin.z < 1, std::to_string(g.origin.z));
    CHECK(g.spin.x == 0 && g.velocity.z == 0);
  }
  run(*a, 77 * 18);  // past 20 s: all gone
  CHECK(live(s) == 0);
}

TEST(metal_bounces_before_it_rests) {
  auto a = arena();
  run(*a, 40);
  sv::SequelServer& s = a->s();
  sv::gibs_throw(s, {{-24, -24, 0}, {24, 24, 64}, {}, 100, sv::SqGibSet::tank});
  CHECK(live(s) == 7);  // a meat, 4 metal, a chest and the gear
  int metal = 0;
  for (const sv::SequelGib& g : s.gibs) metal += g.live && g.metal ? 1 : 0;
  CHECK(metal == 6);
  // Watch a metal piece: it meets the floor going down and leaves it going up at least once.
  int bounced = 0;
  float last_vz = 0;
  const int k = 1;  // the first metal piece thrown
  for (int f = 0; f < 80; f++) {
    sv::gibs_frame(s);
    s.level_time += 0.1;
    const float vz = s.gibs[k].velocity.z;
    if (last_vz < 0 && vz > 0) bounced++;
    last_vz = vz;
  }
  CHECK(s.gibs[k].metal);
  CHECK(bounced >= 1);
  CHECK(s.gibs[k].resting);
}

TEST(each_monster_names_its_pieces) {
  CHECK(sv::sq_gib_set("monster_infantry") == sv::SqGibSet::flesh);
  CHECK(sv::sq_gib_set("monster_soldier_ss") == sv::SqGibSet::soldier);
  CHECK(sv::sq_gib_set("monster_flipper") == sv::SqGibSet::small);
  CHECK(sv::sq_gib_set("monster_tank_commander") == sv::SqGibSet::tank);
  CHECK(sv::sq_gib_set("monster_makron") == sv::SqGibSet::makron);
  CHECK(sv::sq_gib_set("monster_supertank") == sv::SqGibSet::wreck);
}

TEST(god_mode_wears_a_white_shell) {
  auto a = arena();
  run(*a, 40);
  a->s().players[0].god = true;
  run(*a, 10);
  const sv::WorldEntity& row = a->r->world.entities[1];
  CHECK((row.effects & sv::kSqColourShellEffect) != 0);
  CHECK(row.render_fx == sv::kSqGodShellRender);
}

}  // namespace
