// Tests: Quake II's campaign (games/shooters_edge/server/q2_campaign.cpp) in a hall built
// here: a help computer's message marks the help unread and beeps three times, `help` shows it and
// clears the mark; a level change carries the body (health cut to its maximum) and comes in at the
// spawn point the exit named; a new game starts afresh; the cross-level bits wake a target on the
// next level and a unit's end clears them.
//
// ge:summary Tests: Quake II's campaign: help news and its screen, bodies carried, spawn points
// ge:keywords test q2 campaign help-computer carried-body spawn-point crosslevel
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_campaign_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/server/q2_campaign.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_screens.hpp"
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
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p() const { return s().players[0]; }
  int find(const char* classname) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].classname == classname) return static_cast<int>(i);
    return -1;
  }
};

std::unique_ptr<Game> open_game(const std::vector<Pairs>& extra) {
  auto g = std::make_unique<Game>();
  g->r = sv::q2_runner(sv::make_q2_server({}));
  g->r.console("deathmatch 0");
  g->map.name = "hall";
  g->map.brushes = hall(extra);
  std::string error;
  CHECK_MSG(g->r.start(g->map, error), error);
  g->r.connect(0, "Ana", false, false);
  return g;
}

void run(Game& g, int ticks, int buttons = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++g.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    c.buttons = static_cast<uint8_t>(buttons);
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
  CHECK(g->p().screen == sv::kScreenHelp && g->p().help_unread == 0);
  CHECK(g->r->world.clients[0].numbers.stats[13] == 1 &&
        g->r->world.clients[0].layout == g->p().layout);
  const std::string screen = sv::campaign_help_screen(g->s());
  CHECK_MSG(screen.find("Find the data cd.") != std::string::npos, screen);
  CHECK_MSG(screen.find("medium") != std::string::npos, screen);
  CHECK(g->r.client_command(0, "help") == sv::CommandResult::mod);
  CHECK(g->p().screen == sv::kScreenNone);
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

TEST(a_level_string_names_the_movies_the_map_its_spot_and_a_new_unit) {
  sv::LevelString l = sv::level_string("eou1_.cin+*bunk1$start");
  CHECK(l.shows == "eou1_.cin" && l.map == "bunk1" && l.spawn == "start" && l.unit);
  l = sv::level_string("end.cin+victory.pcx");
  CHECK(l.shows == "end.cin+victory.pcx" && l.map.empty() && !l.unit);
  l = sv::level_string("*boss1$bosstart");
  CHECK(l.shows.empty() && l.map == "boss1" && l.spawn == "bosstart" && l.unit);
  l = sv::level_string("base2");
  CHECK(l.map == "base2" && l.spawn.empty() && !l.unit);
  // A pack's NEW GAME: the star on the first movie, two movies before the map.
  l = sv::level_string("*logo.cin+xin.cin+xswamp");
  CHECK(l.shows == "logo.cin+xin.cin" && l.map == "xswamp" && l.unit);
}

TEST(a_units_end_plays_its_movie_after_the_intermission_then_the_next_unit) {
  auto g = open_game({relay("go", "exit"),
                      {{"classname", "target_changelevel"},
                       {"targetname", "exit"},
                       {"map", "eou1_.cin+*hall$east"}}});
  run(*g, 10);
  sv::map_use_targets(g->s(), g->find("trigger_relay"), 0);
  run(*g, kSecond);
  CHECK(g->s().intermission_at > 0 && g->s().pending_map.empty());
  run(*g, 21 * kSecond);  // single player waits on a button, however long
  CHECK(g->s().pending_map.empty());
  run(*g, 10, 1);  // attack, held over a tenth
  CHECK(g->s().pending_map == "hall" && g->s().cinematic == "eou1_.cin");
  CHECK(g->s().next_unit && g->s().next_spawn == "east");
  next_level(*g);
  CHECK(g->s().cinematic.empty());
  CHECK_MSG(g->p().move.origin[0] == 600 * 8, std::to_string(g->p().move.origin[0]));
}

TEST(the_games_last_exit_shows_its_movie_and_picture_and_no_map_follows) {
  auto g = open_game({relay("go", "exit"),
                      {{"classname", "target_changelevel"},
                       {"targetname", "exit"},
                       {"map", "end.cin+victory.pcx"}}});
  run(*g, 10);
  sv::map_use_targets(g->s(), g->find("trigger_relay"), 0);
  CHECK(g->s().intermission_at == 0);  // no unit follows: no intermission
  CHECK(g->s().pending_map.empty() && g->s().cinematic == "end.cin+victory.pcx");
  CHECK(g->s().game_over);
  g->s().cinematic.clear();  // the window took it
  sv::map_use_targets(g->s(), g->find("trigger_relay"), 0);
  CHECK(g->s().cinematic.empty());  // the game is over: the exit is spent
}

// A dead co-op player comes back when he presses attack, as he entered the level, keeping the keys
// he held when he died (the sweep of every map in co-op found the dead never came back).
TEST(in_coop_the_dead_come_back_on_attack_keeping_their_keys) {
  auto g = std::make_unique<Game>();
  g->r = sv::q2_runner(sv::make_q2_server({}));
  g->r.console("deathmatch 0");
  g->r.console("coop 1");
  g->map.name = "hall";
  g->map.brushes = hall({});
  std::string error;
  CHECK_MSG(g->r.start(g->map, error), error);
  g->r.connect(0, "Ana", false, false);
  run(*g, 6 * kSecond);  // `kill` waits 5 s after a spawn
  const int key = sv::sq_item("key_blue_key");
  g->p().body.counts[key] = 1;
  g->r.client_command(0, "kill");
  CHECK(!g->p().alive);
  run(*g, 2 * kSecond);  // no button: he stays down
  CHECK(!g->p().alive);
  run(*g, 20, sh::kButtonAttack);
  CHECK(g->p().alive);
  CHECK(g->p().body.counts[key] == 1);
  CHECK(g->p().health == 100);
}

// An item is dropped 0.2 s into the level, once the movers stand where they start: one resting on a
// mover's top stays on it instead of falling through to the floor below.
TEST(an_item_on_a_movers_top_rests_on_it) {
  auto g = std::make_unique<Game>();
  g->r = sv::q2_runner(sv::make_q2_server({}));
  g->r.console("deathmatch 0");
  g->map.name = "hall";
  br::BrushWorld w;
  w.models.resize(2);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  const float blo[3] = {-64, -64, 0}, bhi[3] = {64, 64, 64};
  w.models[1].push_back(br::box_brush(blo, bhi, sh::kSqSolid, "world/wall"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-600 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "func_wall"}, {"model", "*1"}}));
  w.entities.push_back(entity({{"classname", "key_pass"}, {"origin", "0 0 96"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  g->map.brushes = map;
  CHECK_MSG(g->r.start(g->map, error), error);
  g->r.connect(0, "Ana", false, false);
  run(*g, kSecond);
  const sv::Q2Entity& pass = g->s().entities[size_t(g->find("key_pass"))];
  CHECK(!pass.gone);
  CHECK_MSG(pass.origin.z > 64 + 14 && pass.origin.z < 64 + 16, std::to_string(pass.origin.z));
}

namespace {

std::unique_ptr<Game> coop_game(const char* name, const std::vector<Pairs>& extra) {
  auto g = std::make_unique<Game>();
  g->r = sv::q2_runner(sv::make_q2_server({}));
  g->r.console("deathmatch 0");
  g->r.console("coop 1");
  g->map.name = name;
  g->map.brushes = hall(extra);
  std::string error;
  CHECK_MSG(g->r.start(g->map, error), error);
  return g;
}

Pairs coop_spot(const char* origin, const char* targetname) {
  return {{"classname", "info_player_coop"}, {"origin", origin}, {"targetname", targetname}};
}

const sv::Q2Entity* spot_at(const sv::Q2Server& s, float x) {
  for (const sv::Q2Entity& e : s.entities)
    if (e.classname == "info_player_coop" && e.origin.x == x) return &e;
  return nullptr;
}

}  // namespace

// On a map whose co-op spots carry the wrong names, each takes the name of the first named start
// within 550 units; elsewhere the names stand.
TEST(in_coop_a_misnamed_maps_spots_take_the_name_of_their_start) {
  const std::vector<Pairs> spots = {coop_spot("300 0 24", "wrong"), coop_spot("-300 0 24", "far")};
  auto g = coop_game("mine1", spots);
  CHECK(spot_at(g->s(), 300)->targetname == "east");  // 300 from the start named east
  CHECK(spot_at(g->s(), -300)->targetname == "far");  // only the unnamed start is near
  auto other = coop_game("hall", spots);
  CHECK(spot_at(other->s(), 300)->targetname == "wrong");
}

TEST(in_coop_the_security_level_gets_three_spots_from_the_jail) {
  auto g = coop_game("security", {});
  int added = 0;
  for (const sv::Q2Entity& e : g->s().entities)
    if (e.classname == "info_player_coop" && e.targetname == "jail3" && e.origin.y == -164 &&
        e.origin.z == 80 && e.angles.y == 90)
      added++;
  CHECK(added == 3);
  CHECK(spot_at(g->s(), 124) && spot_at(g->s(), 252) && spot_at(g->s(), 316));
}

// A co-op player past the first with no spot of the level's name takes the n-th co-op spot within
// 550 units of the start, and stands on the start only when there is none.
TEST(in_coop_a_player_with_no_named_spot_takes_one_near_the_start) {
  auto g = coop_game("hall", {coop_spot("-500 0 24", "x"), coop_spot("-400 0 24", "x"),
                              coop_spot("400 0 24", "x")});
  for (int i = 0; i < 4; i++) g->r.connect(i, "P" + std::to_string(i), false, false);
  const auto x = [&](int i) { return float(g->s().players[i].move.origin[0]) / 8; };
  CHECK(x(0) == -600);
  CHECK(x(1) == -500);
  CHECK(x(2) == -400);
  CHECK(x(3) == -600);  // the third spot is past 550: the start
}

// The network server seats a player again after a level change by connecting him to the slot the
// rules kept: he keeps what he carried and his score.
TEST(a_player_seated_again_after_a_level_change_keeps_what_he_carried) {
  auto g = coop_game("hall", {});
  g->r.connect(0, "Ana", false, false);
  run(*g, kSecond);
  const int key = sv::sq_item("key_blue_key");
  g->p().body.counts[key] = 1;
  g->p().frags = 3;
  g->r.connect(0, "Ana", false, false);
  CHECK(g->p().body.counts[key] == 1);
  CHECK(g->p().frags == 3);
}

// A deathmatch spot and every teleporter exit, led to or not, are discs a player stands on; outside
// deathmatch a deathmatch spot is no disc.
TEST(deathmatch_spots_and_every_teleporter_exit_are_discs) {
  const std::vector<Pairs> points = {
      {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}},
      {{"classname", "misc_teleporter_dest"}, {"origin", "200 0 24"}, {"targetname", "nowhere"}}};
  for (const char* dm : {"1", "0"}) {
    auto g = std::make_unique<Game>();
    g->r = sv::q2_runner(sv::make_q2_server({}));
    g->r.console(std::string("deathmatch ") + dm);
    g->map.name = "hall";
    g->map.brushes = hall(points);
    std::string error;
    CHECK_MSG(g->r.start(g->map, error), error);
    const sv::Q2Entity& spot = g->s().entities[size_t(g->find("info_player_deathmatch"))];
    const sv::Q2Entity& exit = g->s().entities[size_t(g->find("misc_teleporter_dest"))];
    CHECK(exit.pad == 2);
    CHECK((spot.pad == 2) == (dm[0] == '1'));
    CHECK(g->r->world.models[size_t(exit.modelindex)] == "models/objects/dmspot/tris.md2");
    CHECK(g->r->world.models.back() == sv::kQ2PlayerModel);
  }
}
