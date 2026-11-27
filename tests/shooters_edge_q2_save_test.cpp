// Tests: Quake II's levels kept within a unit and its games saved (games/shooters_edge/server/
// q2_save.cpp), in halls built here: a level left and entered again keeps what was taken,
// counted and fired and goes on in time, 10 s past; its unfired cross-level targets check again;
// `sv_noreload`, a new unit, a new game and deathmatch keep nothing; a game saved and loaded plays
// on with the digest of the game never saved; the autosave puts the player at the spawn point
// with what he carried; the refusals; death's load menu.
//
// ge:summary Tests: Quake II's kept levels, its saves and loads bit for bit, the autosave
// ge:keywords test q2 save load autosave revisit kept-level sv_noreload menu_loadgame digest
// ge:skill shooter testing save
// ge:test tests/shooters_edge_q2_save_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_save.hpp"
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

sv::MapSpec spec(const char* name, const std::vector<Pairs>& extra) {
  sv::MapSpec m;
  m.name = name;
  m.brushes = hall(extra);
  return m;
}

Pairs relay(const char* name, const char* target) {
  return {{"classname", "trigger_relay"}, {"targetname", name}, {"target", target}};
}

Pairs changelevel(const char* name, const char* map) {
  return {{"classname", "target_changelevel"}, {"targetname", name}, {"map", map}};
}

struct Game {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p() const { return s().players[0]; }
  int find(const std::string& targetname) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].targetname == targetname) return static_cast<int>(i);
    return -1;
  }
  int classname(const char* name) const {
    for (size_t i = 0; i < s().entities.size(); i++)
      if (s().entities[i].classname == name) return static_cast<int>(i);
    return -1;
  }
};

std::unique_ptr<Game> runner(const char* deathmatch = "0") {
  auto g = std::make_unique<Game>();
  g->r = sv::q2_runner(sv::make_q2_server({}));
  g->r.console(std::string("deathmatch ") + deathmatch);
  return g;
}

std::unique_ptr<Game> open_game(const sv::MapSpec& map, const char* deathmatch = "0") {
  auto g = runner(deathmatch);
  std::string error;
  CHECK_MSG(g->r.start(map, error), error);
  g->r.connect(0, "Ana", false, false);
  return g;
}

// Host ticks of 13 ms; the move `k` of a script (walking, turning, firing, jumping by turns).
sh::MoveCmd scripted(int k) {
  sh::MoveCmd c;
  c.msec = 13;
  c.forward = (k / 40) % 2 == 0 ? 200.0f : -100.0f;
  c.side = (k / 25) % 3 == 1 ? 150.0f : 0.0f;
  c.angles.y = static_cast<float>((k * 3) % 360) * 0.25f;
  if (k % 30 < 4) c.buttons |= sh::kButtonAttack;
  if (k % 90 == 50) c.buttons |= sh::kButtonJump;
  return c;
}

void run(Game& g, int ticks, int script_from = -1, uint8_t held = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++g.sequence;
    sh::MoveCmd c;
    c.msec = 13;
    if (script_from >= 0) c = scripted(script_from + i);
    c.buttons |= held;
    pk.cmds[0] = pk.cmds[1] = pk.cmds[2] = c;
    g.r.frame(&pk, 1);
  }
}

constexpr int kSecond = 77;  // host ticks of 13 ms

// The change the rules asked for, started on `map` as the host would.
void go_to(Game& g, const sv::MapSpec& map) {
  CHECK(!g.s().pending_map.empty());
  std::string error;
  CHECK_MSG(g.r.start(map, error), error);
}

void leave_by(Game& g, const char* relay_name) {
  sv::map_use_targets(g.s(), g.find(relay_name), 0);
  run(g, 2);
}

}  // namespace

