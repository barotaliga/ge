// Tests: Ground Zero's map things (games/shooters_edge/server/q2_gz_things.cpp) in a hall built
// here: a steam jet's puff and its lasting form, the reactor core shown and hidden by uses, the
// black light and the orb turning and gone in deathmatch, the player killer, and the disguise a
// trigger gives, a shot betrays and the pack's command gives.
//
// ge:summary Tests: Ground Zero's steam jets, reactor core, black light, orb, killer, disguise
// ge:keywords test q2 ground-zero target-steam misc-nuke-core target-blacklight target-orb
// ge:keywords target-killplayers trigger-disguise disguise violator te-steam
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_things_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
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

// A floored hall with a start at x -600 and a field round it (model 1), a block of lava (model 2),
// and `extra` entities.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& extra) {
  br::BrushWorld w;
  w.models.resize(3);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  const float flo[3] = {-660, -60, 0}, fhi[3] = {-540, 60, 96};
  w.models[1].push_back(br::box_brush(flo, fhi, sh::kSqSolid, "world/trigger"));
  const float llo[3] = {300, -64, 0}, lhi[3] = {400, 64, 64};
  w.models[2].push_back(br::box_brush(llo, lhi, sh::kSqLava, "world/lava"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  for (const Pairs& m : extra) w.entities.push_back(entity(m));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Game {
  sv::RulesRunner r;
  sv::MapSpec map;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p() const { return s().players[0]; }
  int find(const char* classname) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].classname == classname) return static_cast<int>(i);
    return -1;
  }
  int named(const char* name) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].targetname == name) return static_cast<int>(i);
    return -1;
  }
  sv::Q2Entity& at(int e) const { return s().entities[size_t(e)]; }
};

std::unique_ptr<Game> open_game(const std::vector<Pairs>& extra, const char* dm = "0",
                                sv::Q2Pack pack = sv::Q2Pack::ground_zero, const char* coop = "0",
                                const char* name = "hall") {
  auto g = std::make_unique<Game>();
  sv::Q2ServerOptions o;
  o.pack = pack;
  g->r = sv::q2_runner(sv::make_q2_server(o));
  g->r.console(std::string("deathmatch ") + dm);
  g->r.console(std::string("coop ") + coop);
  g->map.name = name;
  g->map.brushes = hall(extra);
  std::string error;
  CHECK_MSG(g->r.start(g->map, error), error);
  g->r.connect(0, "Ana", false, false);
  return g;
}

void run(Game& g, int ticks) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++g.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    g.r.frame(&pk, 1);
  }
}

Pairs relay(const char* name, const char* target) {
  return {{"classname", "trigger_relay"}, {"targetname", name}, {"target", target}};
}

const sv::NetEvent* steam(const Game& g) {
  const sv::NetEvent* last = nullptr;
  for (const sv::NetEvent& ev : g.s().out.events)
    if (ev.text == "te_steam") last = &ev;
  return last;
}

}  // namespace

TEST(a_steam_jet_puffs_along_its_angle_and_a_timer_makes_it_last) {
  auto g = open_game(
      {relay("go", "jet"),
       {{"classname", "target_steam"},
        {"targetname", "jet"},
        {"origin", "0 0 64"},
        {"angle", "90"},
        {"count", "20"},
        {"sounds", "208"},
        {"speed", "120"}},
       {{"classname", "func_timer"}, {"targetname", "tick"}, {"wait", "1"}, {"target", "jet2"}},
       {{"classname", "target_steam"},
        {"targetname", "jet2"},
        {"origin", "0 0 64"},
        {"angle", "0"}}});
  run(*g, 10);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  const sv::NetEvent* e = steam(*g);
  CHECK(e != nullptr);
  CHECK(e->ent == -1);  // a relay has no wait: a puff
  CHECK(e->values[3] == 20 && e->level == 208 && e->dir.x == 120);
  CHECK(e->values[1] > 0.99f && e->dir.y == 0);  // along +y
  // Used from a timer that fires each second, it lasts its second: the long form, with an id.
  g->s().out.events.clear();
  sv::map_use_targets(g->s(), g->named("tick"), -1);
  e = steam(*g);
  CHECK(e != nullptr);
  CHECK(e->ent >= 0 && e->dir.y == 1000);
  CHECK(e->values[3] == 32 && e->level == 8 && e->dir.x == 75);  // its defaults
}

TEST(the_reactor_core_shows_and_hides_by_its_uses) {
  auto g =
      open_game({relay("go", "core"),
                 {{"classname", "misc_nuke_core"}, {"targetname", "core"}, {"origin", "0 0 64"}}});
  run(*g, 10);
  const int core = g->find("misc_nuke_core");
  CHECK(core >= 0 && !g->at(core).hidden && g->at(core).modelindex > 0);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(g->at(core).hidden);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(!g->at(core).hidden);
}

