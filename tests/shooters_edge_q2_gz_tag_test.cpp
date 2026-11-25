// Tests: Ground Zero's tag game and its random respawns (games/shooters_edge/server/q2_gz.hpp):
// the token placed or made, taken with its prize, the holder's scores and quad, the token passed
// by a kill or thrown, its wait and its move, the blows a quarter less, the holder's trail and
// `tag3`; `randomrespawn` swapping a taken item for another of its kind.
//
// ge:summary Tests: Ground Zero's tag game (gamerules 2, the token, its scores) and randomrespawn
// ge:keywords test q2 ground-zero rogue tag gamerules dm-tag-token randomrespawn
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_tag_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_powers.hpp"
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

// A long hall with a floor at 0, walls at x ±1024 and a deathmatch start near each end; with
// `token` a tag token placed in the middle, with `quad` a quad damage beside it.
std::shared_ptr<const br::Map> hall(bool token, bool quad = false) {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -256, -16}, floor_hi[3] = {1024, 256, 0};
  const float east_lo[3] = {1024, -256, 0}, east_hi[3] = {1040, 256, 256};
  const float west_lo[3] = {-1040, -256, 0}, west_hi[3] = {-1024, 256, 256};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(east_lo, east_hi, sh::kSqSolid, "world/wall"));
  w.models[0].push_back(br::box_brush(west_lo, west_hi, sh::kSqSolid, "world/wall"));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-900 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "900 0 24"}, {"angle", "180"}}));
  if (token) w.entities.push_back(entity({{"classname", "dm_tag_token"}, {"origin", "0 0 24"}}));
  if (quad) w.entities.push_back(entity({{"classname", "item_quad"}, {"origin", "200 0 24"}}));
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
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p(int i) const { return s().players[i]; }
};

