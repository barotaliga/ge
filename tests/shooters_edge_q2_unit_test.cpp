// Quake II's unit played through by its script (bots/q2_unit.hpp): a map whose door opens
// only for the key lying behind the player, played to the exit out of the unit; a map with a button
// and an objective; the same run twice to the same bits.
//
// ge:summary Tests: Quake II's unit played through: a key's door, a button, the unit's exit
// ge:keywords test q2 unit run_q2_unit way-through key trigger_key button exit target_goal
// ge:skill shooter bots
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/bots/q2_unit.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace bt = shooters_edge::bots;

using Pairs = std::vector<std::pair<std::string, std::string>>;

// A hall x -1024..1024, y -512..512, split at x 0 by a wall whose doorway a door shuts.
struct Hall {
  br::BrushWorld world;
  int models = 1;
  explicit Hall(bool door = true, const char* facing = "0") {
    world.models.resize(1);
    box(0, -1024, -512, -16, 1024, 512, 0);
    box(0, -1040, -512, -16, -1024, 512, 256);
    box(0, 1024, -512, -16, 1040, 512, 256);
    box(0, -1040, 512, -16, 1040, 528, 256);
    box(0, -1040, -528, -16, 1040, -512, 256);
    box(0, -8, -512, 0, 8, -64, 256);
    box(0, -8, 64, 0, 8, 512, 256);
    box(0, -8, -64, 128, 8, 64, 256);
    put({{"classname", "info_player_start"}, {"origin", "-800 0 24"}, {"angle", facing}});
    if (door)
      brush(-8, -64, 0, 8, 64, 128,
            {{"classname", "func_door"}, {"targetname", "gate"}, {"angle", "-1"}, {"wait", "-1"}});
  }
  void box(int model, float x0, float y0, float z0, float x1, float y1, float z1,
           int32_t contents = sh::kSqSolid) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[static_cast<size_t>(model)].push_back(
        br::box_brush(lo, hi, contents, "world/wall"));
  }
  void brush(float x0, float y0, float z0, float x1, float y1, float z1, Pairs pairs,
             int32_t contents = sh::kSqSolid) {
    world.models.emplace_back();
    box(models, x0, y0, z0, x1, y1, z1, contents);
    pairs.emplace_back("model", "*" + std::to_string(models++));
    put(std::move(pairs));
  }
  void put(Pairs pairs) {
    br::Entity e;
    e.pairs = std::move(pairs);
    world.entities.push_back(e);
  }
  void exit_to(const char* map) {
    brush(900, -96, 0, 1000, 96, 96, {{"classname", "trigger_multiple"}, {"target", "out"}});
    put({{"classname", "target_changelevel"}, {"targetname", "out"}, {"map", map}});
  }
  std::shared_ptr<const br::Map> read() const {
    std::vector<uint8_t> bytes;
    std::string error;
    CHECK_MSG(br::write_brush_world(world, &bytes, &error), error);
    auto map = std::make_shared<br::Map>();
    CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
    return map;
  }
};

std::map<std::string, std::shared_ptr<const br::Map>> unit() {
  std::map<std::string, std::shared_ptr<const br::Map>> maps;
  {
    Hall h;  // the key behind him, its lock at the door
    h.put({{"classname", "key_blue_key"}, {"origin", "-600 -400 24"}});
    h.brush(-120, -64, 0, -40, 64, 96, {{"classname", "trigger_multiple"}, {"target", "lock"}});
    h.put({{"classname", "trigger_key"},
           {"targetname", "lock"},
           {"item", "key_blue_key"},
           {"target", "gate"}});
    h.exit_to("second");
    maps["first"] = h.read();
  }
  {
    Hall h;  // a button on the north wall, an objective past the door
    h.brush(-500, 496, 32, -440, 512, 96,
            {{"classname", "func_button"}, {"target", "gate"}, {"angle", "90"}});
    h.brush(300, -512, 0, 320, 512, 96, {{"classname", "trigger_once"}, {"target", "goal"}});
    h.put({{"classname", "target_goal"}, {"targetname", "goal"}});
    h.exit_to("*after");
    maps["second"] = h.read();
  }
  return maps;
}

bool load(void* ctx, const std::string& name, sv::MapSpec& out, std::string& error) {
  const auto& maps = *static_cast<std::map<std::string, std::shared_ptr<const br::Map>>*>(ctx);
  const auto it = maps.find(name);
  if (it == maps.end()) {
    error = "no map " + name;
    return false;
  }
  out = sv::MapSpec();
  out.name = name;
  out.brushes = it->second;
  return true;
}

