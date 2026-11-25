// Tests: Ground Zero's pushers, gravity triggers, barrels and plats
// (games/shooters_edge/server/q2_gz_triggers.cpp): a pusher switched by its uses and one that is
// a wall, a gravity trigger's fraction reaching a player and its toggle, a barrel set off by lava,
// a rising plat's bad area and a monster sending a plat its way; the base game's unchanged.
//
// ge:summary Tests: Ground Zero's pushers, gravity triggers, barrels in lava and plats
// ge:keywords test q2 ground-zero trigger-push start-off trigger-gravity misc-explobox
// ge:keywords barrel-lava func-plat plat-bad-area
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_triggers_test.cpp
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_map_kinds.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

using Pairs = std::vector<std::pair<std::string, std::string>>;

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1,
                   int32_t contents = sh::kSqSolid) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, contents, contents == sh::kSqSolid ? "world/wall" : "world/lava");
}

struct Hall {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  int index(const char* classname, int nth = 0) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].classname == classname && nth-- == 0) return static_cast<int>(i);
    return -1;
  }
  sv::Q2Entity& named(const char* classname, int nth = 0) const {
    return s().entities[size_t(index(classname, nth))];
  }
};

// A floored hall with a lava pool in its far corner; Ana starts at -1500 0. `models` are brush
// models *1, *2 ... for `things`.
std::unique_ptr<Hall> open_hall(bool pack, const std::vector<br::WorldBrush>& models,
                                const std::vector<Pairs>& things,
                                const std::vector<std::string>& lines = {}) {
  br::BrushWorld w;
  w.models.resize(1);
  w.models[0].push_back(box(-2048, -1024, -16, 2048, 1024, 0));
  w.models[0].push_back(box(1500, 600, 0, 1700, 800, 64, sh::kSqLava));
  for (const br::WorldBrush& m : models) w.models.push_back({m});
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-1500 0 24"}, {"angle", "0"}}));
  for (const Pairs& t : things) w.entities.push_back(entity(t));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  auto h = std::make_unique<Hall>();
  sv::Q2ServerOptions o;
  if (pack) o.pack = sv::Q2Pack::ground_zero;
  h->r = sv::q2_runner(sv::make_q2_server(o));
  h->r.console("deathmatch 0");
  for (const std::string& l : lines) h->r.console(l);
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  CHECK_MSG(h->r.start(spec, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

// Host ticks of 13 ms, Ana standing.
void run(Hall& h, int ticks) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++h.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    h.r.frame(&pk, 1);
  }
}

constexpr int kSecond = 77;  // host ticks of 13 ms in a second

bool said(const sv::Q2Server& s, const std::string& line) {
  for (const std::string& l : s.console)
    if (l == line) return true;
  return false;
}

std::unique_ptr<Hall> pushers(bool pack) {
  return open_hall(pack, {box(0, 0, 0, 64, 64, 64), box(200, 0, 0, 264, 64, 64)},
                   {{{"classname", "trigger_push"}, {"model", "*1"}, {"spawnflags", "2"}},
                    {{"classname", "trigger_push"},
                     {"model", "*2"},
                     {"targetname", "wind"},
                     {"spawnflags", "6"}},
                    {{"classname", "trigger_relay"}, {"targetname", "go"}, {"target", "wind"}}});
}

}  // namespace

TEST(in_the_pack_a_start_off_pusher_is_a_wall_or_waits_for_its_use) {
  auto h = pushers(true);
  run(*h, 2);
  const sv::Q2Entity& wall = h->named("trigger_push");
  CHECK(wall.solid && !wall.trigger);
  CHECK(said(h->s(), "trigger_push is START_OFF but not targeted."));
  CHECK(!h->named("trigger_push", 1).trigger);
  sv::map_use_targets(h->s(), h->index("trigger_relay"), -1);
  CHECK(h->named("trigger_push", 1).trigger);
  sv::map_use_targets(h->s(), h->index("trigger_relay"), -1);
  CHECK(!h->named("trigger_push", 1).trigger);
  CHECK(sv::gz_push_silent(h->s(), h->named("trigger_push", 1)));
  // Outside the pack both push from the start and a use changes nothing.
  auto base = pushers(false);
  run(*base, 2);
  CHECK(!base->named("trigger_push").solid && base->named("trigger_push").trigger);
  sv::map_use_targets(base->s(), base->index("trigger_relay"), -1);
  CHECK(base->named("trigger_push", 1).trigger);
  CHECK(!sv::gz_push_silent(base->s(), base->named("trigger_push", 1)));
}

TEST(in_the_pack_a_gravity_trigger_reaches_a_player_by_its_fraction_and_toggles) {
  const auto hall = [](bool pack) {
    return open_hall(pack, {box(-1600, -100, 0, -1400, 100, 128)},
                     {{{"classname", "trigger_gravity"},
                       {"model", "*1"},
                       {"gravity", "0.5"},
                       {"targetname", "g"},
                       {"spawnflags", "1"}},
                      {{"classname", "trigger_relay"}, {"targetname", "go"}, {"target", "g"}}});
  };
  auto h = hall(true);
  run(*h, kSecond / 2);
  CHECK_MSG(h->s().players[0].gravity == 0.5f, std::to_string(h->s().players[0].gravity));
  CHECK(h->named("trigger_gravity").random == 0.5f);
  sv::map_use_targets(h->s(), h->index("trigger_relay"), -1);  // TOGGLE: off
  run(*h, 2);
  CHECK(h->s().players[0].gravity == 1);
  sv::map_use_targets(h->s(), h->index("trigger_relay"), -1);  // and on again
  run(*h, 2);
  CHECK(h->s().players[0].gravity == 0.5f);
  // Outside the pack the key is a whole number (0) and a player's gravity is never touched.
  auto base = hall(false);
  run(*base, kSecond / 2);
  CHECK(base->s().players[0].gravity == 1);
  CHECK(base->named("trigger_gravity").random == 0);
}