TEST(a_level_entered_again_keeps_what_was_taken_and_counted_and_goes_on) {
  const sv::MapSpec a = spec("base1", {relay("out", "to_b"),
                                       relay("tick", "count"),
                                       changelevel("to_b", "base2"),
                                       {{"classname", "item_armor_shard"}, {"origin", "-600 0 24"}},
                                       {{"classname", "trigger_counter"},
                                        {"targetname", "count"},
                                        {"count", "3"},
                                        {"spawnflags", "1"}}});
  const sv::MapSpec b = spec("base2", {relay("back", "to_a"), changelevel("to_a", "base1$east")});
  auto g = open_game(a);
  run(*g, 2 * kSecond);
  const int shard = g->classname("item_armor_shard");
  CHECK(g->s().entities[size_t(shard)].gone);  // taken where the player came in
  sv::map_use_targets(g->s(), g->find("tick"), 0);
  CHECK_MSG(g->s().entities[size_t(g->find("count"))].count == 2,
            std::to_string(g->s().entities[size_t(g->find("count"))].count));
  const double left_at = g->s().level_time;
  leave_by(*g, "out");
  go_to(*g, b);
  CHECK(g->s().kept.size() == 1 && g->s().kept[0].first == "base1");
  CHECK(!g->s().autosave.empty());
  run(*g, kSecond);
  leave_by(*g, "back");
  go_to(*g, a);
  CHECK(g->s().entities[size_t(g->classname("item_armor_shard"))].gone);
  CHECK(g->s().entities[size_t(g->find("count"))].count == 2);
  // Its time went on: what it had, the two settling frames and the ten seconds alone.
  CHECK_MSG(g->s().level_time + 0.001 >= left_at + 10.0, std::to_string(g->s().level_time));
  CHECK_MSG(g->p().move.origin[0] == 600 * 8, std::to_string(g->p().move.origin[0]));
  CHECK(g->p().alive);
}

TEST(an_unfired_crosslevel_target_checks_again_on_a_return) {
  const sv::MapSpec a =
      spec("base1",
           {relay("out", "to_b"),
            changelevel("to_b", "base2"),
            {{"classname", "target_crosslevel_target"}, {"spawnflags", "4"}, {"target", "news"}},
            {{"classname", "target_help"}, {"targetname", "news"}, {"message", "Open."}}});
  const sv::MapSpec b = spec(
      "base2",
      {relay("set", "bit"),
       relay("back", "to_a"),
       {{"classname", "target_crosslevel_trigger"}, {"targetname", "bit"}, {"spawnflags", "4"}},
       changelevel("to_a", "base1")});
  auto g = open_game(a);
  run(*g, 3 * kSecond);
  CHECK(g->s().help_changed == 0);  // the bit was not set when it looked
  leave_by(*g, "out");
  go_to(*g, b);
  run(*g, kSecond);
  sv::map_use_targets(g->s(), g->find("set"), 0);
  leave_by(*g, "back");
  go_to(*g, a);  // it looked again within the 10 s alone
  CHECK_MSG(g->s().help_changed == 1 && g->s().help[1] == "Open.",
            std::to_string(g->s().help_changed));
}

TEST(noreload_a_new_unit_a_new_game_and_deathmatch_keep_nothing) {
  const auto there_and_back = [](const char* deathmatch, const char* extra, const char* exit) {
    const sv::MapSpec a =
        spec("base1", {relay("out", "to_b"),
                       changelevel("to_b", exit),
                       {{"classname", "item_armor_shard"}, {"origin", "-600 0 24"}}});
    const sv::MapSpec b = spec("base2", {relay("back", "to_a"), changelevel("to_a", "base1")});
    auto g = open_game(a, deathmatch);
    if (extra != nullptr) g->r.console(extra);
    run(*g, kSecond);
    leave_by(*g, "out");
    if (deathmatch[0] != '0' || std::string(exit)[0] == '*') {
      run(*g, 6 * kSecond);                // past the hold
      run(*g, 10, -1, sh::kButtonAttack);  // single player leaves on a button
    }
    go_to(*g, b);
    run(*g, kSecond);
    leave_by(*g, "back");
    if (deathmatch[0] != '0') {  // a match's intermission waits for a button too
      run(*g, 6 * kSecond);
      run(*g, 10, -1, sh::kButtonAttack);
    }
    go_to(*g, a);
    const bool fresh =
        !g->s().entities[size_t(g->classname("item_armor_shard"))].gone && g->s().level_time < 1.0;
    return fresh;
  };
  CHECK(there_and_back("0", "sv_noreload 1", "base2"));
  CHECK(there_and_back("0", nullptr, "*base2"));
  CHECK(there_and_back("1", "dmflags 4096", "base2"));
  CHECK(!there_and_back("0", nullptr, "base2"));  // the plain change keeps it
  // A new game forgets the unit.
  const sv::MapSpec a = spec("base1", {relay("out", "to_b"), changelevel("to_b", "base2")});
  auto g = open_game(a);
  run(*g, kSecond);
  leave_by(*g, "out");
  go_to(*g, spec("base2", {}));
  CHECK(g->s().kept.size() == 1);
  CHECK(g->r.console("map base1"));
  go_to(*g, a);
  CHECK(g->s().kept.empty());
}

