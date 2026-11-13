// Tests: the sequel's poses (games/shooters_edge/server/sequel_poses.cpp) in a hall built here: a
// spawned player brings his weapon up, then stands, runs, shoots, flinches at a blow, dies on a
// death held to its last frame and leaves it to his corpse; a guard stands, attacks the player it
// sees and dies on a death it holds; items turn where they lie, but not ammo, health or one placed
// untouchable, and the frames and the turn reach the world's rows; a living player carries his
// weapon for others to see, a dead one none.
//
// ge:summary Tests: the sequel's poses: the player's sequences, a guard's runs, items turning
// ge:keywords test sequel poses animation frames player-animation monster-animation turning-items
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_poses_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_poses.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace fr = sv::sq_frames;

using Pairs = std::vector<std::pair<std::string, std::string>>;

br::Entity entity(Pairs pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long floored hall, the player's start at its west end, and `things` placed in it.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& things) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
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
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p() const { return s().players[0]; }
  int frame() const { return p().pose.frame; }
};

std::unique_ptr<Hall> open_hall(const std::vector<Pairs>& things, const char* dm = "0") {
  auto h = std::make_unique<Hall>();
  h->r = sv::sequel_runner(sv::make_sequel_server({}));
  h->r.console(std::string("deathmatch ") + dm);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(things);
  std::string error;
  CHECK_MSG(h->r.start(map, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

// One rules frame (8 host ticks of 13 ms, the last one past the frame's 100 ms) a call.
void run(Hall& h, int frames, uint8_t buttons = 0, int16_t forward = 0, int16_t up = 0) {
  for (int i = 0; i < frames * 8; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++h.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    c.buttons = buttons;
    c.forward = forward;
    c.up = up;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    h.r.frame(&pk, 1);
  }
}

bool in(int frame, sv::SqSpan s) { return frame >= s.first && frame <= s.last; }

std::string at(int frame) { return "frame " + std::to_string(frame); }

}  // namespace

TEST(a_spawned_player_raises_his_weapon_then_stands_and_runs) {
  auto h = open_hall({});
  run(*h, 1);
  CHECK_MSG(in(h->frame(), fr::kPain[2]), at(h->frame()));  // the blaster coming up
  run(*h, 5);
  CHECK_MSG(in(h->frame(), fr::kStand), at(h->frame()));
  const int a = h->frame();
  run(*h, 1);
  CHECK_MSG(h->frame() == a + 1 || h->frame() == fr::kStand.first, at(h->frame()));
  run(*h, 3, 0, 300);
  CHECK_MSG(in(h->frame(), fr::kRun), at(h->frame()));
  run(*h, 10, 0, 300);  // the run loops
  CHECK_MSG(in(h->frame(), fr::kRun), at(h->frame()));
}

TEST(a_shot_plays_the_attack_and_a_blow_a_pain_in_turn) {
  auto h = open_hall({});
  run(*h, 6);
  run(*h, 1, sh::kButtonAttack);
  CHECK_MSG(in(h->frame(), fr::kAttack), at(h->frame()));
  run(*h, 12);
  CHECK_MSG(in(h->frame(), fr::kStand), at(h->frame()));
  sv::fight_hurt(h->s(), 0, -1, {}, 5, 0, 0);
  run(*h, 1);
  CHECK_MSG(in(h->frame(), fr::kPain[1]), at(h->frame()));  // the first pain is the second
  run(*h, 6);
  sv::fight_hurt(h->s(), 0, -1, {}, 5, 0, 0);
  run(*h, 1);
  CHECK_MSG(in(h->frame(), fr::kPain[2]), at(h->frame()));
}

TEST(a_jump_holds_its_second_frame_in_the_air_and_lands) {
  auto h = open_hall({});
  run(*h, 6);
  run(*h, 2, 0, 0, 300);
  CHECK_MSG(h->frame() == fr::kJump.first + 1, at(h->frame()));
  for (int i = 0; i < 20 && !h->p().on_ground; i++) run(*h, 1);
  run(*h, 1);
  CHECK_MSG(h->frame() > fr::kJump.first + 1 && in(h->frame(), fr::kJump), at(h->frame()));
}

TEST(a_dead_player_holds_his_death_and_his_corpse_keeps_it) {
  auto h = open_hall({}, "1");
  run(*h, 6);
  sv::fight_kill(h->s(), 0, -1, 0);
  run(*h, 1);
  CHECK_MSG(in(h->frame(), fr::kDeath[1]), at(h->frame()));  // the first death is the second
  run(*h, 15);
  CHECK(h->frame() == fr::kDeath[1].last);
  run(*h, 1, sh::kButtonAttack);  // respawns
  CHECK(h->p().alive);
  bool kept = false;
  for (const sv::SequelBody& b : h->s().bodies)
    if (b.live && b.frame == fr::kDeath[1].last) kept = true;
  CHECK(kept);
  CHECK(h->r->world.entities[1].frame == h->frame());  // the player's row shows his pose
}

TEST(a_guard_stands_attacks_whom_it_hears_and_holds_its_death) {
  auto h =
      open_hall({{{"classname", "monster_soldier_light"}, {"origin", "0 0 24"}, {"angle", "0"}}});
  const sv::SqMonsterPoses* t = sv::sq_monster_poses("monster_soldier_light");
  CHECK(t != nullptr);
  run(*h, 1);
  sv::SequelMonster& m = h->s().monsters[0];
  CHECK_MSG(in(m.pose.frame, t->stand), at(m.pose.frame));
  run(*h, 10);
  run(*h, 5, sh::kButtonAttack);  // the blaster: heard, it turns on her
  bool attacked = false;
  for (int i = 0; i < 100 && !attacked; i++) {
    run(*h, 1);
    attacked = in(m.pose.frame, t->attack[2][0]) || in(m.pose.frame, t->attack[2][1]);
  }
  CHECK(attacked);
  const int row = sv::map_row(m.entity);
  CHECK(h->r->world.entities[size_t(row)].frame == m.pose.frame);
  CHECK(sv::map_damage(h->s(), m.entity, 0, 25));
  CHECK(m.dead);
  run(*h, 60);
  bool held = false;
  for (const sv::SqSpan d : t->death)
    if (!d.none() && m.pose.frame == d.last) held = true;
  CHECK_MSG(held, at(m.pose.frame));
}

TEST(items_turn_where_they_lie_but_not_ammo_health_or_an_untouchable_one) {
  auto h = open_hall({{{"classname", "item_armor_body"}, {"origin", "0 0 24"}},
                      {{"classname", "ammo_shells"}, {"origin", "0 64 24"}},
                      {{"classname", "item_health"}, {"origin", "0 -64 24"}},
                      {{"classname", "weapon_railgun"}, {"origin", "64 0 24"}, {"spawnflags", "1"}},
                      {{"classname", "item_adrenaline"}, {"origin", "-64 0 24"}}});
  run(*h, 1);
  std::vector<std::pair<std::string, bool>> seen;
  const sv::SequelServer& s = h->s();
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].item >= 0) {
      const sv::WorldEntity& r = s.world.entities[size_t(sv::map_row(int(i)))];
      seen.emplace_back(s.entities[i].classname, r.turns);
      CHECK(r.effects == (r.turns ? sv::kSqTurnEffect : 0));
    }
  const std::vector<std::pair<std::string, bool>> want = {{"item_armor_body", true},
                                                          {"ammo_shells", false},
                                                          {"item_health", false},
                                                          {"weapon_railgun", false},
                                                          {"item_adrenaline", true}};
  CHECK(seen == want);
}

TEST(a_living_player_carries_his_weapon_for_others_and_a_dead_one_none) {
  auto h = open_hall({}, "1");
  run(*h, 6);
  const sv::SequelServer& s = h->s();
  const int carried = s.world.entities[1].carried_model;
  CHECK(carried > 0 && size_t(carried) < s.models.size());
  CHECK(s.models[size_t(carried)] == "#w_blaster.md2");
  // The carried names, in a client's order (it numbers them from 1).
  std::vector<std::string> names;
  for (const std::string& m : s.models)
    if (!m.empty() && m[0] == '#') names.push_back(m);
  CHECK(names.size() == 11 && names[5] == "#a_grenades.md2" && names[10] == "#w_bfg.md2");
  sv::fight_kill(h->s(), 0, -1, 0);
  run(*h, 1);
  CHECK(s.world.entities[1].carried_model == 0);
}