TEST(in_the_pack_a_barrel_in_lava_goes_off) {
  const auto hall = [](bool pack) {
    return open_hall(pack, {}, {{{"classname", "misc_explobox"}, {"origin", "1600 700 0"}}});
  };
  auto h = hall(true);
  run(*h, kSecond);
  CHECK(h->named("misc_explobox").gone);
  auto base = hall(false);
  run(*base, kSecond);
  CHECK(!base->named("misc_explobox").gone);
}

TEST(in_the_pack_a_rising_plat_keeps_a_bad_area_until_it_is_down) {
  auto h = open_hall(true, {box(-200, -200, 0, -72, -72, 128)},
                     {{{"classname", "func_plat"}, {"model", "*1"}}});
  run(*h, kSecond / 2);
  sv::Q2Entity& plat = h->named("func_plat");
  CHECK(plat.state == sv::q2map::kAtFirst);  // at rest at the bottom
  CHECK(h->s().bad_areas.empty());
  plat.think = sv::q2map::kPlatUp;  // what a monster's ride asks: up at once
  plat.think_at = h->s().level_time + 0.001;
  run(*h, kSecond / 5);
  CHECK(h->s().bad_areas.size() == 1);
  if (h->s().bad_areas.empty()) return;
  CHECK(h->s().bad_areas[0].plat == h->index("func_plat"));
  // The raised plat's footprint, from its foot to 64 over it.
  CHECK(h->s().bad_areas[0].lo.z == 0 && h->s().bad_areas[0].hi.z == 64);
  CHECK(h->s().bad_areas[0].lo.x == -200 && h->s().bad_areas[0].hi.x == -72);
  run(*h, kSecond * 6);  // up, 3 s at the top, down again
  CHECK(h->named("func_plat").state == sv::q2map::kAtFirst);
  CHECK(h->s().bad_areas.empty());
}

TEST(in_the_pack_a_walker_with_its_enemy_above_sends_the_plat_it_stands_on_up) {
  const auto hall = [](bool pack) {
    auto h =
        open_hall(pack, {box(-200, -200, 0, -72, -72, 128), box(-1600, -100, 0, -1400, 100, 200)},
                  {{{"classname", "func_plat"}, {"model", "*1"}},
                   {{"classname", "func_wall"}, {"model", "*2"}},
                   {{"classname", "monster_soldier"}, {"origin", "-136 -136 40"}}});
    run(*h, kSecond / 2);
    return h;
  };
  auto h = hall(true);
  // Ana on her ledge, wholly above the soldier on the lowered plat.
  sv::map_place_player(h->s(), 0, {-1500, 0, 224}, {});
  run(*h, 2);
  CHECK(!h->s().monsters.empty());
  sv::Q2Monster& m = h->s().monsters[0];
  m.enemy = 0;
  CHECK(m.on_ground);
  CHECK(sv::gz_plat_ride(h->s(), m, {}));
  run(*h, kSecond / 5);
  CHECK(h->named("func_plat").state == sv::q2map::kGoing);
  auto base = hall(false);
  base->s().monsters[0].enemy = 0;
  CHECK(!sv::gz_plat_ride(base->s(), base->s().monsters[0], {}));
}

TEST(in_the_pack_co_op_spawns_its_own_entities_and_leaves_out_the_not_in_co_op_ones) {
  const auto hall = [](bool pack, std::vector<std::string> lines) {
    return open_hall(
        pack, {},
        {{{"classname", "misc_explobox"}, {"origin", "0 0 0"}, {"spawnflags", "4096"}},
         {{"classname", "misc_explobox"}, {"origin", "100 0 0"}, {"spawnflags", "1792"}},
         {{"classname", "misc_explobox"}, {"origin", "200 0 0"}}},
        lines);
  };
  const auto kept = [](const Hall& h) {
    return std::to_string(!h.named("misc_explobox", 0).gone) +
           std::to_string(!h.named("misc_explobox", 1).gone) +
           std::to_string(!h.named("misc_explobox", 2).gone);
  };
  // Co-op in the pack: never-in-co-op left out, the three skill bits shown at every skill.
  for (const char* skill : {"skill 0", "skill 1", "skill 2"}) {
    auto h = hall(true, {"coop 1", skill});
    CHECK_MSG(kept(*h) == "011", kept(*h));
  }
  // Single player, the base game's co-op, and coop_baseq2: the skill rule alone.
  for (const auto& [pack, lines] :
       {std::pair{true, std::vector<std::string>{}},
        std::pair{false, std::vector<std::string>{"coop 1"}},
        std::pair{true, std::vector<std::string>{"coop 1", "coop_baseq2 1"}}}) {
    auto h = hall(pack, lines);
    CHECK_MSG(kept(*h) == "101", kept(*h));
  }
}
