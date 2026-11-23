// Tests: Ground Zero's changes to the base monsters (games/shooters_edge/server/
// q2_gz_base_monsters.cpp): the tanks' faster rockets, the hornet's bracketing volley, the
// kamikaze's charge, the flyer's box and strafing burst, no pain in the air, the soldiers' spread,
// and the shared AI's data; each only under the pack.
//
// ge:summary Tests: Ground Zero's base monsters: rockets, volleys, the kamikaze, strafing, pain
// ge:keywords test q2 ground-zero rogue monsters tank hornet kamikaze flyer strafing-burst
// ge:keywords soldier spread ledge-jump
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_base_monsters_test.cpp
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_monster_ai.hpp"
#include "shooters_edge/server/q2_monster_pain.hpp"
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

// A long hall floored at 0 and roofed at 256, the player's start at its west end, and `things`.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& things) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  const float roof_lo[3] = {-1024, -256, 256}, roof_hi[3] = {1024, 256, 272};
  const float east_lo[3] = {1024, -256, 0}, east_hi[3] = {1040, 256, 256};
  const float west_lo[3] = {-1040, -256, 0}, west_hi[3] = {-1024, 256, 256};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(roof_lo, roof_hi, sh::kSqSolid, "world/roof"));
  w.models[0].push_back(br::box_brush(east_lo, east_hi, sh::kSqSolid, "world/wall"));
  w.models[0].push_back(br::box_brush(west_lo, west_hi, sh::kSqSolid, "world/wall"));
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

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& things, const char* skill = "1",
                                sv::Q2Pack pack = sv::Q2Pack::ground_zero) {
  auto h = std::make_unique<Hall>();
  sv::Q2ServerOptions o;
  o.pack = pack;
  h->r = sv::q2_runner(sv::make_q2_server(o));
  h->r.console("deathmatch 0");
  h->r.console(std::string("skill ") + skill);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(things);
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

int find(const Hall& h, const char* classname) {
  for (size_t i = 0; i < h.s().monsters.size(); i++)
    if (h.s().entities[size_t(h.s().monsters[i].entity)].classname == classname &&
        !h.s().monsters[i].spare)
      return static_cast<int>(i);
  return -1;
}

int bolts(const Hall& h) {
  int n = 0;
  for (const sv::Q2Missile& m : h.s().missiles) n += m.live;
  return n;
}

// The speed of the first rocket in flight (0 none).
float rocket_speed(const Hall& h) {
  for (const sv::Q2Missile& m : h.s().missiles)
    if (m.live && m.kind == sv::SqShot::rocket) return sh::length(m.velocity);
  return 0;
}

int rockets(const Hall& h) {
  int n = 0;
  for (const sv::Q2Missile& m : h.s().missiles) n += m.live && m.kind == sv::SqShot::rocket;
  return n;
}

// The first rocket's speed a tank facing Ana fires in `pack` at skill 2.
float tank_rocket(sv::Q2Pack pack) {
  auto h = open_hall({thing("monster_tank", "200 0 24", "180")}, "2", pack);
  run(*h, 2);
  h->m(find(*h, "monster_tank")).enemy = 0;
  for (int i = 0; i < 20 * kSecond; i++) {
    run(*h, 1);
    if (const float v = rocket_speed(*h); v > 0) return v;
    h->s().players[0].health = 100;  // Ana stays to be shot at
  }
  return 0;
}

}  // namespace

TEST(a_tanks_rockets_fly_500_and_100_a_skill_under_the_pack_only) {
  const float gz = tank_rocket(sv::Q2Pack::ground_zero);
  CHECK_MSG(gz > 699 && gz < 701, std::to_string(gz));
  const float base = tank_rocket(sv::Q2Pack::base);
  CHECK_MSG(base > 549 && base < 551, std::to_string(base));
}

