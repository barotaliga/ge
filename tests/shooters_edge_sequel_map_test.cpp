// Tests: the sequel's movers and triggers on its runner (games/shooters_edge/server/sequel_map.cpp)
// over a hall built here. A plat sent down by a use follows the accelerated move frame by frame
// (the clean-room note's worked example: 0, 5, 15, 30, 50, 70, 90, 107, 119, 126, 128); a player
// who steps on it at the bottom rides it up; a door opens when a player walks into its field, stops
// his shots while closed, and closes after its wait; a push sets the velocity; a hurting trigger
// takes health; a teleporter moves the player to its destination; a trigger_once fires a level
// change.
//
// ge:summary Tests: the sequel's movers and triggers: plat's curve, riders, doors, push, teleport
// ge:keywords test sequel func-plat func-door accelerated-move riders trigger-push trigger-hurt
// ge:keywords teleporter target-changelevel use-targets
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_map_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

// A long floored hall: a plat (named `lift`, raised 128 above its bottom), a door across the hall
// that rises when walked to, a push and a hurting trigger, a teleporter and a trigger_once that
// ends the level.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(6);
  w.models[0].push_back(box(-1024, -256, -16, 1024, 256, 0));
  w.models[1].push_back(box(400, -48, 0, 496, 48, 136));      // the plat, raised
  w.models[2].push_back(box(-300, -256, 0, -290, 256, 128));  // the door
  w.models[3].push_back(box(600, -64, 0, 700, 64, 64));       // the push
  w.models[4].push_back(box(800, -64, 0, 900, 64, 64));       // the hurt
  w.models[5].push_back(box(-1000, -64, 0, -950, 64, 64));    // the exit's trigger
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "func_plat"}, {"model", "*1"}, {"targetname", "lift"}}));
  w.entities.push_back(
      entity({{"classname", "trigger_relay"}, {"targetname", "go"}, {"target", "lift"}}));
  w.entities.push_back(entity({{"classname", "func_door"}, {"model", "*2"}, {"angle", "-1"}}));
  w.entities.push_back(
      entity({{"classname", "trigger_push"}, {"model", "*3"}, {"angle", "-1"}, {"speed", "50"}}));
  w.entities.push_back(entity({{"classname", "trigger_hurt"}, {"model", "*4"}}));
  w.entities.push_back(
      entity({{"classname", "misc_teleporter"}, {"origin", "-800 150 0"}, {"target", "far"}}));
  w.entities.push_back(entity({{"classname", "misc_teleporter_dest"},
                               {"origin", "300 -150 24"},
                               {"targetname", "far"},
                               {"angle", "90"}}));
  w.entities.push_back(entity({{"classname", "trigger_once"}, {"model", "*5"}, {"target", "out"}}));
  w.entities.push_back(
      entity({{"classname", "target_changelevel"}, {"targetname", "out"}, {"map", "next"}}));
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
  int named(const char* classname) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].classname == classname) return static_cast<int>(i);
    return -1;
  }
  sv::SequelEntity& e(const char* classname) const {
    return s().entities[size_t(named(classname))];
  }
};

std::unique_ptr<Hall> open_hall(const char* deathmatch = "1") {
  auto h = std::make_unique<Hall>();
  h->r = sv::sequel_runner(sv::make_sequel_server({}));
  h->r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(h->r.start(map, error), error);
  h->r.connect(0, "Ana", false, false);
  return h;
}

// Host ticks of 13 ms, Ana walking `forward` along yaw `yaw`.
void run(Hall& h, int ticks, int16_t forward = 0, float yaw = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++h.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    c.forward = forward;
    c.angles = {0, yaw, 0};
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    h.r.frame(&pk, 1);
  }
}

// The rules frames, one at a time: host ticks until the next one has run.
void frames(Hall& h, int n) {
  for (int i = 0; i < n; i++) {
    const int64_t at = h.s().frame_number;
    while (h.s().frame_number == at) run(h, 1);
  }
}

