// Tests: Ground Zero's shared monster AI (games/shooters_edge/server/q2_gz_ai.cpp): the dodge that
// ducks a high shot and side-steps a low one, walkers strafing, blind fire at where the player was
// last seen, the jump down a ledge, the medic's raised dead out of the count, and the monsters
// whose shots start no fight; the base game's rules kept outside the pack.
//
// ge:summary Tests: Ground Zero's shared monster AI: dodge, strafing, blind fire, jumps, medics
// ge:keywords test q2 ground-zero rogue monsters dodge duck side-step strafe blind-fire
// ge:keywords ledge-jump medic revive ignore-shots infighting
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_ai_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace ai = shooters_edge::server::monster_ai;

using Pairs = std::vector<std::pair<std::string, std::string>>;

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long hall floored at 0 and roofed at 256 with a wall at x 512, the player's start at its west
// end; `ledge` raises the floor east of x -100 to 64 (and the wall's west face stays at 512).
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& things, bool ledge) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  const float wall_lo[3] = {512, -256, 0}, wall_hi[3] = {528, 256, 256};
  const float roof_lo[3] = {-1024, -256, 256}, roof_hi[3] = {1024, 256, 272};
  const float step_lo[3] = {-100, -256, 0}, step_hi[3] = {512, 256, 64};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(roof_lo, roof_hi, sh::kSqSolid, "world/roof"));
  w.models[0].push_back(br::box_brush(wall_lo, wall_hi, sh::kSqSolid, "world/wall"));
  if (ledge) w.models[0].push_back(br::box_brush(step_lo, step_hi, sh::kSqSolid, "world/ledge"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  for (const Pairs& m : things) w.entities.push_back(entity(m));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Hall {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Monster& m(int i = 0) const { return s().monsters[size_t(i)]; }
  sv::Q2Entity& e(int i = 0) const { return s().entities[size_t(m(i).entity)]; }
};

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& things, bool pack = true,
                                const char* skill = "3", bool ledge = false) {
  auto h = std::make_unique<Hall>();
  sv::Q2ServerOptions o;
  if (pack) o.pack = sv::Q2Pack::ground_zero;
  h->r = sv::q2_runner(sv::make_q2_server(o));
  h->r.console("deathmatch 0");
  h->r.console(std::string("skill ") + skill);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(things, ledge);
  std::string error;
  CHECK_MSG(h->r.start(map, error), error);
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

Pairs thing(const char* classname, const char* origin, const char* angle) {
  return {{"classname", classname}, {"origin", origin}, {"angle", angle}};
}

}  // namespace

TEST(on_nightmare_a_soldier_ducks_a_high_shot_and_side_steps_a_low_one) {
  auto h = open_hall({thing("monster_soldier_light", "-300 0 24", "180")});
  run(*h, 2);
  const sh::Vec3f at = h->e().origin;
  // A bolt at 1000 a second from 300 off: high (at its eye) is ducked, a hold of eta and no more.
  sv::monster_dodge(h->s(), h->m().entity, 0, 300, 1000, {at.x, at.y, at.z + 20});
  CHECK(h->m().duck_frame == 0 && h->m().dodge_until == 0);
  CHECK_MSG(
      h->m().duck_hold > h->s().level_time + 0.25 && h->m().duck_hold < h->s().level_time + 0.3,
      std::to_string(h->m().duck_hold - h->s().level_time));
  CHECK(h->m().enemy == 0);
  // Low (at its knees), it steps aside instead, away from the side the shot strikes.
  auto g = open_hall({thing("monster_soldier_light", "-300 0 24", "180")});
  run(*g, 2);
  const sh::Vec3f to = g->e().origin;
  sv::monster_dodge(g->s(), g->m().entity, 0, 300, 1000, {to.x, to.y - 10, to.z - 20});
  CHECK(g->m().duck_frame == -1 && g->m().dodge_until > g->s().level_time);
  const float y0 = g->e().origin.y;
  run(*g, kSecond / 4);
  CHECK_MSG(g->e().origin.y > y0 + 4, std::to_string(g->e().origin.y - y0));  // to its other side
}

TEST(a_berserker_only_side_steps_and_outside_the_pack_never_dodges) {
  auto h = open_hall({thing("monster_berserk", "-300 0 24", "180")});
  run(*h, 2);
  const sh::Vec3f at = h->e().origin;
  sv::monster_dodge(h->s(), h->m().entity, 0, 300, 1000, {at.x, at.y, at.z + 20});
  CHECK(h->m().dodge_until > h->s().level_time && h->m().duck_frame == -1);
  auto base = open_hall({thing("monster_berserk", "-300 0 24", "180")}, false);
  run(*base, 2);
  sv::monster_dodge(base->s(), base->m().entity, 0, 300, 1000, {at.x, at.y, at.z + 20});
  CHECK(base->m().dodge_until == 0 && base->m().duck_frame == -1);
}

TEST(walkers_strafe_four_times_in_ten_fliers_six_the_daedalus_eight) {
  auto h = open_hall({thing("monster_gunner", "-300 0 24", "180"),
                      thing("monster_flyer", "-300 100 96", "180"),
                      thing("monster_daedalus", "-300 -100 96", "180")});
  run(*h, 2);
  CHECK(ai::gz_slide_chance(h->s(), h->m(0), ai::Range::near) == 0.4);
  CHECK(ai::gz_slide_chance(h->s(), h->m(1), ai::Range::near) == 0.6);
  CHECK(ai::gz_slide_chance(h->s(), h->m(2), ai::Range::near) == 0.8);
  CHECK(ai::gz_slide_chance(h->s(), h->m(0), ai::Range::melee) == 0);
  CHECK(ai::gz_slide_step(h->s(), h->m(0), 20) == 8 &&
        ai::gz_slide_step(h->s(), h->m(1), 20) == 20);
  auto base = open_hall({thing("monster_gunner", "-300 0 24", "180")}, false);
  run(*base, 2);
  CHECK(ai::gz_slide_chance(base->s(), base->m(0), ai::Range::near) == -1);
}

TEST(a_light_soldier_behind_a_wall_fires_blind_at_where_ana_was_and_less_often_after) {
  // Beyond the wall at 512: it saw Ana a moment ago at her start, then lost her.
  auto h = open_hall({thing("monster_soldier_light", "700 0 24", "180")});
  run(*h, 2);
  sv::Q2Monster& m = h->m();
  m.enemy = 0;
  m.blind_at = {-600, 0, 24};
  m.trail_time = h->s().level_time;
  m.attack_finished = 0;
  int bolts = 0;
  for (int i = 0; i < 4 * kSecond && bolts == 0; i++) {
    run(*h, 1);
    for (const sv::Q2Missile& b : h->s().missiles) bolts += b.live && b.kind == sv::SqShot::bolt;
    m.lost = false;  // it stays where it is
  }
  CHECK_MSG(bolts > 0, "it never fired blind");
  CHECK_MSG(m.blind_delay >= 4.1 && m.blind_delay <= 7.1, std::to_string(m.blind_delay));
}

TEST(an_infantryman_on_a_ledge_jumps_down_to_ana_and_not_outside_the_pack) {
  for (const bool pack : {true, false}) {
    auto h = open_hall({thing("monster_infantry", "-40 0 88", "180")}, pack, "3", true);
    run(*h, 2);
    h->m().enemy = 0;
    for (int i = 0; i < 4 * kSecond; i++) {
      h->m().attack_finished = 1e9;  // it holds its fire: only the chase
      run(*h, 1);
    }
    if (pack)
      CHECK_MSG(h->e().origin.z < 40, std::to_string(h->e().origin.z));
    else
      CHECK_MSG(h->e().origin.z > 80, std::to_string(h->e().origin.z));
  }
}

TEST(a_medics_shot_starts_no_fight_and_its_raised_dead_are_not_counted) {
  auto h = open_hall(
      {thing("monster_soldier", "100 0 24", "0"), thing("monster_medic", "250 0 24", "0")});
  run(*h, 10);
  // The medic's bolt hits the guard: in the pack, no fight; outside it, the guard turns on it.
  CHECK(sv::monster_damage(h->s(), h->m(0).entity, -2 - h->m(1).entity, 1, {1, 0, 0}));
  CHECK(h->m(0).enemy == -1);
  auto base = open_hall(
      {thing("monster_soldier", "100 0 24", "0"), thing("monster_medic", "250 0 24", "0")}, false);
  run(*base, 10);
  CHECK(sv::monster_damage(base->s(), base->m(0).entity, -2 - base->m(1).entity, 1, {1, 0, 0}));
  CHECK(base->m(0).enemy == -2 - base->m(1).entity);
  // Killed and raised: back, but not counted again.
  CHECK(sv::map_damage(h->s(), h->m(0).entity, 0, 12));
  CHECK(sv::map_damage(h->s(), h->m(0).entity, 0, 12));
  CHECK(h->m(0).dead);
  for (int i = 0; i < 10 && h->m(0).dead; i++) run(*h, kSecond);
  CHECK_MSG(!h->m(0).dead, "the medic never raised the guard");
  CHECK(h->m(0).revived && h->s().monsters_total == 2);
}