TEST(the_hornet_brackets_a_players_path_with_four_rockets_at_750) {
  auto h = open_hall({thing("monster_boss2", "300 0 96", "180")}, "1");
  run(*h, 2);
  const int b = find(*h, "monster_boss2");
  CHECK(b >= 0);
  h->m(b).enemy = 0;
  int most = 0;
  bool fast = false;
  for (int i = 0; i < 30 * kSecond && !fast; i++) {
    run(*h, 1);
    most = std::max(most, rockets(*h));
    const float v = rocket_speed(*h);
    fast = v > 749 && v < 751;
    h->s().players[0].health = 100;
  }
  CHECK_MSG(fast, "no rocket at 750");
  CHECK(most >= 1);
}

TEST(a_kamikaze_charges_and_bursts_on_its_enemy_for_50) {
  auto h = open_hall({thing("monster_kamikaze", "100 0 96", "180")});
  run(*h, 2);
  const int k = find(*h, "monster_kamikaze");
  CHECK(k >= 0);
  CHECK(h->e(k).maxs.z == 16);
  h->m(k).enemy = 0;
  h->s().players[0].body.armour_count = 0;
  const float before = h->s().players[0].health;
  for (int i = 0; i < 4 * kSecond && !h->m(k).dead; i++) run(*h, 1);
  CHECK_MSG(h->m(k).dead, "it never reached Ana");
  CHECK_MSG(before - h->s().players[0].health == 50,
            std::to_string(before - h->s().players[0].health));
  // It feels no pain.
  auto g = open_hall({thing("monster_kamikaze", "800 0 96", "180")});
  run(*g, 2);
  const int j = find(*g, "monster_kamikaze");
  CHECK(sv::monster_damage(g->s(), g->m(j).entity, -1, 10, {}));
  CHECK(g->m(j).pain_at < 0);
}

TEST(a_flyer_has_the_packs_lower_box_and_strafes_its_bursts_above_easy) {
  auto h = open_hall({thing("monster_flyer", "300 0 96", "180")}, "3");
  run(*h, 2);
  const int f = find(*h, "monster_flyer");
  CHECK(f >= 0 && h->e(f).maxs.z == 16);
  h->m(f).enemy = 0;
  bool strafed = false;
  for (int i = 0; i < 20 * kSecond && !strafed; i++) {
    run(*h, 1);
    strafed = h->m(f).strafe != 0;
    h->s().players[0].health = 100;
  }
  CHECK_MSG(strafed, "it never strafed a burst at skill 3");
  // On easy it never does.
  auto e = open_hall({thing("monster_flyer", "300 0 96", "180")}, "0");
  run(*e, 2);
  const int g = find(*e, "monster_flyer");
  e->m(g).enemy = 0;
  for (int i = 0; i < 10 * kSecond; i++) {
    run(*e, 1);
    CHECK(e->m(g).strafe == 0);
    e->s().players[0].health = 100;
  }
}

TEST(the_infantry_feels_no_pain_in_the_air_and_soldiers_aim_tighter_on_hard) {
  auto h = open_hall(
      {thing("monster_infantry", "300 0 24", "180"), thing("monster_soldier", "400 0 24", "180")},
      "2");
  run(*h, kSecond);
  const int i = find(*h, "monster_infantry");
  CHECK(i >= 0);
  h->m(i).on_ground = false;
  const double pain = h->m(i).pain_at;
  CHECK(sv::monster_damage(h->s(), h->m(i).entity, -1, 10, {}));
  CHECK(h->m(i).pain_at == pain);
  const int s = find(*h, "monster_soldier");
  CHECK(ai::gz_base_spread(h->s(), h->m(s)) == 0.5f);
  auto base = open_hall({thing("monster_soldier", "400 0 24", "180")}, "2", sv::Q2Pack::base);
  run(*base, 2);
  CHECK(ai::gz_base_spread(base->s(), base->m(find(*base, "monster_soldier"))) == 1.0f);
}