TEST(a_game_saved_and_loaded_plays_on_as_the_game_never_saved) {
  const sv::MapSpec a =
      spec("base1", {{{"classname", "monster_soldier"}, {"origin", "300 0 24"}, {"angle", "180"}}});
  auto g = open_game(a);
  const int shotgun = sv::sq_item("weapon_shotgun");
  run(*g, 3 * kSecond, 0);
  g->p().body.counts[shotgun] = 1;
  g->p().body.ammo[int(sv::SqAmmo::shells)] = 30;
  run(*g, 2 * kSecond, 3 * kSecond);
  std::vector<uint8_t> file;
  std::string error;
  CHECK_MSG(sv::q2_save(g->s(), file, error), error);
  CHECK(sv::q2_save_map(file, "test", error) == "base1");
  auto h = runner();
  CHECK_MSG(sv::q2_load(h->r, a, file, "test", error), error);
  h->sequence = g->sequence;
  CHECK_MSG(sv::q2_digest(h->s()) == sv::q2_digest(g->s()), "the loaded game differs");
  for (int k = 0; k < 6 * kSecond; k++) {
    run(*g, 1, 5 * kSecond + k);
    run(*h, 1, 5 * kSecond + k);
    if (sv::q2_digest(h->s()) != sv::q2_digest(g->s())) {
      CHECK_MSG(false, "the loaded game went its own way at tick " + std::to_string(k));
      break;
    }
  }
  CHECK(h->p().body.counts[shotgun] == 1);
  CHECK(!h->s().obituaries.empty() || h->s().monsters_killed >= 0);
}

// Every piece of a monster's mind, a player's marks the monsters read, the pieces flying and
// the bad areas come back from a save as they were: a load that left one at its default played on
// differently (a monster's footstep roll on base1 within 6 ticks; Ground Zero's gibs at once).
TEST(a_save_keeps_the_monsters_whole_minds_the_players_marks_and_the_pieces) {
  const sv::MapSpec a =
      spec("base1", {{{"classname", "monster_soldier"}, {"origin", "300 0 24"}, {"angle", "180"}}});
  auto g = open_game(a);
  run(*g, 1 * kSecond, 0);
  sv::Q2Server& s = g->s();
  CHECK(!s.monsters.empty());
  sv::Q2Monster& m = s.monsters[0];
  m.pose.heard = 7;
  m.pose.cue_fidget = true;
  m.dodge_until = 12.5;
  m.hint_check = 3.25;
  m.strafe = -1;
  m.blind_at = {1, 2, 3};
  m.boss.timer = 4.5;
  m.boss.legs = 9;
  sv::Q2Player& p = g->p();
  p.hostile_until = 8.75;
  p.impact_frame = 11;
  p.impact_at = {4, 5, 6};
  p.trail_last = {7, 8, 9};
  p.time_budget_ms = 321;
  s.gibs[3].live = true;
  s.gibs[3].origin = {10, 20, 30};
  s.gibs[3].dies_at = 6.5;
  s.next_gib = 4;
  s.bad_areas.push_back({{0, 0, 0}, {16, 16, 16}, {8, 8, 8}, 9.5, -1});
  std::vector<uint8_t> file;
  std::string error;
  CHECK_MSG(sv::q2_save(s, file, error), error);
  auto h = runner();
  CHECK_MSG(sv::q2_load(h->r, a, file, "test", error), error);
  const sv::Q2Server& b = h->s();
  const sv::Q2Monster& n = b.monsters[0];
  CHECK(n.pose.heard == 7);
  CHECK(n.pose.cue_fidget);
  CHECK(n.dodge_until == 12.5);
  CHECK(n.hint_check == 3.25);
  CHECK(n.strafe == -1);
  CHECK(n.blind_at.y == 2);
  CHECK(n.boss.timer == 4.5);
  CHECK(n.boss.legs == 9);
  const sv::Q2Player& q = b.players[0];
  CHECK(q.hostile_until == 8.75);
  CHECK(q.impact_frame == 11);
  CHECK(q.impact_at.z == 6);
  CHECK(q.trail_last.x == 7);
  CHECK(q.time_budget_ms == 321);
  CHECK(b.gibs[3].live);
  CHECK(b.gibs[3].origin.y == 20);
  CHECK(b.gibs[3].dies_at == 6.5);
  CHECK(b.next_gib == 4);
  CHECK(b.bad_areas.size() == 1 && b.bad_areas[0].until == 9.5);
}

