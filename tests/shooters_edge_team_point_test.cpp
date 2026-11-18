// What the player points at for team chat (games/shooters_edge/client/team_point.hpp): the
// choice by the view's line, sight and the pointing set, the team's words, and the codes and
// canned messages that read it (team_chat.hpp's %t %x %y %j, tp_msgpoint).
//
// ge:summary Tests pointing for team chat: the choice, the words, %t %x %y %j and tp_msgpoint
// ge:keywords test point pointing tp_msgpoint percent-codes team-chat point_at
// ge:skill shooter console testing
#include <string>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/team_chat.hpp"
#include "shooters_edge/client/team_point.hpp"

namespace cl = shooters_edge::client;
namespace hud = shooters_edge::hud;
using ge::console::Console;

namespace {

void expect(const std::string& got, const std::string& want) {
  CHECK_MSG(got == want, "got '" + got + "', not '" + want + "': read team_point.cpp's choice");
}

cl::PointThing item(const char* model, float x, float y, int skin = 0) {
  cl::PointThing t;
  t.model = model;
  t.skin = skin;
  t.origin = {x, y, -16};  // its middle level with the eye
  return t;
}

cl::PointThing player(const char* name, float x, float y, bool teammate) {
  cl::PointThing t;
  t.model = "progs/player.mdl";
  t.origin = {x, y, -30};
  t.player = true;
  t.teammate = teammate;
  t.name = name;
  return t;
}

// The eye at the origin looking along +x.
cl::PointScene scene() {
  cl::PointScene s;
  s.on = true;
  return s;
}

struct Fixture {
  Console con{false};
  cl::TeamChat chat;
  cl::PointScene now = scene();
  Fixture() {
    chat.send = [](const std::string&) { return true; };
    chat.scene = [this] { return now; };
    cl::add_team_chat(con, chat);
    con.execute("set name Sailor");
  }
  void frame(double t) {
    hud::Status s;
    s.t = t;
    s.health = 100;
    cl::team_frame(con, chat, s, "dm6");
  }
};

}  // namespace

TEST(the_thing_nearest_the_view_s_line_is_chosen) {
  Console con{false};
  cl::TeamChat chat;
  cl::add_team_chat(con, chat);
  cl::PointScene s = scene();
  s.things = {item("progs/armor.mdl", 400, 100, 2), item("progs/quaddama.mdl", 600, 20),
              item("progs/armor.mdl", -300, 0, 0)};  // behind the eye
  cl::Pointed p = cl::point_at(con, s, {});
  CHECK(p.any);
  expect(p.thing, "quad");
  // Out of sight: the next one in sight.
  const cl::ClearLine wall = [](ge::shooter::Vec3f, ge::shooter::Vec3f to) { return to.x < 500; };
  p = cl::point_at(con, s, wall);
  expect(p.thing, "ra");
  // Too far off the line (more than 1.7 times its distance ahead): nothing.
  s.things = {item("progs/armor.mdl", 20, 200, 1)};
  CHECK(!cl::point_at(con, s, {}).any);
  // Not in the set: a small health, and the weapons outside deathmatch 1.
  s.things = {item("maps/b_bh25.bsp", 200, 0), item("progs/g_rock2.mdl", 300, 0)};
  s.deathmatch = 3;
  CHECK(!cl::point_at(con, s, {}).any);
  s.deathmatch = 1;
  expect(cl::point_at(con, s, {}).thing, "rl");
}

TEST(players_are_named_by_side_and_glow) {
  Console con{false};
  cl::TeamChat chat;
  cl::add_team_chat(con, chat);
  cl::PointScene s = scene();
  cl::PointThing foe = player("Rex", 300, 0, false);
  foe.quaded = true;
  s.things = {foe};
  expect(cl::point_at(con, s, {}).thing, "quaded enemy");
  s.things = {player("Ann", 300, 0, true)};
  expect(cl::point_at(con, s, {}).thing, "Ann");
  con.execute("tp_name_teammate mate");
  expect(cl::point_at(con, s, {}).thing, "mate");
  s.things[0].model = "progs/eyes.mdl";
  s.things[0].teammate = false;
  expect(cl::point_at(con, s, {}).thing, "eyes");
  s.spectator = true;
  expect(cl::point_at(con, s, {}).thing, "Ann");
}

TEST(the_codes_and_tp_msgpoint_read_what_is_pointed_at) {
  Fixture f;
  auto fill = [&](const char* t) { return cl::fill_team_codes(f.con, f.chat, t); };
  f.frame(1);
  expect(fill("%t|%x|%y|%j"), "nothing|nothing|someplace|nothing");
  expect(cl::team_message(f.con, f.chat, "point"), "");
  f.now.things = {item("progs/armor.mdl", 400, 0, 1)};
  f.frame(2);
  expect(fill("%t|%x|%y|%j"), "ya at someplace|ya|someplace|ya at someplace");
  expect(cl::team_message(f.con, f.chat, "point"), "\rSai: ya at $[{%y}$] {%E}");
  // Remembered for %j after it is out of view, until tp_pointtimeout.
  f.now.things.clear();
  f.frame(10);
  expect(fill("%x|%j"), "nothing|ya at someplace");
  f.frame(20);
  expect(fill("%j"), "nothing");
  // An enemy, a teammate, a glowing enemy.
  f.now.things = {player("Rex", 300, 0, false)};
  f.frame(21);
  expect(cl::team_message(f.con, f.chat, "point"), "\rSai: {%E} enemy at $[{%y}$]");
  f.now.things = {player("Ann", 300, 0, true)};
  f.frame(22);
  expect(cl::team_message(f.con, f.chat, "point"), "\rSai: {&c0b0Ann&cfff} at $[{%y}$] {%E}");
  expect(cl::team_message(f.con, f.chat, "utake"), "\rSai: {&c0b0Ann&cfff} take $[{%l}$]");
  f.now.things = {player("Rex", 300, 0, false)};
  f.now.things[0].pented = true;
  f.frame(23);
  expect(cl::team_message(f.con, f.chat, "point"), "\rSai: enemy pent at $[{%y}$]");
  expect(cl::team_message(f.con, f.chat, "getpent"), "\rSai: enemy pent at $[{%y}$]");
}
