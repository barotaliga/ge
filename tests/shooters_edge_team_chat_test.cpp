// Team chat in a player's config (games/shooters_edge/client/team_chat.hpp): the percent codes
// from the player's state, the fun characters, and the f_ aliases the game's events run.
//
// ge:summary Tests team chat's percent codes, fun characters and the f_ event triggers
// ge:keywords test team-chat percent-codes fun-characters f_newmap f_death say_team
// ge:keywords tp_msgreport canned-messages f_cfgload echo
// ge:skill shooter console testing
#include <algorithm>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/team_chat.hpp"

namespace cl = shooters_edge::client;
namespace hud = shooters_edge::hud;
using ge::console::Console;

namespace {

hud::Status rocketeer() {
  hud::Status s;
  s.t = 10;
  s.health = 34;
  s.armour = 25;
  s.armour_class = hud::ArmourClass::medium;
  s.ammo = {12, 0, 23, 30};  // shells, nails, rockets, cells
  s.weapon = hud::Weapon::rocket_launcher;
  s.owned = (1u << 0) | (1u << 5);  // the shotgun and the rocket launcher
  s.power_s[static_cast<size_t>(hud::Power::damage)] = 20;
  return s;
}

struct Fixture {
  Console con{false};
  cl::TeamChat chat;
  std::vector<std::string> sent;
  Fixture() {
    chat.send = [this](const std::string& line) {
      sent.push_back(line);
      return true;
    };
    cl::add_team_chat(con, chat);
  }
};

void expect(const std::string& got, const std::string& want) {
  CHECK_MSG(got == want, "got '" + got + "', not '" + want + "': read team_chat.cpp's codes");
}

}  // namespace

TEST(percent_codes_read_the_players_state) {
  Fixture f;
  cl::team_frame(f.con, f.chat, rocketeer(), "dm6");
  auto fill = [&](const char* t) { return cl::fill_team_codes(f.con, f.chat, t); };
  expect(fill("%h/%a %A %c"), "34/25 y 30");
  expect(fill("%w %b %p"), "rl:23 rl:23 quad");
  expect(fill("need %u"), "need armor/health");
  expect(fill("at %l, died %d"), "at someplace, died someplace");
  expect(fill("100%% %z %"), "100% %z %");
  expect(fill("[%<5r>h][%<4>a][%<3c>h]"), "[   34][25  ][34 ]");
  expect(fill("%[h] %[a] %[p]"),
         "\x10"
         "34\x11 \x10y:25\x11 \x10quad\x11");
  expect(fill("%e %o"), "%e %o");
  f.con.execute("tp_name_rl launcher; tp_weapon_order 2");
  expect(fill("%w %b"), "launcher:23 sg:12");
}

// In Quake II the weapon codes name its own weapons, by its keys (hud::KeyedWeapons).
TEST(the_weapon_codes_name_q2_s_own_weapons) {
  Fixture f;
  hud::Status s = rocketeer();
  s.keyed.set = true;
  s.keyed.in_hand = 8;                                  // the hyperblaster
  s.keyed.carried = (1u << 1) | (1u << 8) | (1u << 9);  // the blaster, it and the railgun
  s.keyed.ammo[1] = -1;
  s.keyed.ammo[8] = 50;
  s.keyed.ammo[9] = 7;
  cl::team_frame(f.con, f.chat, s, "q2dm1");
  auto fill = [&](const char* t) { return cl::fill_team_codes(f.con, f.chat, t); };
  expect(fill("%w %b"), "hb:50 rg:7");
  s.keyed.carried = 1u << 1;
  s.keyed.in_hand = 1;
  cl::team_frame(f.con, f.chat, s, "q2dm1");
  expect(fill("%w %b"), "blaster:0 blaster:0");
}

TEST(fun_characters_become_glyphs) {
  expect(cl::fun_characters("$[go$]", true), "\x10go\x11");
  expect(cl::fun_characters("$x41$x42$x00!", true), "AB !");
  expect(cl::fun_characters("$1$2 $$G $z $xg1 $", true), "\x13\x14 $G $z $xg1 $");
  expect(cl::fun_characters("^a", true), "^a");
  expect(cl::fun_characters("^a ^ ", false), "\xe1 ^ ");
}

TEST(say_team_sends_the_filled_line) {
  Fixture f;
  cl::team_frame(f.con, f.chat, rocketeer(), "dm6");
  f.con.execute("say_team \"%h health $[low$]\"");
  f.con.execute("set cl_parseSay 0; say %h");
  CHECK_MSG(f.sent.size() == 2, "two lines sent, not " + std::to_string(f.sent.size()));
  expect(f.sent[0], "say_team \"34 health \x10low\x11\"");
  expect(f.sent[1], "say \"%h\"");
}