TEST(a_save_is_refused_when_it_cannot_be_and_a_damaged_one_does_not_load) {
  std::vector<uint8_t> file;
  std::string error;
  auto dm = open_game(spec("base1", {}), "1");
  run(*dm, 10);
  CHECK(!sv::q2_save(dm->s(), file, error));
  CHECK_MSG(error.find("deathmatch") != std::string::npos, error);
  auto g = open_game(spec("base1", {}));
  run(*g, 10);
  sv::fight_hurt(g->s(), 0, -1, {}, 1000, 0, 0, 0);
  CHECK(!g->p().alive);
  CHECK(!sv::q2_save(g->s(), file, error));
  CHECK_MSG(error.find("dead") != std::string::npos, error);
  auto ok = open_game(spec("base1", {}));
  run(*ok, 10);
  CHECK_MSG(sv::q2_save(ok->s(), file, error), error);
  file[file.size() / 2] ^= 0x40;
  auto h = runner();
  CHECK(!sv::q2_load(h->r, spec("base1", {}), file, "broken", error));
  CHECK_MSG(error.find("broken") != std::string::npos, error);
}

TEST(the_autosave_brings_the_player_in_at_the_spawn_point_with_what_he_carried) {
  const sv::MapSpec a = spec("base1", {relay("out", "to_b"), changelevel("to_b", "base2$east")});
  const sv::MapSpec b = spec("base2", {});
  auto g = open_game(a);
  run(*g, kSecond);
  const int shotgun = sv::sq_item("weapon_shotgun");
  g->p().body.counts[shotgun] = 1;
  g->p().body.health = 61;
  leave_by(*g, "out");
  go_to(*g, b);
  const std::vector<uint8_t> autosave = g->s().autosave;
  CHECK(!autosave.empty());
  std::string error;
  CHECK(sv::q2_save_map(autosave, "save0", error) == "base2");
  auto h = runner();
  CHECK_MSG(sv::q2_load(h->r, b, autosave, "save0", error), error);
  CHECK(h->s().clients[0].active && h->p().alive);
  CHECK(h->p().body.counts[shotgun] == 1);
  CHECK(h->p().body.health == 61);
  CHECK_MSG(h->p().move.origin[0] == 600 * 8, std::to_string(h->p().move.origin[0]));
  CHECK(h->s().kept.size() == 1 && h->s().kept[0].first == "base1");
}

TEST(a_button_a_second_after_dying_alone_asks_for_the_load_menu) {
  auto g = open_game(spec("base1", {}));
  run(*g, 10);
  sv::fight_hurt(g->s(), 0, -1, {}, 1000, 0, 0, 0);
  const auto asked = [&] {
    for (const sv::NetEvent& e : g->s().out.events)
      if (e.kind == sv::EventKind::stuffcmd && e.text == "menu_loadgame\n") return true;
    return false;
  };
  run(*g, kSecond / 2, -1, sh::kButtonAttack);  // too soon
  CHECK(!asked());
  run(*g, kSecond, -1, sh::kButtonAttack);
  CHECK(asked());
  CHECK(!g->p().alive);  // nothing is loaded by itself
}

TEST(a_client_numbering_its_packets_afresh_on_the_next_map_is_heard) {
  // The window numbers its packets from 1 on every map; the rules kept from level to level must
  // take them, not wait for the old count.
  const sv::MapSpec a = spec("base1", {relay("out", "to_b"), changelevel("to_b", "base2")});
  auto g = open_game(a);
  run(*g, 2 * kSecond);
  leave_by(*g, "out");
  go_to(*g, spec("base2", {}));
  g->sequence = 0;
  run(*g, 10, -1, sh::kButtonAttack);
  CHECK_MSG(g->p().last_sequence == 10, std::to_string(g->p().last_sequence));
  CHECK((g->p().buttons & sh::kButtonAttack) != 0);
}
