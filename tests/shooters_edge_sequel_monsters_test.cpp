// Tests: the sequel's monsters (games/shooters_edge/server/sequel_monsters.cpp) in a hall built
// here: removed in deathmatch; a guard facing a player sees him, turns on him and shoots him; one
// facing away does not, until the player's weapon is heard; shot dead it leaves a corpse shots
// pass, counts as killed, fires its targets and drops its item; a big blow throws it apart; one
// with a path walks it; one waiting hidden appears on a use and turns on the user.
//
// ge:summary Tests: the sequel's monsters: sight, noise, attack, death, corpse, gibs, path, waiting
// ge:keywords test sequel monsters monster-ai sight noise corpse gib path-corner triggered-spawn
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_monsters_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_monsters.hpp"
#include "shooters_edge/server/sequel_server.hpp"

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

// A long floored hall, the player's start at its west end, and `monsters` placed in it.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& monsters) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  for (const Pairs& m : monsters) w.entities.push_back(entity(m));
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
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p() const { return s().players[0]; }
  sv::SequelMonster& m(int i = 0) const { return s().monsters[size_t(i)]; }
  sv::SequelEntity& e(int i = 0) const { return s().entities[size_t(m(i).entity)]; }
};

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& monsters, const char* dm = "0") {
  auto h = std::make_unique<Hall>();
  h->r = sv::sequel_runner(sv::make_sequel_server({}));
  h->r.console(std::string("deathmatch ") + dm);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(monsters);
  std::string error;
  CHECK_MSG(h->r.start(map, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

// Host ticks of 13 ms, Ana looking along `yaw` with `buttons`.
void run(Hall& h, int ticks, uint8_t buttons = 0, float yaw = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++h.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    c.buttons = buttons;
    c.angles = {0, yaw, 0};
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    h.r.frame(&pk, 1);
  }
}

Pairs guard(const char* origin, const char* angle) {
  return {{"classname", "monster_soldier_light"}, {"origin", origin}, {"angle", angle}};
}

constexpr int kSecond = 77;  // host ticks of 13 ms in a second

}  // namespace

TEST(monsters_are_not_spawned_in_deathmatch) {
  auto h = open_hall({guard("0 0 24", "180")}, "1");
  run(*h, 10);
  CHECK(h->s().monsters.empty());
  CHECK(h->s().monsters_total == 0);
}

TEST(a_guard_facing_the_player_sees_him_and_shoots_him) {
  auto h = open_hall({guard("0 0 24", "180")});
  run(*h, 10);
  CHECK(h->s().monsters_total == 1);
  CHECK_MSG(h->e().origin.z >= 24 && h->e().origin.z < 24.1f,
            std::to_string(h->e().origin.z));  // on the floor
  run(*h, kSecond);
  CHECK(h->m().enemy == 0);
  for (int i = 0; i < 30 && h->p().alive && h->p().health == 100; i++) run(*h, kSecond);
  CHECK_MSG(h->p().health < 100, std::to_string(h->p().health));
  CHECK(h->e().origin.x < 0);  // it came on toward him
}

TEST(a_guard_facing_away_is_deaf_to_nothing_and_hears_a_shot) {
  auto h = open_hall({guard("0 0 24", "0")});
  run(*h, 3 * kSecond);
  CHECK(h->m().enemy == -1);
  run(*h, kSecond / 2, sh::kButtonAttack);  // the blaster: heard
  CHECK(h->m().enemy == 0);
}

TEST(a_guard_shot_dead_leaves_a_corpse_counts_and_drops_its_item) {
  Pairs g = guard("0 0 24", "0");
  g.push_back({"item", "ammo_shells"});
  auto h = open_hall({g});
  run(*h, 10);
  // Ana's blaster at it down the hall until it falls.
  for (int i = 0; i < 20 && !h->m().dead; i++) run(*h, kSecond / 2, sh::kButtonAttack);
  CHECK(h->m().dead && !h->e().box && !h->e().gone);
  CHECK(h->s().monsters_killed == 1);
  bool dropped = false;
  for (const sv::SequelDrop& d : h->s().drops)
    if (d.live && d.item == sv::sq_item("ammo_shells")) dropped = true;
  CHECK(dropped);
  // A line passes the corpse.
  const sh::SequelTrace t =
      sv::map_trace(h->s(), {-100, 0, 4}, {}, {}, {100, 0, 4}, sh::kSqSolid | sh::kSqMonster);
  CHECK(t.fraction == 1);
}

TEST(a_big_blow_throws_a_monster_apart) {
  auto h = open_hall({guard("0 0 24", "0")});
  run(*h, 10);
  CHECK(sv::map_damage(h->s(), h->m().entity, 0, 200));
  CHECK(h->m().dead && h->e().gone);
  run(*h, 1);
  CHECK(h->r->world.entities[size_t(sv::map_row(h->m().entity))].free);
}

TEST(a_monster_with_a_path_walks_it_until_it_finds_an_enemy) {
  auto h = open_hall({{{"classname", "monster_infantry"},
                       {"origin", "800 0 24"},
                       {"angle", "0"},
                       {"target", "c1"}},
                      {{"classname", "path_corner"}, {"origin", "500 0 8"}, {"targetname", "c1"}}});
  run(*h, 10);
  const float x0 = h->e().origin.x;
  run(*h, 3 * kSecond);  // 50 a second toward the corner, Ana beyond its sight
  CHECK_MSG(h->e().origin.x < x0 - 100, std::to_string(h->e().origin.x));
  CHECK(h->m().enemy == -1);
}

TEST(a_waiting_monster_appears_on_a_use_and_turns_on_the_user) {
  Pairs g = guard("0 0 24", "0");
  g.push_back({"spawnflags", "2"});
  g.push_back({"targetname", "ambush"});
  auto h = open_hall({g});
  run(*h, 10);
  CHECK(h->m().waiting && h->e().hidden && !h->e().box);
  CHECK(h->r->world.entities[size_t(sv::map_row(h->m().entity))].free);
  sv::monster_use(h->s(), h->m().entity, 0);  // used by Ana, as her trigger would
  CHECK(!h->m().waiting && h->e().box && h->m().enemy == 0);
  run(*h, 1);
  CHECK(!h->r->world.entities[size_t(sv::map_row(h->m().entity))].free);
}
