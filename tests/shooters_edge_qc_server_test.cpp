// Tests: the mod server's map loading and its extension fields (games/shooters_edge/server/
// qc_server.hpp). Spawning from entity text: the world first with the map's name and model, a
// spawn function by classname, the spawn flags that inhibit an entity (not in deathmatch; not on
// the skill being played), a block with no classname or no spawn function skipped with a message,
// an error in a spawn function stopping the load, the model list, precaching only while loading,
// the world locked once loaded, and the pool reusing its last entity when full. The extension
// fields: maxspeed and gravity read by the player move, movement written from each command,
// visclients filled each frame, and the fields kept as data for a later send.
//
// ge:summary Tests: spawning from entity text, the inhibit rules, and the extension fields
// ge:keywords test qc server load_map entity-text spawnflags inhibit skill deathmatch classname
// ge:keywords maxspeed gravity movement visclients hideentity trackent extension-fields
// ge:skill progs shooter testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "shooter/client.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::ent;
using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

// A mod of spawn functions: worldspawn and thing do nothing, broken calls error, and the fields
// a later send reads.
pg::Progs spawn_mod() {
  pg::Assembler a;
  const int32_t error = a.builtin("error", 10);
  a.field("visclients", pg::kTypeFloat);
  a.field("hideentity", pg::kTypeFloat);
  a.field("hideplayers", pg::kTypeFloat);
  a.field("trackent", pg::kTypeEntity);
  for (const char* name : {"worldspawn", "thing"}) {
    a.begin_function(name, {}, 0);
    a.emit(pg::kOpDone);
    a.end_function();
  }
  a.begin_function("broken", {}, 0);
  a.call(error, {{a.constant_s("this entity is broken"), 1}});
  a.emit(pg::kOpDone);
  a.end_function();
  pg::Progs p;
  std::string e;
  CHECK_MSG(a.build(p, e), e);
  return p;
}

std::string things(std::initializer_list<int> flags) {
  std::string t = "{ \"classname\" \"worldspawn\" }\n";
  for (int f : flags)
    t += "{ \"classname\" \"thing\" \"spawnflags\" \"" + std::to_string(f) + "\" }\n";
  return t;
}

int count_class(Fixture& fx, const char* cls) {
  int n = 0;
  for (int e = 1; e < fx.s->sv.num_edicts; e++)
    if (!fx.s->sv.edicts[static_cast<size_t>(e)].free && fx.str_field(e, pg::kFClassname) == cls)
      n++;
  return n;
}

}  // namespace

TEST(the_world_is_first_with_the_maps_name_and_model) {
  Fixture fx;
  CHECK_MSG(fx.loaded, fx.error);
  CHECK(fx.s->models[1] == "maps/arena.bsp");
  CHECK(fx.str_field(0, pg::kFModel) == "maps/arena.bsp");
  CHECK(std::string(pg::vm_string(fx.vm(), pg::global_int(fx.vm(), pg::kGMapname))) == "arena");
  CHECK(fx.str_field(0, fx.field("message")) == "the test arena");
  CHECK(fx.vm().world_locked);
  CHECK(!fx.s->loading);
  // The map's entities start after the 32 client slots.
  CHECK(count_class(fx, "info_player_deathmatch") == 4);
  for (int e = 1; e <= sh::kMaxClients; e++) CHECK(fx.s->sv.edicts[static_cast<size_t>(e)].free);
  // Spawned in the text's order: the first spot is entity 33.
  CHECK(fx.str_field(33, pg::kFClassname) == "info_player_deathmatch");
  CHECK(fx.f(33, pg::kFOrigin, 0) == -416.0f && fx.f(33, pg::kFAngles, 1) == 45.0f);
}

TEST(spawnflags_inhibit_by_deathmatch_and_skill) {
  const pg::Progs mod = spawn_mod();
  const std::string text = things({0, 256, 512, 1024, 2048, 256 | 1024});
  {
    Fixture fx(text.c_str(), {"deathmatch 1"}, &mod);
    CHECK_MSG(fx.loaded, fx.error);
    CHECK(count_class(fx, "thing") == 5 && fx.s->inhibited == 1);  // only 2048
  }
  {
    Fixture fx(text.c_str(), {"deathmatch 0", "skill 0"}, &mod);
    CHECK(count_class(fx, "thing") == 4 && fx.s->inhibited == 2);  // 256, 256|1024
  }
  {
    Fixture fx(text.c_str(), {"deathmatch 0", "skill 1"}, &mod);
    CHECK(count_class(fx, "thing") == 5 && fx.s->inhibited == 1);  // 512
  }
  {
    Fixture fx(text.c_str(), {"deathmatch 0", "skill 3"}, &mod);
    CHECK(count_class(fx, "thing") == 4 && fx.s->inhibited == 2);  // 1024, 256|1024
  }
}

TEST(blocks_without_a_classname_or_spawn_function_are_skipped) {
  const pg::Progs mod = spawn_mod();
  Fixture fx(
      "{ \"classname\" \"worldspawn\" }\n{ \"origin\" \"1 2 3\" }\n"
      "{ \"classname\" \"monster_nothing\" }\n{ \"classname\" \"thing\" }",
      {}, &mod);
  CHECK_MSG(fx.loaded, fx.error);
  CHECK(count_class(fx, "thing") == 1);
  CHECK(count_class(fx, "monster_nothing") == 0);
  bool no_class = false, no_fn = false;
  for (const std::string& m : fx.s->map_messages) {
    if (m.rfind("no classname for entity", 0) == 0) no_class = true;
    if (m == "no spawn function for monster_nothing") no_fn = true;
  }
  CHECK(no_class && no_fn);
}