TEST(the_black_light_and_the_orb_turn_and_are_single_players) {
  auto g = open_game({{{"classname", "target_blacklight"}, {"origin", "0 0 64"}},
                      {{"classname", "target_orb"}, {"origin", "64 0 64"}}});
  run(*g, 10);
  const int light = g->find("target_blacklight"), orb = g->find("target_orb");
  CHECK(!g->at(light).gone && g->at(light).frame == 1 && g->at(orb).frame == 2);
  const sh::Vec3f before = g->at(light).angles;
  run(*g, 20);
  const sh::Vec3f after = g->at(light).angles;
  CHECK(before.x != after.x || before.y != after.y || before.z != after.z);
  auto dm = open_game({{{"classname", "target_blacklight"}, {"origin", "0 0 64"}}}, "1");
  CHECK(dm->at(dm->find("target_blacklight")).gone);
}

TEST(the_player_killer_kills_every_player) {
  auto g =
      open_game({relay("go", "end"), {{"classname", "target_killplayers"}, {"targetname", "end"}}});
  run(*g, 40);
  CHECK(g->p().alive);
  g->r.client_command(0, "god");
  sv::map_use_targets(g->s(), g->named("go"), 0);
  CHECK(!g->p().alive);  // past god mode
}

TEST(the_disguise_trigger_gives_it_and_a_shot_gives_him_away) {
  auto g = open_game({{{"classname", "trigger_disguise"}, {"model", "*1"}, {"spawnflags", "2"}}});
  run(*g, 40);
  CHECK(g->p().disguised);
  CHECK(!sv::monster_targetable(g->s(), 0));
  sv::monster_noise(g->s(), 0);  // his weapon
  CHECK(sv::monster_targetable(g->s(), 0));
  run(*g, 77);
  CHECK(!sv::monster_targetable(g->s(), 0));
}

TEST(the_disguise_trigger_starts_off_and_a_use_turns_it_on) {
  auto g =
      open_game({relay("go", "mask"),
                 {{"classname", "trigger_disguise"}, {"model", "*1"}, {"targetname", "mask"}}});
  run(*g, 40);
  CHECK(!g->p().disguised);
  sv::map_use_targets(g->s(), g->named("go"), 0);
  run(*g, 5);
  CHECK(g->p().disguised);
}

TEST(the_pack_s_names_are_nothing_to_the_base_game) {
  auto g =
      open_game({{{"classname", "misc_nuke_core"}, {"origin", "0 0 64"}}}, "0", sv::Q2Pack::base);
  run(*g, 10);
  CHECK(g->at(g->find("misc_nuke_core")).gone);
}

TEST(co_op_on_the_rising_lava_map_starts_at_the_lowest_lava_start_above_the_lava) {
  const std::vector<Pairs> extra = {
      {{"classname", "func_water"},  // the smart water, as the map places it
       {"model", "*2"},
       {"spawnflags", "2"},
       {"targetname", "rise"}},
      {{"classname", "info_player_coop_lava"}, {"origin", "0 0 100"}},  // under the line (128)
      {{"classname", "info_player_coop_lava"}, {"origin", "200 0 300"}},
      {{"classname", "info_player_coop_lava"}, {"origin", "-200 0 200"}}};
  auto g = open_game(extra, "0", sv::Q2Pack::ground_zero, "1", "rmine2");
  run(*g, 20);
  const float x = float(g->p().move.origin[0]) / 8;
  CHECK_MSG(x == -200, std::to_string(x));
  auto plain = open_game(extra, "0", sv::Q2Pack::ground_zero, "1", "hall");
  run(*plain, 20);
  CHECK(float(plain->p().move.origin[0]) / 8 == -600);  // elsewhere the usual start
  auto single = open_game(extra);
  CHECK(single->find("info_player_coop_lava") < 0 ||
        single->at(single->find("info_player_coop_lava")).gone);  // co-op's alone
}

// Smart water: used, the lava rises (at its creep, the player being below its top), fires its
// targets, and stops at the height its `health` names; in the base game it is plain water.
TEST(a_used_smart_water_rises_after_the_players_and_stops_at_its_height) {
  const std::vector<Pairs> extra = {relay("go", "rise"),
                                    {{"classname", "func_water"},
                                     {"model", "*2"},
                                     {"spawnflags", "2"},
                                     {"targetname", "rise"},
                                     {"target", "after"},
                                     {"health", "74"}},
                                    {{"classname", "target_steam"}, {"targetname", "after"}}};
  auto g = open_game(extra);
  run(*g, 20);
  const int lava = g->find("func_water");
  const float start = g->at(lava).origin.z + g->at(lava).maxs.z;
  CHECK_MSG(start == 64, std::to_string(start));
  sv::map_use_targets(g->s(), g->named("go"), -1);
  CHECK(steam(*g) != nullptr);  // its target fired as it starts
  double t0 = g->s().level_time;
  while (g->s().level_time < t0 + 1) run(*g, 1);
  const float risen = g->at(lava).origin.z + g->at(lava).maxs.z;
  CHECK_MSG(risen > 66 && risen < 72, std::to_string(risen));  // about 5 a second
  t0 = g->s().level_time;
  while (g->s().level_time < t0 + 4) run(*g, 1);
  const float top = g->at(lava).origin.z + g->at(lava).maxs.z;
  CHECK_MSG(top >= 74 && top < 75, std::to_string(top));
  auto base = open_game(extra, "0", sv::Q2Pack::base);
  run(*base, 20);
  sv::map_use_targets(base->s(), base->named("go"), -1);
  CHECK((base->at(base->find("func_water")).count & 4) == 0);  // plain water: no rising
}

