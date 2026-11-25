// Tests: Ground Zero's Doppleganger (games/shooters_edge/server/q2_gz_decoy.cpp): the decoy set
// down ahead of its player with his look, kept when there is no room, its end at 30 s, the sphere
// it lets out at whoever brought it down, and the item left off the map outside deathmatch.
//
// ge:summary Tests: Ground Zero's Doppleganger decoy, its end and its spheres
// ge:keywords test q2 ground-zero doppleganger decoy hunter-sphere vengeance-sphere
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_decoy_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_map.hpp"
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

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

struct Hall {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
};

// A floored hall with a wall at x 1000; Ana starts at 0 0 facing +x, Ben at -600 0. `extra`: a
// map's things beside them.
std::unique_ptr<Hall> open_hall(const char* deathmatch, const std::vector<Pairs>& extra = {}) {
  br::BrushWorld w;
  w.models.resize(1);
  w.models[0].push_back(box(-2048, -1024, -16, 2048, 1024, 0));
  w.models[0].push_back(box(1000, -1024, 0, 1032, 1024, 256));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "0"}}));
  for (const Pairs& t : extra) w.entities.push_back(entity(t));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  auto h = std::make_unique<Hall>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  h->r = sv::q2_runner(sv::make_q2_server(o));
  h->r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  CHECK_MSG(h->r.start(spec, error), error);
  h->r.connect(0, "Ana", false, false);
  if (std::string(deathmatch) == "0") return h;
  int32_t* at = h->s().players[0].move.origin;  // out of the way of Ben's start
  at[1] = static_cast<int32_t>(at[1] + 200 * 8);
  h->r.connect(1, "Ben", false, false);
  return h;
}

// Host ticks of 13 ms, everyone standing.
void run(Hall& h, int ticks) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk[2];
    for (int c = 0; c < 2; c++) {
      pk[c].client = c;
      pk[c].sequence = h.sequence + 1;
      sh::MoveCmd m;
      m.msec = 13;
      pk[c].cmds[0] = pk[c].cmds[1] = pk[c].cmds[2] = m;
    }
    h.sequence++;
    h.r.frame(pk, 2);
  }
}

constexpr int kSecond = 77;  // host ticks of 13 ms in a second

int decoy(const Hall& h, int owner) {
  for (size_t i = 0; i < h.s().entities.size(); i++)
    if (h.s().entities[i].classname == "doppleganger" && h.s().entities[i].owner == owner)
      return static_cast<int>(i);
  return -1;
}

int spheres(const Hall& h, sv::SqShot kind) {
  int n = 0;
  for (const sv::Q2Missile& m : h.s().missiles) n += m.live && m.kind == kind ? 1 : 0;
  return n;
}

int doppleganger() { return sv::sq_item("item_doppleganger", sv::Q2Pack::ground_zero); }

// Ana at `x` facing +x, holding one Doppleganger; Ben 600 behind the start.
void hold(Hall& h, float x) {
  if (h.s().clients[1].active) sv::map_place_player(h.s(), 1, {-600, 300, 24}, {});
  sv::map_place_player(h.s(), 0, {x, 0, 24}, {});
  run(h, 2);
  h.s().players[0].body.counts[size_t(doppleganger())] = 1;
}

}  // namespace

TEST(a_doppleganger_stands_ahead_of_its_player_in_his_look_and_goes_at_30_s) {
  auto h = open_hall("1");
  run(*h, kSecond / 2);
  hold(*h, 0);
  CHECK(sv::fight_use(h->s(), 0, doppleganger()));
  CHECK(h->s().players[0].body.counts[size_t(doppleganger())] == 0);
  const int d = decoy(*h, 0);
  CHECK(d >= 0);
  if (d < 0) return;
  const sv::Q2Entity& e = h->s().entities[size_t(d)];
  CHECK(!e.gone && e.health == 30);
  CHECK_MSG(e.origin.x > 40 && e.origin.x < 56, std::to_string(e.origin.x));
  run(*h, kSecond);
  CHECK_MSG(e.origin.z > 23.9f && e.origin.z < 24.1f, std::to_string(e.origin.z));  // on the floor
  const sv::WorldEntity& r = h->s().world.entities[size_t(sv::map_row(d))];
  const sv::WorldEntity& ana = h->s().world.entities[1];
  CHECK(!r.free && r.modelindex == ana.modelindex && r.skin == ana.skin);
  CHECK(r.carried_model == ana.carried_model && (r.render_fx & 0x8000) != 0);
  CHECK(r.origin.z == e.origin.z + 8 && r.frame >= 0 && r.frame < 40);
  run(*h, 30 * kSecond);
  CHECK(h->s().entities[size_t(d)].gone);
  CHECK(spheres(*h, sv::SqShot::vengeance_sphere) + spheres(*h, sv::SqShot::hunter_sphere) == 0);
}

TEST(a_doppleganger_with_no_room_ahead_is_kept) {
  auto h = open_hall("1");
  run(*h, kSecond / 2);
  hold(*h, 980);  // nose to the wall
  CHECK(sv::fight_use(h->s(), 0, doppleganger()));
  CHECK(h->s().players[0].body.counts[size_t(doppleganger())] == 1);
  CHECK(h->s().entities[size_t(decoy(*h, 0))].gone);
}

TEST(a_doppleganger_brought_down_lets_out_a_sphere_at_who_hurt_it) {
  auto h = open_hall("1");
  run(*h, kSecond / 2);
  hold(*h, 0);
  CHECK(sv::fight_use(h->s(), 0, doppleganger()));
  const int d = decoy(*h, 0);
  run(*h, 2);
  // Ben, 600 off, wounds it and then finishes it: a vengeance sphere for him.
  CHECK(sv::map_damage(h->s(), d, 1, 10, {}, false));
  CHECK(!h->s().entities[size_t(d)].gone && h->s().entities[size_t(d)].health == 20);
  CHECK(h->s().players[1].alive);
  CHECK(sv::map_damage(h->s(), d, 1, 25, {}, false));
  CHECK(h->s().entities[size_t(d)].gone);
  CHECK(spheres(*h, sv::SqShot::vengeance_sphere) == 1);
  // One blow that kills it outright: no sphere.
  hold(*h, 0);
  CHECK(sv::fight_use(h->s(), 0, doppleganger()));
  run(*h, 2);
  CHECK(sv::map_damage(h->s(), d, 1, 100, {}, false));
  CHECK(h->s().entities[size_t(d)].gone);
  CHECK(spheres(*h, sv::SqShot::vengeance_sphere) == 1);
}

TEST(outside_deathmatch_the_doppleganger_is_not_on_the_map_nor_taken) {
  auto h = open_hall("0", {{{"classname", "item_doppleganger"}, {"origin", "200 0 16"}}});
  run(*h, 2);
  for (const sv::Q2Entity& e : h->s().entities)
    if (e.classname == "item_doppleganger") CHECK(e.gone);
  CHECK(decoy(*h, 0) < 0);  // no decoy rows either
  sv::SqBody b;
  CHECK(!sv::sq_pick_up(doppleganger(), b, false, 1, 0, 0));
  CHECK(sv::sq_pick_up(doppleganger(), b, true, 1, 0, 0));
  CHECK(!sv::sq_pick_up(doppleganger(), b, true, 1, 0, 0));  // one at a time
}
