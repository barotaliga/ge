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