bt::UnitReport play(std::map<std::string, std::shared_ptr<const br::Map>>& maps, int partners = 0) {
  bt::UnitOptions opt;
  opt.partners.assign(static_cast<size_t>(partners), ge::shooterbots::AimSkill::expert);
  opt.load = &load;
  opt.load_ctx = &maps;
  opt.first = "first";
  opt.map_seconds = 90;
  return bt::run_q2_unit(opt);
}

}  // namespace

TEST(the_script_takes_the_key_opens_its_door_and_leaves_the_unit) {
  auto maps = unit();
  const bt::UnitReport r = play(maps);
  CHECK_MSG(r.ok, r.error);
  CHECK(r.unit_done && r.out_to == "after");
  CHECK(r.maps.size() == 2);
  if (r.maps.size() != 2) return;
  const bt::UnitMap& a = r.maps[0];
  CHECK(a.finished && a.next == "second");
  CHECK(a.keys == 1 && a.keys_taken == 1 && a.took_keys[0] == "key_blue_key");
  const bt::UnitMap& b = r.maps[1];
  CHECK(b.finished && b.goals == 1 && b.goals_found == 1);
  CHECK(b.way.steps_done >= 1);  // the button
}

TEST(a_party_of_two_plays_the_unit_in_coop_and_leaves_it) {
  // Two bots in co-op, the work split between them (party.hpp): the key's door opened, the button
  // pressed, the unit left; and the party's run replays to the bit. (In co-op a key stays where it
  // lay, every player taking his own, so it is not counted taken by its going.)
  auto maps = unit();
  const bt::UnitReport r = play(maps, 1);
  CHECK_MSG(r.ok, r.error);
  CHECK(r.unit_done && r.out_to == "after");
  CHECK(r.maps.size() == 2);
  if (r.maps.size() != 2) return;
  CHECK_MSG(r.maps[0].finished && r.maps[0].next == "second", r.maps[0].end);
  CHECK(r.maps[0].party.size() == 1 && !r.maps[0].party[0].empty());
  CHECK(r.maps[0].shares > 0);
  CHECK(r.maps[1].finished && r.maps[1].goals_found == 1);
  const bt::UnitReport again = play(maps, 1);
  CHECK(again.ok && again.digest == r.digest);
}

TEST(the_same_unit_plays_to_the_same_bits) {
  auto maps = unit();
  const bt::UnitReport a = play(maps), b = play(maps);
  CHECK(a.ok && b.ok && a.digest == b.digest && a.digest != 0);
}

TEST(a_map_with_no_way_out_runs_out_of_time) {
  std::map<std::string, std::shared_ptr<const br::Map>> maps;
  Hall h;  // the door's lock with no key anywhere
  h.brush(-120, -64, 0, -40, 64, 96, {{"classname", "trigger_multiple"}, {"target", "lock"}});
  h.put({{"classname", "trigger_key"},
         {"targetname", "lock"},
         {"item", "key_blue_key"},
         {"target", "gate"}});
  h.exit_to("second");
  maps["first"] = h.read();
  bt::UnitOptions opt;
  opt.load = &load;
  opt.load_ctx = &maps;
  opt.first = "first";
  opt.map_seconds = 20;
  const bt::UnitReport r = bt::run_q2_unit(opt);
  CHECK_MSG(r.ok, r.error);
  CHECK(!r.unit_done && r.maps.size() == 1 && !r.maps[0].finished);
  CHECK(r.maps[0].end == "the time ran out");
}

// A pane of glass fills the doorway: his nav runs through it as open floor, so he shoots it out of
// his way and walks on to the exit.
// The pane is a window (seen through, as the full game's are) and he starts facing away from it
// (the start spot's turn his own, so his moves and shots go where he means).
TEST(the_script_shoots_the_glass_in_his_way) {
  std::map<std::string, std::shared_ptr<const br::Map>> maps;
  Hall h(false, "180");
  h.brush(-4, -64, 0, 4, 64, 128, {{"classname", "func_explosive"}, {"health", "10"}},
          sh::kSqWindow);
  h.exit_to("*after");
  maps["first"] = h.read();
  const bt::UnitReport r = play(maps);
  CHECK_MSG(r.ok, r.error);
  CHECK_MSG(r.unit_done && r.out_to == "after",
            r.maps.empty() ? std::string("no map")
                           : r.maps[0].end + " breaks " + std::to_string(r.maps[0].breaks) +
                                 " damage " + std::to_string(r.maps[0].damage));
  CHECK(!r.maps.empty() && r.maps[0].breaks > 0);
}
