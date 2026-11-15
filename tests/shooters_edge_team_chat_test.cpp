// Team chat in a player's config (games/shooters_edge/client/team_chat.hpp): the percent codes
// from the player's state, the fun characters, and the f_ aliases the game's events run.
//
// ge:summary Tests team chat's percent codes, fun characters and the f_ event triggers
// ge:keywords test team-chat percent-codes fun-characters f_newmap f_death say_team
// ge:skill shooter console testing
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
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
