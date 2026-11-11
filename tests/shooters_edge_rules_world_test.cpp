// Tests: the rules' world the QC server fills (games/shooters_edge/server/rules_world.hpp). After
// a map's load it holds the map's name and lists and one row a pool slot; after a client joins,
// its slot, name and numbers; after each frame, every entity's row as the loop and the program
// have it; after a client leaves, the slot empty and its numbers zero.
//
// ge:summary Tests: the rules' world filled by the QC server at the start and every frame
// ge:keywords test rules-world fill_world entity-rows client-rows player-numbers model-shown
// ge:keywords alpha entity_alpha
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
    const bool shown = sv::model_shown(s, e);
    CHECK(r.modelindex == (shown ? static_cast<int>(fx.f(e, s.q.layout->f.modelindex, 0)) : 0));
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
  CHECK_BITS(n.health, fx.f(p, fx.s->q.layout->f.health, 0));
  CHECK_BITS(n.frags, fx.f(p, fx.s->q.layout->f.frags, 0));
  CHECK_BITS(n.view_z, fx.f(p, fx.s->q.layout->f.view_ofs, 2));
}

// A program hides a taken item by emptying its model text and keeping its index: the row then has
// no model (nothing draws or sends it), and the text set again brings it back.
TEST(an_entity_whose_model_text_is_emptied_is_not_shown) {
  Fixture fx;
  const int p = fx.join(0, "runner");
  const int model = fx.s->q.layout->f.model;
  uint32_t& text = pg::entity_field(fx.vm(), p, model)[0];
  const uint32_t kept = text;
  CHECK(!fx.str_field(p, model).empty());
  CHECK(fx.f(p, fx.s->q.layout->f.modelindex) > 0);
  CHECK(sv::model_shown(*fx.s, p));
  text = 0;  // the empty text
  CHECK(!sv::model_shown(*fx.s, p));
  sv::fill_world(*fx.s, fx.s->world, false);
  CHECK(fx.s->world.entities[static_cast<size_t>(p)].modelindex == 0);
  CHECK(fx.f(p, fx.s->q.layout->f.modelindex) > 0);  // the index kept for the way back
  text = kept;
  CHECK(sv::model_shown(*fx.s, p));
  sv::fill_world(*fx.s, fx.s->world, false);
  CHECK(fx.s->world.entities[static_cast<size_t>(p)].modelindex ==
        static_cast<int>(fx.f(p, fx.s->q.layout->f.modelindex)));
  check_rows(fx);
}

// How much of an entity shows: a mod's `alpha` field above 0 wins, else the map's `alpha` key,
// else solid. The test mod has no `alpha` field, so a float it leaves alone (`frags`) stands in.
TEST(an_entitys_alpha_comes_from_the_mods_field_then_the_maps_key) {
  Fixture fx;
  const int p = fx.join(0, "runner");
  CHECK(fx.s->world_fields.alpha == -1);  // no field: the map's key or solid
  CHECK(sv::entity_alpha(*fx.s, p) == 1.0f);
  pg::EntityExtras& x = fx.s->extras[static_cast<size_t>(p)];
  x.has_alpha = true;
  x.alpha = 0.25f;
  CHECK(sv::entity_alpha(*fx.s, p) == 0.25f);
  const int frags = fx.s->q.layout->f.frags;
  fx.s->world_fields.alpha = frags;
  uint32_t& field = pg::entity_field(fx.vm(), p, frags)[0];
  field = pg::as_word(0);  // unset: the map's key still holds
  CHECK(sv::entity_alpha(*fx.s, p) == 0.25f);
  field = pg::as_word(0.5f);
  sv::fill_world(*fx.s, fx.s->world, false);
  CHECK(fx.s->world.entities[static_cast<size_t>(p)].alpha == 0.5f);
  field = pg::as_word(3);  // past whole: solid
  CHECK(sv::entity_alpha(*fx.s, p) == 1.0f);
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
