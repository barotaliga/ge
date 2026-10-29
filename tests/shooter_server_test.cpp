// THE SERVER'S POOL AND ITS CALLS: slots past the players' are handed out in order and a freed
// one is reused at once before the map's time reaches 2 s, else only 0.5 s after it was freed;
// a link grows an item's box 15 units sideways and every other box 1 unit on every axis; linking
// with touches runs each trigger the box overlaps (not itself, not a solid) with self and other
// set and the caller's globals put back; an impact runs the mover's touch, then the other's; the
// frame's sounds are kept up to their cap and counted past it; the hash moves with a field.
//
// ge:summary Tests: the edict pool, link boxes, trigger touches, impacts, sounds, the hash
// ge:keywords test server spawn_edict free_edict link_edict touch_links impact sounds hash
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/server.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;

TEST(slots_are_reused_at_once_early_and_after_half_a_second_later) {
  Bench b;
  b.start();
  const int first = sh::spawn_edict(b.sv);
  CHECK(first == b.sv.set.maxclients + 1);
  const int second = sh::spawn_edict(b.sv);
  CHECK(second == first + 1);
  sh::free_edict(b.sv, first);
  CHECK(sh::spawn_edict(b.sv) == first);  // the map's time is 1.0: reused at once

  b.sv.time = 10;
  sh::free_edict(b.sv, first);
  b.sv.time = 10.4;
  CHECK(sh::spawn_edict(b.sv) == second + 1);  // too soon: a new slot
  b.sv.time = 10.6;
  CHECK(sh::spawn_edict(b.sv) == first);
}

TEST(the_pool_runs_out_at_max_edicts) {
  Bench b;
  sh::ServerSettings set;
  set.maxclients = 2;
  set.max_edicts = 8;
  b.start({ge::test::loop::box({-64, -64, -64}, {64, 64, 0})}, {}, set);
  int last = 0;
  for (int i = 0; i < 5; i++) last = sh::spawn_edict(b.sv);
  CHECK(last == 7);
  CHECK(sh::spawn_edict(b.sv) == -1);
}

TEST(links_grow_items_sideways_and_everything_else_by_one) {
  Bench b;
  b.start();
  const int box =
      b.spawn_box({10, 20, 30}, {-4, -4, -4}, {4, 4, 4}, sh::kSolidBbox, sh::kMovetypeNone);
  CHECK(b.ent(box).absmin.x == 5 && b.ent(box).absmin.z == 25);
  CHECK(b.ent(box).absmax.y == 25 && b.ent(box).absmax.z == 35);
  const int item = sh::spawn_edict(b.sv);
  b.ent(item).solid = sh::kSolidTrigger;
  b.ent(item).flags = sh::kFlagItem;
  sh::set_size(b.sv, item, {-16, -16, 0}, {16, 16, 56});
  sh::set_origin(b.sv, item, {0, 0, 0});
  CHECK(b.ent(item).absmin.x == -31 && b.ent(item).absmax.y == 31);
  CHECK(b.ent(item).absmin.z == 0 && b.ent(item).absmax.z == 56);
}

TEST(touches_run_each_overlapped_trigger_and_put_the_globals_back) {
  Bench b;
  int self_seen = -1, other_seen = -1, runs = 0;
  const sh::Func touch = b.add([&](sh::Server& sv) {
    self_seen = sv.g.self;
    other_seen = sv.g.other;
    runs++;
  });
  b.start();
  const int trig =
      b.spawn_box({0, 0, 32}, {-32, -32, -32}, {32, 32, 32}, sh::kSolidTrigger, sh::kMovetypeNone);
  b.ent(trig).touch = touch;
  const int solid =
      b.spawn_box({0, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  b.ent(solid).touch = touch;  // a solid's touch is never a trigger touch
  const int far =
      b.spawn_box({500, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  b.sv.g.self = 77;
  b.sv.g.other = 78;

  sh::link_edict(b.sv, far, true);
  CHECK(runs == 0);
  const int mover =
      b.spawn_box({16, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidSlidebox, sh::kMovetypeNone);
  sh::link_edict(b.sv, mover, true);
  CHECK(runs == 1);
  CHECK(self_seen == trig && other_seen == mover);
  CHECK(b.sv.g.self == 77 && b.sv.g.other == 78);
  sh::link_edict(b.sv, trig, true);  // a trigger linked does not touch itself
  CHECK(runs == 1);
  CHECK(b.sv.counts.trigger_touches == 1);
}

TEST(an_impact_runs_the_movers_touch_then_the_others) {
  Bench b;
  std::vector<std::pair<int, int>> seen;
  const sh::Func touch = b.add([&](sh::Server& sv) { seen.push_back({sv.g.self, sv.g.other}); });
  b.start();
  const int a = b.spawn_box({0, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  const int c =
      b.spawn_box({64, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  b.ent(a).touch = touch;
  b.ent(c).touch = touch;
  sh::impact(b.sv, a, c);
  CHECK(seen.size() == 2);
  CHECK((seen[0] == std::pair<int, int>{a, c}));
  CHECK((seen[1] == std::pair<int, int>{c, a}));
  b.ent(c).solid = sh::kSolidNot;  // a thing that does not interact is not touched
  sh::impact(b.sv, a, c);
  CHECK(seen.size() == 3);
}

TEST(sounds_are_kept_to_their_cap_and_counted_past_it) {
  Bench b;
  b.start();
  for (int i = 0; i < sh::kMaxSounds + 3; i++) sh::start_sound(b.sv, 0, 0, "weapon/test", 1, 1);
  CHECK(b.sv.sound_count == sh::kMaxSounds);
  CHECK(b.sv.counts.sounds_dropped == 3);
}

TEST(the_hash_moves_with_a_field_and_not_without) {
  Bench b;
  b.start();
  const int e = b.spawn_box({0, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  const uint64_t h = sh::server_hash(b.sv);
  CHECK(sh::server_hash(b.sv) == h);
  b.ent(e).velocity.z = 1;
  CHECK(sh::server_hash(b.sv) != h);
}

TEST(the_world_is_model_one_and_never_linked) {
  Bench b;
  b.start();
  CHECK(b.ent(0).modelindex == 1 && b.ent(0).solid == sh::kSolidBsp);
  sh::link_edict(b.sv, 0, true);
  CHECK(b.ent(0).area_node == -1);
  CHECK(sh::world_contents(b.sv, {0, 0, -8}) == sh::kContentsSolid);
  CHECK(sh::world_contents(b.sv, {0, 0, 8}) == sh::kContentsEmpty);
}