TEST(the_shared_ai_reads_each_kinds_ledge_dodge_and_blind_fire) {
  const ai::GzLedge* l = ai::gz_ledge("monster_parasite");
  CHECK(l != nullptr && l->drop == 256 && l->climb == 68 && l->up_up == 450);
  CHECK(ai::gz_ledge("monster_tank") == nullptr);
  const ai::GzDodge* d = ai::gz_dodge_of("monster_berserk");
  CHECK(d != nullptr && !d->ducks && d->side_steps);
  const ai::GzBlind* b = ai::gz_blind_of("monster_tank");
  CHECK(b != nullptr && b->wait_lo == 3 && b->wait_hi == 5);
  CHECK(ai::gz_blind_of("monster_soldier") == nullptr);
}

TEST(the_maiden_wears_the_packs_model_and_the_hover_flinches_long_less_by_skill) {
  auto h = open_hall({thing("monster_chick", "300 0 24", "180")});
  run(*h, 2);
  const int c = find(*h, "monster_chick");
  CHECK(c >= 0);
  CHECK(h->s().models[size_t(h->e(c).modelindex)] == "models/monsters/bitch2/tris.md2");
  auto base = open_hall({thing("monster_chick", "300 0 24", "180")}, "1", sv::Q2Pack::base);
  run(*base, 2);
  const int b = find(*base, "monster_chick");
  CHECK(base->s().models[size_t(base->e(b).modelindex)] == "models/monsters/bitch/tris.md2");
  // The hover's heavy blow: the long flinch 0.45 − 0.1 a skill of the time, else the middle one.
  auto v = open_hall({thing("monster_hover", "300 0 96", "180")});
  run(*v, 2);
  CHECK(std::string(ai::gz_base_pain_class(v->s(), v->m(find(*v, "monster_hover")), "x")) ==
        "gz/monster_hover");
  for (int k = 0; k < 3; k++) {
    ge::core::Rng rng = ge::core::make_rng(uint32_t(7 + k));
    int longs = 0, mids = 0;
    for (int i = 0; i < 4000; i++) {
      const sv::PainChoice p =
          sv::monster_pain("gz/monster_hover", 30, k, 0, 0, 0, false, false, rng);
      longs += p.run.first == 113;
      mids += p.run.first == 141;
    }
    const float want = 0.45f - 0.1f * float(k);
    CHECK_MSG(longs + mids == 4000, std::to_string(longs + mids));
    CHECK_MSG(std::abs(float(longs) / 4000 - want) < 0.03f, std::to_string(longs));
  }
}

TEST(the_gunner_lobs_only_with_a_clear_line_and_the_parasite_drains_only_within_reach) {
  // Ana at -600: a gunner 60 off keeps to its chaingun, one 400 off may lob.
  auto h = open_hall(
      {thing("monster_gunner", "-540 0 24", "180"), thing("monster_parasite", "-200 0 24", "180")});
  run(*h, 2);
  const int g = find(*h, "monster_gunner");
  CHECK(g >= 0 && ai::attack_at(h->m(g), 3).fire == ai::Fire::grenade);
  h->m(g).enemy = 0;
  CHECK(ai::gz_base_pick(h->s(), h->m(g), 3, ai::Range::near) == 2);
  const int p = find(*h, "monster_parasite");
  h->m(p).enemy = 0;
  CHECK(ai::gz_base_pick(h->s(), h->m(p), 2, ai::Range::near) == -1);  // 400 off
  auto f = open_hall({thing("monster_gunner", "-200 0 24", "180"),
                      thing("monster_parasite", "-450 120 24", "180")});
  run(*f, 2);
  const int g2 = find(*f, "monster_gunner");
  f->m(g2).enemy = 0;
  CHECK(ai::gz_base_pick(f->s(), f->m(g2), 3, ai::Range::near) == 3);
  const int p2 = find(*f, "monster_parasite");
  f->m(p2).enemy = 0;
  CHECK(ai::gz_base_pick(f->s(), f->m(p2), 2, ai::Range::near) == 2);  // 150 off
  // Not under the pack: the pick stands.
  auto b = open_hall({thing("monster_gunner", "-540 0 24", "180")}, "1", sv::Q2Pack::base);
  run(*b, 2);
  const int g3 = find(*b, "monster_gunner");
  b->m(g3).enemy = 0;
  CHECK(ai::gz_base_pick(b->s(), b->m(g3), 3, ai::Range::near) == 3);
}