TEST(events_run_the_players_aliases) {
  Fixture f;
  f.con.execute(
      "alias f_newmap \"echo new map\"; alias f_spawn \"echo spawned\";"
      "alias f_weaponchange \"echo weapon\"; alias f_death \"echo dead\";"
      "alias f_respawn \"echo back\"");
  f.con.clear_output();
  hud::Status s = rocketeer();
  cl::team_frame(f.con, f.chat, s, "dm6");
  f.con.run();
  s.health = 0;
  cl::team_frame(f.con, f.chat, s, "dm6");  // a death with no teammate: not run
  f.con.run();
  f.con.execute("tp_forceTriggers 1");
  s.health = 100;
  s.weapon = hud::Weapon::shotgun;
  cl::team_frame(f.con, f.chat, s, "dm6");
  f.con.run();
  s.health = -5;
  cl::team_frame(f.con, f.chat, s, "dm6");
  f.con.run();
  f.con.execute("tp_triggers 0");
  cl::team_frame(f.con, f.chat, s, "e1m1");
  f.con.run();
  std::string got;
  for (const std::string& l : f.con.output()) got += l + "|";
  expect(got, "new map|spawned|weapon|back|weapon|dead|");
}

TEST(an_item_the_server_says_was_taken_runs_f_bonusflash_once_a_frame) {
  Fixture f;
  f.con.execute("alias f_bonusflash \"echo gold\"");
  const hud::Status s = rocketeer();
  cl::team_frame(f.con, f.chat, s, "dm6", 0);
  cl::team_frame(f.con, f.chat, s, "dm6", 2);  // two in one frame: run once
  cl::team_frame(f.con, f.chat, s, "dm6", 2);  // none new
  cl::team_frame(f.con, f.chat, s, "dm6", 3);
  f.con.run();
  std::string got;
  for (const std::string& l : f.con.output()) got += l + "|";
  expect(got, "gold|gold|");
}

TEST(canned_messages_say_the_players_state) {
  Fixture f;
  f.con.execute("set name Sailor");
  cl::team_frame(f.con, f.chat, rocketeer(), "dm6");
  auto msg = [&](const char* w) { return cl::team_message(f.con, f.chat, w); };
  expect(msg("report"), "\rSai: quad {&cff0%a&cfff}/%h rl:23 $[{%l}$] {&c2aac&cfff}:30");
  expect(msg("need"), "\rSai: {&c0b0team&cfff} quad need %u $[{%l}$]");
  expect(msg("safe"), "\rSai: {&c0b0safe&cfff} {&c0b0[&cfff}{%l}{&c0b0]&cfff} {&cff0%a&cfff}/rl");
  expect(msg("killme"), "\rSai: {&cb1akill me [&cfff}{%l}{&cf2a]&cfff} rl:23 {&c2aac&cfff}:30");
  expect(msg("coming"), "\rSai: quad coming $[{%l}$]");
  expect(msg("took"), "");
  expect(msg("point"), "");
  f.con.execute("set nick cap; set tp_poweruptextstyle 1");
  expect(msg("help"), "\rcap: {&c03Fq&cfff} {&cff0help&cfff} {&cff0[&cfff}{%l}{&cff0]&cfff} {%e}");
  hud::Status dead = rocketeer();
  dead.health = 0;
  cl::team_frame(f.con, f.chat, dead, "dm6");
  expect(msg("report"), "\rcap: quad over {&cf00DROPPED} rl {&cf00[&cfff}{%d}{&cf00]&cfff} {%E}");
  expect(msg("safe"), "");
  // Sent filled in, as say_team sends it.
  cl::team_frame(f.con, f.chat, rocketeer(), "dm6");
  f.con.execute("tp_msgutake");
  CHECK_MSG(f.sent.size() == 1, "one line sent, not " + std::to_string(f.sent.size()));
  expect(f.sent.empty() ? "" : f.sent[0], "say_team \"\rcap: you take \x10{someplace}\x11\"");
}

