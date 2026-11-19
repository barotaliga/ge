// The sequel's player commands (server/sequel_commands.hpp): chat and its flood guard, an unknown
// word said, the cheat gate, give, god, noclip, weaplast, the gestures, the lists, the refusals of
// use and drop, and the intermission's few words.
//
// ge:summary Tests: the sequel's player commands, chat, cheats, give, gestures, lists, refusals
// ge:keywords sequel client-command say flood cheats give god noclip weaplast wave players test
// ge:keywords spectator-chat sv_spectalk g_select_empty g_quick_weap g_swap_speed
// ge:skill testing shooter
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/sequel_move.hpp"
#include "shooters_edge/server/sequel_combat.hpp"
#include "shooters_edge/server/sequel_commands.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_poses.hpp"
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

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-300 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "300 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Game {
  sv::RulesRunner r;
  int players = 0;
  uint32_t sequence = 0;
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p(int i) const { return s().players[i]; }
};

std::unique_ptr<Game> game(const char* deathmatch, int players = 2, const char* extra = nullptr) {
  auto g = std::make_unique<Game>();
  g->r = sv::sequel_runner(sv::make_sequel_server({}));
  g->r.console(std::string("deathmatch ") + deathmatch);
  if (extra != nullptr) g->r.console(extra);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(g->r.start(map, error), error);
  static const char* kNames[] = {"Ana", "Ben", "Cy", "Di"};
  for (int i = 0; i < players; i++) g->r.connect(i, kNames[i], false, false);
  g->players = players;
  return g;
}

// Host ticks of 13 ms, every player standing still.
void run(Game& g, int ticks) {
  for (int t = 0; t < ticks; t++) {
    std::vector<sh::Packet> packets(static_cast<size_t>(g.players));
    for (int c = 0; c < g.players; c++) {
      sh::Packet& p = packets[static_cast<size_t>(c)];
      p.client = c;
      p.sequence = ++g.sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      p.cmds[0] = p.cmds[1] = p.cmds[2] = cmd;
    }
    g.r.frame(packets.data(), g.players);
  }
}

// The prints sent since `from` (events_total then), to `slot`, at `level` (-1 any).
std::vector<std::string> prints(const Game& g, int slot, int level = -1) {
  std::vector<std::string> out;
  for (const sv::NetEvent& e : g.s().out.events)
    if (e.kind == sv::EventKind::print && (e.to & (1u << slot)) != 0 &&
        (level < 0 || e.level == level))
      out.push_back(e.text);
  return out;
}

bool printed(const Game& g, int slot, const std::string& part, int level = -1) {
  for (const std::string& t : prints(g, slot, level))
    if (t.find(part) != std::string::npos) return true;
  return false;
}

}  // namespace

TEST(say_reaches_everyone_and_an_unknown_word_is_said) {
  auto g = game("1");
  run(*g, 10);
  g->r.client_command(0, "say \"hello there\"");
  CHECK(printed(*g, 1, "Ana: hello there\n", 3));
  CHECK(printed(*g, 0, "Ana: hello there\n", 3));
  g->r.client_command(1, "inven_drop now");
  CHECK(printed(*g, 0, "Ben: inven_drop now\n", 3));
}

TEST(say_team_goes_to_the_team_when_teams_are_on) {
  auto g = game("1", 3, "dmflags 64");
  g->r.user_info(0, "skin", "male/grunt");
  g->r.user_info(1, "skin", "male/grunt");
  g->r.user_info(2, "skin", "male/viper");
  run(*g, 10);
  g->r.client_command(0, "say_team rockets here");
  CHECK(printed(*g, 1, "(Ana): rockets here", 3));
  CHECK(!printed(*g, 2, "rockets here"));
  auto plain = game("1", 2);
  run(*plain, 10);
  plain->r.client_command(0, "say_team hi");
  CHECK(printed(*plain, 1, "Ana: hi", 3));  // no teams: said to everyone
}

TEST(a_spectators_team_line_reaches_only_the_spectators_and_spectalk_keeps_them_apart) {
  auto g = game("1", 2);
  g->r.connect(2, "Cy", true, false);  // a spectator
  g->r.connect(3, "Di", true, false);
  g->players = 4;
  run(*g, 10);
  g->r.client_command(2, "say_team who has the rail");
  CHECK(printed(*g, 3, "[SPEC] Cy: who has the rail\n", 3));
  CHECK(!printed(*g, 0, "who has the rail") && !printed(*g, 1, "who has the rail"));
  g->r.client_command(2, "say nice shot");
  CHECK(printed(*g, 0, "Cy: nice shot\n", 3));  // sv_spectalk 1: to everyone
  g->r.console("sv_spectalk 0");
  g->r.client_command(3, "say quiet now");
  CHECK(printed(*g, 2, "[SPEC] Di: quiet now\n", 3));
  CHECK(!printed(*g, 0, "quiet now"));  // a server with no lobby is always in its match
}

