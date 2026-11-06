// Tests: the doors and plats of our programs (games/shooters_edge/qc/common/map.qc), on the
// campaign host with the arena's door ("*1") and plat ("*2"). Doors whose boxes touch open
// together unless spawnflag 4 keeps one apart; a key door's field opens it only for a player
// holding its key; a door with health opens to a blow; a named plat waits at its top until used.
// A door sounds as its `sounds` says, a key door its locked and unlocking sounds by the world's
// type, and a door with spawnflag 32 stays open until it is used again.
//
// ge:summary Tests: linked doors, key doors, shot doors and the named plat in our programs
// ge:keywords test door-group linked-doors key-door door-link shot-door named-plat plat-waits
// ge:keywords campaign-mod door-sounds locked-sound toggle-door
// ge:skill progs shooter testing
#include <string>

#include "check.hpp"
#include "shooters_edge_campaign_fixture.hpp"

using namespace campfix;

namespace {

constexpr float kBottom = 1;   // STATE_BOTTOM
constexpr float kTop = 0;      // STATE_TOP
constexpr float kUp = 2;       // STATE_UP
constexpr float kDown = 3;     // STATE_DOWN
constexpr float kWaiting = 4;  // STATE_WAITING
constexpr float kSilverKey = 131072;

std::string door(const std::string& keys) {
  return "{\n\"classname\" \"func_door\"\n\"model\" \"*1\"\n" + keys + "}\n";
}

// Frames enough for every door to find its group (0.1 s) and to settle.
void settle(Game& g) {
  for (int i = 0; i < 12; i++) CHECK_MSG(g.frame(0), g.error);
}

// The player stood just west of door `d`, inside its field, and some frames run.
void walk_up_to(Game& g, int d) {
  sh::Edict& p = g.s().sv.edicts[1];
  const sh::Edict& t = g.ed(d);
  p.origin = {t.absmin.x - 30, (t.absmin.y + t.absmax.y) / 2, 24};
  p.velocity = {};
  for (int i = 0; i < 6; i++) CHECK_MSG(g.frame(0), g.error);
}

// A string field of `e`, as text.
std::string text(Game& g, int e, const char* name) {
  const uint32_t w = *pg::entity_field(g.s().q.vm, e, pg::find_field(mod(), name)->offset);
  const char* t = pg::vm_string(g.s().q.vm, static_cast<int32_t>(w));
  return t ? t : "";
}

// The samples started on `e` since the frame began, joined by spaces.
std::string sounds_on(Game& g, int e) {
  std::string out;
  for (int i = 0; i < g.s().sv.sound_count; i++)
    if (g.s().sv.sounds[i].ent == e)
      out += (out.empty() ? "" : " ") + std::string(g.s().sv.sounds[i].sample);
  return out;
}

}  // namespace

TEST(doors_that_touch_open_together_unless_one_is_kept_apart) {
  const auto opened = [](const std::string& second) {
    Game g(door("\"targetname\" \"a\"\n") + door("\"targetname\" \"b\"\n" + second));
    settle(g);
    const int a = g.find("func_door");
    const int b = g.find("func_door", a);
    CHECK(a > 0 && b > 0);
    CHECK(g.named(a, "state") == kBottom && g.named(b, "state") == kBottom);
    CHECK(g.call("door_use", {}, a));
    CHECK(g.named(a, "state") == kUp);
    CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
    return g.named(b, "state");
  };
  CHECK(opened("") == kUp);                            // linked: both open
  CHECK(opened("\"spawnflags\" \"4\"\n") == kBottom);  // kept apart: only the one used
}

TEST(a_key_door_opens_only_for_a_player_with_its_key) {
  Game g(door("\"spawnflags\" \"16\"\n\"wait\" \"-1\"\n"));
  settle(g);
  const int d = g.find("func_door");
  walk_up_to(g, d);
  CHECK_MSG(g.named(d, "state") == kBottom, std::to_string(g.named(d, "state")));
  g.set(1, pg::find_field(mod(), "items")->offset, g.named(1, "items") + kSilverKey);
  walk_up_to(g, d);
  CHECK_MSG(g.named(d, "state") != kBottom, std::to_string(g.named(d, "state")));
  CHECK(g.named(1, "items") >= kSilverKey);  // the key is kept
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}

TEST(an_unnamed_door_opens_when_a_player_comes_near) {
  Game g(door(""));
  settle(g);
  const int d = g.find("func_door");
  walk_up_to(g, d);
  CHECK_MSG(g.named(d, "state") != kBottom, std::to_string(g.named(d, "state")));
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}

