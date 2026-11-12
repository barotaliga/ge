// Tests: the sequel's campaign (games/shooters_edge/server/sequel_campaign.cpp) in a hall built
// here: a help computer's message marks the help unread and beeps three times, `help` shows it and
// clears the mark; a level change carries the body (health cut to its maximum) and comes in at the
// spawn point the exit named; a new game starts afresh; the cross-level bits wake a target on the
// next level and a unit's end clears them.
//
// ge:summary Tests: the sequel's campaign: help news and its screen, bodies carried, spawn points
// ge:keywords test sequel campaign help-computer carried-body spawn-point crosslevel
// ge:skill shooter testing
// ge:test tests/shooters_edge_sequel_campaign_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_campaign.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_server.hpp"

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

// A floored hall with two starts (the unnamed one west, "east" east) and `extra` entities.
std::shared_ptr<const br::Map> hall(const std::vector<Pairs>& extra) {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "info_player_start"},
                               {"origin", "600 0 24"},
                               {"angle", "180"},
                               {"targetname", "east"}}));
  for (const Pairs& m : extra) w.entities.push_back(entity(m));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Game {
  sv::RulesRunner r;
  sv::MapSpec map;
  uint32_t sequence = 0;
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p() const { return s().players[0]; }
  int find(const char* classname) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].classname == classname) return static_cast<int>(i);
    return -1;
  }
};

std::unique_ptr<Game> open_game(const std::vector<Pairs>& extra) {
  auto g = std::make_unique<Game>();
  g->r = sv::sequel_runner(sv::make_sequel_server({}));
  g->r.console("deathmatch 0");
  g->map.name = "hall";
  g->map.brushes = hall(extra);
  std::string error;
  CHECK_MSG(g->r.start(g->map, error), error);
  g->r.connect(0, "Ana", false, false);
  return g;
}

void run(Game& g, int ticks) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++g.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    g.r.frame(&pk, 1);
  }
}

// The next map, as the host starts it on the change the rules asked for.
void next_level(Game& g) {
  CHECK(!g.s().pending_map.empty());
  std::string error;
  CHECK_MSG(g.r.start(g.map, error), error);
}

constexpr int kSecond = 77;  // host ticks of 13 ms

Pairs relay(const char* name, const char* target) {
  return {{"classname", "trigger_relay"}, {"targetname", name}, {"target", target}};
}

}  // namespace

TEST(a_help_message_beeps_three_times_until_help_is_read) {
  auto g = open_game({relay("go", "news"),
                      {{"classname", "target_help"},
                       {"targetname", "news"},
                       {"spawnflags", "1"},
                       {"message", "Find the data cd."}}});
  run(*g, 10);
  CHECK(g->p().help_unread == 0);
  sv::map_use_targets(g->s(), g->find("trigger_relay"), 0);
  CHECK(g->s().help[0] == "Find the data cd." && g->s().help_changed == 1);
  run(*g, 10);  // the next rules frame
  CHECK(g->p().help_unread >= 1);
  run(*g, 20 * kSecond);
  CHECK_MSG(g->p().help_unread == 4, std::to_string(g->p().help_unread));  // three beeps
  CHECK(g->r.client_command(0, "help") == sv::CommandResult::mod);
  CHECK(g->p().help_open && g->p().help_unread == 0);
  const std::string screen = sv::campaign_help_screen(g->s());
  CHECK_MSG(screen.find("Find the data cd.") != std::string::npos, screen);
  CHECK_MSG(screen.find("medium") != std::string::npos, screen);
  CHECK(g->r.client_command(0, "help") == sv::CommandResult::mod);
  CHECK(!g->p().help_open);
}

TEST(a_level_change_carries_the_body_to_the_named_spawn_point) {
  auto g = open_game(
      {relay("go", "exit"),
       {{"classname", "target_changelevel"}, {"targetname", "exit"}, {"map", "hall$east"}}});
  run(*g, 10);
  const int shotgun = sv::sq_item("weapon_shotgun");
  g->p().body.counts[shotgun] = 1;
  g->p().body.health = 150;  // over its maximum
  sv::map_use_targets(g->s(), g->find("trigger_relay"), 0);
  run(*g, 2);
  next_level(*g);
  CHECK(g->p().body.counts[shotgun] == 1);
  CHECK_MSG(g->p().body.health == 100, std::to_string(g->p().body.health));
  CHECK_MSG(g->p().move.origin[0] == 600 * 8, std::to_string(g->p().move.origin[0]));
  // A new game starts afresh, at the unnamed start.
  CHECK(g->r.console("map hall"));
  next_level(*g);
  CHECK(g->p().body.counts[shotgun] == 0);
  CHECK(g->p().move.origin[0] == -600 * 8);
}

TEST(the_cross_level_bits_wake_a_target_on_the_next_level_and_a_unit_clears_them) {
  const std::vector<Pairs> extra = {
      relay("set", "bit"),
      relay("go", "exit"),
      {{"classname", "target_crosslevel_trigger"}, {"targetname", "bit"}, {"spawnflags", "2"}},
      {{"classname", "target_changelevel"}, {"targetname", "exit"}, {"map", "hall"}},
      {{"classname", "target_crosslevel_target"}, {"spawnflags", "2"}, {"target", "news"}},
      {{"classname", "target_help"}, {"targetname", "news"}, {"message", "The way is open."}}};
  auto g = open_game(extra);
  run(*g, 3 * kSecond);
  CHECK(g->s().help_changed == 0);  // the bit was not set when it looked
  int set = -1, go = -1;
  for (size_t i = 0; i < g->s().entities.size(); i++) {
    if (g->s().entities[i].targetname == "set") set = static_cast<int>(i);
    if (g->s().entities[i].targetname == "go") go = static_cast<int>(i);
  }
  sv::map_use_targets(g->s(), set, 0);
  CHECK(g->s().cross_bits == 2);
  sv::map_use_targets(g->s(), go, 0);
  run(*g, 2);
  next_level(*g);
  run(*g, 3 * kSecond);
  CHECK(g->s().help_changed == 1 && g->s().help[1] == "The way is open.");
  // A unit's end (`*`) clears the bits.
  g->s().entities[size_t(g->find("target_changelevel"))].map = "*hall";
  sv::map_use_targets(g->s(), go, 0);
  CHECK(g->s().cross_bits == 0);
}