TEST(soldiers_run_and_fire_a_time_in_four_and_a_blind_one_stands_till_it_has_an_enemy) {
  auto h = open_hall({thing("monster_soldier_light", "0 0 24", "180"),
                      thing("monster_soldier_ss", "100 0 24", "180")},
                     "2");
  run(*h, 2);
  const int l = find(*h, "monster_soldier_light"), ss = find(*h, "monster_soldier_ss");
  CHECK(l >= 0 && ss >= 0);
  int runs = 0;
  for (int i = 0; i < 2000; i++) {
    runs += ai::gz_base_sight_run(h->s(), h->m(l), 600);
    CHECK(ai::gz_base_sight_run(h->s(), h->m(ss), 600) == 0);
  }
  CHECK_MSG(runs > 420 && runs < 580, std::to_string(runs));
  CHECK(ai::gz_base_sight_run(h->s(), h->m(l), 40) == 0);  // within melee range
  // The running fire's shot on its third frame, at Ana ahead of it.
  h->m(l).enemy = 0;
  h->e(l).angles.y = 180;
  h->m(l).run_shoot = 2;
  const int before = bolts(*h);
  CHECK(ai::gz_base_run_and_shoot(h->s(), h->m(l)));
  CHECK(bolts(*h) == before + 1);
  CHECK(h->m(l).run_shoot == 3);
  // Facing away, no shot.
  h->e(l).angles.y = 0;
  h->m(l).run_shoot = 2;
  CHECK(ai::gz_base_run_and_shoot(h->s(), h->m(l)));
  CHECK(bolts(*h) == before + 1);
  // Spawn flag 8: it stands blind, facing away from Ana.
  Pairs blind = thing("monster_soldier", "600 0 24", "0");
  blind.push_back({"spawnflags", "8"});
  auto g = open_hall({blind});
  run(*g, 5);
  const int b = find(*g, "monster_soldier");
  CHECK(g->m(b).enemy == -1);
  CHECK_MSG(g->m(b).pose.frame >= 146 && g->m(b).pose.frame <= 175,
            std::to_string(g->m(b).pose.frame));
}

TEST(a_monster_a_tesla_zaps_turns_on_it_keeps_off_its_ground_and_can_destroy_it) {
  auto h = open_hall({thing("monster_infantry", "100 0 24", "0")});
  run(*h, 2);
  const int i = find(*h, "monster_infantry");
  sv::Q2Monster& m = h->m(i);
  // An armed tesla of Ana's on the floor before it.
  int t = -1;
  for (int k = 0; k < int(std::size(h->s().missiles)) && t < 0; k++)
    if (!h->s().missiles[k].live) t = k;
  CHECK(t >= 0);
  sv::Q2Missile& tesla = h->s().missiles[t];
  tesla = {};
  tesla.live = true;
  tesla.kind = sv::SqShot::tesla;
  tesla.owner = 0;
  tesla.origin = {0, 0, 0};
  tesla.stage = 2;
  tesla.health = 30;
  tesla.dies_at = h->s().level_time + 30;
  run(*h, 20);
  CHECK_MSG(m.enemy == ai::foe_of_missile(t), std::to_string(m.enemy));
  CHECK(h->s().bad_areas.size() == 1);
  CHECK(ai::gz_bad_area_refuses(h->s(), m, {-10, 0, 0}));  // inside: not nearer the tesla
  CHECK(!ai::gz_bad_area_refuses(h->s(), m, {10, 0, 0}));  // but out
  for (int k = 0; k < 20 * kSecond && tesla.live; k++) {
    run(*h, 1);
    h->e(i).health = h->e(i).max_health;  // it outlasts the zaps
  }
  CHECK_MSG(!tesla.live, "the infantry never shot the tesla down");
  run(*h, kSecond / 5);
  CHECK(m.enemy != ai::foe_of_missile(t));  // back to Ana, or to none
}