TEST(a_door_with_health_opens_to_a_blow_and_can_be_shot_again) {
  Game g(door("\"health\" \"1\"\n"));
  settle(g);
  const int d = g.find("func_door");
  walk_up_to(g, d);
  CHECK(g.named(d, "state") == kBottom);  // no field: a player near does not open it
  CHECK(g.hit(d, 1, 10));
  CHECK(g.named(d, "state") == kUp);
  CHECK(g.named(d, "health") == 1);
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}

TEST(a_named_plat_waits_at_its_top_until_it_is_used) {
  Game g(
      "{\n\"classname\" \"func_plat\"\n\"model\" \"*2\"\n\"targetname\" \"p\"\n\"height\" "
      "\"64\"\n}\n");
  settle(g);
  const int p = g.find("func_plat");
  const float top = g.ed(p).origin.z;
  CHECK(g.named(p, "state") == kWaiting);
  CHECK(top == g.named(p, "pos1_z"));
  CHECK(g.call("plat_use", {}, p));
  CHECK(g.named(p, "state") == kDown);
  for (int i = 0; i < kFrames * 4; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK_MSG(g.named(p, "state") == kBottom, std::to_string(g.named(p, "state")));
  CHECK_MSG(g.ed(p).origin.z < top, std::to_string(g.ed(p).origin.z) + " " + std::to_string(top) +
                                        " " + std::to_string(g.named(p, "pos2_z")));
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}

TEST(a_door_sounds_as_its_sounds_say_and_a_group_once) {
  const auto pair = [](const std::string& n) {
    Game g(door("\"targetname\" \"a\"\n\"sounds\" \"" + n + "\"\n"));
    settle(g);
    const int d = g.find("func_door");
    return text(g, d, "noise1") + " " + text(g, d, "noise2");
  };
  CHECK_MSG(pair("0") == "misc/null.wav misc/null.wav", pair("0"));
  CHECK_MSG(pair("1") == "doors/doormv1.wav doors/drclos4.wav", pair("1"));
  CHECK_MSG(pair("2") == "doors/hydro1.wav doors/hydro2.wav", pair("2"));
  CHECK_MSG(pair("3") == "doors/stndr1.wav doors/stndr2.wav", pair("3"));
  CHECK_MSG(pair("4") == "doors/ddoor1.wav doors/ddoor2.wav", pair("4"));
  // Two linked doors: the one used is the group's master's to sound, and it sounds once.
  Game g(door("\"targetname\" \"a\"\n\"sounds\" \"1\"\n") +
         door("\"targetname\" \"b\"\n\"sounds\" \"1\"\n"));
  settle(g);
  const int a = g.find("func_door");
  const int b = g.find("func_door", a);
  CHECK(g.call("door_use", {}, b));
  CHECK_MSG(sounds_on(g, a) + "|" + sounds_on(g, b) == "doors/doormv1.wav|",
            sounds_on(g, a) + "|" + sounds_on(g, b));
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}

TEST(a_key_door_sounds_locked_from_the_door_touched_and_unlocking_as_it_opens) {
  Game g(door("\"spawnflags\" \"8\"\n\"wait\" \"-1\"\n\"sounds\" \"4\"\n"));
  settle(g);
  const int d = g.find("func_door");
  CHECK_MSG(text(g, d, "noise3") == "doors/medtry.wav", text(g, d, "noise3"));  // world type 0
  CHECK_MSG(text(g, d, "noise") == "doors/meduse.wav", text(g, d, "noise"));
  CHECK_MSG(g.frame(0), g.error);
  g.s().sv.g.other = 1;                        // the player walks into it
  CHECK(g.call("door_trigger_touch", {}, d));  // walked into without the gold key
  CHECK_MSG(sounds_on(g, d) == "doors/medtry.wav", sounds_on(g, d));
  CHECK(g.named(d, "state") == kBottom);
  CHECK_MSG(g.frame(0), g.error);
  CHECK(g.call("door_use", {}, d));  // a trigger opens it all the same
  CHECK_MSG(sounds_on(g, d) == "doors/meduse.wav doors/ddoor1.wav", sounds_on(g, d));
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}

TEST(a_toggle_door_stays_open_until_it_is_used_again) {
  Game g(door("\"targetname\" \"t\"\n\"spawnflags\" \"32\"\n"));
  settle(g);
  const int d = g.find("func_door");
  const float shut = g.ed(d).origin.z;
  CHECK(g.call("door_use", {}, d));
  for (int i = 0; i < kFrames * 8; i++) CHECK_MSG(g.frame(0), g.error);  // well past its wait
  CHECK_MSG(g.named(d, "state") == kTop, std::to_string(g.named(d, "state")));
  CHECK(g.call("door_use", {}, d));
  CHECK(g.named(d, "state") == kDown);
  for (int i = 0; i < kFrames * 4; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK(g.named(d, "state") == kBottom);
  CHECK(g.ed(d).origin.z == shut);
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}
