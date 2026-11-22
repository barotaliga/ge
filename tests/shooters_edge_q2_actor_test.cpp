// Quake II's misc_actor and target_actor (games/shooters_edge/server/q2_actor.hpp): the spawn
// lines, the use, the walk to its points and what it says there, the taunt.
//
// ge:summary Tests Quake II's misc_actor: spawn lines, use, the walk, the point's message, taunts
// ge:keywords q2 misc_actor target_actor actor taunt test
// ge:skill shooter
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_actor.hpp"
#include "shooters_edge/server/q2_monsters.hpp"
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

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -512, -16}, hi[3] = {1024, 512, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "misc_actor"},
                               {"origin", "0 0 24"},
                               {"targetname", "guy"},
                               {"target", "p1"},
                               {"health", "70"}}));
  w.entities.push_back(entity({{"classname", "target_actor"},
                               {"origin", "200 0 24"},
                               {"targetname", "p1"},
                               {"target", "p2"},
                               {"message", "Follow me"}}));
  w.entities.push_back(
      entity({{"classname", "target_actor"}, {"origin", "400 0 24"}, {"targetname", "p2"}}));
  w.entities.push_back(entity({{"classname", "misc_actor"}, {"origin", "50 300 24"}}));
  w.entities.push_back(entity({{"classname", "target_actor"}, {"origin", "60 300 24"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  CHECK(br::write_brush_world(w, &bytes, &error) && br::read_bsp(bytes, map.get(), &error));
  return map;
}

struct Hall {
  sv::RulesRunner r = sv::q2_runner(sv::make_q2_server({}));
  int sequence = 0;
  std::vector<sv::NetEvent> prints;  // every print sent, kept across frames
  sv::Q2Server& s() const { return *sv::q2_of(r); }
};

std::unique_ptr<Hall> open() {
  auto h = std::make_unique<Hall>();
  h->r.console("deathmatch 0");
  sv::MapSpec spec;
  spec.name = "actor_hall";
  spec.brushes = hall();
  std::string error;
  CHECK_MSG(h->r.start(spec, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

void run(Hall& h, int frames) {
  for (int i = 0; i < frames * 8; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++h.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    h.r.frame(&pk, 1);
    for (const sv::NetEvent& e : h.s().out.events)
      if (e.kind == sv::EventKind::print) h.prints.push_back(e);
  }
}

bool said(const sv::Q2Server& s, const std::string& line) {
  return std::find(s.console.begin(), s.console.end(), line) != s.console.end();
}

int actor(const sv::Q2Server& s) {
  for (size_t i = 0; i < s.entities.size(); i++)
    if (s.entities[i].classname == "misc_actor" && !s.entities[i].gone) return static_cast<int>(i);
  return -1;
}

}  // namespace

TEST(an_actor_with_no_name_is_left_out_and_its_point_kept) {
  auto h = open();
  CHECK(said(h->s(), "untargeted misc_actor at (50 300 24)"));
  CHECK(said(h->s(), "target_actor with no targetname at (60 300 24)"));
  const int a = actor(h->s());
  CHECK(a >= 0 && h->s().entities[size_t(a)].health == 70);  // its own health
  CHECK(h->s().monsters_total == 0);                         // a good guy, never counted
}

TEST(an_actor_stands_until_used_then_walks_its_points_and_speaks) {
  auto h = open();
  const int a = actor(h->s());
  run(*h, 10);
  CHECK(h->s().entities[size_t(a)].origin.x < 1);  // it waits
  CHECK(sv::monster_use(h->s(), a, 0));
  run(*h, 80);
  CHECK(h->s().entities[size_t(a)].origin.x > 350);  // on to the last point
  CHECK(h->s().console.back() == "G_PickTarget called with NULL targetname");
  CHECK(sv::monster_use(h->s(), a, 0));  // used again: its target is spent
  CHECK(h->s().console.back().rfind("misc_actor has bad target (null) at (", 0) == 0);
  // What it said at the first point reached everyone, as chat, under its name.
  const std::string line = sv::actor_name(h->s(), a) + ": Follow me\n";
  CHECK(std::any_of(h->prints.begin(), h->prints.end(), [&](const sv::NetEvent& e) {
    return e.text == line && e.level == 3 && e.to == 0xffffffffu;
  }));
}

TEST(an_actor_shot_by_a_player_taunts_him_and_never_turns_on_him) {
  auto h = open();
  const int a = actor(h->s());
  bool taunted = false;
  for (int k = 0; k < 20 && !taunted; k++) {
    sv::monster_damage(h->s(), a, 0, 1, {1, 0, 0}, false, nullptr);
    run(*h, 31);  // past its 3 s between pains
    for (const sv::NetEvent& e : h->prints) {
      const std::string name = sv::actor_name(h->s(), a) + ": ";
      for (const char* word : {"Watch it!\n", "#$@*&!\n", "Idiot!\n"})
        taunted = taunted || (e.text == name + word && e.to == 1u && e.level == 3);
    }
  }
  CHECK(taunted);
  for (const sv::Q2Monster& m : h->s().monsters)
    if (m.entity == a) CHECK(m.enemy == -1);
}
