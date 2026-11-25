// Tests: Ground Zero's plat that goes either way (games/shooters_edge/server/q2_gz_plat.cpp) in a
// hall built here: called from the bottom it rises and comes back down, a rider at the bottom is
// carried up, a toggle stays where it is sent, one resting at the top waits there, a named one
// idles at the top until a use sends it down, and its targets fire at each end.
//
// ge:summary Tests: Ground Zero's func_plat2 called, ridden, toggled, resting at the top, named
// ge:keywords test q2 ground-zero func-plat2 plat2 plat-toggle plat-top called-plat
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_plat_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
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

// A floored hall with a plat2 (model 1) 128 tall built raised over x 400..496, `keys` its own.
std::shared_ptr<const br::Map> hall(const Pairs& keys) {
  br::BrushWorld w;
  w.models.resize(2);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  const float plo[3] = {400, -48, 0}, phi[3] = {496, 48, 136};
  w.models[1].push_back(br::box_brush(plo, phi, sh::kSqSolid, "world/plat"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  Pairs plat = {{"classname", "func_plat2"}, {"model", "*1"}, {"lip", "8"}};
  for (const auto& kv : keys) plat.push_back(kv);
  w.entities.push_back(entity(plat));
  w.entities.push_back(
      entity({{"classname", "trigger_relay"}, {"targetname", "go"}, {"target", "lift"}}));
  w.entities.push_back(
      entity({{"classname", "trigger_relay"}, {"targetname", "ends"}, {"target", "count"}}));
  w.entities.push_back(entity({{"classname", "trigger_counter"},
                               {"targetname", "count"},
                               {"count", "100"},
                               {"spawnflags", "1"}}));
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
  sv::Q2Player& p() const { return s().players[0]; }
  sv::Q2Entity& plat() const {
    for (sv::Q2Entity& e : s().entities)
      if (e.classname == "func_plat2") return e;
    return s().entities[0];
  }
};

std::unique_ptr<Hall> open_hall(const Pairs& keys, const char* deathmatch = "0") {
  auto h = std::make_unique<Hall>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  h->r = sv::q2_runner(sv::make_q2_server(o));
  h->r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall(keys);
  std::string error;
  CHECK_MSG(h->r.start(map, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

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

constexpr int kSecond = 77;  // host ticks of 13 ms

void place(Hall& h, float x, float z) {
  int32_t* o = h.p().move.origin;
  o[0] = static_cast<int32_t>(x * 8);
  o[1] = 0;
  o[2] = static_cast<int32_t>(z * 8);
  for (int32_t& v : h.p().move.velocity) v = 0;
}

float z_of(const sv::Q2Player& p) { return float(p.move.origin[2]) / 8; }

}  // namespace

TEST(a_plat2_rests_at_the_bottom_and_carries_its_rider_up_then_comes_down) {
  auto h = open_hall({});
  run(*h, 40);
  CHECK_MSG(h->plat().origin.z == -128, std::to_string(h->plat().origin.z));  // 136 - lip 8 down
  place(*h, 448, 16 + 24 + 1);  // standing on it at the bottom: he wants the top
  run(*h, 3 * kSecond);         // 0.5 s on it sets off, and it is up in about a second
  CHECK_MSG(z_of(h->p()) > 136 + 23, std::to_string(z_of(h->p())));
  run(*h, 5 * kSecond / 2);  // uncalled at the top: down again 2 s on
  CHECK_MSG(h->plat().origin.z == -128, std::to_string(h->plat().origin.z));
}

TEST(a_toggle_plat2_stays_at_the_end_it_was_sent_to) {
  auto h = open_hall({{"spawnflags", "2"}});
  run(*h, 40);
  place(*h, 448, 16 + 24 + 1);
  run(*h, 2 * kSecond);  // up by 2.1 s; its lockout ends 2 s after it set off
  CHECK(h->plat().origin.z == 0);
  place(*h, -600, 24);  // off it (a rider at the top would send it down again)
  run(*h, 8 * kSecond);
  CHECK(h->plat().origin.z == 0);  // still at the top
}

TEST(a_plat2_with_flag_4_rests_at_the_top) {
  auto h = open_hall({{"spawnflags", "4"}});
  run(*h, 4 * kSecond);
  CHECK(h->plat().origin.z == 0);
}

TEST(a_named_plat2_idles_at_the_top_until_a_use_sends_it_down) {
  auto h = open_hall({{"targetname", "lift"}, {"target", "ends"}});
  run(*h, 2 * kSecond);
  CHECK(h->plat().origin.z == 0);
  sv::map_use_targets(
      h->s(),
      [&] {
        for (size_t i = 0; i < h->s().entities.size(); i++)
          if (h->s().entities[i].targetname == "go") return static_cast<int>(i);
        return -1;
      }(),
      -1);
  run(*h, 4 * kSecond);
  CHECK_MSG(h->plat().origin.z == -128, std::to_string(h->plat().origin.z));
  int count = -1;
  for (const sv::Q2Entity& e : h->s().entities)
    if (e.classname == "trigger_counter") count = e.count;
  CHECK_MSG(count == 99, std::to_string(count));  // its targets at the bottom
}

TEST(a_plat2_is_twice_as_quick_in_deathmatch) {
  auto sp = open_hall({}), dm = open_hall({}, "1");
  CHECK(dm->plat().speed == 2 * sp->plat().speed && dm->plat().accel == 2 * sp->plat().accel);
}
