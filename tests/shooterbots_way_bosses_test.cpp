// The way through's shot breakables and doors (plugins/shooterbots/way_bosses.hpp): a power box
// whose relay removes a fast push (hangar1's duct, a 500 units/s current into a turning fan, ends
// only when the box is shot) is found through its relays, and a slow push or one nothing removes is
// not; a door a shot opens is one of the fighter's shot doors, a breakable or a trigger is not.
//
// ge:summary Tests for the breakable that calms a current and the door a shot opens
// ge:keywords test way_bosses way_calmed_push way_shot_door trigger_push killtarget hangar1
// ge:keywords shot-door current
// ge:skill shooterbots testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/way_bosses.hpp"

namespace sb = ge::shooterbots;

namespace {

std::vector<sb::EntityBlock> blocks_of(const std::string& text) {
  std::vector<sb::EntityBlock> blocks;
  std::string error;
  CHECK_MSG(sb::parse_entity_text(text.c_str(), blocks, error), error);
  return blocks;
}

// hangar1's duct, cut down: the box fires t5, whose relay removes the push t6 (speed `speed`).
std::string duct(const char* speed) {
  return std::string(R"({
"classname" "worldspawn"
}
{
"classname" "func_explosive"
"model" "*30"
"health" "20"
"target" "t5"
}
{
"classname" "target_speaker"
"targetname" "t5"
}
{
"classname" "trigger_relay"
"targetname" "t5"
"target" "t6"
"killtarget" "t6"
}
{
"classname" "trigger_push"
"targetname" "t6"
"model" "*29"
"speed" ")") +
         speed + R"("
}
)";
}

}  // namespace

TEST(a_box_whose_relay_removes_a_fast_push_calms_it) {
  const std::vector<sb::EntityBlock> blocks = blocks_of(duct("50"));
  const char* push = sb::way_calmed_push(blocks, "t5");
  CHECK(push != nullptr);
  CHECK(push != nullptr && std::string(push) == "*29");
}

TEST(a_slow_push_or_a_target_that_removes_none_is_no_current) {
  CHECK(sb::way_calmed_push(blocks_of(duct("20")), "t5") == nullptr);  // 200 units/s: walked
  CHECK(sb::way_calmed_push(blocks_of(duct("50")), "t6") == nullptr);  // the push's own name
  CHECK(sb::way_calmed_push(blocks_of(duct("50")), nullptr) == nullptr);
}

TEST(a_door_a_shot_opens_is_a_shot_door_and_a_breakable_or_trigger_is_not) {
  sb::Fighter f;
  sb::ButtonSpot door;  // way_complete_init's shot door: its own one door
  door.ent = 7;
  door.shoot = true;
  door.doors = {7};
  f.button_spots.push_back(door);
  sb::ButtonSpot box = door;  // a breakable that calms a current
  box.ent = 8;
  box.doors = {8};
  box.model = "*30";
  box.calm_lo = {-362, -1024, 882};
  box.calm_hi = {-242, -960, 928};
  f.button_spots.push_back(box);
  sb::ButtonSpot trigger = door;  // a shot trigger, kept by its model
  trigger.ent = 9;
  trigger.doors = {9};
  trigger.model = "*12";
  f.button_spots.push_back(trigger);
  sb::ButtonSpot button = door;  // a pressed button that opens door 7
  button.ent = 10;
  button.shoot = false;
  button.doors = {7};
  f.button_spots.push_back(button);
  CHECK(sb::way_shot_door(f, 7));
  CHECK(!sb::way_shot_door(f, 8));
  CHECK(!sb::way_shot_door(f, 9));
  CHECK(!sb::way_shot_door(f, 10));
  CHECK(!sb::way_shot_door(f, -1));
}