TEST(echo_fills_in_and_cfg_load_runs_f_cfgload) {
  Fixture f;
  cl::team_frame(f.con, f.chat, rocketeer(), "dm6");
  f.con.clear_output();
  f.con.execute("echo %h health $[ok$] ^a");
  expect(f.con.output().empty() ? "" : f.con.output().back(), "34 health \x10ok\x11 \xe1");
  f.con.set_files(
      [&](std::string_view name, std::string& text) {
        if (name != "team.cfg") return false;
        text = "alias f_cfgload \"echo loaded\"\n";
        return true;
      },
      [](std::string_view, std::string_view) { return false; });
  f.con.clear_output();
  f.con.execute("cfg_load team; cfg_load missing");
  std::string got;
  for (const std::string& l : f.con.output()) got += l + "|";
  // Once, for the file that was there; the missing one runs nothing.
  CHECK_MSG(got.find("loaded|") == 0 && got.find("loaded", 1) == std::string::npos, got);
}

TEST(pickups_are_read_from_the_state) {
  Fixture f;
  f.con.execute("set name Sailor; alias f_took \"echo took\"; tp_forceTriggers 1");
  hud::Status s = rocketeer();
  cl::team_frame(f.con, f.chat, s, "dm6");
  auto fill = [&](const char* t) { return cl::fill_team_codes(f.con, f.chat, t); };
  expect(fill("%i|%X|%Y|%k"), "nothing|nothing|someplace|nothing at someplace");
  expect(cl::team_message(f.con, f.chat, "took"), "");
  s.t = 11;
  s.armour = 200;
  s.armour_class = hud::ArmourClass::heavy;
  cl::team_frame(f.con, f.chat, s, "dm6");
  expect(fill("%i|%X|%Y|%k"), "ra at someplace|ra|someplace|ra at someplace");
  expect(cl::team_message(f.con, f.chat, "took"), "\rSai: quad took ra $[{%Y}$]");
  s.t = 12;
  s.owned |= 1u << 1;  // the double shotgun: not one of the plain words
  cl::team_frame(f.con, f.chat, s, "dm6");
  expect(cl::team_message(f.con, f.chat, "took"), "\rSai: quad took {ssg} $[{%Y}$]");
  s.t = 13;
  s.ammo[2] += 5;
  s.ammo[3] += 6;  // two kinds at once: a pack
  cl::team_frame(f.con, f.chat, s, "dm6");
  expect(fill("%X"), "pack");
  s.t = 40;  // past tp_tooktimeout
  cl::team_frame(f.con, f.chat, s, "dm6");
  expect(fill("%i"), "nothing");
  f.con.run();
  int took = 0;
  for (const std::string& l : f.con.output()) took += l == "took" ? 1 : 0;
  CHECK_MSG(took == 3, "f_took ran " + std::to_string(took) + " times, not 3");
}

TEST(set_ex_fills_in_and_set_tp_takes_only_teamplay_settings) {
  Fixture f;
  cl::team_frame(f.con, f.chat, rocketeer(), "dm6");
  f.con.execute("set_ex said %h $[ok$]; set_ex2 said2 %h $[ok$]");
  CHECK(f.con.cvars().find("said")->value == "34 \x10ok\x11");
  CHECK(f.con.cvars().find("said2")->value == "34 $[ok$]");
  f.con.clear_problems();
  f.con.execute("set_tp tp_name_at near; set_tp cl_parseSay 0");
  CHECK(f.con.cvars().find("tp_name_at")->value == "near");
  CHECK(f.con.cvars().find("cl_parseSay")->value == "1" && !f.con.problems().empty());
  f.con.add_macro("health", [] { return std::string("34"); });
  f.con.clear_output();
  f.con.execute("macrolist");
  CHECK(!f.con.output().empty() && f.con.output().back().ends_with(" in all"));
  CHECK(std::find(f.con.output().begin(), f.con.output().end(), "$health") != f.con.output().end());
}

// The team's $ macros (team_macros.hpp) read what the codes read, in every classic game.
TEST(the_team_macros_name_the_players_state) {
  Fixture f;
  cl::team_frame(f.con, f.chat, rocketeer(), "dm6");
  const auto x = [&](const char* t) { return f.con.expand_text(t); };
  expect(x("$health/$armor $armortype $shells $rockets $cells $ammo"), "34/25 y 12 23 30 23");
  expect(x("$weapon $weaponnum [$weapons] $bestweapon:$bestammo"), "rl 7 [rl/sg/axe] rl:23");
  expect(x("$powerups|$colored_powerups|$colored_short_powerups|$tp_powerups"), "quad|quad|q|quad");
  f.con.execute("tp_poweruptextstyle 1");
  expect(x("$tp_powerups $need $location $took $point"),
         "q armor/health someplace nothing nothing");
  expect(x("$ledstatus $ledpoint $matchstatus $team1"), "$R $B disconnected team1");
  hud::Status s = rocketeer();
  s.keyed.set = true;  // Quake II: its own weapons by their words
  s.keyed.in_hand = 9;
  s.keyed.carried = (1u << 1) | (1u << 9);
  s.keyed.ammo[9] = 7;
  cl::team_frame(f.con, f.chat, s, "q2dm1");
  expect(x("$weapon $weaponnum $weapons $bestweapon $ammo"), "rg 9 rg/blaster rg 7");
}