// `tenths` of host time in ticks of 13 ms, both standing still.
void run(Arena& a, int tenths) {
  const double end = a.s().host_time + tenths * 0.1;
  while (a.s().host_time + 1e-9 < end) {
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

void place(Arena& a, int i, float x) {
  int32_t* o = a.p(i).move.origin;
  o[0] = static_cast<int32_t>(x * 8);
  o[1] = 0;
  for (int32_t& v : a.p(i).move.velocity) v = 0;
}

// Ground Zero in deathmatch on the hall with `gamerules` and `extra` set first; Ana and Ben in.
std::unique_ptr<Arena> arena(const char* gamerules, bool token, bool quad = false,
                             const char* extra = nullptr) {
  auto a = std::make_unique<Arena>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  a->r = sv::q2_runner(sv::make_q2_server(o));
  a->r.console("deathmatch 1");
  a->r.console("sv_fps 10");
  a->r.console(std::string("gamerules ") + gamerules);
  if (extra != nullptr) a->r.console(extra);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(token, quad);
  std::string error;
  CHECK_MSG(a->r.start(map, error), error);
  a->r.connect(0, "Ana", false, false);
  a->r.connect(1, "Ben", false, false);
  run(*a, 10);
  return a;
}

int token_row() { return sv::sq_item("dm_tag_token", sv::Q2Pack::ground_zero); }

// The live drop that is the token, or null.
const sv::Q2Drop* token_drop(const Arena& a) {
  for (const sv::Q2Drop& d : a.s().drops)
    if (d.live && d.item == token_row()) return &d;
  return nullptr;
}

// The map's token entity that still lies there, or null.
const sv::Q2Entity* token_entity(const Arena& a) {
  for (const sv::Q2Entity& e : a.s().entities)
    if (e.item == token_row() && !e.gone && !e.hidden) return &e;
  return nullptr;
}

// Ana walks onto the map's token in the middle of the hall.
void ana_takes(Arena& a) {
  place(a, 0, 0);
  run(a, 1);
  CHECK(a.s().tag_holder == 0);
}

}  // namespace

TEST(the_token_stays_off_the_map_unless_tag_is_played) {
  auto off = arena("0", true);
  CHECK(token_entity(*off) == nullptr);
  CHECK(token_drop(*off) == nullptr);
  CHECK(!sv::gz_tag_on(off->s()));
  auto odd = arena("1", true);  // any other value plays as plain deathmatch
  CHECK(token_entity(*odd) == nullptr);
  auto on = arena("2", true);
  CHECK(sv::gz_tag_on(on->s()));
  CHECK(token_entity(*on) != nullptr);
  CHECK(token_drop(*on) == nullptr);  // the map placed one: none made
}

TEST(a_map_with_no_token_gets_one_at_a_deathmatch_start) {
  auto a = arena("2", false);
  // Made at a deathmatch start, where a player stands: taken at once, or lying there.
  const sv::Q2Drop* d = token_drop(*a);
  CHECK(d != nullptr || a->s().tag_holder >= 0);
  if (d != nullptr) {
    CHECK(d->origin.x == -900 || d->origin.x == 900);
    CHECK(d->dies_at == 0);
  } else {
    CHECK(a->p(a->s().tag_holder).body.counts[token_row()] == 1);
  }
  auto plain = arena("0", false);
  CHECK(token_drop(*plain) == nullptr && plain->s().tag_holder < 0);
}

TEST(taking_the_token_gives_its_prize_and_makes_the_holder) {
  auto a = arena("2", true);
  a->p(0).health = 40;
  ana_takes(*a);
  const sv::Q2Player& ana = a->p(0);
  CHECK(ana.body.counts[token_row()] == 1);
  CHECK(ana.health == 100);  // healed to the maximum
  CHECK(ana.body.armour == sv::SqArmour::body);
  CHECK(token_entity(*a) == nullptr);  // the map's token is gone, not to come back
  CHECK(sv::gz_tag_holder(a->s(), 0));
  CHECK(!sv::gz_tag_holder(a->s(), 1));
}

TEST(the_holder_scores_three_and_a_quad_every_fifth_kill) {
  auto a = arena("2", true, false, "dmflags 1024");  // the dead back at once
  ana_takes(*a);
  for (int k = 1; k <= 5; k++) {
    sv::fight_kill(a->s(), 1, 0, int(sv::SqWeapon::railgun));
    CHECK(a->p(0).frags == float(3 * k));
    run(*a, 30);  // Ben back
    place(*a, 1, 600);
  }
  const sv::SqPower* quad = sv::sq_power("item_quad");
  CHECK(quad != nullptr);
  if (quad != nullptr) CHECK(a->p(0).*quad->until > a->s().level_time + 25);
  CHECK(a->s().tag_kills == 0);
}

TEST(killing_the_holder_scores_five_and_takes_the_token) {
  auto a = arena("2", true);
  ana_takes(*a);
  place(*a, 1, 600);
  a->p(1).health = 30;
  sv::fight_kill(a->s(), 0, 1, int(sv::SqWeapon::railgun));
  CHECK(a->p(1).frags == 5);
  CHECK(a->s().tag_holder == 1);
  CHECK(a->p(1).health == 100);                  // the prize with it
  CHECK(a->p(1).body.counts[token_row()] == 0);  // won by a kill: no token carried
  CHECK(token_drop(*a) == nullptr);              // and none thrown
}

TEST(a_holder_dead_by_his_own_hand_throws_it_and_it_waits_then_moves) {
  auto a = arena("2", true);
  ana_takes(*a);
  sv::fight_kill(a->s(), 0, 0, -1);  // a suicide: one off as ever
  CHECK(a->p(0).frags == -1);
  CHECK(a->s().tag_holder == -1);
  const sv::Q2Drop* d = token_drop(*a);
  CHECK(d != nullptr);
  if (d == nullptr) return;
  CHECK(d->owner == -1);
  CHECK(d->takeable_at > a->s().level_time + 0.5);
  // Ben right on it in its first second may not take it.
  place(*a, 1, d->origin.x);
  run(*a, 1);
  CHECK(a->s().tag_holder == -1);
  place(*a, 1, 600);
  // Left alone: moved 30 s after its first second, to the start farthest from the living.
  run(*a, 20);
  const float before = token_drop(*a)->origin.x;
  run(*a, 300);
  d = token_drop(*a);
  CHECK(d != nullptr);
  if (d != nullptr) {
    CHECK(d->origin.x == -900);  // Ben stands at 600: the west start is the farther
    CHECK(d->origin.x != before);
  }
}

TEST(a_holder_who_leaves_throws_it) {
  auto a = arena("2", true);
  ana_takes(*a);
  a->r.drop(0);
  CHECK(a->s().tag_holder == -1);
  CHECK(token_drop(*a) != nullptr);
}

TEST(blows_without_the_holder_are_a_quarter_less) {
  auto a = arena("2", true);
  CHECK(sv::gz_tag_damage(a->s(), 1, -1, 40) == 30);  // nobody holds it: the world too
  CHECK(sv::gz_tag_damage(a->s(), 1, 0, 1) == 0);     // toward zero
  ana_takes(*a);
  CHECK(sv::gz_tag_damage(a->s(), 0, 1, 40) == 40);  // to the holder
  CHECK(sv::gz_tag_damage(a->s(), 1, 0, 40) == 40);  // from the holder
  auto plain = arena("0", true);
  CHECK(sv::gz_tag_damage(plain->s(), 1, -1, 40) == 40);
}

TEST(randomrespawn_brings_a_taken_item_back_as_another_of_its_kind) {
  auto a = arena("0", false, true, "randomrespawn 1");
  sv::Q2Entity* quad = nullptr;
  for (sv::Q2Entity& e : a->s().entities)
    if (e.classname == "item_quad") quad = &e;
  CHECK(quad != nullptr);
  if (quad == nullptr) return;
  CHECK(!sv::gz_random_respawn(a->s(), *quad));  // as the map placed it: never swapped
  int swapped = 0;
  for (int i = 0; i < 40; i++) {
    quad->picked = true;
    const int was = quad->item;
    if (sv::gz_random_respawn(a->s(), *quad)) {
      swapped++;
      CHECK(quad->item != was);
      CHECK(sv::kSqItems[quad->item].kind == sv::SqKind::powerup);
      CHECK(quad->classname == sv::kSqItems[quad->item].classname);
      CHECK(quad->classname != "item_adrenaline");
    }
    CHECK(!quad->picked);
  }
  CHECK(swapped > 20);
  a->r.console("randomrespawn 0");
  quad->picked = true;
  CHECK(!sv::gz_random_respawn(a->s(), *quad));
}

TEST(randomrespawn_keeps_the_small_health_and_the_shard) {
  auto a = arena("0", false, false, "randomrespawn 1");
  sv::Q2Entity e;
  e.item = sv::sq_item("item_health_small", sv::Q2Pack::ground_zero);
  e.picked = true;
  CHECK(!sv::gz_random_respawn(a->s(), e));
  e.item = sv::sq_item("item_armor_shard", sv::Q2Pack::ground_zero);
  e.picked = true;
  CHECK(!sv::gz_random_respawn(a->s(), e));
  // Armour by its odds: jacket, combat or body.
  int seen[3] = {};
  for (int i = 0; i < 200; i++) {
    e.item = sv::sq_item("item_armor_body", sv::Q2Pack::ground_zero);
    e.picked = true;
    sv::gz_random_respawn(a->s(), e);
    const std::string cn = sv::kSqItems[e.item].classname;
    seen[cn == "item_armor_jacket" ? 0 : cn == "item_armor_combat" ? 1 : 2]++;
  }
  CHECK(seen[0] > seen[1] && seen[1] > seen[2] && seen[2] > 0);
}