void place(Hall& h, float x, float y, float z) {
  int16_t* o = h.p().move.origin;
  o[0] = static_cast<int16_t>(x * 8);
  o[1] = static_cast<int16_t>(y * 8);
  o[2] = static_cast<int16_t>(z * 8);
  for (int16_t& v : h.p().move.velocity) v = 0;
}

float z_of(const sv::SequelPlayer& p) { return float(p.move.origin[2]) / 8; }

}  // namespace

TEST(a_plat_sent_down_follows_the_accelerated_move) {
  auto h = open_hall();
  run(*h, 40);
  place(*h, -600, 0, 24);
  const int lift = h->named("func_plat");
  const float top = h->s().entities[size_t(lift)].origin.z;
  sv::map_use_targets(h->s(), h->named("trigger_relay"), -1);
  const float want[] = {0, 5, 15, 30, 50, 70, 90, 107, 119, 126, 128};
  for (const float w : want) {
    frames(*h, 1);
    const float down = top - h->s().entities[size_t(lift)].origin.z;
    CHECK_MSG(down > w - 0.07f && down < w + 0.07f, std::to_string(w) + " " + std::to_string(down));
  }
  frames(*h, 1);
  CHECK(h->s().entities[size_t(lift)].origin.z == top - 128);  // at rest at the bottom
  CHECK(h->s().entities[size_t(lift)].velocity.z == 0);
}

TEST(a_player_on_the_lowered_plat_rides_it_up) {
  auto h = open_hall();
  run(*h, 40);
  sv::map_use_targets(h->s(), h->named("trigger_relay"), -1);
  frames(*h, 20);  // down at the bottom: its top 8 above the floor
  place(*h, 448, 0, 8 + 24 + 1);
  run(*h, 8 * 25);  // into the field above it: it rises, and him with it
  CHECK_MSG(z_of(h->p()) > 136 + 23 && z_of(h->p()) < 136 + 26, std::to_string(z_of(h->p())));
  CHECK(h->e("func_plat").origin.z == 0);
}

TEST(a_door_opens_for_a_player_stops_shots_while_shut_and_closes_after_its_wait) {
  auto h = open_hall();
  run(*h, 40);
  sv::SequelEntity& door = h->e("func_door");
  // Shut, it stands between the start and the far side: a line through it is stopped.
  const sh::SequelTrace t =
      sv::map_trace(h->s(), {-400, 0, 40}, {}, {}, {-200, 0, 40}, sh::kSqSolid);
  CHECK(t.fraction < 1 && t.entity == sv::map_row(h->named("func_door")));
  place(*h, -400, 0, 24);
  run(*h, 8 * 10, 200);  // toward it: into its field, which reaches 60 out
  CHECK(door.state != 0);
  run(*h, 8 * 10);
  CHECK(door.origin.z == 120);  // up by its height less the lip
  place(*h, -600, 0, 24);       // away from its field
  run(*h, 8 * 40);              // 3 s open, then down at 200 units/s
  CHECK(door.origin.z == 0 && door.state == 0);
}

TEST(a_push_sets_the_velocity_a_hurt_takes_health_and_a_pad_teleports) {
  auto h = open_hall();
  run(*h, 40);
  place(*h, 650, 0, 24);
  run(*h, 1);
  CHECK_MSG(h->p().move.velocity[2] / 8 > 300, std::to_string(h->p().move.velocity[2] / 8));
  place(*h, 850, 0, 24);
  run(*h, 8 * 10);
  CHECK(h->p().health < 100 && h->p().alive);
  CHECK(h->s().obituaries.empty());
  place(*h, -800, 150, 24);
  run(*h, 2);
  CHECK(h->p().move.origin[0] == 300 * 8 && h->p().move.origin[1] == -150 * 8);
  CHECK(h->p().view_angles.y == 90);
}

TEST(a_trigger_once_fires_the_level_change_outside_deathmatch) {
  auto h = open_hall("0");
  run(*h, 40);
  CHECK(h->r->pending_map.empty());
  place(*h, -975, 0, 24);
  run(*h, 2);
  CHECK(h->r->pending_map == "next");
}