TEST(a_chat_flood_is_stopped_and_told) {
  auto g = game("1");
  run(*g, 10);
  for (int i = 0; i < 4; i++) g->r.client_command(0, "say line " + std::to_string(i));
  CHECK(printed(*g, 1, "line 3"));
  g->r.client_command(0, "say line 4");
  CHECK(!printed(*g, 1, "line 4"));
  CHECK(printed(*g, 0, "you can't talk for 10 seconds", 3));
  g->r.client_command(0, "say line 5");
  CHECK(printed(*g, 0, "You can't talk for"));
}

TEST(the_cheats_are_refused_in_deathmatch_without_cheats) {
  auto g = game("1");
  run(*g, 10);
  g->r.client_command(0, "god");
  CHECK(!g->p(0).god);
  CHECK(printed(*g, 0, "Cheats are off"));
  auto on = game("1", 2, "cheats 1");
  run(*on, 10);
  on->r.client_command(0, "god");
  CHECK(on->p(0).god);
  CHECK(printed(*on, 0, "God mode on"));
  auto single = game("0", 1);
  run(*single, 10);
  single->r.client_command(0, "notarget");
  CHECK(single->p(0).notarget);
}

TEST(god_takes_no_harm_but_the_push_lands_and_kill_clears_it) {
  auto g = game("0", 1);
  run(*g, 10);
  g->r.client_command(0, "god");
  const float health = g->p(0).health;
  sv::fight_hurt(g->s(), 0, -1, {1, 0, 0}, 50, 50, 0, 0);
  CHECK(g->p(0).health == health);
  CHECK(g->p(0).move.velocity[0] != 0);
  sv::fight_hurt(g->s(), 0, -1, {}, 100000, 0, 0, 3);  // a telefrag still kills
  CHECK(!g->p(0).alive);
  auto k = game("0", 1);
  run(*k, 500);
  k->r.client_command(0, "god");
  k->r.client_command(0, "kill");
  CHECK(!k->p(0).god);
  CHECK(!k->p(0).alive);
}

TEST(give_all_and_its_forms) {
  auto g = game("0", 1);
  run(*g, 10);
  sv::SequelPlayer& p = g->p(0);
  g->r.client_command(0, "give all");
  CHECK(p.body.armour == sv::SqArmour::body);
  CHECK(p.body.armour_count == 200);
  CHECK(p.body.ammo[int(sv::SqAmmo::rockets)] == p.body.max_ammo[int(sv::SqAmmo::rockets)]);
  CHECK(sv::sq_holds(p.body, int(sv::SqWeapon::railgun)));
  CHECK(p.body.counts[sv::sq_item_named("Quad Damage")] == 1);
  CHECK(p.body.counts[sv::sq_item_named("Power Shield")] == 1);
  g->r.client_command(0, "give health 7");
  CHECK(p.health == 7);
  g->r.client_command(0, "give rockets 3");
  CHECK(p.body.ammo[int(sv::SqAmmo::rockets)] == 3);
  g->r.client_command(0, "give Rockets");
  CHECK(p.body.ammo[int(sv::SqAmmo::rockets)] == 8);
  g->r.client_command(0, "give nothing at all");
  CHECK(printed(*g, 0, "There is no item called nothing at all."));
}

TEST(noclip_flies_through_walls_until_a_respawn) {
  auto g = game("0", 1);
  run(*g, 10);
  g->r.client_command(0, "noclip");
  CHECK(g->p(0).noclip);
  run(*g, 2);
  CHECK(g->p(0).move.type == ge::classic::SequelType::spectator);
  g->r.client_command(0, "noclip");
  run(*g, 2);
  CHECK(g->p(0).move.type == ge::classic::SequelType::normal);
}

TEST(weaplast_takes_up_the_weapon_held_before) {
  auto g = game("0", 1);
  run(*g, 10);
  sv::SequelPlayer& p = g->p(0);
  g->r.client_command(0, "give Shotgun");
  g->r.client_command(0, "use Shotgun");
  run(*g, 200);
  CHECK(p.hand.weapon == int(sv::SqWeapon::shotgun));
  CHECK(p.last_weapon == int(sv::SqWeapon::blaster));
  g->r.client_command(0, "weaplast");
  run(*g, 200);
  CHECK(p.hand.weapon == int(sv::SqWeapon::blaster));
}

