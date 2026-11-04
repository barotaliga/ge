// Tests: the rules' world the QC server fills (games/shooters_edge/server/rules_world.hpp). After
// a map's load it holds the map's name and lists and one row a pool slot; after a client joins,
// its slot, name and numbers; after each frame, every entity's row as the loop and the program
// have it; after a client leaves, the slot empty and its numbers zero.
//
// ge:summary Tests: the rules' world filled by the QC server at the start and every frame
// ge:keywords test rules-world fill_world entity-rows client-rows player-numbers
// ge:skill progs shooter testing
#include <string>

#include "check.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

// Every row in use against the loop's entity and the program's fields.
void check_rows(Fixture& fx) {
  const sv::QcServer& s = *fx.s;
  const sv::RulesWorld& w = s.world;
  CHECK(w.entity_count == s.sv.num_edicts);
  for (int e = 0; e < w.entity_count; e++) {
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    const sv::WorldEntity& r = w.entities[static_cast<size_t>(e)];
    CHECK(r.free == ed.free);
    CHECK_BITS(r.origin.x, ed.origin.x);
    CHECK_BITS(r.origin.y, ed.origin.y);
    CHECK_BITS(r.origin.z, ed.origin.z);
    CHECK_BITS(r.velocity.z, ed.velocity.z);
    CHECK_BITS(r.angles.y, ed.angles.y);
    CHECK_BITS(r.mins.z, ed.mins.z);
    CHECK(r.modelindex == static_cast<int>(fx.f(e, s.q.layout->f.modelindex, 0)));
    CHECK(r.frame == static_cast<int>(fx.f(e, s.q.layout->f.frame, 0)));
  }
}

TEST(a_maps_load_fills_the_lists_and_a_row_a_slot) {
  Fixture fx;
  CHECK(fx.loaded);
  const sv::RulesWorld& w = fx.s->world;
  CHECK(w.map_name == fx.s->map_name);
  CHECK(w.models == fx.s->models);
  CHECK(w.sounds == fx.s->sounds);
  CHECK(w.statics.size() == fx.s->statics.size());
  CHECK(w.entities.size() >= static_cast<size_t>(fx.s->sv.num_edicts));
  CHECK(w.models.size() > 1 && w.models[0].empty());
  check_rows(fx);
  for (const sv::WorldClient& c : w.clients) CHECK(!c.active);
}

TEST(a_client_and_every_entity_follow_each_frame) {
  Fixture fx;
  const int p = fx.join(0, "runner");
  const sv::RulesWorld& w = fx.s->world;
  CHECK(w.clients[0].active);
  CHECK(w.clients[0].bot);
  CHECK(w.clients[0].name == "runner");
  CHECK(w.clients[0].userid == fx.s->clients[0].userid);
  sh::Packet k;
  k.client = 0;
  for (int i = 0; i < 40; i++) {
    k.sequence = static_cast<uint32_t>(i + 1);
    for (sh::MoveCmd& c : k.cmds) {
      c.msec = 13;
      c.forward = 400;
    }
    sv::qc_frame(*fx.s, &k, 1);
  }
  CHECK_BITS(w.time, fx.s->sv.time);
  check_rows(fx);
  const sh::Edict& ed = fx.s->sv.edicts[static_cast<size_t>(p)];
  CHECK(ed.velocity.x != 0 || ed.velocity.y != 0);
  const sv::PlayerNumbers& n = w.clients[0].numbers;
  CHECK(n.health == static_cast<int>(fx.f(p, fx.s->q.layout->f.health, 0)));
  CHECK_BITS(n.frags, fx.f(p, fx.s->q.layout->f.frags, 0));
  CHECK_BITS(n.view_z, fx.f(p, fx.s->q.layout->f.view_ofs, 2));
}

TEST(a_client_that_leaves_empties_its_slot) {
  Fixture fx;
  fx.join(0, "runner");
  fx.join(1, "second");
  CHECK(fx.s->world.clients[1].name == "second");
  sv::disconnect(*fx.s, 0);
  const sv::WorldClient& c = fx.s->world.clients[0];
  CHECK(!c.active);
  CHECK(c.name.empty());
  CHECK(c.numbers.health == 0);
  CHECK(fx.s->world.clients[1].active);
}

}  // namespace