// A server joined: the level's end and the countdown run their aliases; $team1 and $team2.
TEST(the_match_runs_f_mapend_and_the_countdowns) {
  Fixture f;
  f.con.execute(
      "alias f_mapend \"echo end\"; alias f_countdownstart \"echo count\";"
      "alias f_countdownbreak \"echo break\"");
  cl::DemoState s;
  s.names[0] = "me";
  s.names[1] = "Zed";
  s.names[2] = "amy";
  s.teams = {"red", "Blue", "red"};
  s.serverinfo = {{"teamplay", "2"}, {"status", "Standby"}};
  const hud::Status st = rocketeer();
  cl::team_frame(f.con, f.chat, st, "dm6", 0, &s);
  s.serverinfo[1].second = "Countdown";
  cl::team_frame(f.con, f.chat, st, "dm6", 0, &s);
  s.serverinfo[1].second = "Standby";
  cl::team_frame(f.con, f.chat, st, "dm6", 0, &s);
  s.intermission = true;
  cl::team_frame(f.con, f.chat, st, "dm6", 0, &s);
  cl::team_frame(f.con, f.chat, st, "dm6", 0, &s);
  f.con.clear_output();
  f.con.run();
  std::string got;
  for (const std::string& l : f.con.output()) got += l + "|";
  expect(got, "count|break|end|");
  expect(f.con.expand_text("$team1 $team2 $matchstatus"), "Blue red standby");
}

TEST(a_made_up_name_and_flood_protection) {
  Fixture f;
  cl::team_frame(f.con, f.chat, rocketeer(), "dm6");
  f.con.execute("cl_fakename Ace; say_team hi; say hi");
  expect(f.sent[0], "say_team \"\rAce: hi\"");
  expect(f.sent[1], "say \"hi\"");
  f.con.execute("cl_floodprot 1; cl_fp_messages 3; cl_fp_persecond 10");
  f.con.clear_output();
  f.con.execute("say a; say b; say c");
  CHECK_MSG(f.sent.size() == 3, "the third line in ten seconds is held back");
  CHECK(!f.con.output().empty() && f.con.output().back() == "Flood Protection");
}

TEST(a_death_with_the_flag_runs_f_flagdeath_in_f_deaths_place) {
  Fixture f;
  f.con.execute(
      "alias f_death \"echo dead\"; alias f_flagdeath \"echo flag\"; tp_forceTriggers 1;"
      "alias f_trackspectate \"echo track\"");
  cl::DemoState s;
  s.format = cl::DemoFormat::client;
  s.protocol = 28;
  s.player = 2;
  s.players.resize(32);
  s.names[2] = "me";
  hud::Status st = rocketeer();
  cl::team_frame(f.con, f.chat, st, "ctf1", 0, &s);
  s.players[2].effects = 16;  // the red flag's glow on the player
  cl::team_frame(f.con, f.chat, st, "ctf1", 0, &s);
  CHECK(cl::carries_flag(s));
  s.players[2].effects = 0;  // dropped in the frame he died in
  st.health = 0;
  cl::team_frame(f.con, f.chat, st, "ctf1", 0, &s);
  st.health = 100;
  cl::team_frame(f.con, f.chat, st, "ctf1", 0, &s);
  st.health = 0;  // a death with no flag
  cl::team_frame(f.con, f.chat, st, "ctf1", 0, &s);
  f.con.clear_output();
  f.con.run();
  f.con.execute("tp_triggers 0");
  cl::spectate_event(f.con, "f_trackspectate");  // the camera's hooks have no gate
  f.con.run();
  std::string got;
  for (const std::string& l : f.con.output()) got += l + "|";
  expect(got, "flag|dead|track|");
  // Quake II's flag on the player's entity, and a Team Fortress key among the items.
  cl::DemoState q2;
  q2.protocol = 34;
  q2.player = 0;
  q2.entities.resize(4);
  q2.entities[1].effects = 1 << 19;
  CHECK(cl::carries_flag(q2));
  cl::DemoState tf;
  tf.format = cl::DemoFormat::client;
  tf.players.resize(32);
  tf.game = "fortress";
  tf.stats[cl::kStatItems] = 131072;
  CHECK(cl::carries_flag(tf));
  tf.game = "qw";
  CHECK(!cl::carries_flag(tf));
}