TEST(wave_plays_a_gesture_standing_and_names_it) {
  auto g = game("0", 1);
  run(*g, 200);
  g->r.client_command(0, "wave 1");
  CHECK(printed(*g, 0, "salute"));
  run(*g, 10);
  const sv::SqPose& pose = g->p(0).pose;
  CHECK(pose.frame >= sv::sq_frames::kSalute.first && pose.frame <= sv::sq_frames::kSalute.last);
  g->r.client_command(0, "wave 9");  // past 4: point
  CHECK(printed(*g, 0, "point"));
}

TEST(the_lists_and_the_refusals) {
  auto g = game("1");
  run(*g, 10);
  g->r.client_command(0, "players");
  CHECK(printed(*g, 0, "  0 Ana\n"));
  CHECK(printed(*g, 0, "2 players"));
  g->r.client_command(0, "playerlist");
  CHECK(printed(*g, 0, " Ben\n"));
  g->r.client_command(0, "use Railgun");
  CHECK(printed(*g, 0, "You have no Railgun."));
  g->r.client_command(0, "use Body Armor");
  CHECK(printed(*g, 0, "Body Armor can't be used."));
  g->r.client_command(0, "drop Banana");
  CHECK(printed(*g, 0, "There is no item called Banana."));
}

TEST(the_intermission_takes_only_the_scores_and_the_chat) {
  auto g = game("1", 2, "cheats 1");
  run(*g, 10);
  g->s().intermission_at = g->s().level_time;
  g->r.client_command(0, "god");
  CHECK(!g->p(0).god);
  g->r.client_command(0, "say gg");
  CHECK(printed(*g, 1, "Ana: gg"));
  g->r.client_command(0, "unknownword");
  CHECK(!printed(*g, 1, "unknownword"));
}

TEST(an_empty_weapon_is_refused_unless_g_select_empty) {
  auto g = game("0", 1);
  run(*g, 10);
  sv::SequelPlayer& p = g->p(0);
  g->r.client_command(0, "give all");
  run(*g, 40);  // the hand settles
  const int shells = int(sv::SqAmmo::shells);
  p.body.ammo[shells] = 0;
  g->r.client_command(0, "use Shotgun");
  CHECK(printed(*g, 0, "No Shells for Shotgun."));
  CHECK(p.hand.pending != int(sv::SqWeapon::shotgun));
  g->r.console("g_select_empty 1");
  g->r.client_command(0, "use Shotgun");
  CHECK(p.hand.pending == int(sv::SqWeapon::shotgun));
}

TEST(quick_weapon_keys_skip_ahead_and_show_the_choice) {
  for (const char* quick : {"1", "0"}) {
    auto g = game("0", 1);
    g->r.console(std::string("g_quick_weap ") + quick);
    run(*g, 10);
    sv::SequelPlayer& p = g->p(0);
    g->r.client_command(0, "give all");
    run(*g, 40);
    const int held = p.hand.weapon;
    p.picked = 0;
    g->r.client_command(0, "weapnext");
    const int first = p.hand.pending;
    g->r.client_command(0, "weapnext");  // before the first is raised
    const int second = p.hand.pending;
    CHECK(first >= 0 && first != held);
    CHECK_MSG((second != first) == (quick[0] == '1'),
              std::string(quick) + ": " + std::to_string(first) + " " + std::to_string(second));
    CHECK((p.picked > 0) == (quick[0] == '1'));  // the bar shows the choice for 0.9 s
    if (quick[0] == '1') CHECK(g->s().level_time - p.picked_at > 2.0);
  }
}

TEST(g_swap_speed_lowers_and_raises_faster) {
  int frames[2] = {0, 0};
  for (int k = 0; k < 2; k++) {
    auto g = game("0", 1);
    g->r.console(k == 0 ? "g_swap_speed 1" : "g_swap_speed 4");
    run(*g, 10);
    sv::SequelPlayer& p = g->p(0);
    g->r.client_command(0, "give all");
    run(*g, 60);
    g->r.client_command(0, "use Railgun");
    const int64_t from = g->s().frame_number;
    while (p.hand.weapon != int(sv::SqWeapon::railgun) ||
           p.hand.state != sv::SqHandState::ready) {
      run(*g, 1);
      if (g->s().frame_number - from > 100) break;
    }
    frames[k] = static_cast<int>(g->s().frame_number - from);
  }
  CHECK_MSG(frames[1] < frames[0] && frames[1] >= 2,
            std::to_string(frames[0]) + " frames, then " + std::to_string(frames[1]));
}