TEST(an_error_in_a_spawn_function_stops_the_load) {
  const pg::Progs mod = spawn_mod();
  Fixture fx("{ \"classname\" \"worldspawn\" }\n{ \"classname\" \"broken\" }", {}, &mod);
  CHECK(!fx.loaded);
  CHECK(fx.error.find("this entity is broken") != std::string::npos);
  CHECK(fx.s->map_stopped);
}

TEST(a_text_that_does_not_parse_stops_the_load) {
  const pg::Progs mod = spawn_mod();
  Fixture fx("{ \"classname\" \"worldspawn\" ", {}, &mod);
  CHECK(!fx.loaded);
  CHECK(!fx.error.empty());
}

TEST(a_full_pool_reuses_its_last_entity) {
  Fixture fx;
  int last = 0;
  for (int i = 0; i < 2000; i++) {
    const int e = fx.spawn();
    CHECK(e > 0);
    if (e <= 0) break;
    if (e == last) break;
    last = e;
  }
  CHECK(last == fx.s->sv.set.max_edicts - 1);
  CHECK(fx.s->sv.counts.spawns_full >= 1);
}

TEST(maxspeed_and_gravity_from_the_mod_steer_the_player_move) {
  Fixture fx;
  const int p = fx.join(0, "runner");
  // The test mod's PutClientInServer gave maxspeed 320 and gravity 1; the loop has them.
  CHECK(fx.s->sv.edicts[static_cast<size_t>(p)].maxspeed == 320.0f);
  CHECK(fx.s->sv.edicts[static_cast<size_t>(p)].gravity == 1.0f);
  // QC halves the speed: the run tops out at 160.
  fx.in_call([&] { fx.set_f(p, fx.field("maxspeed"), 160); });
  sh::Packet k;
  k.client = 0;
  for (int i = 0; i < 120; i++) {
    k.sequence = static_cast<uint32_t>(i + 1);
    for (sh::MoveCmd& c : k.cmds) {
      c.msec = 13;
      c.forward = 400;
    }
    sv::qc_frame(*fx.s, &k, 1);
  }
  const sh::Vec3f v = fx.s->sv.edicts[static_cast<size_t>(p)].velocity;
  const float speed = std::sqrt(v.x * v.x + v.y * v.y);
  CHECK_MSG(speed > 150 && speed <= 160.5f, "speed " + std::to_string(speed));
  // The movement field holds the last command's axes.
  CHECK(fx.f(p, fx.field("movement"), 0) == 400.0f);
  CHECK(fx.f(p, fx.field("movement"), 1) == 0.0f);
  // Gravity 0.5: a jump goes higher than at 1.
  auto apex = [&](float g) {
    fx.in_call([&] { fx.set_f(p, fx.field("gravity"), g); });
    float top = -1e9f;
    for (int i = 0; i < 100; i++) {
      k.sequence++;
      for (sh::MoveCmd& c : k.cmds) {
        c.forward = 0;
        c.buttons = i < 2 ? 2 : 0;
      }
      sv::qc_frame(*fx.s, &k, 1);
      const float z = fx.s->sv.edicts[static_cast<size_t>(p)].origin.z;
      if (z > top) top = z;
    }
    return top;
  };
  const float normal = apex(1), light = apex(0.5f);
  CHECK_MSG(light > normal + 10,
            "apex at gravity 0.5 " + std::to_string(light) + " vs 1 " + std::to_string(normal));
}

TEST(visclients_holds_who_sees_each_entity) {
  const pg::Progs mod = spawn_mod();
  Fixture fx("{ \"classname\" \"worldspawn\" }\n{ \"classname\" \"thing\" }", {}, &mod);
  const int a = fx.join(0, "a"), b = fx.join(1, "b");
  sv::qc_frame(*fx.s, nullptr, 0);
  const int vis = fx.field("visclients");
  CHECK(fx.f(33, vis) == 3.0f);  // both clients see the thing
  CHECK(fx.f(a, vis) == 2.0f);   // a client is not in its own set
  CHECK(fx.f(b, vis) == 1.0f);
  fx.s->vis = [](void*, sh::Vec3f, sh::Vec3f, bool) { return false; };
  sv::qc_frame(*fx.s, nullptr, 0);
  CHECK(fx.f(33, vis) == 0.0f);
}

TEST(the_send_fields_are_kept_as_data) {
  const pg::Progs mod = spawn_mod();
  Fixture fx("{ \"classname\" \"worldspawn\" }\n{ \"classname\" \"thing\" }", {}, &mod);
  CHECK(fx.s->q.fields.hideentity >= 0 && fx.s->q.fields.hideplayers >= 0);
  CHECK(fx.s->q.fields.trackent >= 0 && fx.s->q.fields.visclients >= 0);
  fx.set_f(33, fx.field("hideentity"), 1);
  sv::qc_frame(*fx.s, nullptr, 0);
  CHECK(fx.f(33, fx.field("hideentity")) == 1.0f);  // nothing reads it yet; nothing clears it
}