TEST(co_op_leaves_out_what_says_not_in_co_op_and_takes_what_every_skill_leaves_out) {
  const std::vector<Pairs> extra = {
      {{"classname", "weapon_shotgun"}, {"origin", "0 0 16"}, {"spawnflags", "4096"}},
      {{"classname", "weapon_railgun"}, {"origin", "64 0 16"}, {"spawnflags", "1792"}}};
  auto g = open_game(extra, "0", sv::Q2Pack::ground_zero, "1");
  CHECK(g->at(g->find("weapon_shotgun")).gone);
  CHECK(!g->at(g->find("weapon_railgun")).gone);
  auto sp = open_game(extra);
  CHECK(!sp->at(sp->find("weapon_shotgun")).gone && sp->at(sp->find("weapon_railgun")).gone);
}

TEST(an_anger_sets_its_monsters_on_a_point_and_they_fire_at_it) {
  auto g =
      open_game({relay("go", "angry"),
                 {{"classname", "target_anger"},
                  {"targetname", "angry"},
                  {"target", "guard"},
                  {"killtarget", "spot"}},
                 {{"classname", "monster_soldier"},
                  {"targetname", "guard"},
                  {"origin", "200 0 24"},
                  {"angle", "180"},
                  {"spawnflags", "1"}},
                 {{"classname", "info_notnull"}, {"targetname", "spot"}, {"origin", "600 0 40"}}});
  run(*g, 40);
  const int spot = g->named("spot");
  sv::map_use_targets(g->s(), g->named("go"), -1);
  CHECK(g->at(spot).health == 300);
  const sv::Q2Monster& m = g->s().monsters[0];
  CHECK_MSG(m.enemy == -2 - spot, std::to_string(m.enemy));
  run(*g, 4 * 77);
  bool fired = false;
  for (const sv::NetEvent& ev : g->s().out.events)
    fired = fired || ev.text.rfind("te_monster_flash", 0) == 0;
  CHECK(fired);
}

// Set on its target, a monster shrugs off the player's blows until its health is down to a third;
// then the anger is spent and it turns on him. A point target is fired at every chance, and a
// wall's middle is aimed at, not its origin.
TEST(an_angry_monster_ignores_other_blows_until_a_third_of_its_health) {
  auto g =
      open_game({relay("go", "angry"),
                 {{"classname", "target_anger"},
                  {"targetname", "angry"},
                  {"target", "guard"},
                  {"killtarget", "spot"}},
                 {{"classname", "monster_soldier"},
                  {"targetname", "guard"},
                  {"origin", "200 0 24"},
                  {"angle", "180"},
                  {"spawnflags", "1"}},
                 {{"classname", "info_notnull"}, {"targetname", "spot"}, {"origin", "600 0 40"}}});
  run(*g, 40);
  const int spot = g->named("spot");
  sv::map_use_targets(g->s(), g->named("go"), -1);
  sv::Q2Monster& m = g->s().monsters[0];
  CHECK(m.anger == spot && sv::gz_anger_point(g->s(), m));
  sv::Q2Entity& body = g->at(m.entity);
  sv::monster_damage(g->s(), m.entity, 0, 2, {1, 0, 0});
  CHECK_MSG(m.enemy == -2 - spot, std::to_string(m.enemy));
  body.health = body.max_health / 3 + 1;
  sv::monster_damage(g->s(), m.entity, 0, 2, {1, 0, 0});  // down to a third: spent
  CHECK_MSG(m.enemy == 0 && m.anger == -1, std::to_string(m.enemy));
  // A box's middle and its half size, about the middle.
  sv::Q2Entity& t = g->at(spot);
  t.mins = {10, 20, 30};
  t.maxs = {30, 60, 90};
  const sh::Vec3f mid = sv::gz_anger_at(g->s(), spot);
  CHECK(mid.x == 620 && mid.y == 40 && mid.z == 100);
  sh::Vec3f lo, hi;
  sv::gz_anger_box(g->s(), spot, lo, hi);
  CHECK(lo.x == -10 && hi.y == 20 && hi.z == 30);
}

TEST(an_anger_without_its_killtarget_says_so_and_goes) {
  auto g = open_game({{{"classname", "target_anger"}, {"target", "guard"}}});
  CHECK(g->at(g->find("target_anger")).gone);
  bool said = false;
  for (const std::string& l : g->s().console)
    said = said || l == "target_anger without killtarget!";
  CHECK(said);
}
